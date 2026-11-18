#!/bin/bash
# gchd-test.sh - interactive hardware test runner for the elgato_gchd driver.
#
#   bash gchd-test.sh TESTNAME [--branch BRANCH] [--no-build]
#
# TESTNAME is a file in tests/ next to this script (tests/TESTNAME.sh).
# The script builds and loads the module, runs the steps of the test file,
# checks the captured stream automatically and writes everything to one log
# file. It stops only for physical actions and visual checks.
#
# Requirements: v4l2-ctl (v4l-utils), ffmpeg/ffprobe/ffplay, sudo.
# Environment: GCHD_REPO (default ~/elgato-gchd), GCHD_DEV (default
# /dev/video0).

set -u

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO=${GCHD_REPO:-$HOME/elgato-gchd}
DEV=${GCHD_DEV:-/dev/video0}
TEST=${1:-}
BRANCH=
BUILD=1
shift || true
while [ $# -gt 0 ]; do
	case "$1" in
	--branch) BRANCH=$2; shift 2 ;;
	--no-build) BUILD=0; shift ;;
	*) echo "unknown option $1"; exit 2 ;;
	esac
done
if [ -z "$TEST" ] || [ ! -f "$HERE/tests/$TEST.sh" ]; then
	echo "usage: bash gchd-test.sh TESTNAME [--branch BRANCH] [--no-build]"
	echo "tests:"; ls "$HERE/tests" 2>/dev/null | sed 's/\.sh$//; s/^/  /'
	exit 2
fi

STAMP=$(date +%Y%m%d-%H%M%S)
LOGDIR=$HOME/gchd-test-logs
WORK=$LOGDIR/$TEST-$STAMP
LOG=$WORK.log
mkdir -p "$WORK"
STEP=0
STEP_NAME=
RESULTS=()

# ---------------------------------------------------------------- output

B=$'\e[1m'; R=$'\e[31m'; G=$'\e[32m'; Y=$'\e[33m'; N=$'\e[0m'

log()  { printf '%s\n' "$*" >> "$LOG"; }
say()  { printf '%s\n' "$*"; log "$*"; }
result() {	# result PASS|FAIL|WARN text
	local c=$G; [ "$1" = FAIL ] && c=$R; [ "$1" = WARN ] && c=$Y
	printf '%s[%s]%s %s\n' "$c" "$1" "$N" "$2"
	log "[$1] $2"
	RESULTS+=("$1 | step $STEP $STEP_NAME | $2")
}

# Physical action: show the instruction, wait for Enter.
prompt() {
	printf '\n%s>>> %s%s\n    Press Enter when done.' "$B" "$*" "$N"
	read -r _
	log "PROMPT: $*"
}

# Visual check: y/n plus an optional note; recorded as PASS/FAIL.
confirm() {
	local a note
	while :; do
		printf '\n%s??? %s [y/n]%s ' "$B" "$*" "$N"
		read -r a
		case "$a" in y|Y|n|N) break ;; esac
	done
	printf '    Note (optional, Enter to skip): '
	read -r note
	if [ "$a" = y ] || [ "$a" = Y ]; then
		result PASS "$*${note:+ -- $note}"
	else
		result FAIL "$*${note:+ -- $note}"
	fi
}

# Run a command; command and output go to the log, output also to the screen.
run() {
	log "\$ $*"
	"$@" 2>&1 | tee -a "$LOG"
	return "${PIPESTATUS[0]}"
}

# ---------------------------------------------------------------- module

build() {
	say "== build"
	cd "$REPO" || exit 1
	run git fetch -q origin
	if [ -n "$BRANCH" ]; then
		run git checkout -q "$BRANCH" || exit 1
	fi
	run git pull -q --ff-only
	log "commit: $(git log -1 --oneline)"
	say "commit: $(git log -1 --oneline)"
	if [ "$BUILD" = 1 ]; then
		make -C "/lib/modules/$(uname -r)/build" M="$REPO/dkms" clean modules W=1 \
			> "$WORK/build.txt" 2>&1
		local rc=$? w
		w=$(grep "warning:" "$WORK/build.txt" | grep -vc "compiler differs")
		log "build rc=$rc warnings=$w"
		grep -E "warning:|error:" "$WORK/build.txt" | grep -v "compiler differs" >> "$LOG"
		if [ $rc -ne 0 ]; then
			say "build failed, see $WORK/build.txt"; exit 1
		fi
	fi
}

