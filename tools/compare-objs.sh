#!/bin/bash
# Build dkms/ at two git revisions and compare the generated machine code.
#
# Used to prove that a cleanup/refactor (renames, macros, moved code) does not
# change behaviour (RULES.md C9). Identical output means identical code.
#
# Usage: tools/compare-objs.sh [OLD_REV] [NEW_REV|WORKTREE]
#   OLD_REV  defaults to origin/dev
#   NEW_REV  defaults to WORKTREE (the current, possibly uncommitted, dkms/)
# Env:     KDIR (default: /lib/modules/$(uname -r)/build, falls back to
#          the newest /usr/src/linux-headers-*-generic)
set -euo pipefail

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
		sed -E 's/^[[:space:]]*[0-9a-f]+:[[:space:]]+//; s/[0-9a-f]+ <([^>+]+)(\+0x[0-9a-f]+)?>/<\1>/g; s/(\.[a-z.0-9_]+)\+0x[0-9a-f]+/\1+X/g' |
		awk '/^[0-9a-f]+ <.*>:$/ { name=$2; next } NF { print name "\t" $0 }' |
		sort -s -k1,1 >"$2.code"
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
