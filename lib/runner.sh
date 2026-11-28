# SPDX-License-Identifier: GPL-2.0
# shellcheck shell=bash
# Runner of the test suite: output and log, results, questions to the
# tester, the device, captures, the kernel log. Sourced by gchd-test.

B=$'\e[1m'; R=$'\e[31m'; G=$'\e[32m'; Y=$'\e[33m'; C=$'\e[36m'; N=$'\e[0m'

RESULTS=()	# "PASS|FAIL|WARN|SKIP | part | case | text"
PART=		# part running
CASE=		# case running
MODE=fast	# fast or deep
DEV=
LOG=/dev/null
WORK=

log()  { printf '%s\n' "$*" >> "$LOG"; }
say()  { printf '%s\n' "$*"; log "$*"; }

# result PASS|FAIL|WARN|SKIP TEXT
result() {
	local c=$G
	case "$1" in FAIL) c=$R ;; WARN|SKIP) c=$Y ;; esac
	printf '%s[%s]%s %s\n' "$c" "$1" "$N" "$2"
	log "[$1] $2"
	RESULTS+=("$1 | $PART | $CASE | $2")
}

# ------------------------------------------------------------ the tester

# prompt TEXT: an action of the tester (cable, source, program); Enter.
prompt() {
	printf '\n%s>>> %s%s\n    Press Enter when done.' "$B" "$*" "$N"
	read -r _
	log "PROMPT: $*"
	debug_on
}

# confirm QUESTION: what only the tester can see; y/n and a note.
confirm() {
	local a note
	while :; do
		printf '\n%s??? %s [y/n]%s ' "$B" "$*" "$N"
		read -r a
		case "$a" in y|Y|n|N) break ;; esac
	done
	printf '    Note (optional, Enter to skip): '
	read -r note
	case "$a" in
	y|Y) result PASS "$*${note:+ -- $note}" ;;
	*) result FAIL "$*${note:+ -- $note}" ;;
	esac
}

# ask_yn QUESTION: true for yes (no result recorded).
ask_yn() {
	local a
	while :; do
		printf '%s%s [y/n]%s ' "$C" "$*" "$N"
		read -r a
		case "$a" in y|Y) log "ASKED: $* -> yes"; return 0 ;;
		n|N) log "ASKED: $* -> no"; return 1 ;; esac
	done
}

# Sources and equipment the tester has: have KEY QUESTION (asked once per
# run), needs KEY...: false (and the case SKIP) when one is missing.
declare -A HAVE
have() {
	[ -n "${HAVE[$1]:-}" ] && return 0
	if ask_yn "$2"; then HAVE[$1]=1; else HAVE[$1]=0; fi
}
needs() {
	local k
	for k in "$@"; do
		if [ "${HAVE[$k]:-0}" != 1 ]; then
			result SKIP "needs: $k"
			return 1
		fi
	done
}

# ------------------------------------------------------------ commands

# run COMMAND...: command and output into the log, output on the screen.
run() {
	local r
	log "\$ $*"
	"$@" 2>&1 | tee -a "$LOG"
	r=${PIPESTATUS[0]}
	debug_on
	return "$r"
}

# expect DESCRIPTION REGEX COMMAND...: PASS when the output matches.
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

# check DESCRIPTION CONDITION [VALUES]: PASS when the awk condition holds
# (numbers measured by the suite); VALUES go into the result.
check() {
	if awk "BEGIN { exit !($2) }" 2>/dev/null; then
		result PASS "$1${3:+ ($3)}"
	else
		result FAIL "$1${3:+ ($3)}"
	fi
}

# v CONTROL: a control's value (number only).
v() { v4l2-ctl -d "$DEV" -C "$1" 2>/dev/null | sed -n 's/^[^:]*: \(-\{0,1\}[0-9]*\).*/\1/p'; }
# ctl NAME=VALUE[,...]: set controls (logged).
ctl() { run v4l2-ctl -d "$DEV" -c "$1"; }
# ctl_range CONTROL: "min max default" of a control.
ctl_range() {
	v4l2-ctl -d "$DEV" -l 2>/dev/null | sed -n "s/^ *$1 .*min=\(-\{0,1\}[0-9]*\) max=\(-\{0,1\}[0-9]*\).*default=\(-\{0,1\}[0-9]*\).*/\1 \2 \3/p"
}
# size WIDTH HEIGHT [PIXFMT]: the output size (and format).
size() { run v4l2-ctl -d "$DEV" -v "width=$1,height=$2,pixelformat=${3:-MPEG}"; }
# input N: select an input (0 HDMI, 1 component, 2 composite, 3 S-video).
input() { run v4l2-ctl -d "$DEV" -i "$1"; }
cur_input() { v4l2-ctl -d "$DEV" -I 2>/dev/null | sed -n 's/^Video input *: *\([0-9]\).*/\1/p'; }
cur_size() { v4l2-ctl -d "$DEV" -V 2>/dev/null | sed -n 's#.*Width/Height *: *\([0-9]*\)/\([0-9]*\).*#\1x\2#p'; }

