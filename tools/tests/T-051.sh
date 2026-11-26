# T-051: component bitrate as composite (bitrate mode, a bitrate kept per
# mode, the output's range; CBR/VBR ranges in the window and the plugin are
# placeholders until TODO R13) in the window and the plugin; the window
# after Composite -> Component (sizes and bitrate from the mode detected
# last). The driver's kept bitrate per mode and across output sizes was
# accepted on 2026-11-27.
# Needs: the PS3 on component in 1080i, OBS with the plugin.

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

rate() { v4l2-ctl -d $D -C video_bitrate; }

step "install the settings programs and the plugin of this branch"
run sudo rmmod elgato_gchd
run bash "$INST" --all --yes --in-terminal < /dev/null
prompt "Unplug the Elgato and plug it in again; wait 5 s."
set_input 1
source_mode 1080i "Tick 1080i (and untick 1080p) so the PS3 sends 1080i."

step "settings"
run v4l2-ctl -d $D -v width=1920,height=1080,pixelformat=MPEG
run v4l2-ctl -d $D -c video_bitrate_mode=0
run v4l2-ctl -d $D -c video_bitrate=16000000
run v4l2-ctl -d $D -c video_bitrate_mode=1
run v4l2-ctl -d $D -c video_bitrate=20000000

step "settings window"
prompt "Open 'Elgato Game Capture HD Settings' (Input Component)."
confirm "Bitrate mode is shown; Constant (CBR) with the slider from 18.67 to 30.00 at 20.00?"
prompt "Choose Input Composite, wait 3 s, then Input Component again; wait 3 s."
confirm "Component again: Output size still offers 1920x1080, 1280x720, 640x480, 480x360; Bitrate mode and the slider (18.67 to 30.00 at 20.00) are shown?"
prompt "Choose Variable (VBR); wait 3 s."
confirm "slider from 8.00 to 15.00 at 8.00 (VBR 16 Mbit/s is shown as its average, 8.00)?"
prompt "Move the slider to about 10.00 and release it; wait 3 s."
expect "window: VBR about 20 Mbit/s (10.00 average)" "video_bitrate: (19[5-9]|20[0-5])[0-9]{5}" rate
prompt "Choose Output size 640x480; wait 3 s; then 1920x1080 again; wait 3 s."
confirm "back at 1920x1080 the slider stands at about 10.00 again (not at the highest)?"
prompt "Choose Output size 480x360; wait 3 s."
confirm "Bitrate mode locked to Variable (VBR), slider 4.00 to 7.50?"
prompt "Output size 1920x1080, Bitrate mode Constant (CBR); wait 3 s; leave the window open."

step "OBS plugin"
prompt "Start OBS (source 'Elgato Game Capture HD', input Component); open the source's properties."
confirm "OBS: Bitrate mode Constant (CBR), Bitrate from 18.67 to 30.00 at about 20.00; dragging the slider fully to the right reaches 30.00 (not 29.99)?"
prompt "In OBS's properties choose Variable (VBR) -> OK; wait 3 s."
expect "OBS: VBR with the kept VBR bitrate (about 20 Mbit/s)" "video_bitrate: (19[5-9]|20[0-5])[0-9]{5}" rate
confirm "the settings window shows Variable (VBR) at about 10.00 by itself?"
prompt "Back to Constant (CBR), slider 30.00 -> OK. Close OBS and the settings window."
run obs_log 8
set_input 2
