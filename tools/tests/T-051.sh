# T-051: component output size changed while capturing (encoder restart).
# Output sizes per source and "Allow 50/60 fps" were accepted on
# 2026-11-26 (sixth run).
# Needs: the PS3 on component, 1080i ticked in its video output settings.

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

source_mode 1080i "Tick 1080i again (untick 1080p if it is ticked)."
step "1080i: output size change while capturing"
run v4l2-ctl -d $D -v width=1920,height=1080,pixelformat=MPEG
set_input 1
# The size change runs in the background, 6 s after the capture starts.
( sleep 6; v4l2-ctl -d $D -v width=1280,height=720,pixelformat=MPEG ) >/dev/null 2>&1 &
run comp_cap
wait
expect "encoder restarted for 1280x720 while capturing" "component output now 3" sh -c "sudo dmesg | grep -E 'encoder restart|component output now' | tail -3"
step "back to the source size while capturing"
( sleep 6; v4l2-ctl -d $D -v width=1920,height=1080,pixelformat=MPEG ) >/dev/null 2>&1 &
run comp_cap
wait
expect "encoder restarted for the source size while capturing" "component output now 0" sh -c "sudo dmesg | grep -E 'encoder restart|component output now' | tail -3"
set_input 2
