# T-050: HDMI, all modes (a laptop's HDMI output): the recorded configuration
# without the back end, encoder values per mode, TS samples (RULES F3),
# output sizes and "Allow 50/60 fps", HDMI Color Range while capturing,
# source mode changes while capturing; the bitrate as one slider (below
# 10.00 VBR, from 10.00 CBR) in the driver, the settings window and OBS;
# composite bitrate ranges of the slider model; HDMI after composite;
# v4l2-compliance.
# Needs: a laptop on HDMI (the Elgato as a second display, mirrored), the
# PS3 on composite, OBS with the plugin.

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

step "install the settings programs and the plugin of this branch"
run sudo rmmod elgato_gchd
run bash "$INST" --all --yes --in-terminal < /dev/null
prompt "Connect the laptop's HDMI output to the Elgato (laptop on, the Elgato as a second display, mirrored) and the PS3 to composite (PS3 on). Unplug the Elgato and plug it in again; wait 5 s (the first capture is a cold start with HDMI selected)."
set_input 0
run v4l2-ctl -d $D -c allow_50_60_fps=1,hdmi_color_range=0

source_mode "1280x720 at 60 Hz"
step "720p"
sample 720p 1920x1080 "1280 720" progressive 40 "(60/1|60000/1001)"
expect "720p: 1280x720, 640x480, 480x360 offered (no scaled 1280x720)" "1280x720.*640x480.*480x360" sh -c "v4l2-ctl -d $D --list-framesizes MPEG | tr '\n' ' '"
run v4l2-ctl -d $D -c allow_50_60_fps=0
sample 720p_allow_off 1920x1080 "1280 720" progressive 31 "(30/1|30000/1001)"
run v4l2-ctl -d $D -c allow_50_60_fps=1
sample 720p_640x480 640x480 "640 480" progressive 30 "(30/1|30000/1001)"

source_mode "1920x1080 at 60 Hz"
step "1080p"
sample 1080p 1920x1080 "1920 1080" progressive 40 "(30/1|30000/1001)"
sample 1080p_1280x720 1280x720 "1280 720" progressive 31 "(30/1|30000/1001)"
sample 1080p_480x360 480x360 "480 360" progressive 22 "(30/1|30000/1001)"
expect "1080p: 16-30 Mbit/s range covers VBR from 8.00 and CBR to 30.00" "^10000000-30000000 30000000$" range_of

step "HDMI Color Range while capturing (1080p)"
size 1920 1080
set_input 0
mark
viewer_start
prompt "Watch the 'Elgato HDMI' window for 10 s, then press Enter."
run v4l2-ctl -d $D -c hdmi_color_range=1
confirm "With Expanded the picture became more contrasty (blacks darker, whites brighter) at once, without the picture stopping?"
run v4l2-ctl -d $D -c hdmi_color_range=0
confirm "Back to Standard, the picture is as before?"
viewer_stop
expect "no encoder restart for the colour range" "^0$" count_new "encoder restart"

source_mode "1920x1080 at 50 Hz" "If the laptop offers no 50 Hz, choose the lowest refresh it offers for 1920x1080 (30, 25 or 24 Hz) and note it."
step "1080p at 50 Hz (or the lower rate)"
run v4l2-ctl -d $D --get-dv-timings
sample 1080p_low 1920x1080 "1920 1080" progressive 40 "(25/1|30/1|30000/1001|24/1|24000/1001)"
source_mode "720x576 at 50 Hz (or 720x480 at 60 Hz)" "If the laptop offers neither, choose 640x480 and note it."
step "SD progressive"
run v4l2-ctl -d $D --get-dv-timings
sample sd 1920x1080 "720 (480|576)" progressive 30 '(50/1|60/1|60000/1001)'
run v4l2-ctl -d $D -c allow_50_60_fps=0
sample sd_allow_off 1920x1080 "720 (480|576)" progressive 31 '(50/1|60/1|60000/1001)'
run v4l2-ctl -d $D -c allow_50_60_fps=1

step "source mode change while capturing"
size 1920 1080
set_input 0
mark
viewer_start
prompt "While the 'Elgato HDMI' window shows the laptop, set the laptop's HDMI display to 1280x720 at 60 Hz (the picture may go away for a few seconds and comes back). Press Enter when it shows 720p again."
viewer_stop
expect "the driver followed the source change" "^[1-9]" count_new "HDMI source mode changed while capturing"
run sh -c "sudo dmesg | tail -n +$((N0 + 1)) | grep -E 'HDMI|detected|failed' | tail -12"

