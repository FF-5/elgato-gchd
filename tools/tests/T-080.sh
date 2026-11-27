# T-080, second run: the picture settings on composite and S-video (now in
# the analog decoder) and on component; the audio gain measured in steps
# with a steady sound. (Passed in the first run: controls, audio while
# capturing, -12 dB, replug, HDMI RGB, window, OBS, text menu, compliance.)
# Needs: the PS3 (composite, S-video, component cables) and a steady sound
# on the Elgato's analog audio input for the measurement (a phone or laptop
# headphone output with a 3.5 mm to RCA cable playing a continuous 1 kHz
# tone, or a PS3 screen with constant sound).

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

step "picture settings: composite"
sweep 2 composite
run v4l2-ctl -d $D -c saturation=-100
viewer_start 2 "Elgato composite"
prompt "Saturation is -100 (black and white). Switch the PS3 off and on again (or pull the yellow cable for 5 s and put it back); wait until the picture is back."
confirm "Composite after the signal came back: still black and white?"
viewer_stop
run v4l2-ctl -d $D -c saturation=0

step "picture settings: S-video"
prompt "Connect the PS3 with the S-video cable."
set_input 3
sweep 3 S-video

step "picture settings: component"
prompt "Connect the PS3 with the component cable (480p or 720p)."
set_input 1
sweep 1 component

step "audio gain measured in steps (steady sound)"
prompt "Connect the steady sound (continuous tone, medium volume, not changing) to the Elgato's analog audio input (red/white); composite video can stay. Start the sound now."
L0=$(level 0); Lm6=$(level -6); Lm12=$(level -12); Lp1=$(level 1); Lp6=$(level 6); Lp12=$(level 12); L0b=$(level 0)
log "mean volume (dB): 0: $L0, -6: $Lm6, -12: $Lm12, +1: $Lp1, +6: $Lp6, +12: $Lp12, 0 again: $L0b"
say "mean volume (dB): 0: $L0, -6: $Lm6, -12: $Lm12, +1: $Lp1, +6: $Lp6, +12: $Lp12, 0 again: $L0b"
expect "sound steady (0 dB twice within 1 dB)" "^ok" louder "$L0" "$L0b" -1 1
expect "-6 dB: 6 dB quieter (4-8)" "^ok" louder "$Lm6" "$L0" 4 8
expect "-12 dB: 12 dB quieter (10-14)" "^ok" louder "$Lm12" "$L0" 10 14
expect "+1 dB: 1 dB louder (0-2)" "^ok" louder "$L0" "$Lp1" 0 2
expect "+6 dB: 6 dB louder (4-8)" "^ok" louder "$L0" "$Lp6" 4 8
expect "+12 dB: 12 dB louder (10-14; less if the tone clips)" "^ok" louder "$L0" "$Lp12" 10 14
run v4l2-ctl -d $D -c analog_audio_gain_db=0
prompt "Stop the sound; reconnect the PS3 audio if you like."
set_input 2
