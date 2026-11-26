# T-051: component output sizes (1280x720, 640x480, 480x360 as the source
# allows) and the control "Allow 50/60 fps"; encoder values and mode
# configuration were accepted on 2026-11-26. Each output is captured and
# checked in the stream (size, field order, level); one size change while
# capturing; v4l2-compliance on component. The PS3 sends the highest
# resolution ticked in its menu: the steps go from 1080p down.
# Needs: the PS3 on component (1080p, 1080i, 720p, 576p, 576i).

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

step "load the test build (fresh plug-in)"
load_module replug
set_input 2
set_fmt 720 480 MPEG
set_input 1
run v4l2-ctl -d $D -c allow_50_60_fps=1

source_mode 1080p
expect "1080p: four output sizes offered" "1920x1080.*1280x720.*640x480.*480x360" sh -c "v4l2-ctl -d $D --list-framesizes MPEG | tr '\n' ' '"
out "1080p -> 1280x720" 1280 720 progressive 31
out "1080p -> 640x480" 640 480 progressive 30
out "1080p -> 480x360" 480 360 progressive 22

source_mode 1080i "Untick 1080p."
out "1080i -> 1280x720" 1280 720 progressive 31
out "1080i -> 640x480" 640 480 progressive 30
out "1080i -> 480x360" 480 360 progressive 22
out "1080i source size" 1920 1080 tt 40
step "1080i: output size change while capturing"
run sh -c "(sleep 5; v4l2-ctl -d $D -v width=1280,height=720,pixelformat=MPEG) & sleep 0; true"
run comp_cap
expect "encoder restarted for 1280x720 while capturing" "component output now 3" sh -c "sudo dmesg | tail -80"
run v4l2-ctl -d $D -v width=1920,height=1080,pixelformat=MPEG

source_mode 720p "Untick 1080i."
expect "720p: 1280x720 not offered as a scaled size" "^1$" sh -c "v4l2-ctl -d $D --list-framesizes MPEG | grep -c 1280x720"
out "720p -> 640x480" 640 480 progressive 30
out "720p -> 480x360" 480 360 progressive 22
run v4l2-ctl -d $D -c allow_50_60_fps=0
out "720p, Allow 50/60 fps off" 1280 720 progressive 31
run v4l2-ctl -d $D -c allow_50_60_fps=1
out "720p, Allow 50/60 fps on" 1280 720 progressive 40

source_mode 576p "Untick 720p."
run v4l2-ctl -d $D -c allow_50_60_fps=0
out "576p, Allow 50/60 fps off" 720 576 progressive 31
run v4l2-ctl -d $D -c allow_50_60_fps=1
out "576p, Allow 50/60 fps on" 720 576 progressive 30
out "576p asked for 640x480: source size" 720 576 progressive 30

source_mode 576i "Untick 576p."
out "576i -> 640x480" 640 480 progressive 30
out "576i source size" 720 576 tt 31

step "v4l2-compliance on component (last: it leaves its own control values)"
expect "v4l2-compliance on component: no failures" "Failed: 0" v4l2-compliance -d $D
set_input 2