# ------------------------------------------------------------ driver

DEBUG_P=/sys/module/elgato_gchd/parameters/debug
# The deep test reads the driver's detailed messages (debug parameter):
# on whenever the driver may have been loaded again; off at the end. The
# fast test uses the driver as installed.
debug_on() {
	[ "$MODE" = deep ] || return 0
	[ -e $DEBUG_P ] && [ "$(cat $DEBUG_P)" != Y ] && echo 1 | sudo tee $DEBUG_P >/dev/null
	return 0
}
debug_off() {
	[ -e $DEBUG_P ] && echo 0 | sudo tee $DEBUG_P >/dev/null
	return 0
}

# find_dev: the driver's video device into DEV (GCHD_DEV overrides).
find_dev() {
	local v name
	[ -n "${GCHD_DEV:-}" ] && { DEV=$GCHD_DEV; return 0; }
	for v in /sys/class/video4linux/video*; do
		[ -e "$v/name" ] || continue
		[ "$(basename "$(readlink -f "$v/device/driver")")" = elgato_gchd ] || continue
		name=$(cat "$v/name")
		case "$name" in *"(H.264)"|"Elgato Game Capture HD") DEV=/dev/$(basename "$v"); return 0 ;; esac
	done
	return 1
}

# wait_dev [SECONDS]: wait for the device (after a replug or reload).
wait_dev() {
	local _
	for _ in $(seq 1 $((${1:-15} * 2))); do
		find_dev && [ -e "$DEV" ] && return 0
		sleep 0.5
	done
	return 1
}

