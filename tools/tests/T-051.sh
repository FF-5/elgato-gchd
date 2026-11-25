# T-051 (first step): component, encoder values of the Windows software
# per source mode at the standard output size (bitrate, GOP, level,
# system_rate, colour, v_vinpelclk, 1080p30 timing). Each mode is captured
# after selecting the input again (the driver detects the mode then).
# Needs: a component source that can switch between 576i, 576p, 720p,
# 1080i and 1080p (and 480i/480p if it can).

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
mode() {	# mode NAME W H FIELD
	step "component $1"
	prompt "Set the component source to $1 (game or menu on screen); wait 5 s."
	set_input 1
	run v4l2-ctl -d $D -I -V
	expect "$1: stream ${2}x${3}, field order $4" "width=$2 height=$3 .*field_order=$4" comp_cap
	confirm "$1: is the picture right (no colour cast, sharp, motion smooth)?"
}

step "load module (fresh plug-in)"
load_module replug
set_input 2
set_fmt 720 480 MPEG

mode 576i 720 576 tt
mode 576p 720 576 progressive
mode 720p 1280 720 progressive
mode 1080i 1920 1080 tt
mode 1080p 1920 1080 progressive

step "480i / 480p (if the source offers them)"
read -r -p "Can the component source output 480i and 480p? [y/N] " a
log "480 modes available: $a"
if [[ "$a" == [yYjJ]* ]]; then
	mode 480i 720 480 tt
	mode 480p 720 480 progressive
fi
set_input 2
