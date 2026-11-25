# T-051 (first step): component, encoder values of the Windows software
# per source mode at the standard output size (bitrate, GOP, level,
# system_rate, colour, v_vinpelclk, 1080p30 timing); checked in the stream:
# size, field order, level, colour primaries. 480i/480p later. A live viewer runs
# while the source's output is changed in its own menu; then a 10 s
# capture is checked. The input is selected again before each start (the
# driver detects the source mode then).
# Needs: a component source that can switch between 576i, 576p, 720p,
# 1080i and 1080p.

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
mode() {	# mode NAME W H FIELD LEVEL COLOUR [HINT]
	step "component $1"
	viewer_start
	prompt "A window 'Elgato component' shows the PS3. In the PS3's menu set its video output to $1.${7:+ $7} If the picture freezes or disappears after the change, close the window: it reopens with a fresh detection. Press Enter when the window shows the PS3 in $1."
	confirm "$1: is the picture right (colours as on a TV, sharp, motion smooth)?"
	viewer_stop
	set_input 1
	expect "$1: stream ${2}x${3} $4, level $5, colour $6 (encoder values as recorded)" \
		"width=$2 height=$3 .*level=$5 color_primaries=$6 field_order=$4" comp_cap
	run sh -c "sudo dmesg | grep 'detect component' | tail -1"
}

step "load the test build (fresh plug-in)"
load_module replug
set_input 2
set_fmt 720 480 MPEG
set_input 1

# Captured with the driver of dev 0.5.10 (old encoder values) on
# 2026-11-26: 576p, 720p, 1080i, 1080p right. Now the recorded values.
mode 576i 720 576 tt 31 smpte170m "In the PS3's video output settings untick 576p (the PS3 sends the highest resolution ticked)."
mode 576p 720 576 progressive 30 smpte170m
mode 720p 1280 720 progressive 40 bt709
mode 1080i 1920 1080 tt 40 bt709
mode 1080p 1920 1080 progressive 40 bt709
set_input 2