# load_module [replug] [module parameters...]
load_module() {
	local replug=0
	if [ "${1:-}" = replug ]; then replug=1; shift; fi
	sudo rmmod elgato_gchd 2>/dev/null
	if [ $replug = 1 ]; then
		prompt "Unplug the Elgato from the PC (or detach it in VirtualBox), wait 3 s, plug it in again."
	fi
	sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
	run sudo insmod "$REPO/dkms/elgato_gchd.ko" "$@" || { result FAIL "insmod $*"; return 1; }
	local i
	for i in $(seq 1 20); do [ -e "$DEV" ] && break; sleep 0.5; done
	[ -e "$DEV" ] || { result FAIL "$DEV did not appear"; return 1; }
	log "module: $(modinfo -F version "$REPO/dkms/elgato_gchd.ko") $*"
}

# ---------------------------------------------------------------- steps

DMESG_NOISE='bulk-IN data|START poll=|firmware chunk|SCMD write succeeded|SCMD interrupt|callbacks suppressed|bulk-IN first reads|no analog family|pre-config raw_status|retrying detection|open requested by pid'
DMESG_BAD='signal lost|START failed|state-change timeout|SCMD timeout|completed 10 times|failed:|failed at step|ring_dropped=[1-9]|urb_errors=[1-9]|BUG|Oops|WARNING'

# step NAME: start a step (clears dmesg).
step() {
	end_step
	STEP=$((STEP + 1))
	STEP_NAME=$*
	printf '\n%s=== step %d: %s%s\n' "$B" "$STEP" "$*" "$N"
	log ""
	log "=== step $STEP: $*"
	sudo dmesg -C
}

# end_step: dmesg of the step into the log, failure lines flagged.
end_step() {
	[ $STEP -eq 0 ] && return
	local d bad
	d=$(sudo dmesg | grep -vE "$DMESG_NOISE")
	log "--- dmesg step $STEP"
	printf '%s\n' "$d" >> "$LOG"
	bad=$(printf '%s\n' "$d" | grep -E "$DMESG_BAD" | head -5)
	if [ -n "$bad" ]; then
		result WARN "dmesg: $(printf '%s' "$bad" | head -1 | sed 's/^\[[^]]*\] //')"
	fi
	sudo dmesg -C
}

# v4l2 report (input, format, rate, standard).
report() {
	run v4l2-ctl -d "$DEV" --get-input --get-fmt-video --get-parm \
		--get-standard --get-detected-standard 2>&1 | grep -vE "Transfer|YCbCr|Quantization|Flags|Bytes per|Colorspace|Read buffers"
}

