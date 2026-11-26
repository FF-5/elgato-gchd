# T-051: component 576p with scaled outputs (PAL PS3): 640x480 and
# 480x360, "Allow 50/60 fps" at 640x480. Not recorded; the driver follows
# the 480p recordings. Everything else on component was accepted on
# 2026-11-26.
# Needs: the PAL PS3 on component.

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
set_fmt 720 576 MPEG
set_input 1
run v4l2-ctl -d $D -c allow_50_60_fps=1

source_mode 576p "Untick 1080p, 1080i and 720p (the PS3 sends the highest resolution ticked)."
expect "576p: three output sizes offered" "720x576.*640x480.*480x360" sh -c "v4l2-ctl -d $D --list-framesizes MPEG | tr '\n' ' '"
out "576p -> 640x480, option on" 640 480 progressive 30
out "576p -> 480x360" 480 360 progressive 22
run v4l2-ctl -d $D -c allow_50_60_fps=0
out "576p -> 640x480, option off" 640 480 progressive 31
run v4l2-ctl -d $D -c allow_50_60_fps=1
step "576p scaled: picture"
run v4l2-ctl -d $D -v width=640,height=480,pixelformat=MPEG
play 12 "576p at 640x480: is the picture right (whole picture, not cut, motion smooth)?"
run v4l2-ctl -d $D -v width=480,height=360,pixelformat=MPEG
play 12 "576p at 480x360: is the picture right?"
run v4l2-ctl -d $D -v width=720,height=576,pixelformat=MPEG
set_input 2
