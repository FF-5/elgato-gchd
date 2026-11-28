# SPDX-License-Identifier: GPL-2.0
# shellcheck shell=bash
# Cases of the analog SD inputs (composite, S-video): AIN is the input
# number, ANAME its name; the part files set them in their setup.

AIN=2
ANAME=Composite
BASE=		# 480i (NTSC source) or 576i (PAL source)
SRC=		# the source's mode as last asked for
CBR_KBPS=0

# Stream of a source mode at the standard size: "WIDTH HEIGHT RATE FIELD".
a_expect() {
	case "$1" in
	480i|240p) echo "720 480 60000/1001 tt" ;;
	576i|288p) echo "720 576 50/1 tt" ;;
	esac
}
# Frame rate of the scaled sizes (640x480, 480x360).
a_rate() { case "$BASE" in 480i) echo 30000/1001 ;; *) echo 25/1 ;; esac; }

# source_mode MODE: ask the tester to set the source to MODE (once), with
# a window showing the source's picture to do it.
source_mode() {
	[ "$SRC" = "$1" ] && return 0
	viewer_start "Elgato $ANAME"
	prompt "A window 'Elgato $ANAME' shows the source. Set the source to $1, then keep a colourful picture on screen (e.g. a game's title or menu screen). The window closes when you press Enter."
	viewer_stop
	SRC=$1
}

# Default settings of the input (the bitrate: CBR at the output's highest).
a_defaults() {
	local top
	ctl brightness=0,contrast=0,saturation=0,hue=0,analog_audio_gain_db=0,stretch_sd_input=0
	size 720 480
	top=$(ctl_range video_bitrate | awk '{ print $3 }')
	ctl "video_bitrate_mode=1,video_bitrate=${top:-11250000}"
}

a_setup() {
	input $AIN
	viewer_start "Elgato $ANAME"
	prompt "Connect the source to the Elgato's $ANAME input (video and the red/white audio) and switch it on; a window 'Elgato $ANAME' shows its picture. The window closes when you press Enter."
	viewer_stop
	have ntsc "Can the source output NTSC (60 Hz) 480i on $ANAME?"
	have pal "Can the source output PAL (50 Hz) 576i on $ANAME?"
	if [ "${HAVE[ntsc]}" = 1 ]; then BASE=480i; elif [ "${HAVE[pal]}" = 1 ]; then BASE=576i; else BASE=; fi
	if [ "$MODE" = deep ]; then
		have p240 "Can it output 240p (NTSC progressive, e.g. older consoles)?"
		have p288 "Can it output 288p (PAL progressive)?"
		have tone "Can you play a steady sound (a constant tone, e.g. from a phone with a 3.5 mm to RCA cable) into the red/white audio input for the audio level test?"
	fi
	SRC=
	input $AIN
	a_defaults
	[ -n "$BASE" ] || say "${R}No NTSC or PAL source on $ANAME: its cases are skipped.${N}"
}

# needs_base: a source the cases can use.
needs_base() { [ -n "$BASE" ] || { result SKIP "no NTSC or PAL source"; return 1; }; }

# ------------------------------------------------------------ cases

a_capture() {
	needs_base || return
	source_mode $BASE
	size 720 480
	# shellcheck disable=SC2046
	capture 8 $(a_expect $BASE)
	case $BASE in
	480i) expect "standard NTSC" "NTSC" v4l2-ctl -d "$DEV" --get-standard ;;
	576i) expect "standard PAL" "PAL" v4l2-ctl -d "$DEV" --get-standard ;;
	esac
}

a_modes() {
	local m k n=0
	for m in 480i 576i 240p 288p; do
		case $m in 480i) k=ntsc ;; 576i) k=pal ;; 240p) k=p240 ;; 288p) k=p288 ;; esac
		[ "${HAVE[$k]:-0}" = 1 ] || continue
		n=$((n + 1))
		source_mode $m
		kmark
		# shellcheck disable=SC2046
		capture 8 $(a_expect $m)
		expect "$m detected" "configuring $m|analog mode now $m" kmsg
	done
	[ $n -gt 0 ] || result SKIP "no source mode"
}

a_mode_change() {
	local p
	case $BASE in 480i) p=240p; needs ntsc p240 || return ;; 576i) p=288p; needs pal p288 || return ;; *) needs_base; return ;; esac
	source_mode $BASE
	viewer_start "Elgato $ANAME"
	prompt "A window 'Elgato $ANAME' shows the source. Now switch the source to $p (e.g. start a game that uses it) and wait until the picture is back."
	SRC=$p
	sleep 3
	expect "change to $p followed" "analog mode now $p" kmsg
	kmark
	prompt "Switch the source back to $BASE and wait until the picture is back."
	SRC=$BASE
	sleep 3
	expect "change back to $BASE followed" "analog mode now $BASE" kmsg
	viewer_stop
	confirm "The window showed the picture again after each change (perhaps reopened), in the right shape?"
}

