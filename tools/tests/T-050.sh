# T-050 addition: HDMI 480p (720x480 at 60 Hz) from the laptop: samples at
# the source size with "Allow 50/60 fps" on and off, 640x480 and 480x360.
# T-050 is accepted (2026-11-27); 576p was tested, 480p not yet.
# Needs: the laptop on HDMI.

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

step "install"
run sudo rmmod elgato_gchd
run bash "$INST" --core --yes --in-terminal < /dev/null
prompt "Laptop on HDMI. Unplug the Elgato and plug it in again; wait 5 s."
set_input 0
run v4l2-ctl -d $D -c allow_50_60_fps=1

source_mode "720x480 at 60 Hz (480p)"
step "480p"
expect "480p detected (720x480 progressive)" "Active width: 720.*Active height: 480.*[Pp]rogressive" sh -c "v4l2-ctl -d $D --get-dv-timings | tr '\n' ' '"
expect "480p: 720x480, 640x480, 480x360 offered" "720x480.*640x480.*480x360" sh -c "v4l2-ctl -d $D --list-framesizes MPEG | tr '\n' ' '"
sample 480p 1920x1080 "720 480" progressive 30 "(60/1|60000/1001)"
run v4l2-ctl -d $D -c allow_50_60_fps=0
sample 480p_allow_off 1920x1080 "720 480" progressive 31 "(60/1|60000/1001)"
run v4l2-ctl -d $D -c allow_50_60_fps=1
sample 480p_640x480 640x480 "640 480" progressive 30 "(30/1|30000/1001)"
sample 480p_480x360 480x360 "480 360" progressive 22 "(30/1|30000/1001)"
size 1920 1080
run ls -l $SAMPLES
prompt "Upload the hdmi_480p* files from $SAMPLES to traces/hdmi/samples/."
