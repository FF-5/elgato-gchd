# T-050: HDMI after a composite capture with a signal (output size chosen
# after selecting HDMI), the bitrate range at 1080p, the H.264 level without
# the removed profile/level controls, v4l2-compliance (DV timings with a
# scaled output size before) with its full output. Accepted on 2026-11-27
# (laptop on HDMI): cold start, modes and samples, output sizes, Allow
# 50/60 fps, colour range, source change, bitrate slider in driver, window
# and OBS, composite slider ranges.
# Needs: the laptop on HDMI at 1920x1080 60 Hz, the PS3 on composite (on,
# showing its menu).

INST="$REPO/install-driver.sh"
LOGS="$HOME/.config/obs-studio/logs"
obs_log() {
	local f
	f=$(ls -t "$LOGS" | head -1)
	grep -h '\[gchd\]' "$LOGS/$f" | tail -"$1"
}
D=$DEV
SAMPLES=$HOME/gchd-test-logs/T-050-samples

# Live viewer: ffplay restarted whenever it ends, so the source can be
# changed while watching.
VSTOP=/tmp/gchd-viewer-stop
viewer_start() {
	rm -f $VSTOP
	( while [ ! -e $VSTOP ]; do
		v4l2-ctl -d $D -i 0 >/dev/null 2>&1
		ffplay -loglevel error -autoexit -window_title "Elgato HDMI" -f mpegts $D >/dev/null 2>&1
		sleep 1
	  done ) &
	VIEWER=$!
}
viewer_stop() {
	touch $VSTOP
	pkill -f "window_title Elgato HDMI" 2>/dev/null
	wait $VIEWER 2>/dev/null
	sleep 2
}
size() { run v4l2-ctl -d $D -v width=$1,height=$2,pixelformat=MPEG; }
source_mode() {	# source_mode NAME [HINT]
	step "HDMI source $1"
	size 1920 1080
	viewer_start
	prompt "A window 'Elgato HDMI' shows the laptop's HDMI picture. In the laptop's display settings set the HDMI display (the Elgato) to $1.${2:+ $2} If the picture freezes or disappears, close the window (it reopens). Press Enter when it shows the laptop in $1."
	viewer_stop
	run v4l2-ctl -d $D --list-framesizes MPEG
	run v4l2-ctl -d $D --get-dv-timings
}
# sample NAME REQ SIZE FIELD LEVEL FPS: request REQ (WxH; 1920x1080: the
# source size), up to 20 s / 20 MB, cut at the start of its last video
# frame; SIZE ("W H" pattern), field order, level, frame rate (patterns) and
# decoding checked; saved for the traces branch.
sample() {
	local f=$SAMPLES/hdmi_$1.ts pos
	mkdir -p $SAMPLES
	# HDMI first: the output size belongs to the selected input.
	set_input 0
	size ${2%x*} ${2#*x}
	set_input 0
	timeout -k 5 20 head -c 20000000 < $D > $f.raw 2>/dev/null
	pos=$(ffprobe -v error -select_streams v -show_packets -show_entries packet=pos -of csv=p=0 $f.raw | cut -d, -f1 | grep -v N/A | tail -1)
	head -c "${pos:-0}" $f.raw > $f
	ffprobe -v error -show_entries stream=codec_name,width,height,field_order,r_frame_rate,sample_aspect_ratio,level,profile,color_primaries,sample_rate,channels:format=duration -of default=nw=1 $f > $f.ffprobe.txt 2>&1
	ffmpeg -v error -i $f -f null - > $f.ffmpeg.txt 2>&1
	expect "$1: $3, $4, level $5, $6 fps, decodes without errors" \
		"width=${3% *} height=${3#* } .*level=$5 .*field_order=$4 r_frame_rate=$6 .*errors=0" \
		sh -c "tr '\n' ' ' < $f.ffprobe.txt; echo errors=\$(wc -l < $f.ffmpeg.txt)"
}
rates() { v4l2-ctl -d $D -C video_bitrate_mode,video_bitrate | sed 's/ ([^)]*)//' | tr '\n' ' '; echo; }
range_of() {	# min-max default of the bitrate control
	v4l2-ctl -d $D -l | grep -E '^ *video_bitrate ' | sed 's/.*min=\([0-9]*\) max=\([0-9]*\).*default=\([0-9]*\).*/\1-\2 \3/'
}
N0=0
mark() { N0=$(sudo dmesg | wc -l); }
count_new() { sudo dmesg | tail -n +$((N0 + 1)) | grep -cE "$1"; }

CVSTOP=/tmp/gchd-cviewer-stop
step "install"
run sudo rmmod elgato_gchd
run bash "$INST" --core --yes --in-terminal < /dev/null
prompt "Laptop on HDMI at 1920x1080 60 Hz; PS3 on composite (on, showing its menu). Unplug the Elgato and plug it in again; wait 5 s."

step "composite with a signal, then HDMI"
set_input 2
size 720 480
rm -f $CVSTOP
( while [ ! -e $CVSTOP ]; do v4l2-ctl -d $D -i 2 >/dev/null 2>&1; ffplay -loglevel error -autoexit -window_title "Elgato composite" -f mpegts $D >/dev/null 2>&1; sleep 1; done ) &
CV=$!
prompt "A window 'Elgato composite' should show the PS3 (composite). If it stays black, check the PS3's composite cable and that the PS3 shows its menu. Press Enter when the window shows the PS3."
confirm "The 'Elgato composite' window shows the PS3?"
touch $CVSTOP; pkill -f "window_title Elgato composite" 2>/dev/null; wait $CV 2>/dev/null; sleep 2
run sh -c "sudo dmesg | grep -E 'configuring|analog mode|signal detected' | tail -3"
sample after_composite_1080p 1920x1080 "1920 1080" progressive 40 "(30/1|30000/1001)"
expect "no H.264 profile/level controls" "^0$" sh -c "v4l2-ctl -d $D -l | grep -c 'h264_profile\|h264_level'"
expect "1080p: bitrate range 10-30 Mbit/s, highest 30" "^10000000-30000000 30000000$" range_of

step "v4l2-compliance (full output), starting with a scaled output size"
set_input 0
size 1280 720
CO=$HOME/gchd-test-logs/T-050-compliance.txt
v4l2-compliance -d $D > $CO 2>&1
run sh -c "grep -E 'fail|FAIL|warn' $CO | head -20; grep -E '^Total' $CO"
expect "compliance run 1: 0 failures" "Failed: 0" grep -E '^Total' $CO
v4l2-compliance -d $D > $CO.2 2>&1
run sh -c "grep -E 'fail|FAIL|warn' $CO.2 | head -20; grep -E '^Total' $CO.2"
expect "compliance run 2 (right after): 0 failures" "Failed: 0" grep -E '^Total' $CO.2