a_family_change() {
	local o
	needs ntsc pal || return
	o=576i; [ "$BASE" = 576i ] && o=480i
	source_mode $BASE
	viewer_start "Elgato $ANAME"
	prompt "A window 'Elgato $ANAME' shows the source. Now change the source to $o (the other TV system: another console or its video setting) and wait until the picture is back."
	SRC=$o
	sleep 5
	expect "change to $o followed" "analog mode now $o|signal detected: input=$AIN mode=720x${o%i}" kmsg
	viewer_stop
	# shellcheck disable=SC2046
	capture 8 $(a_expect $o)
	confirm "The window showed the new source's picture after the change (perhaps reopened)?"
	source_mode $BASE
}

a_sizes() {
	needs_base || return
	source_mode $BASE
	size 640 480
	capture 8 640 480 "$(a_rate)" progressive
	size 480 360
	capture 8 480 360 "$(a_rate)" progressive
	size 720 480
}

a_stretch() {
	needs_base || return
	source_mode $BASE
	local a
	ctl stretch_sd_input=0
	capture 6
	a=$(aspect "$CAP")
	if [ "$a" != 16:9 ]; then result PASS "stretch off: not widened ($a)"; else result FAIL "stretch off: 16:9"; fi
	ctl stretch_sd_input=1
	# shellcheck disable=SC2046
	capture 6 $(a_expect $BASE)
	expect "stretch on: 16:9, same width" "^16:9$" aspect "$CAP"
	ctl stretch_sd_input=0
}

a_bitrate_cbr() {
	local top
	needs_base || return
	source_mode $BASE
	size 720 480
	top=$(ctl_range video_bitrate | awk '{ print $3 }')
	ctl "video_bitrate_mode=1,video_bitrate=$top"
	capture 12
	CBR_KBPS=$CAP_KBPS
	check "CBR $((top / 1000)) kbit/s measured" "$CAP_KBPS >= 0.8 * $top / 1000 && $CAP_KBPS <= 1.3 * $top / 1000" "$CAP_KBPS kbit/s"
}

