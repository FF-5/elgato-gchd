# T-051: the settings window and the OBS plugin for component (output
# sizes the source allows, "Allow 50/60 fps"). The driver parts of T-051
# were accepted on 2026-11-26.
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

step "install the settings programs and the plugin of this branch"
run sudo rmmod elgato_gchd
run bash "$INST" --all --yes --in-terminal < /dev/null
prompt "Unplug the Elgato and plug it in again; wait 5 s."
set_input 1

source_mode 1080i "Tick 1080i (and untick 1080p) so the PS3 sends 1080i."

step "settings window"
prompt "Open 'Elgato Game Capture HD Settings'. Input Component."
confirm "Output size lists 1920x1080, 1280x720, 640x480, 480x360; an 'Allow 50/60 fps' switch is shown; no Stretch, Bitrate mode or Bitrate?"
prompt "Choose Output size 1280x720; wait 3 s."
expect "window: output 1280x720" "1280/720" v4l2-ctl -d $D -V
prompt "Switch 'Allow 50/60 fps' off; wait 3 s."
expect "window: Allow 50/60 fps off" "allow_50_60_fps: 0" v4l2-ctl -d $D -C allow_50_60_fps
prompt "Switch it on again and choose Output size 1920x1080; wait 3 s; leave the window open."
expect "window: back to the source size" "1920/1080" v4l2-ctl -d $D -V

step "OBS plugin"
prompt "Start OBS (the source 'Elgato Game Capture HD' shown, input Component); open the source's properties."
confirm "OBS: Output size lists 1920x1080, 1280x720, 640x480, 480x360 and 'Allow 50/60 fps' is shown; no Stretch, Bitrate mode or Bitrate?"
prompt "In OBS's properties choose 640x480; wait 5 s (the preview follows)."
expect "OBS: output 640x480" "640/480" v4l2-ctl -d $D -V
confirm "the preview shows the PS3 at 640x480, and the settings window shows 640x480 by itself?"
prompt "Choose 1920x1080 again -> OK. Close OBS and the settings window."
run obs_log 8
set_input 2
