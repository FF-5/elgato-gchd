# T-051: component bitrate (range per output, CBR at the highest, VBR
# below) in the driver, the settings window and the OBS plugin; the H.264
# profile is no longer offered. Output sizes and "Allow 50/60 fps" in the
# window and the plugin were accepted on 2026-11-26.
# Needs: the PS3 on component in 1080i (or 1080p), OBS with the plugin.

INST="$REPO/install-driver.sh"
LOGS="$HOME/.config/obs-studio/logs"
obs_log() {
	local f
	f=$(ls -t "$LOGS" | head -1)
	grep -h '\[gchd\]' "$LOGS/$f" | tail -"$1"
}

D=$DEV
# comp_cap W H FIELD: 10 s capture; size, field order, frame rate, aspect
# ratio and bitrate of the stream.
comp_cap() {
	local f=/tmp/gchd-comp.ts info kbps
	timeout -k 5 30 bash -c "head -c 60000000 < $D > $f & p=\$!; sleep 10; kill \$p 2>/dev/null; wait \$p" 2>/dev/null
	[ -s $f ] || { echo "no data"; return; }
	info=$(ffprobe -v error -select_streams v:0 -show_entries \
		stream=width,height,field_order,r_frame_rate,sample_aspect_ratio,level,color_primaries \
		-of default=nw=1 $f | tr '\n' ' ')
	kbps=$(( $(stat -c %s $f) * 8 / 10000 ))
	echo "$info ~${kbps} kbit/s errors=$(ffmpeg -v error -i $f -f null - 2>&1 | wc -l)"
	rm -f $f
}
# Live viewer: ffplay restarted whenever it ends (window closed, stream
# ended), so the source's own menus can be used while watching.
VSTOP=/tmp/gchd-viewer-stop
viewer_start() {
	rm -f $VSTOP
	( while [ ! -e $VSTOP ]; do
		v4l2-ctl -d $D -i 1 >/dev/null 2>&1
		ffplay -loglevel error -autoexit -window_title "Elgato component" -f mpegts $D >/dev/null 2>&1
		sleep 1
	  done ) &
	VIEWER=$!
}
viewer_stop() {
	touch $VSTOP
	pkill -f "window_title Elgato component" 2>/dev/null
	wait $VIEWER 2>/dev/null
	sleep 2
}
# out NAME W H FIELD LEVEL: set the output size, select the input again
# (fresh detection), capture and check.
out() {
	run v4l2-ctl -d $D -v width=$2,height=$3,pixelformat=MPEG
	set_input 1
	expect "$1: stream ${2}x${3} $4, level $5" "width=$2 height=$3 .*level=$5 .*field_order=$4" comp_cap
}
source_mode() {	# source_mode NAME [HINT]
	step "component source $1"
	# Output at the source size while watching.
	run v4l2-ctl -d $D -v width=1920,height=1080,pixelformat=MPEG
	viewer_start
	prompt "A window 'Elgato component' shows the PS3. Set the PS3's video output to $1.${2:+ $2} If the picture freezes or disappears, close the window (it reopens). Press Enter when it shows the PS3 in $1."
	viewer_stop
	# The sizes the source detected by the viewer allows.
	run v4l2-ctl -d $D --list-framesizes MPEG
}

range_of() {	# the bitrate control's range
	v4l2-ctl -d $D -l | grep -E '^ *video_bitrate ' | sed 's/.*min=\([0-9]*\) max=\([0-9]*\).*/\1-\2/'
}

step "install the settings programs and the plugin of this branch"
run sudo rmmod elgato_gchd
run bash "$INST" --all --yes --in-terminal < /dev/null
prompt "Unplug the Elgato and plug it in again; wait 5 s."
set_input 1
source_mode 1080i "Tick 1080i (and untick 1080p) so the PS3 sends 1080i."

step "driver: bitrate range per output"
expect "1080i source size: 16-30 Mbit/s" "^16000000-30000000$" range_of
run v4l2-ctl -d $D -v width=640,height=480,pixelformat=MPEG
expect "640x480: 6-11.25 Mbit/s" "^6000000-11250000$" range_of
run v4l2-ctl -d $D -v width=1280,height=720,pixelformat=MPEG
expect "1280x720: 8-15 Mbit/s" "^8000000-15000000$" range_of
run v4l2-ctl -d $D -v width=1920,height=1080,pixelformat=MPEG
run v4l2-ctl -d $D -c video_bitrate=20000000
expect "1080i at 20 Mbit/s kept" "video_bitrate: 20000000" v4l2-ctl -d $D -C video_bitrate
run comp_cap
run v4l2-ctl -d $D -c video_bitrate=30000000

step "settings window"
prompt "Open 'Elgato Game Capture HD Settings' (Input Component)."
confirm "a Bitrate slider from 16.00 to 30.00 Mbit/s is shown; no H.264 profile row?"
prompt "Move the Bitrate slider to about 20.00 and release it; wait 3 s."
expect "window: about 20 Mbit/s" "video_bitrate: (19[5-9]|20[0-5])[0-9]{5}" v4l2-ctl -d $D -C video_bitrate
prompt "Choose Output size 640x480; wait 3 s."
confirm "the slider now goes from 6.00 to 11.25 (and stands at 11.25)?"
prompt "Choose Output size 1920x1080 and the slider fully to the right (30.00); then Input Composite; wait 3 s."
confirm "Composite: no H.264 profile row either; Bitrate mode and Bitrate as before?"
run v4l2-ctl -d $D -i 1

step "OBS plugin"
prompt "Start OBS (the source 'Elgato Game Capture HD', input Component); open the source's properties."
confirm "OBS: a Bitrate slider from 16.00 to 30.00 Mbit/s; no H.264 profile; no Bitrate mode?"
prompt "In OBS's properties move the Bitrate slider to about 25.00 -> OK; wait 3 s."
expect "OBS: about 25 Mbit/s" "video_bitrate: (24[5-9]|25[0-5])[0-9]{5}" v4l2-ctl -d $D -C video_bitrate
prompt "Set it back to 30.00 -> OK. Close OBS and the settings window."
run obs_log 8
set_input 2
