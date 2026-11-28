# T-091, third run: the OBS plugin's profile controls (question boxes for
# replace and delete, the list updated, no messages). Everything else
# passed in the second run.
# Needs: the PS3 on composite, OBS with the plugin.
INST="$REPO/install-driver.sh"
D=$DEV
PDIR=${XDG_CONFIG_HOME:-$HOME/.config}/elgato-gchd/profiles
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
count_new() { sudo dmesg | tail -n +$((N0 + 1)) | grep -v "bulk-IN stats" | grep -cE "$1"; }
# cur: input, size and every profile setting in one line.
cur() {
	printf 'input=%s size=%s ' "$(v4l2-ctl -d $D -I | sed -n 's/^Video input *: *\([0-9]\).*/\1/p')" \
		"$(v4l2-ctl -d $D -V | sed -n 's#.*Width/Height *: *\([0-9]*\)/\([0-9]*\).*#\1x\2#p')"
	v4l2-ctl -d $D -C video_bitrate_mode,video_bitrate,stretch_sd_input,allow_50_60_fps,hdmi_color_range,brightness,contrast,saturation,hue,analog_audio_gain_db |
		sed 's/ (.*//' | LC_ALL=C sort | tr '\n' ' '
	echo
}
P() { gchd-profile -d "$D" "$@"; echo "rc=$?"; }
# Controls sorted by name: allow_50_60_fps analog_audio_gain_db brightness
# contrast hdmi_color_range hue saturation stretch_sd_input video_bitrate
# video_bitrate_mode.
COMP_A="input=2 size=640x480 .*analog_audio_gain_db: -4 brightness: 30 contrast: -20 .*hue: 15 saturation: -40 stretch_sd_input: 1 video_bitrate: 14000000 video_bitrate_mode: 0"
COMPONENT_B="input=1 .*allow_50_60_fps: 0 analog_audio_gain_db: 3 brightness: -10 contrast: 10 .*hue: -30 saturation: 20 "

step "install"
run sudo rmmod elgato_gchd
run bash "$INST" --all --yes --in-terminal < /dev/null
expect "OBS plugin built with Qt" "libQt6Widgets" sh -c "ldd $HOME/.config/obs-studio/plugins/obs-elgato-gchd/bin/64bit/obs-elgato-gchd.so"
prompt "PS3 on composite. Unplug the Elgato and plug it in again; wait 5 s."
mkdir -p "$PDIR"; rm -f "$PDIR"/T-091*.conf
set_input 2
run v4l2-ctl -d $D -v width=640,height=480
run v4l2-ctl -d $D -c video_bitrate_mode=0,video_bitrate=14000000,stretch_sd_input=1,brightness=30,contrast=-20,saturation=-40,hue=15,analog_audio_gain_db=-4
expect "save composite profile" "rc=0" P save "T-091 composite"
set_input 1
run v4l2-ctl -d $D -c allow_50_60_fps=0,brightness=-10,contrast=10,saturation=20,hue=-30,analog_audio_gain_db=3
expect "save component profile" "rc=0" P save "T-091 component"
run gchd-profile -d "$D" load "T-091 composite"

step "OBS plugin"
prompt "Start OBS (source 'Elgato Game Capture HD'); open the source's properties."
prompt "In Profile choose 'T-091 component', click Load; wait 3 s."
expect "OBS: component profile loaded" "$COMPONENT_B" cur
confirm "OBS: Profile still shows 'T-091 component', the properties show Component with brightness -10, and no message below the buttons?"
prompt "In Profile type T-091 obs, click Save."
expect "OBS: new profile saved" "^input=1$" grep "^input=" "$PDIR/T-091 obs.conf"
confirm "OBS: no question for the new name, no message, and 'T-091 obs' is in the Profile list now?"
prompt "Change Input to Composite in the properties; wait 3 s. Profile 'T-091 obs': click Save."
confirm "OBS: a box asks whether to replace profile 'T-091 obs'? Click No."
expect "OBS: No keeps the profile" "^input=1$" grep "^input=" "$PDIR/T-091 obs.conf"
prompt "Click Save again; in the box click Yes."
expect "OBS: Yes replaces it" "^input=2$" grep "^input=" "$PDIR/T-091 obs.conf"
prompt "Profile 'T-091 obs': click Delete."
confirm "OBS: a box asks whether to delete profile 'T-091 obs'? Click Yes."
expect "OBS: profile deleted" "^gone$" sh -c "[ -e '$PDIR/T-091 obs.conf' ] && echo there || echo gone"
confirm "OBS: the Profile field is empty and 'T-091 obs' is no longer in the list?"
prompt "Clear the Profile field, click Load."
confirm "OBS: a box says to choose a profile or type a name?"
prompt "Close the properties (OK) and OBS."

step "clean up"
run gchd-profile delete "T-091 composite"
run gchd-profile delete "T-091 component"
set_input 2
run v4l2-ctl -d $D -v width=720,height=480
run v4l2-ctl -d $D -c brightness=0,contrast=0,saturation=0,hue=0,analog_audio_gain_db=0,stretch_sd_input=0,allow_50_60_fps=1
