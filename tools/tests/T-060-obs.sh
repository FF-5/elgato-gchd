# T-060 (OBS): src/obs-plugin "Elgato Game Capture HD" source (built and
# installed for the user, device found automatically, PS1 picture and
# sound, output size property, PAL -> NTSC swap followed without restart,
# unplug/replug, capture device off when hidden); /dev/elgato-gchd with
# OBS's Media Source; src/settings/gchd-settings.
# Needs: OBS Studio, whiptail, libobs-dev libavformat-dev libavcodec-dev
# libavutil-dev cmake pkg-config. PS1 (PAL) and N64 (NTSC) on composite.

OBSB="$REPO/build/obs-plugin"

step "build and install the plugin"
run sh -c "pkill -f 'gchd-bridge run'; true"
run sh -c "dpkg -s libobs-dev libavformat-dev libavcodec-dev cmake pkg-config 2>/dev/null | grep -c '^Status: install ok'" ||
	true
prompt "If packages are missing: sudo apt install libobs-dev libavformat-dev libavcodec-dev libavutil-dev cmake pkg-config (then Enter)"
run cmake -S "$REPO/src/obs-plugin" -B "$OBSB" -DCMAKE_BUILD_TYPE=Release
run cmake --build "$OBSB"
expect "plugin built" "obs-elgato-gchd.so" ls "$OBSB"
run cmake --install "$OBSB"
expect "plugin installed for the user" "obs-elgato-gchd.so" ls "$HOME/.config/obs-studio/plugins/obs-elgato-gchd/bin/64bit"

step "driver loaded, OBS with the source"
run sudo install -m 0644 "$REPO/udev-rules/55-elgato-gchd-device.rules" "$REPO/udev-rules/56-elgato-gchd-settings.rules" /etc/udev/rules.d/
run sudo udevadm control --reload
load_module replug
expect "/dev/elgato-gchd present" "elgato-gchd" ls -l /dev/elgato-gchd
set_input 2
prompt "PS1 on composite, game running. Start OBS (restart it if it was open). Sources: + -> 'Elgato Game Capture HD' -> OK; in the properties keep Input 'Composite', Output size 'Source size' -> OK."
confirm "the source 'Elgato Game Capture HD' was in the list and its properties showed Device 'Automatic (connected device)'?"
prompt "Wait about 5 s."
expect "capture device started" "starting hardware boot" sudo dmesg
confirm "OBS shows the PS1 picture, smooth (50 frames/s), without interlacing lines?"
confirm "the audio mixer shows the source 'Elgato Game Capture HD' with sound (the meter moves while the game makes sound)?"
run sh -c "grep -h '\\[gchd\\]' $HOME/.config/obs-studio/logs/\$(ls -t $HOME/.config/obs-studio/logs | head -1) | tail -5"

step "output size 640x480 (encoder) in the properties"
prompt "Source properties: Output size '640x480' -> OK; wait about 8 s."
confirm "OBS shows the picture again, now 640x480 (Edit -> Transform -> Edit Transform shows size base 640x480)?"
prompt "Set Output size back to 'Source size' -> OK; wait about 8 s."

step "PAL -> NTSC swap while OBS captures"
prompt "Swap the PS1 for the N64 (NTSC) while OBS stays open; wait about 15 s."
confirm "OBS shows the N64 picture by itself (720x480), without reopening anything?"

step "unplug and replug"
prompt "Unplug the Elgato; wait 5 s; plug it in again; wait about 15 s."
confirm "OBS shows the N64 picture again by itself?"

step "source hidden: capture device off"
prompt "Hide the source (eye icon in Sources); wait 5 s."
expect "hardware shut down while the source is hidden" "hardware shutdown done" sudo dmesg
prompt "Show the source again; wait about 8 s."
confirm "picture back after showing the source?"
prompt "Remove the source 'Elgato Game Capture HD' from the scene (or hide it)."

step "OBS Media Source on /dev/elgato-gchd"
prompt "Sources: + -> Media Source -> untick 'Local File'; Input: /dev/elgato-gchd; Input Format: mpegts; tick 'Restart playback when source becomes active' -> OK. Right-click the source -> Deinterlacing -> Yadif 2x. Wait about 10 s."
confirm "OBS shows the N64 picture with sound (audio mixer) from the Media Source?"
prompt "Remove the Media Source."

step "settings tool"
prompt "In a second terminal: $REPO/src/settings/gchd-settings  -> set Output size 640x480, Bitrate 8000, then Quit."
expect "output size 640x480 set" "640/480" v4l2-ctl -d /dev/elgato-gchd -V
expect "bitrate 8000 kbit/s set" "video_bitrate: 8000000" v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
confirm "the menu showed the current settings and the changes without errors?"
run v4l2-ctl -d /dev/elgato-gchd -v width=720,height=480 -c video_bitrate=11250000
prompt "Close OBS."
run sh -c "grep -h '\\[gchd\\]' $HOME/.config/obs-studio/logs/\$(ls -t $HOME/.config/obs-studio/logs | head -1) | tail -20"
