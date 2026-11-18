# T-060: application device "Elgato Game Capture HD" (registered while the
# module is loaded, YUYV, size chosen by the application) fed by
# service/gchd-bridge: capture device only on while an application streams,
# format follows the source while idle, scaling to a chosen size, input
# change, unplug/replug while reading (black picture), audio, guvcview.
# Needs: pactl (PipeWire or PulseAudio), guvcview. PS1 (PAL) and N64 (NTSC).

BRIDGE="$REPO/service/gchd-bridge"

app_sys()   { dirname "$(ls /sys/class/video4linux/video*/gchd_state 2>/dev/null | head -1)"; }
app_state() { cat "$(app_sys)/gchd_state" 2>/dev/null; }
app_dev()   { echo "/dev/$(basename "$(app_sys)")"; }

# app_read SECONDS [WxH]: read the application device like an application;
# prints the stream line, errors and the last frame count.
app_read() {
	local size=()
	[ -n "${2:-}" ] && size=(-video_size "$2")
	timeout -s INT -k 5 $(($1 + 20)) \
		ffmpeg -hide_banner -nostdin -f v4l2 "${size[@]}" -i "$(app_dev)" -t "$1" -f null - 2>&1 |
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

step "driver: application device registered at module load"
run sh -c "pkill -f 'gchd-bridge run'; sudo modprobe -r v4l2loopback; true"
load_module replug
run v4l2-ctl --list-devices
expect "H.264 device found by name" "Card type *: Elgato Game Capture HD \(H.264\)" v4l2-ctl -d "$DEV" -D
expect "application device idle, Game Capture HD attached" "^idle [0-9]+ [0-9]+ [0-9]+ [0-9]+ video[0-9]+ $(basename "$DEV")$" app_state
expect "sizes 1920x1080 ... 640x480 offered" "1920x1080(.|\n)*720x576(.|\n)*640x480" sh -c "v4l2-ctl -d $(app_dev) --list-formats-ext | tr '\n' ' '"

step "bridge idle: capture device off"
prompt "PS1 on composite, game running."
set_input 2
bash "$BRIDGE" run >> "$WORK/bridge.log" 2>&1 &
BRIDGE_PID=$!
sleep 3
run bash "$BRIDGE" status
expect "bridge running" "service: *running" bash "$BRIDGE" status
expect "capture device off while no application streams" "^0$" sh -c "sudo dmesg | grep -c 'starting hardware boot'"

step "first read (format before detection), then the format follows the source"
app_read 12 > "$WORK/read1.txt"
run cat "$WORK/read1.txt"
expect "frames delivered (>= 200 in 12 s)" "frame= *([2-9][0-9]{2}|[0-9]{4})" cat "$WORK/read1.txt"
expect "reader not cut off (no 'No such device')" "^0$" grep -c "No such device" "$WORK/read1.txt"
sleep 4
run app_state
expect "idle format now the PAL source: 720x576, 50 frames/s" "^idle 720 576 50 1 " app_state
run tail -6 "$WORK/bridge.log"

step "read the source format: 720x576 YUYV, about 50 frames/s"
app_read 15 > "$WORK/read2.txt"
run cat "$WORK/read2.txt"
expect "stream YUYV 720x576" "yuyv422.*720x576" cat "$WORK/read2.txt"
expect "frame count >= 450 in 15 s" "frame= *(4[5-9][0-9]|[5-9][0-9]{2}|[0-9]{4})" cat "$WORK/read2.txt"
expect "capture device started" "starting hardware boot" sudo dmesg

step "application chooses 1280x720: scaled"
app_read 10 1280x720 > "$WORK/read3.txt"
run cat "$WORK/read3.txt"
expect "stream YUYV 1280x720" "yuyv422.*1280x720" cat "$WORK/read3.txt"
expect "frames delivered (>= 250 in 10 s)" "frame= *([2-9][5-9][0-9]|[3-9][0-9]{2}|[0-9]{4})" cat "$WORK/read3.txt"

step "application closed: capture device off"
sleep 6
expect "hardware shut down after the application stopped" "hardware shutdown done" sudo dmesg
expect "state idle" "^idle " app_state

step "audio while capturing"
app_read 14 > /dev/null &
READER=$!
sleep 8
expect "the bridge plays into the audio sink" "stream into elgato_gchd" sink_streams
expect "audio source delivers samples" "n_samples: [1-9]" \
	ffmpeg -hide_banner -nostdin -f pulse -i elgato_gchd_audio -t 4 -af volumedetect -f null -
wait "$READER"

step "input change on the application device while reading"
app_read 30 > "$WORK/read4.txt" &
READER=$!
sleep 10
run v4l2-ctl -d "$(app_dev)" -i 3
sleep 5
run v4l2-ctl -d "$(app_dev)" -i 2
wait "$READER"
run cat "$WORK/read4.txt"
expect "reader kept the device (no 'No such device')" "^0$" grep -c "No such device" "$WORK/read4.txt"
expect "reader got frames (>= 600 in 30 s)" "frame= *([6-9][0-9]{2}|[0-9]{4})" cat "$WORK/read4.txt"
expect "input composite on the H.264 device too" "Video input : 2" v4l2-ctl -d "$DEV" --get-input
run tail -10 "$WORK/bridge.log"

step "unplug and replug while reading"
app_read 45 > "$WORK/read5.txt" &
READER=$!
sleep 8
prompt "Unplug the Elgato (or detach it in VirtualBox) now, wait 5 s, then plug it in again."
sleep 3
run app_state
wait "$READER"
run cat "$WORK/read5.txt"
expect "reader kept the device through unplug/replug" "^0$" grep -c "No such device" "$WORK/read5.txt"
expect "frames throughout (>= 900 in 45 s)" "frame= *(9[0-9]{2}|[0-9]{4})" cat "$WORK/read5.txt"
find_h264_dev
expect "Game Capture HD attached again" "^idle .* $(basename "$DEV")$" app_state
run tail -10 "$WORK/bridge.log"

step "guvcview"
prompt "Start in a second terminal: guvcview -d $(app_dev)   (leave it open)"
confirm "guvcview shows the PS1 picture, smooth, without interlacing lines?"
prompt "Swap the PS1 for the N64 (NTSC) while guvcview stays open; wait about 15 s."
run app_state
confirm "guvcview shows the N64 picture (in the PAL size, scaled) without reopening?"
prompt "Close guvcview."
sleep 3
expect "idle format follows the NTSC source after closing" "^idle 720 480 60000 1001 " app_state
run tail -20 "$WORK/bridge.log"

step "bridge stopped"
run kill "$BRIDGE_PID"
sleep 4
expect "audio source removed" "^0$" sh -c "pactl list short sources | grep -c elgato_gchd"
