# T-051: the OBS plugin's bitrate range right after an output size change
# (from the output mode, not the range read before). Accepted on
# 2026-11-26: bitrate settings per output mode in the driver, the window and
# the plugin (fourteenth run; its FAILs were a test pattern).
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

# The bitrate mode and the bitrate in use.
rates() { v4l2-ctl -d $D -C video_bitrate_mode,video_bitrate | sed 's/ ([^)]*)//' | tr '\n' ' '; echo; }
size() { run v4l2-ctl -d $D -v width=$1,height=$2,pixelformat=MPEG; }
# kept NAME MODE BITRATE: the controls show MODE (0 VBR, 1 CBR) and BITRATE.
kept() {
	expect "$1: mode $2, $3 bit/s" "video_bitrate_mode: $2 video_bitrate: $3" rates
}

step "install the settings programs and the plugin of this branch"
run sudo rmmod elgato_gchd
run bash "$INST" --all --yes --in-terminal < /dev/null
prompt "Unplug the Elgato and plug it in again; wait 5 s."
set_input 1
source_mode 1080i "Tick 1080i (and untick 1080p) so the PS3 sends 1080i."

step "settings"
run v4l2-ctl -d $D -c allow_50_60_fps=1
size 1280 720
run v4l2-ctl -d $D -c video_bitrate_mode=1
run v4l2-ctl -d $D -c video_bitrate=12000000
size 640 480
run v4l2-ctl -d $D -c video_bitrate_mode=1
run v4l2-ctl -d $D -c video_bitrate=10000000
size 1920 1080
run v4l2-ctl -d $D -c video_bitrate_mode=1
run v4l2-ctl -d $D -c video_bitrate=20000000
kept "1920x1080 CBR 20" 1 20000000

step "OBS plugin"
prompt "Start OBS (source 'Elgato Game Capture HD', input Component); open the source's properties."
prompt "Choose Output size 1280x720 and look at the slider at once."
confirm "OBS at once: Constant (CBR), slider from 9.33 to 15.00 at 12.00 (not 30.00)?"
prompt "Choose Output size 640x480 and look at once."
confirm "OBS at once: Constant (CBR), slider from 7.00 to 11.25 at 10.00?"
prompt "Choose Output size Source resolution and look at once."
confirm "OBS at once: Constant (CBR), slider from 18.67 to 30.00 at 20.00?"
prompt "Choose Output size 1280x720 again -> OK; wait 3 s."
kept "OBS 1280x720: CBR 12 Mbit/s" 1 12000000
prompt "Close OBS."
expect "OBS applied 12.00 for 1280x720" "output 3, mode 1, 12.00 Mbit/s" obs_log 8
set_input 2