# capture SECONDS [WIDTH HEIGHT FPS FIELD] [during=DELAY:COMMAND]: read the
# MPEG stream with read(), check it with ffprobe. FPS as ffprobe prints
# r_frame_rate: the field rate for interlaced streams (50/1, 60000/1001),
# the frame rate for progressive ones (25/1, 30000/1001); FIELD progressive
# or tt (top first) or
# "-" to skip. during=: run COMMAND DELAY seconds into the capture.
capture() {
	local secs=$1 f="$WORK/step$STEP-$RANDOM.ts" info errs during= kbps
	shift
	if [ $# -gt 0 ]; then
		local last=${*: -1}
		case "$last" in
		during=*) during=${last#during=}; set -- "${@:1:$(($# - 1))}" ;;
		esac
	fi
	say "capturing ${secs}s to $(basename "$f")"
	if [ -n "$during" ]; then
		( sleep "${during%%:*}"; log "during capture: ${during#*:}"; eval "${during#*:}" >> "$LOG" 2>&1 ) &
	fi
	timeout -k 5 $((secs + 20)) bash -c \
		"head -c \$(( $secs * 4000000 )) < '$DEV' > '$f' & p=\$!; sleep $secs; kill \$p 2>/dev/null; wait \$p" 2>/dev/null
	wait
	if [ ! -s "$f" ]; then
		# A console that is still starting gives no data: one retry.
		say "  no data; retrying once (source may still be starting)"
		timeout -k 5 $((secs + 30)) bash -c \
			"head -c \$(( $secs * 4000000 )) < '$DEV' > '$f' & p=\$!; sleep $((secs + 10)); kill \$p 2>/dev/null; wait \$p" 2>/dev/null
	fi
	if [ ! -s "$f" ]; then
		result FAIL "capture: no data (also after a retry)"
		return 1
	fi
	info=$(ffprobe -v error -select_streams v:0 -show_entries \
		stream=codec_name,width,height,field_order,r_frame_rate -of default=nw=1 "$f" | head -5 | tr '\n' ' ')
	errs=$(ffmpeg -v error -i "$f" -f null - 2>&1 | wc -l)
	kbps=$(( $(stat -c %s "$f") * 8 / 1000 / secs ))
	log "capture: $(stat -c %s "$f") bytes, ~$kbps kbit/s; $info; decode errors $errs"
	say "  $info; ~$kbps kbit/s; decode errors $errs"
	ffprobe -v error -select_streams a:0 -show_entries stream=codec_name -of csv=p=0 "$f" | grep -q mp2 \
		&& log "audio: mp2" || result FAIL "capture: no audio stream"
	if [ $# -ge 4 ]; then
		local ok=1
		echo "$info" | grep -q "width=$1 " || ok=0
		echo "$info" | grep -q "height=$2 " || ok=0
		echo "$info" | grep -q "r_frame_rate=$3 " || ok=0
		[ "$4" = - ] || echo "$info" | grep -q "field_order=$4" || ok=0
		if [ $ok = 1 ]; then result PASS "stream $1x$2 $3 $4, ~$kbps kbit/s"
		else result FAIL "stream expected $1x$2 $3 $4, got: $info"; fi
	fi
	[ "$errs" -gt 20 ] && result WARN "capture: $errs decode errors"
	return 0
}

# play SECONDS QUESTION: show the stream in ffplay, then ask.
play() {
	local secs=$1; shift
	say "ffplay for ${secs}s"
	timeout -k 5 "$secs" ffplay -loglevel error -f mpegts "$DEV" >> "$LOG" 2>&1
	confirm "$*"
}

# set_fmt WIDTH HEIGHT [PIXFMT]
set_fmt() {
	run v4l2-ctl -d "$DEV" -v "width=$1,height=$2,pixelformat=${3:-MPEG}"
}

# expect DESCRIPTION REGEX COMMAND...: PASS when the output matches REGEX
# (grep -E), FAIL otherwise.
expect() {
	local desc=$1 re=$2 out
	shift 2
	log "\$ $*"
	out=$("$@" 2>&1)
	printf '%s\n' "$out" >> "$LOG"
	if printf '%s\n' "$out" | grep -qE "$re"; then
		result PASS "$desc"
	else
		result FAIL "$desc (expected /$re/, got: $(printf '%s' "$out" | tr '\n' ' ' | cut -c1-160))"
	fi
}

# set_ctrl NAME=VALUE[,NAME=VALUE...] (v4l2-ctl control names)
set_ctrl() {
	run v4l2-ctl -d "$DEV" -c "$1"
}

set_input() {
	run v4l2-ctl -d "$DEV" -i "$1"
}

# ---------------------------------------------------------------- main

trap 'echo; echo "interrupted; log: $LOG"; exit 130' INT
sudo -v || exit 1
log "gchd-test $TEST $STAMP host $(uname -r)"
say "log: $LOG"
build
# shellcheck source=/dev/null
. "$HERE/tests/$TEST.sh"
end_step

{
	echo
	echo "=== summary ($TEST, $(cd "$REPO" && git log -1 --oneline))"
	printf '%s\n' "${RESULTS[@]}"
} | tee -a "$LOG"
fails=$(printf '%s\n' "${RESULTS[@]}" | grep -c '^FAIL')
warns=$(printf '%s\n' "${RESULTS[@]}" | grep -c '^WARN')
echo
echo "${B}FAIL: $fails  WARN: $warns${N}"
echo "Paste into TASK.md: the summary above, or the whole log: $LOG"
