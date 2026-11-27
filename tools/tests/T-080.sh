# T-080, fourth run: picture settings made before the capture are used
# from the capture start and kept after the signal returns (composite,
# S-video); the neutral picture at 0. Passed before: everything else.
# Needs: the PS3 with the composite and the S-video cable.
INST="$REPO/install-driver.sh"
D=$DEV
VSTOP=/tmp/gchd-viewer-stop
viewer_start() {	# viewer_start INPUT TITLE
	rm -f $VSTOP
	( while [ ! -e $VSTOP ]; do
		v4l2-ctl -d $D -i $1 >/dev/null 2>&1
		ffplay -loglevel error -autoexit -window_title "$2" -f mpegts $D >/dev/null 2>&1
		sleep 1
	  done ) &
	VIEWER=$!
}
viewer_stop() {
	touch $VSTOP
	pkill -f "window_title Elgato" 2>/dev/null
	wait $VIEWER 2>/dev/null
	sleep 2
}
N0=0
mark() { N0=$(sudo dmesg | wc -l); }
count_new() { sudo dmesg | tail -n +$((N0 + 1)) | grep -cE "$1"; }
pic() { v4l2-ctl -d $D -C brightness,contrast,saturation,hue,analog_audio_gain_db | tr '\n' ' '; echo; }
# level GAIN: 12 s of the composite stream at GAIN dB; mean volume in dB.
level() {
	local f=/tmp/gchd-audio.ts
	v4l2-ctl -d $D -c analog_audio_gain_db=$1
	v4l2-ctl -d $D -i 2 >/dev/null
	timeout -k 5 25 head -c 30000000 < $D > $f 2>/dev/null
	ffmpeg -hide_banner -ss 4 -t 8 -i $f -map 0:a -af volumedetect -f null - 2>&1 | sed -n 's/.*mean_volume: \(-*[0-9.]*\) dB.*/\1/p'
}
# louder A B MIN MAX: B - A within MIN ... MAX dB.
louder() { [ -n "$1" ] && [ -n "$2" ] || { echo "no (no level measured)"; return; }; awk -v a="$1" -v b="$2" -v lo="$3" -v hi="$4" 'BEGIN { d = b - a; print (d >= lo && d <= hi ? "ok" : "no"), d }'; }
# sweep INPUT NAME: each picture setting to both ends and back.
sweep() {
	mark
	viewer_start $1 "Elgato $2"
	prompt "A window 'Elgato $2' shows the PS3. Watch it during the next changes (3 s each)."
	for c in "brightness=-100" "brightness=100" "brightness=0"; do run v4l2-ctl -d $D -c $c; sleep 3; done
	confirm "$2 brightness: dark, then bright, then normal, without stopping?"
	for c in "contrast=-100" "contrast=100" "contrast=0"; do run v4l2-ctl -d $D -c $c; sleep 3; done
	confirm "$2 contrast: flat, then hard, then normal?"
	for c in "saturation=-100" "saturation=100" "saturation=0"; do run v4l2-ctl -d $D -c $c; sleep 3; done
	confirm "$2 saturation: black and white, then very colourful, then normal?"
	for c in "hue=-180" "hue=180" "hue=0"; do run v4l2-ctl -d $D -c $c; sleep 3; done
	confirm "$2 hue: colours shifted one way, then the other way, then normal?"
	viewer_stop
	expect "$2: no encoder restart for the picture changes" "^0$" count_new "encoder restart"
}

step "install"
run sudo rmmod elgato_gchd
run bash "$INST" --all --yes --in-terminal < /dev/null
prompt "PS3 on composite (video and audio). Unplug the Elgato and plug it in again; wait 5 s."
set_input 2
run v4l2-ctl -d $D -v width=720,height=480,pixelformat=MPEG
run v4l2-ctl -d $D -c brightness=0,contrast=0,saturation=0,hue=0,analog_audio_gain_db=0

# kept INPUT NAME: settings before the capture, then a signal loss.
kept() {
	run v4l2-ctl -d $D -c saturation=-100,brightness=40
	viewer_start $1 "Elgato $2"
	prompt "A window 'Elgato $2' shows the PS3."
	confirm "$2 from the start: black and white and brighter than normal?"
	prompt "Pull the video cable (yellow for composite, the S-video plug) for 5 s and put it back; wait until the picture is back."
	confirm "$2 after the signal came back: still black and white and brighter?"
	viewer_stop
	run v4l2-ctl -d $D -c saturation=0,brightness=0
	viewer_start $1 "Elgato $2"
	prompt "Watch the window 'Elgato $2' again (all settings 0)."
	confirm "$2 at 0: normal colours and brightness (like before this test)?"
	viewer_stop
}

step "settings kept from the capture start: composite"
kept 2 composite

step "settings kept from the capture start: S-video"
prompt "Connect the PS3 with the S-video cable."
set_input 3
kept 3 S-video

set_input 2
