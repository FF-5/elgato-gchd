# T-091, second run: every setting restored by a load (menu controls are
# saved as numbers now), a load while capturing, and the simpler profile
# controls of the settings window, text menu and OBS plugin (Load, Save,
# Delete; a save asks for a new name or a profile to replace). Passed in
# the first run: gchd-profile installed, menu location, list, refusals.
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
prompt "PS3 on composite. Unplug the Elgato and plug it in again; wait 5 s."
expect "menu entry named 'Elgato Game Capture HD'" "^Name=Elgato Game Capture HD$" cat /usr/local/share/applications/elgato-gchd-settings.desktop
mkdir -p "$PDIR"; rm -f "$PDIR"/T-091*.conf

step "gchd-profile: every setting back after a load"
set_input 2
run v4l2-ctl -d $D -v width=640,height=480
run v4l2-ctl -d $D -c video_bitrate_mode=0,video_bitrate=14000000,stretch_sd_input=1,brightness=30,contrast=-20,saturation=-40,hue=15,analog_audio_gain_db=-4
expect "composite settings set" "$COMP_A" cur
expect "save composite profile" "rc=0" P save "T-091 composite"
expect "saved as numbers" "^video_bitrate_mode=0$" grep "^video_bitrate_mode=" "$PDIR/T-091 composite.conf"
set_input 1
run v4l2-ctl -d $D -c allow_50_60_fps=0,brightness=-10,contrast=10,saturation=20,hue=-30,analog_audio_gain_db=3
expect "save component profile" "rc=0" P save "T-091 component"
run v4l2-ctl -d $D -c brightness=0,contrast=0,saturation=0,hue=0,analog_audio_gain_db=0,allow_50_60_fps=1
expect "load composite profile" "rc=0" P load "T-091 composite"
expect "composite profile: every setting back" "$COMP_A" cur
expect "load component profile" "rc=0" P load "T-091 component"
expect "component profile: every setting back" "$COMPONENT_B" cur

step "load while capturing"
run gchd-profile -d "$D" load "T-091 composite"
run v4l2-ctl -d $D -v width=720,height=480
run v4l2-ctl -d $D -c brightness=0,contrast=0,saturation=0,hue=0,stretch_sd_input=0
viewer_start 2 "Elgato composite"
prompt "A window 'Elgato composite' shows the PS3 in normal colours."
mark
expect "load while capturing" "rc=0" P load "T-091 composite"
sleep 6
expect "settings of the profile while capturing" "$COMP_A" cur
expect "at most one encoder restart" "^[01]$" count_new "encoder restart"
confirm "The window (maybe reopened by itself) shows the PS3 paler and brighter (saturation -40, brightness +30)?"
mark
expect "load the component profile while capturing" "rc=0" P load "T-091 component"
sleep 4
expect "input changed while capturing" "^input=1 " cur
viewer_stop
expect "no driver error" "^0$" count_new "failed|error"
run gchd-profile -d "$D" load "T-091 composite"

step "settings window"
prompt "Open 'Elgato Game Capture HD' (system settings in the application menu)."
confirm "Top of the window: a 'Profile' list with only the names (no input in brackets), and the buttons Load, Save, Delete (no Rename, no '...')?"
prompt "Choose 'T-091 component' and click Load; wait 3 s."
expect "window: component profile loaded" "$COMPONENT_B" cur
confirm "Window: Input shows Component and the sliders the profile's values (brightness -10, hue -30)?"
prompt "Click Save: a dialog asks for a new name or a profile to replace. Type T-091 window, click Save."
expect "window: new profile saved" "^input=1$" grep "^input=" "$PDIR/T-091 window.conf"
prompt "Choose Input Composite in the window; wait 3 s. Click Save, open the dialog's list, choose 'T-091 window', click Save and confirm Replace with Yes."
expect "window: profile replaced" "^input=2$" grep "^input=" "$PDIR/T-091 window.conf"
prompt "Choose 'T-091 window', click Delete and confirm with Yes. Close the window."
expect "window: profile deleted" "^gone$" sh -c "[ -e '$PDIR/T-091 window.conf' ] && echo there || echo gone"

step "text menu"
prompt "Run gchd-settings in another terminal: Profiles, Load, choose 'T-091 composite'; OK. Then Profiles, Save, '(new profile)', type T-091 menu; OK. Then Quit."
expect "text menu: composite profile loaded" "$COMP_A" cur
expect "text menu: new profile saved" "^input=2$" grep "^input=" "$PDIR/T-091 menu.conf"
confirm "Text menu: Profiles offered only Load, Save, Delete, and the lists showed only names?"
run gchd-profile delete "T-091 menu"

step "OBS plugin"
prompt "Start OBS (source 'Elgato Game Capture HD'); open the source's properties."
confirm "OBS: a 'Profiles' group with one Profile field (a list you can also type in) and the buttons Load, Save, Delete?"
prompt "In Profile choose 'T-091 component', click Load; wait 3 s."
expect "OBS: component profile loaded" "$COMPONENT_B" cur
confirm "OBS: the message \"Profile 'T-091 component' loaded.\" and the properties show Component with brightness -10?"
prompt "In Profile type T-091 obs, click Save."
expect "OBS: new profile saved" "^input=1$" grep "^input=" "$PDIR/T-091 obs.conf"
prompt "Change Input to Composite in the properties; wait 3 s. Profile 'T-091 obs': click Save (a question appears below), then Save again."
expect "OBS: profile replaced" "^input=2$" grep "^input=" "$PDIR/T-091 obs.conf"
prompt "Profile 'T-091 obs': click Delete (a question appears), then Delete again."
expect "OBS: profile deleted" "^gone$" sh -c "[ -e '$PDIR/T-091 obs.conf' ] && echo there || echo gone"
prompt "Close the properties (OK) and OBS."

step "clean up"
run gchd-profile delete "T-091 composite"
run gchd-profile delete "T-091 component"
set_input 2
run v4l2-ctl -d $D -v width=720,height=480
run v4l2-ctl -d $D -c brightness=0,contrast=0,saturation=0,hue=0,analog_audio_gain_db=0,stretch_sd_input=0,allow_50_60_fps=1
