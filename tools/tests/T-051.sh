# T-051: TS samples per component mode for the merge (RULES F3: decoded
# without errors, size and field order; saved for the traces branch), and
# repeated bitrate changes while capturing at 1280x720 (an encoder restart
# timed out once, 2026-11-26). Everything else of T-051 is accepted.
# Needs: the PAL PS3 on component (1080i, 720p, 576p, 576i).

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

SAMPLES=$HOME/gchd-test-logs/T-051-samples
# sample NAME W H FIELD LEVEL: 4 s of the stream at W x H saved as
# component_NAME.ts with its ffprobe and ffmpeg output; checked.
sample() {
	local f=$SAMPLES/component_$1.ts
	mkdir -p $SAMPLES
	run v4l2-ctl -d $D -v width=$2,height=$3,pixelformat=MPEG
	set_input 1
	timeout -k 5 30 bash -c "head -c 40000000 < $D > $f & p=\$!; sleep 4; kill \$p 2>/dev/null; wait \$p" 2>/dev/null
	ffprobe -v error -show_entries stream=codec_name,width,height,field_order,r_frame_rate,sample_aspect_ratio,level,profile,color_primaries,sample_rate,channels -of default=nw=1 $f > $f.ffprobe.txt 2>&1
	ffmpeg -v error -i $f -f null - > $f.ffmpeg.txt 2>&1
	expect "$1: ${2}x${3} $4, level $5, decodes without errors" \
		"width=$2 height=$3 .*level=$5 .*field_order=$4 .*errors=0" \
		sh -c "tr '\n' ' ' < $f.ffprobe.txt; echo errors=\$(wc -l < $f.ffmpeg.txt)"
}

step "install"
run sudo rmmod elgato_gchd
run bash "$INST" --all --yes --in-terminal < /dev/null
prompt "Unplug the Elgato and plug it in again; wait 5 s."
set_input 1
run v4l2-ctl -d $D -c allow_50_60_fps=1

source_mode 1080i "Tick 1080i (and untick 1080p) so the PS3 sends 1080i."
step "samples 1080i"
sample 1080i 1920 1080 tt 40
sample 1080i_1280x720 1280 720 progressive 31

step "bitrate changes while capturing at 1280x720"
run v4l2-ctl -d $D -v width=1280,height=720,pixelformat=MPEG
set_input 1
N0=$(sudo dmesg | wc -l)
since() { sudo dmesg | tail -n +$((N0 + 1)); }
count_new() { since | grep -cE "$1"; }
restarts() { since | grep -E "encoder restart|output now|failed"; }
( timeout -k 5 60 cat $D > /dev/null ) &
READER=$!
sleep 5
for b in 9500000 14000000 10000000 15000000 12000000 9330000 13000000 12000000; do
	run v4l2-ctl -d $D -c video_bitrate=$b
	sleep 4
done
sleep 5
kill $READER 2>/dev/null; wait $READER 2>/dev/null
expect "8 encoder restarts while capturing" "^([8-9]|[1-9][0-9])$" count_new "component output now 3"
expect "no failed restart" "^0$" count_new "restart failed|change failed"
run restarts

source_mode 720p "Tick only 720p."
step "sample 720p"
sample 720p 1280 720 progressive 40

source_mode 576p "Tick only 576p."
step "sample 576p"
sample 576p 720 576 progressive 30

source_mode 576i "Untick all HD resolutions and 576p."
step "sample 576i"
sample 576i 720 576 tt 31

step "samples"
run ls -l $SAMPLES
prompt "Upload the folder $SAMPLES to the traces branch as traces/component/samples/ (or tell me where it is)."
set_input 2