# The device's USB product ID (005d, 0044, 004e, 0051).
usb_id() {
	local d
	for d in /sys/bus/usb/devices/*; do
		[ "$(cat "$d/idVendor" 2>/dev/null)" = 0fd9 ] || continue
		cat "$d/idProduct"; return 0
	done
	echo none
}

driver_version() { modinfo -F version elgato_gchd 2>/dev/null || echo "not installed"; }

# The kernel log is read, never cleared: KMARK is its length when the case
# (or kmark) started; ksince prints what came after.
KMARK=0
kmark() { KMARK=$(sudo dmesg | wc -l); }
ksince() { sudo dmesg | tail -n +$((KMARK + 1)); }
# kmsg: the driver's kernel messages since the mark.
kmsg() { ksince | grep elgato_gchd | sed 's/^\[[^]]*\] //'; }
# kcount REGEX: how many of them match.
kcount() { kmsg | grep -cE "$1"; }

DMESG_BAD='START failed|state-change timeout|SCMD timeout|failed:|failed at step|ring_dropped=[1-9]|urb_errors=[1-9]|BUG|Oops|WARNING|Call Trace'

# ------------------------------------------------------------ cases

# case_start TITLE / case_end: a case is one step of the log; its kernel
# messages are kept, errors among them flagged.
case_start() {
	CASE=$1
	printf '\n%s=== %s: %s%s\n' "$B" "$PART" "$CASE" "$N"
	log ""
	log "=== $PART: $CASE"
	debug_on
	kmark
	CASE_KMARK=$KMARK
}
case_end() {
	local d bad
	KMARK=$CASE_KMARK
	d=$(ksince)
	log "--- kernel messages"
	printf '%s\n' "$d" | grep -vE "bulk-IN stats|firmware chunk|SCMD (write|interrupt)" >> "$LOG"
	bad=$(printf '%s\n' "$d" | grep -E "$DMESG_BAD" | head -1 | sed 's/^\[[^]]*\] //')
	[ -n "$bad" ] && result WARN "kernel log: $bad"
	return 0
}

# ------------------------------------------------------------ captures

# grab FILE SECONDS: read the stream into FILE (no checks).
grab() {
	timeout -k 5 $(($2 + 20)) bash -c \
		"head -c \$(( $2 * 5000000 )) < '$DEV' > '$1' & p=\$!; sleep $2; kill \$p 2>/dev/null; wait \$p" 2>/dev/null
	[ -s "$1" ]
}

# capture SECONDS [WIDTH HEIGHT RATE FIELD] [noaudio] [during=S:COMMAND]
# Captures into CAP (file), checks data, audio, decoding, and with the
# numbers the stream (FIELD: tt, progressive, or - for any); CAP_KBPS is
# the measured bitrate.
CAP=
CAP_KBPS=0
capture() {
	local secs=$1 info errs during='' audio=1 a
	shift
	local args=()
	for a in "$@"; do
		case "$a" in
		noaudio) audio=0 ;;
		during=*) during=${a#during=} ;;
		*) args+=("$a") ;;
		esac
	done
	CAP="$WORK/$(echo "$PART-$CASE" | tr -c 'A-Za-z0-9-' '_' | cut -c1-60)-$RANDOM.ts"
	say "capturing ${secs}s"
	if [ -n "$during" ]; then
		( sleep "${during%%:*}"; log "during capture: ${during#*:}"; eval "${during#*:}" >> "$LOG" 2>&1 ) &
	fi
	grab "$CAP" "$secs"
	wait
	if [ ! -s "$CAP" ]; then
		say "  no data; once more (the source may still be starting)"
		grab "$CAP" $((secs + 10))
	fi
	if [ ! -s "$CAP" ]; then
		result FAIL "capture: no data"
		CAP_KBPS=0
		return 1
	fi
	info=$(ffprobe -v error -select_streams v:0 -show_entries \
		stream=codec_name,width,height,field_order,r_frame_rate,display_aspect_ratio \
		-of default=nw=1 "$CAP" | head -6 | tr '\n' ' ')
	errs=$(ffmpeg -v error -i "$CAP" -f null - 2>&1 | wc -l)
	CAP_KBPS=$(( $(stat -c %s "$CAP") * 8 / 1000 / secs ))
	log "capture: $(stat -c %s "$CAP") bytes, ~$CAP_KBPS kbit/s; $info; decode errors $errs"
	say "  $info ~$CAP_KBPS kbit/s, decode errors $errs"
	if [ $audio = 1 ]; then
		ffprobe -v error -select_streams a:0 -show_entries stream=codec_name -of csv=p=0 "$CAP" |
			grep -q mp2 || result FAIL "capture: no audio stream"
	fi
	if [ ${#args[@]} -ge 4 ]; then
		local ok=1
		echo "$info" | grep -q "width=${args[0]} " || ok=0
		echo "$info" | grep -q "height=${args[1]} " || ok=0
		echo "$info" | grep -q "r_frame_rate=${args[2]} " || ok=0
		[ "${args[3]}" = - ] || echo "$info" | grep -q "field_order=${args[3]}" || ok=0
		if [ $ok = 1 ]; then
			result PASS "stream ${args[0]}x${args[1]} ${args[2]} ${args[3]}"
		else
			result FAIL "stream: expected ${args[0]}x${args[1]} ${args[2]} ${args[3]}, got: $info"
		fi
	fi
	[ "$errs" -gt 20 ] && result WARN "capture: $errs decode errors"
	return 0
}

# viewer_start TITLE / viewer_stop: a window with the stream, reopened when
# the capture restarts.
VSTOP=
VIEWER=
viewer_start() {
	VSTOP=$WORK/viewer-stop
	rm -f "$VSTOP"
	( while [ ! -e "$VSTOP" ]; do
		ffplay -loglevel error -autoexit -window_title "$1" -f mpegts "$DEV" >/dev/null 2>&1
		sleep 1
	  done ) &
	VIEWER=$!
	sleep 3
}
viewer_stop() {
	[ -n "$VIEWER" ] || return 0
	touch "$VSTOP"
	pkill -f "ffplay .*-window_title" 2>/dev/null
	wait "$VIEWER" 2>/dev/null
	VIEWER=
	sleep 2
}

# ------------------------------------------------------------ report

# summary: the results and the system, to paste into an issue.
summary() {
	local p f w s
	p=$(printf '%s\n' "${RESULTS[@]}" | grep -c '^PASS')
	f=$(printf '%s\n' "${RESULTS[@]}" | grep -c '^FAIL')
	w=$(printf '%s\n' "${RESULTS[@]}" | grep -c '^WARN')
	s=$(printf '%s\n' "${RESULTS[@]}" | grep -c '^SKIP')
	{
		echo
		echo "=== gchd-test summary"
		echo "test: $RUN_NAME ($MODE)"
		echo "device: USB 0fd9:$(usb_id); driver: $(driver_version)"
		echo "system: $(. /etc/os-release 2>/dev/null; echo "${PRETTY_NAME:-unknown}"), kernel $(uname -r) $(uname -m)"
		echo "suite: $SUITE_VERSION"
		[ -n "${BUILT:-}" ] && echo "built: $BUILT"
		printf '%s\n' "${RESULTS[@]}"
		echo "PASS: $p  FAIL: $f  WARN: $w  SKIP: $s"
	} | tee -a "$LOG"
	echo
	echo "Log: $LOG"
}
