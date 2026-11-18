# T-060: application bridge (service/gchd-bridge): v4l2loopback device with
# YUYV frames, audio source, capture device only on while an application
# reads, input selection, source change while open, guvcview.
# Needs: v4l2loopback-dkms, pactl (PipeWire or PulseAudio), guvcview.

LOOP=/dev/video42
BRIDGE="$REPO/service/gchd-bridge"

# loop_read SECONDS: read the loopback device like an application; prints
# the stream line and the last frame count.
loop_read() {
	ffmpeg -hide_banner -nostdin -f v4l2 -i "$LOOP" -t "$1" -f null - 2>&1 |
		tr '\r' '\n' > "$WORK/ffmpeg-read.txt"
	grep -m1 'Stream #0' "$WORK/ffmpeg-read.txt"
	grep '^frame=' "$WORK/ffmpeg-read.txt" | tail -1
}

# sink_streams: streams playing into the bridge's sink "elgato_gchd".
sink_streams() {
	local i
	i=$(pactl list short sinks | awk '$2 == "elgato_gchd" { print $1 }')
	pactl list short sink-inputs | awk -v i="$i" '$2 == i { print "stream into elgato_gchd:", $0 }'
}

step "v4l2loopback and the driver"
run sh -c "modinfo v4l2loopback | grep -E '^(version|filename)'" ||
	prompt "Install v4l2loopback: sudo apt install v4l2loopback-dkms (then Enter)"
load_module replug
run sudo modprobe -r v4l2loopback
run sudo modprobe v4l2loopback devices=1 video_nr=42 exclusive_caps=1 "card_label=Elgato Game Capture HD"
expect "loopback device present" "video42" v4l2-ctl --list-devices

step "bridge idle: black picture, capture device off"
prompt "PS1 on composite, game running."
set_input 2
bash "$BRIDGE" run >> "$WORK/bridge.log" 2>&1 &
BRIDGE_PID=$!
sleep 4
expect "bridge running and both devices found" "loopback device: /dev/video42" bash "$BRIDGE" status
run grep -q "service: *running" <(bash "$BRIDGE" status) || run cat "$WORK/bridge.log"
expect "loopback offers YUYV of the driver's current size while idle" "720/(480|576)" v4l2-ctl -d "$LOOP" --get-fmt-video
expect "audio source present" "elgato_gchd_audio" pactl list short sources
expect "capture device off while idle" "^0$" sh -c "sudo dmesg | grep -c 'starting hardware boot'"

step "application reads: capture starts, 720x576 at 50 frames/s"
loop_read 15 > "$WORK/read.txt"
run cat "$WORK/read.txt"
expect "loopback stream YUYV 720x480 or 720x576" "yuyv422.*720x(480|576)" cat "$WORK/read.txt"
expect "frame count >= 400 in 15 s (capture running)" "frame= *([4-9][0-9]{2}|[0-9]{4})" cat "$WORK/read.txt"
expect "capture device started" "starting hardware boot" sudo dmesg

step "application closed: capture device off"
sleep 5
expect "hardware shut down after the last reader" "hardware shutdown done" sudo dmesg

step "audio while capturing"
loop_read 14 > /dev/null &
READER=$!
sleep 7
expect "the bridge plays into the audio sink" "stream into elgato_gchd" sink_streams
expect "audio source delivers samples" "n_samples: [1-9]" \
	ffmpeg -hide_banner -nostdin -f pulse -i elgato_gchd_audio -t 4 -af volumedetect -f null -
wait "$READER"

step "input change through the bridge while an application reads"
loop_read 24 > "$WORK/input-change.txt" &
READER=$!
sleep 8
run bash "$BRIDGE" input svideo
sleep 5
run bash "$BRIDGE" input composite
wait "$READER"
run cat "$WORK/input-change.txt"
expect "reader kept running (frame count >= 500)" "frame= *([5-9][0-9]{2}|[0-9]{4})" cat "$WORK/input-change.txt"
expect "input back on composite" "Video input : 2" v4l2-ctl -d "$DEV" --get-input

step "guvcview"
prompt "Start: guvcview -d $LOOP   (in a second terminal; leave it open). If guvcview opens another device, select \"Elgato Game Capture HD\" at $LOOP, not the one at $DEV."
confirm "guvcview shows the PS1 picture, smooth, no interlacing lines?"
prompt "Swap the PS1 for the N64 (NTSC) while guvcview stays open; wait about 10 s."
confirm "guvcview shows the N64 picture again (scaled to the PAL size)?"
prompt "Close guvcview."
sleep 7
expect "idle size follows the NTSC source after closing" "720/480" v4l2-ctl -d "$LOOP" --get-fmt-video

step "bridge stopped"
run kill "$BRIDGE_PID"
sleep 2
expect "audio source removed" "^0$" sh -c "pactl list short sources | grep -c elgato_gchd"
run tail -30 "$WORK/bridge.log"
run sudo modprobe -r v4l2loopback