a_bitrate_more() {
	local r min max top
	needs_base || return
	source_mode $BASE
	size 720 480
	r=$(ctl_range video_bitrate); min=${r%% *}; max=$(echo "$r" | awk '{ print $2 }'); top=${r##* }
	# VBR at the lowest: the average (half the control value) is lower.
	ctl "video_bitrate_mode=0,video_bitrate=$min"
	capture 12
	[ "$CBR_KBPS" -gt 0 ] || CBR_KBPS=$((top / 1000))
	check "VBR at the lowest: lower than CBR at the highest" "$CAP_KBPS < 0.8 * $CBR_KBPS" "VBR $CAP_KBPS, CBR $CBR_KBPS kbit/s"
	# Outside the range: limited to it.
	v4l2-ctl -d "$DEV" -c "video_bitrate_mode=1,video_bitrate=$((max + 5000000))" >/dev/null 2>&1
	expect "above the highest: limited" "^$max$" v video_bitrate
	v4l2-ctl -d "$DEV" -c "video_bitrate=1000" >/dev/null 2>&1
	expect "below the lowest: limited" "^$min$" v video_bitrate
	# Each output size keeps its own bitrate.
	size 640 480
	ctl "video_bitrate_mode=1,video_bitrate=10000000"
	size 720 480
	ctl "video_bitrate_mode=1,video_bitrate=$((top - 250000))"
	size 640 480
	expect "640x480 kept its bitrate" "^10000000$" v video_bitrate
	size 720 480
	expect "720x480 kept its bitrate" "^$((top - 250000))$" v video_bitrate
	# A change while capturing (encoder restart), the stream goes on.
	ctl "video_bitrate=$top"
	kmark
	capture 14 "during=5:v4l2-ctl -d $DEV -c video_bitrate=10000000"
	expect "bitrate changed while capturing" "^10000000$" v video_bitrate
	expect "encoder restarted for it" "encoder restart" kmsg
	ctl "video_bitrate=$top"
}

# pic SETTINGS: capture with the picture settings, stats into PS.
PS=
pic() {
	ctl "$1"
	capture 5 >/dev/null
	PS=$(pic_stats "$CAP")
	log "picture $1: $PS"
	say "  picture $1: Y $(pstat 1 "$PS"), saturation $(pstat 2 "$PS"), hue $(pstat 3 "$PS"), Y 10-90% $(pstat 4 "$PS")-$(pstat 5 "$PS")"
}

a_picture() {
	local p0 y0 s0
	needs_base || return
	source_mode $BASE
	size 720 480
	pic brightness=0,contrast=0,saturation=0,hue=0; p0=$PS
	y0=$(pstat 1 "$p0"); s0=$(pstat 2 "$p0")
	[ -n "$y0" ] || { result FAIL "no picture to measure"; return; }
	pic brightness=100
	check "brightness +100: brighter" "$(pstat 1 "$PS") > $y0 + 10" "Y $y0 -> $(pstat 1 "$PS")"
	pic brightness=-100
	check "brightness -100: darker" "$(pstat 1 "$PS") < $y0 - 10" "Y $y0 -> $(pstat 1 "$PS")"
	pic brightness=0,saturation=-100
	check "saturation -100: no colour" "$(pstat 2 "$PS") < 3 || $(pstat 2 "$PS") < 0.2 * $s0" "saturation $s0 -> $(pstat 2 "$PS")"
	ctl saturation=0
}

a_picture_more() {
	local p0 sp0 s0 h0 sp
	needs_base || return
	source_mode $BASE
	size 720 480
	pic brightness=0,contrast=0,saturation=0,hue=0; p0=$PS
	sp0=$(awk -v a="$(pstat 4 "$p0")" -v b="$(pstat 5 "$p0")" 'BEGIN { print b - a }')
	s0=$(pstat 2 "$p0"); h0=$(pstat 3 "$p0")
	pic contrast=100
	sp=$(awk -v a="$(pstat 4 "$PS")" -v b="$(pstat 5 "$PS")" 'BEGIN { print b - a }')
	check "contrast +100: wider luma spread" "$sp > 1.15 * $sp0" "spread $sp0 -> $sp"
	pic contrast=-100
	sp=$(awk -v a="$(pstat 4 "$PS")" -v b="$(pstat 5 "$PS")" 'BEGIN { print b - a }')
	check "contrast -100: narrower luma spread" "$sp < 0.85 * $sp0" "spread $sp0 -> $sp"
	pic contrast=0,saturation=100
	check "saturation +100: more colour" "$(pstat 2 "$PS") > 1.2 * $s0" "saturation $s0 -> $(pstat 2 "$PS")"
	if awk "BEGIN { exit !($s0 > 5) }"; then
		pic saturation=0,hue=90
		check "hue +90: colours shifted" "$(hue_diff "$h0" "$(pstat 3 "$PS")") >= 10" "hue $h0 -> $(pstat 3 "$PS")"
		pic hue=-90
		check "hue -90: colours shifted" "$(hue_diff "$h0" "$(pstat 3 "$PS")") >= 10" "hue $h0 -> $(pstat 3 "$PS")"
	else
		result SKIP "hue: too little colour in the picture to measure"
	fi
	ctl brightness=0,contrast=0,saturation=0,hue=0
	# Changed while capturing: no encoder restart, applied.
	kmark
	capture 10 "during=4:v4l2-ctl -d $DEV -c saturation=-100"
	expect "no encoder restart for a picture change" "^0$" kcount "encoder restart"
	# The last 3 seconds (after the change): decoded from the start.
	PS=$(ffmpeg -v error -i "$CAP" -an -vf "trim=start=7,signalstats,metadata=print:file=-" -f null - 2>/dev/null |
		awk -F= '/signalstats.SATAVG=/ { s += $2; n++ } END { if (n) printf "0 %.1f\n", s / n }')
	check "saturation -100 applied while capturing" "$(pstat 2 "$PS") < 3 || $(pstat 2 "$PS") < 0.2 * $s0" "saturation at the end $(pstat 2 "$PS")"
	ctl saturation=0
}

a_audio() {
	local l0 l
	needs_base || return
	needs tone || return
	source_mode $BASE
	prompt "Start the steady sound now (medium volume) and leave it on until asked."
	ctl analog_audio_gain_db=0; capture 8 >/dev/null; l0=$(level "$CAP")
	for g in -6 -12 6 12; do
		ctl analog_audio_gain_db=$g; capture 8 >/dev/null; l=$(level "$CAP")
		if [ $g = 12 ]; then
			check "gain +12 dB measured (10-14; less if the sound clips)" "$l - $l0 >= 8 && $l - $l0 <= 14" "$l0 -> $l dB"
		else
			check "gain $g dB measured" "$l - $l0 >= $g - 2 && $l - $l0 <= $g + 2" "$l0 -> $l dB"
		fi
	done
	ctl analog_audio_gain_db=0
	prompt "Stop the steady sound."
}

a_h264() {
	needs_base || return
	source_mode $BASE
	size 720 480 H264
	capture 6 noaudio
	expect "H.264 format" "^h264$" ffprobe -v error -select_streams v:0 -show_entries stream=codec_name -of csv=p=0 "$CAP"
	size 720 480 MPEG
}

a_reopen() {
	needs_base || return
	source_mode $BASE
	capture 4 >/dev/null && result PASS "first capture"
	capture 4 >/dev/null && result PASS "second capture right after"
	sleep 1
	capture 4 >/dev/null && result PASS "third capture"
}

a_signal() {
	needs_base || return
	source_mode $BASE
	viewer_start "Elgato $ANAME"
	prompt "A window 'Elgato $ANAME' shows the source. Pull the video cable out of the Elgato's $ANAME input, wait 5 s, put it back, and wait until the picture is back."
	sleep 2
	expect "signal lost logged" "signal lost" kmsg
	expect "source found again" "signal detected: input=$AIN" kmsg
	viewer_stop
	confirm "The picture came back by itself after the cable was put back?"
}

a_unplug() {
	needs_base || return
	source_mode $BASE
	viewer_start "Elgato $ANAME"
	prompt "A window 'Elgato $ANAME' shows the source. Unplug the Elgato's USB cable from the computer, wait 5 s, plug it in again."
	viewer_stop
	if wait_dev 20; then result PASS "device back after the replug"; else result FAIL "device not back"; return; fi
	expect "device ready logged" "Elgato Game Capture HD ready as /dev/video[0-9]+" kmsg
	expect "input kept after the replug" "^$AIN$" cur_input
	capture 6 >/dev/null && result PASS "capture after the replug"
}

a_persist() {
	needs_base || return
	ctl brightness=20,saturation=-30,analog_audio_gain_db=-3,stretch_sd_input=1
	size 640 480
	prompt "Unplug the Elgato's USB cable, wait 3 s, plug it in again."
	wait_dev 20 || { result FAIL "device not back"; return; }
	expect "kept after a replug" "input=$AIN 640x480 20 -30 -3 1" a_state
	if sudo modprobe -r elgato_gchd 2>>"$LOG" && sudo modprobe elgato_gchd && wait_dev 20; then
		sleep 2
		expect "kept after the driver was loaded again" "input=$AIN 640x480 20 -30 -3 1" a_state
	else
		result FAIL "the driver could not be loaded again (in use?)"
	fi
	a_defaults
}
a_state() { echo "input=$(cur_input) $(cur_size) $(v brightness) $(v saturation) $(v analog_audio_gain_db) $(v stretch_sd_input)"; }

a_compliance() {
	command -v v4l2-compliance >/dev/null || { result SKIP "v4l2-compliance not installed (package v4l-utils)"; return; }
	v4l2-compliance -d "$DEV" > "$WORK/compliance-$PART.txt" 2>&1
	grep -E "fail|FAIL" "$WORK/compliance-$PART.txt" | head -10 | tee -a "$LOG"
	expect "v4l2-compliance: 0 failures" "Failed: 0" grep -E "^Total" "$WORK/compliance-$PART.txt"
}

# analog_cases PART: the cases of an analog input (tag, title, function).
analog_cases() {
	tcase "$1" fast "capture at the source's mode" a_capture
	tcase "$1" deep "every source mode" a_modes
	tcase "$1" deep "source mode change while capturing" a_mode_change
	tcase "$1" deep "TV system change (60/50 Hz) while capturing" a_family_change
	tcase "$1" fast "output sizes 640x480 and 480x360" a_sizes
	tcase "$1" deep "stretch SD input" a_stretch
	tcase "$1" fast "bitrate: CBR measured" a_bitrate_cbr
	tcase "$1" deep "bitrate: VBR, limits, per output size, change while capturing" a_bitrate_more
	tcase "$1" fast "picture: brightness and saturation measured" a_picture
	tcase "$1" deep "picture: contrast, saturation, hue, change while capturing" a_picture_more
	tcase "$1" deep "audio gain measured" a_audio
	tcase "$1" fast "H.264 format" a_h264
	tcase "$1" deep "close and reopen" a_reopen
	tcase "$1" deep "signal lost and back while capturing" a_signal
	tcase "$1" deep "USB unplugged while capturing" a_unplug
	tcase "$1" deep "settings kept after a replug and a driver reload" a_persist
	tcase "$1" fast "v4l2-compliance" a_compliance
}
