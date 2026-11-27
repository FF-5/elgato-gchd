# T-080: picture settings (brightness, contrast, saturation, hue) and the
# analog audio gain: while capturing without an encoder restart, the gain
# measured as the audio level, remembered after a replug, HDMI RGB with
# picture settings, settings window, text menu, OBS plugin, compliance.
# Needs: the PS3 on composite playing something with continuous sound (a
# video or game music), the laptop on HDMI, OBS with the plugin.

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

step "install"
run sudo rmmod elgato_gchd
run bash "$INST" --all --yes --in-terminal < /dev/null
prompt "PS3 on composite playing something with continuous sound; laptop on HDMI. Unplug the Elgato and plug it in again; wait 5 s."
set_input 2
run v4l2-ctl -d $D -v width=720,height=480,pixelformat=MPEG
run v4l2-ctl -d $D -c brightness=0,contrast=0,saturation=0,hue=0,analog_audio_gain_db=0
expect "controls present, all 0" "brightness: 0 contrast: 0 saturation: 0 hue: 0 analog_audio_gain_db: 0" pic

step "picture settings while capturing (composite)"
mark
viewer_start 2 "Elgato composite"
prompt "A window 'Elgato composite' shows the PS3. Watch it during the next changes (each is applied 3 s later)."
for c in "brightness=-100" "brightness=100" "brightness=0"; do run v4l2-ctl -d $D -c $c; sleep 3; done
confirm "Brightness: the picture became dark, then bright, then normal, without stopping?"
for c in "contrast=-100" "contrast=100" "contrast=0"; do run v4l2-ctl -d $D -c $c; sleep 3; done
confirm "Contrast: flat grey, then very hard, then normal?"
for c in "saturation=-100" "saturation=100" "saturation=0"; do run v4l2-ctl -d $D -c $c; sleep 3; done
confirm "Saturation: black and white, then very colourful, then normal?"
for c in "hue=-180" "hue=180" "hue=0"; do run v4l2-ctl -d $D -c $c; sleep 3; done
confirm "Hue: the colours shifted one way, then the other way, then normal?"
run v4l2-ctl -d $D -c analog_audio_gain_db=-12; sleep 4
run v4l2-ctl -d $D -c analog_audio_gain_db=12; sleep 4
run v4l2-ctl -d $D -c analog_audio_gain_db=0; sleep 2
confirm "Audio: quieter, then louder (maybe distorted), then normal, while the picture kept running?"
viewer_stop
expect "no encoder restart for picture or audio changes" "^0$" count_new "encoder restart"

step "audio gain measured (composite)"
L0=$(level 0); Lm=$(level -12); Lp=$(level 6)
log "mean volume: 0 dB: $L0, -12 dB: $Lm, +6 dB: $Lp"
say "mean volume: 0 dB: $L0 dB, -12 dB: $Lm dB, +6 dB: $Lp dB"
expect "-12 dB: about 12 dB quieter (8-16)" "^ok" sh -c "awk -v a='$L0' -v b='$Lm' 'BEGIN { d = a - b; print (d >= 8 && d <= 16 ? \"ok\" : \"no\"), d }'"
expect "+6 dB: about 6 dB louder (3-9)" "^ok" sh -c "awk -v a='$L0' -v b='$Lp' 'BEGIN { d = b - a; print (d >= 3 && d <= 9 ? \"ok\" : \"no\"), d }'"
v4l2-ctl -d $D -c analog_audio_gain_db=0

step "remembered after a replug"
run v4l2-ctl -d $D -c brightness=20,contrast=-10,saturation=30,hue=45,analog_audio_gain_db=-3
prompt "Unplug the Elgato and plug it in again; wait 5 s."
expect "kept: 20 -10 30 45 -3" "brightness: 20 contrast: -10 saturation: 30 hue: 45 analog_audio_gain_db: -3" pic
run v4l2-ctl -d $D -c brightness=0,contrast=0,saturation=0,hue=0,analog_audio_gain_db=0

step "HDMI (laptop, RGB): picture settings with the RGB conversion"
viewer_start 0 "Elgato HDMI"
prompt "A window 'Elgato HDMI' shows the laptop."
run v4l2-ctl -d $D -c saturation=-100; sleep 3
confirm "HDMI: black and white?"
run v4l2-ctl -d $D -c saturation=0,contrast=50; sleep 3
confirm "HDMI: harder contrast, colours right (no tint)?"
run v4l2-ctl -d $D -c hdmi_color_range=1; sleep 3
confirm "Expanded with contrast +50: still a picture with the colours right (even harder)?"
run v4l2-ctl -d $D -c hdmi_color_range=0,contrast=0
viewer_stop

step "settings window"
prompt "Open 'Elgato Game Capture HD Settings' (Input Composite)."
confirm "Rows Brightness, Contrast, Saturation, Hue (degrees), Audio gain (dB) with sliders at 0, and a 'Reset picture' button?"
prompt "Move Brightness to about 40; wait 2 s."
expect "window: brightness about 40" "brightness: (3[5-9]|4[0-5]) " pic
prompt "Click 'Reset picture'; wait 2 s. Choose Input HDMI; wait 3 s."
expect "window: reset" "brightness: 0 contrast: 0 saturation: 0 hue: 0" pic
confirm "HDMI: the Audio gain row is hidden, the picture rows are shown?"
prompt "Close the window."

step "OBS plugin"
prompt "Start OBS (source 'Elgato Game Capture HD', input Composite); open the source's properties."
confirm "OBS: sliders Brightness, Contrast, Saturation, Hue (degrees), Audio gain (dB)?"
prompt "Set Hue to -60 -> OK; wait 3 s."
expect "OBS: hue -60" "hue: -60 " pic
run v4l2-ctl -d $D -c hue=0
prompt "Close OBS."

step "text menu"
prompt "Run gchd-settings in another terminal: choose Picture, Contrast, enter 25; then Quit."
expect "text menu: contrast 25" "contrast: 25 " pic
run v4l2-ctl -d $D -c contrast=0

step "v4l2-compliance"
CO=$HOME/gchd-test-logs/T-080-compliance.txt
v4l2-compliance -d $D > $CO 2>&1
run sh -c "grep -E 'fail|FAIL' $CO | head -10; grep -E '^Total' $CO"
expect "compliance: 0 failures" "Failed: 0" grep -E '^Total' $CO
run v4l2-ctl -d $D -c brightness=0,contrast=0,saturation=0,hue=0,analog_audio_gain_db=0
set_input 2
