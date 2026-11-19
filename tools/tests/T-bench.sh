# T-bench: latency benchmark. The driver is loaded with timing=1 and logs
# "gchd-timing" marks (requests, device commands and their completion,
# first stream data, first buffer). The operations are repeated; at the end
# gchd-timing.py prints the tables (driver: ms since the request; driver
# and the recordings of the Windows software: ms since the NULL command)
# and saves them to timing.md in the log folder.
# Needs: a console on composite (PS1 or N64), python3.

# reader SECONDS: read the transport stream like an application.
reader() {
	timeout -s INT -k 5 $(($1 + 15)) \
		ffmpeg -hide_banner -nostdin -loglevel error -f mpegts -i "$DEV" -t "$1" -f null - 2>&1 | tail -2
}

step "driver with timing marks"
run sh -c "pkill -f 'gchd-bridge run'; true"
load_module replug timing=1
set_input 2
run v4l2-ctl -d "$DEV" -v width=720,height=480,pixelformat=MPEG -c video_bitrate=11250000,video_bitrate_mode=1
prompt "Console on composite, game running (a moving picture)."

step "capture start x5"
for n in 1 2 3 4 5; do
	say "capture start $n"
	reader 8 > /dev/null
	sleep 4
done

step "bitrate change x5 while capturing"
reader 55 > /dev/null &
READER=$!
sleep 10
for n in 1 2 3 4 5; do
	if [ $((n % 2)) = 1 ]; then
		run v4l2-ctl -d "$DEV" -c video_bitrate_mode=0,video_bitrate=8000000
	else
		run v4l2-ctl -d "$DEV" -c video_bitrate_mode=1,video_bitrate=11250000
	fi
	sleep 8
done
wait "$READER"
run v4l2-ctl -d "$DEV" -c video_bitrate_mode=1,video_bitrate=11250000
sleep 4

step "output size change x4 while capturing"
reader 45 > /dev/null &
READER=$!
sleep 10
for size in 640x480 720x480 640x480 720x480; do
	run v4l2-ctl -d "$DEV" -v width=${size%x*},height=${size#*x}
	sleep 8
done
wait "$READER"
sleep 4

step "input change x2 while capturing (S-video without a source, back to composite)"
reader 40 > /dev/null &
READER=$!
sleep 10
for n in 1 2; do
	run v4l2-ctl -d "$DEV" -i 3
	sleep 5
	run v4l2-ctl -d "$DEV" -i 2
	sleep 8
done
wait "$READER"
sleep 4

step "source change x3 (console reset) while capturing"
reader 60 > /dev/null &
READER=$!
sleep 8
for n in 1 2 3; do
	prompt "Press RESET on the console now (change $n of 3); wait for the picture, then Enter."
done
wait "$READER"

step "tables"
git -C "$REPO" show origin/tools:tools/gchd-timing.py > "$WORK/gchd-timing.py"
for t in gchd_NTSC_composite_output_bitrate_audio_small gchd_PAL_composite_bitrate_audio_small \
	 gchd_288p_standard_bitrate_change_small gchd_288p_mobile_bitrate_change_small; do
	git -C "$REPO" show "origin/traces:traces/composite/$t.ops.txt" > "$WORK/$t.ops.txt"
done
end_step
python3 "$WORK/gchd-timing.py" both --driver "$LOG" --trace "$WORK"/*.ops.txt | tee "$WORK/timing.md" | tee -a "$LOG"
expect "timing marks recorded" "capture-start" cat "$WORK/timing.md"
