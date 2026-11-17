#!/bin/bash
# Build dkms/ at two git revisions and compare the generated machine code.
#
# Used to prove that a cleanup/refactor (renames, macros, moved code) does not
# change behaviour (RULES.md C9). Identical output means identical code.
#
# Usage: tools/compare-objs.sh [--relaxed] [--alias old=new ...] [OLD_REV] [NEW_REV|WORKTREE]
#   --alias    rename symbol `old` to `new` in OLD_REV's dump (renamed functions)
#   --relaxed  compare, per function, only the ordered list of call targets
#              and immediate constants ($0x..). Register allocation and stack
#              layout may differ. Use it when strict mode reports differences
#              that are only codegen noise. Strict mode is the default and the
#              stronger proof.
#   OLD_REV  defaults to origin/dev
#   NEW_REV  defaults to WORKTREE (the current, possibly uncommitted, dkms/)
# Env:     KDIR (default: /lib/modules/$(uname -r)/build, falls back to
#          the newest /usr/src/linux-headers-*-generic)
set -euo pipefail

RELAXED=0
ALIASES=()
while [ $# -gt 0 ]; do
	case "$1" in
	--relaxed) RELAXED=1; shift ;;
	--alias) ALIASES+=("$2"); shift 2 ;;
	*) break ;;
	esac
done
OLD=${1:-origin/dev}
NEW=${2:-WORKTREE}
TOP=$(git rev-parse --show-toplevel)
KDIR=${KDIR:-/lib/modules/$(uname -r)/build}
if [ ! -d "$KDIR" ]; then
	KDIR=$(ls -d /usr/src/linux-headers-*-generic 2>/dev/null | sort -V | tail -1)
fi
[ -d "$KDIR" ] || { echo "no kernel headers found; set KDIR" >&2; exit 2; }

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

export_tree() { # rev dest
	mkdir -p "$2"
	if [ "$1" = WORKTREE ]; then
		cp -r "$TOP/dkms/." "$2/"
	else
		git -C "$TOP" archive "$1" dkms | tar -x -C "$2" --strip-components=1
	fi
}

dump() { # dir out
	make -s -C "$KDIR" M="$1" modules >"$1/build.log" 2>&1 ||
		{ cat "$1/build.log"; exit 1; }
	# Code: one line per instruction, prefixed with its function, sorted by
	# function so that moving a function between .c files is not a
	# difference. Addresses and offsets are dropped.
	objdump -d --no-show-raw-insn -r "$1/elgato_gchd.ko" |
		grep -v 'file format' |
		awk '/^[0-9a-f]+ <[^>]+>:$/ { name=$2; next } NF { print name "\t" $0 }' |
		sed -E 's/\t[[:space:]]*[0-9a-f]+:[[:space:]]+/\t/; s/[0-9a-f]+ <([^>+]+)(\+0x[0-9a-f]+)?>/<\1>/g; s/(\.[a-z.0-9_]+)\+0x[0-9a-f]+/\1+X/g' |
		sort -s -t "$(printf '\t')" -k1,1 >"$2.code"
	if [ $RELAXED = 1 ]; then
		# Keep only call targets and immediates, in order, per function.
		awk -F'\t' '{
			line=$2
			if (line ~ /call/) { match(line, /<[^>]+>/); if (RSTART) print $1 "\tcall " substr(line, RSTART, RLENGTH); next }
			while (match(line, /\$0x[0-9a-f]+/)) { print $1 "\t" substr(line, RSTART, RLENGTH); line = substr(line, RSTART + RLENGTH) }
		}' "$2.code" >"$2.code.tmp" && mv "$2.code.tmp" "$2.code"
	fi
	# Data: printable strings of all .rodata* sections, with source line
	# numbers (from FORTIFY/WARN strings) normalised.
	: >"$2.data"
	for sec in $(objdump -h "$1/elgato_gchd.ko" | awk '$2 ~ /^\.rodata/ {print $2}'); do
		objcopy -O binary --only-section="$sec" "$1/elgato_gchd.ko" "$1/sec.bin"
		strings -n 3 "$1/sec.bin" | sed -E 's/\.([ch]):[0-9]+/.\1:N/g' >>"$2.data"
	done
	sort -o "$2.data" "$2.data"
}

# Build both trees in the same directory so that embedded paths are equal.
export_tree "$OLD" "$WORK/old"
export_tree "$NEW" "$WORK/new"
mkdir "$WORK/build"
cp -r "$WORK/old/." "$WORK/build/"; dump "$WORK/build" "$WORK/old.dump"
rm -rf "$WORK/build"; mkdir "$WORK/build"
cp -r "$WORK/new/." "$WORK/build/"; dump "$WORK/build" "$WORK/new.dump"

for a in "${ALIASES[@]+"${ALIASES[@]}"}"; do
	o=${a%%=*}; n=${a#*=}
	sed -i -E "s/(^|[^A-Za-z0-9_]|__pfx_)${o}([^A-Za-z0-9_]|\$)/\\1${n}\\2/g" "$WORK/old.dump.code" "$WORK/old.dump.data"
done
sort -s -t "$(printf '\t')" -k1,1 -o "$WORK/old.dump.code" "$WORK/old.dump.code"

status=0
if ! diff -u "$WORK/old.dump.code" "$WORK/new.dump.code" >"$WORK/code.diff"; then
	echo "CODE DIFFERS ($(grep -c '^[-+][^-+]' "$WORK/code.diff") lines):"
	head -200 "$WORK/code.diff"
	status=1
fi
if ! diff -q "$WORK/old.dump.data" "$WORK/new.dump.data" >/dev/null; then
	echo "STRING/RODATA DIFFERS (log messages or tables changed; review):"
	diff "$WORK/old.dump.data" "$WORK/new.dump.data" | head -50
	status=1
fi
[ $status = 0 ] && echo "IDENTICAL: $OLD vs $NEW generate the same code and data."
exit $status
