# T-060: application device of the driver ("Elgato Game Capture HD", YUYV)
# fed by service/gchd-bridge: capture device only on while an application
# streams, re-registration on a format change (standard, input), audio
# source, guvcview reconnecting after a re-registration.
# Needs: pactl (PipeWire or PulseAudio), guvcview. PS1 (PAL) and N64 (NTSC).

BRIDGE="$REPO/service/gchd-bridge"

app_attr() { cat /sys/bus/usb/drivers/elgato_gchd/*/app 2>/dev/null; }
app_dev()  { echo "/dev/$(app_attr | awk '{ print $2 }')"; }

# app_read SECONDS: read the application device like an application;
# prints the stream line and the last frame count. Time limit SECONDS + 20.
app_read() {
	timeout -s INT -k 5 $(($1 + 20)) \
		ffmpeg -hide_banner -nostdin -f v4l2 -i "$(app_dev)" -t "$1" -f null - 2>&1 |
		tr '\r' '\n' > "$WORK/ffmpeg-read.txt"
	grep -m1 'Stream #0' "$WORK/ffmpeg-read.txt"
	grep -E 'Error|error|No such device' "$WORK/ffmpeg-read.txt" | tail -2
	grep '^frame=' "$WORK/ffmpeg-read.txt" | tail -1
}

# sink_streams: streams playing into the bridge's sink "elgato_gchd".
sink_streams() {
	local i
	i=$(pactl list short sinks | awk '$2 == "elgato_gchd" { print $1 }')
	pactl list short sink-inputs | awk -v i="$i" '$2 == i { print "stream into elgato_gchd:", $0 }'
}

step "driver with three devices"
run sh -c "pkill -f 'gchd-bridge run'; sudo modprobe -r v4l2loopback; true"
load_module replug
run v4l2-ctl --list-devices
expect "H.264 device name" "Card type *: Elgato Game Capture HD \(H.264\)" v4l2-ctl -d "$DEV" -D
expect "application device registered, idle, H.264 device named" "^idle video[0-9]+ [0-9]+ [0-9]+ [0-9]+ [0-9]+ video[0-9]+ $(basename "$DEV")$" app_attr
expect "application device offers YUYV" "YUYV" v4l2-ctl -d "$(app_dev)" --list-formats-ext

step "bridge idle: capture device off"
prompt "PS1 on composite, game running."
set_input 2
bash "$BRIDGE" run >> "$WORK/bridge.log" 2>&1 &
BRIDGE_PID=$!
sleep 3
run bash "$BRIDGE" status
expect "bridge running" "service: *running" bash "$BRIDGE" status
expect "application device on composite: 720x480 or 720x576" " 720 (480|576) " app_attr
expect "capture device off while no application streams" "^0$" sh -c "sudo dmesg | grep -c 'starting hardware boot'"

step "first read: source detected (PAL), device registered again with 720x576"
app_read 20 > "$WORK/read1.txt"
run cat "$WORK/read1.txt"
sleep 4
run app_attr
expect "application device now 720x576, 50 frames/s" " 720 576 50 1 " app_attr
run tail -8 "$WORK/bridge.log"

step "read: 720x576 YUYV, about 50 frames/s"
app_read 15 > "$WORK/read2.txt"
run cat "$WORK/read2.txt"
expect "stream YUYV 720x576" "yuyv422.*720x576" cat "$WORK/read2.txt"
expect "frame count >= 450 in 15 s" "frame= *([4-9][5-9][0-9]|[5-9][0-9]{2}|[0-9]{4})" cat "$WORK/read2.txt"
expect "capture device started" "starting hardware boot" sudo dmesg

step "application closed: capture device off"
sleep 6
expect "hardware shut down after the application stopped" "hardware shutdown done" sudo dmesg
expect "state idle" "^idle " app_attr

step "audio while capturing"
app_read 14 > /dev/null &
READER=$!
sleep 8
expect "the bridge plays into the audio sink" "stream into elgato_gchd" sink_streams
expect "audio source delivers samples" "n_samples: [1-9]" \
	ffmpeg -hide_banner -nostdin -f pulse -i elgato_gchd_audio -t 4 -af volumedetect -f null -
wait "$READER"

step "input change on the application device while reading"
app_read 30 > "$WORK/read3.txt" &
READER=$!
sleep 10
run v4l2-ctl -d "$(app_dev)" -i 3
sleep 4
run app_attr
wait "$READER"
run cat "$WORK/read3.txt"
expect "S-video (no source, NTSC size): device re-registered with 720x480" " 720 480 60000 1001 " app_attr
run v4l2-ctl -d "$(app_dev)" -i 2
sleep 2
expect "composite again: 720x576" " 720 576 50 1 " app_attr
expect "input composite on the H.264 device too" "Video input : 2" v4l2-ctl -d "$DEV" --get-input

step "guvcview"
prompt "Start in a second terminal: guvcview -d $(app_dev)   (leave it open)"
confirm "guvcview shows the PS1 picture, smooth, without interlacing lines?"
prompt "Swap the PS1 for the N64 (NTSC) while guvcview stays open; wait about 15 s."
run app_attr
confirm "guvcview reconnected by itself and shows the N64 picture (720x480)?"
prompt "Close guvcview."
run tail -20 "$WORK/bridge.log"

step "bridge stopped"
run kill "$BRIDGE_PID"
sleep 4
expect "audio source removed" "^0$" sh -c "pactl list short sources | grep -c elgato_gchd"