step "bitrate as one slider (driver, 720p)"
size 1920 1080
run v4l2-ctl -d $D -c video_bitrate_mode=1,video_bitrate=20000000
expect "720p CBR 20 Mbit/s" "video_bitrate_mode: 1 video_bitrate: 20000000" rates
run v4l2-ctl -d $D -c video_bitrate_mode=0,video_bitrate=19000000
expect "720p VBR 19 Mbit/s (9.50 shown)" "video_bitrate_mode: 0 video_bitrate: 19000000" rates
run v4l2-ctl -d $D -c video_bitrate_mode=0,video_bitrate=25000000
expect "VBR above 19.98 Mbit/s gives 19.98" "video_bitrate_mode: 0 video_bitrate: 19980000" rates
run v4l2-ctl -d $D -c video_bitrate_mode=1,video_bitrate=10000000
expect "CBR 10 Mbit/s (10.00, the first CBR value)" "video_bitrate_mode: 1 video_bitrate: 10000000" rates

step "settings window"
prompt "Open 'Elgato Game Capture HD Settings' (Input HDMI)."
confirm "Rows: Output size (1920... source size, 640x480, 480x360), Bitrate (one slider, no Bitrate mode row), Allow 50/60 fps, HDMI color range; the slider ends at 26.25?"
prompt "Move the slider to about 9.50 and release it; wait 3 s."
expect "window 9.50: VBR 19 Mbit/s" "video_bitrate_mode: 0 video_bitrate: 1(8[5-9]|9[0-5])[0-9]{5}" rates
prompt "Move the slider to about 15.00 and release it; wait 3 s."
expect "window 15.00: CBR 15 Mbit/s" "video_bitrate_mode: 1 video_bitrate: 1(4[5-9]|5[0-5])[0-9]{5}" rates
prompt "Choose HDMI color range Expanded; wait 3 s."
expect "window: Expanded" "hdmi_color_range: 1" v4l2-ctl -d $D -C hdmi_color_range
prompt "Choose Standard again; choose Output size 480x360; wait 3 s."
confirm "480x360: the slider goes from 2.00 to 7.50 (VBR)?"
prompt "Choose the source size again; close the window."

step "OBS plugin"
prompt "Start OBS (source 'Elgato Game Capture HD', input HDMI); open the source's properties."
confirm "OBS: Output size, Bitrate (one slider, at about 15.00), Allow 50/60 fps and HDMI color range are shown, no Bitrate mode?"
prompt "Move the bitrate slider to about 8.00 -> OK; wait 3 s."
expect "OBS 8.00: VBR 16 Mbit/s" "video_bitrate_mode: 0 video_bitrate: 1(5[5-9]|6[0-5])[0-9]{5}" rates
prompt "Close OBS."
run obs_log 6

step "composite: bitrate ranges of the slider, then HDMI after composite"
set_input 2
size 720 480
expect "composite source size: 6 to 19.98 Mbit/s, highest 11.25" "^6000000-19980000 11250000$" range_of
size 480 360
expect "composite 480x360: 4 to 15 Mbit/s" "^4000000-15000000 15000000$" range_of
size 720 480
set_input 2
expect "composite capture (PS3) before HDMI" "width=720 height=(480|576)" sh -c "timeout -k 5 20 head -c 6000000 < $D > /tmp/gchd-c.ts; ffprobe -v error -select_streams v:0 -show_entries stream=width,height -of default=nw=1 /tmp/gchd-c.ts | tr '\n' ' '"
sample after_composite 1920x1080 "1280 720" progressive 40 "(60/1|60000/1001)"

step "v4l2-compliance"
run sh -c "v4l2-compliance -d $D 2>&1 | tail -3"
expect "compliance: 0 failures" "Failed: 0" sh -c "v4l2-compliance -d $D 2>&1 | grep -E '^Total'"
run ls -l $SAMPLES
prompt "Upload the folder $SAMPLES to the traces branch as traces/hdmi/samples/ (the .ts, .raw, .ffprobe.txt, .ffmpeg.txt files)."
