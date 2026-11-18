# T-060 (OBS plugin): obs-plugin/ "Elgato Game Capture HD" source: built
# and installed for the user, device found automatically, PS1 picture and
# sound, output size property (encoder), PAL -> NTSC swap followed without
# restart, unplug/replug, capture device off when the source is hidden.
# Needs: OBS Studio, libobs-dev libavformat-dev libavcodec-dev
# libavutil-dev cmake pkg-config. PS1 (PAL) and N64 (NTSC) on composite.

OBSB="$REPO/build/obs-plugin"

step "build and install the plugin"
run sh -c "pkill -f 'gchd-bridge run'; true"
run sh -c "dpkg -s libobs-dev libavformat-dev libavcodec-dev cmake pkg-config 2>/dev/null | grep -c '^Status: install ok'" ||
	true
prompt "If packages are missing: sudo apt install libobs-dev libavformat-dev libavcodec-dev libavutil-dev cmake pkg-config (then Enter)"
run cmake -S "$REPO/obs-plugin" -B "$OBSB" -DCMAKE_BUILD_TYPE=Release
run cmake --build "$OBSB"
expect "plugin built" "obs-elgato-gchd.so" ls "$OBSB"
run cmake --install "$OBSB"
expect "plugin installed for the user" "obs-elgato-gchd.so" ls "$HOME/.config/obs-studio/plugins/obs-elgato-gchd/bin/64bit"

step "driver loaded, OBS with the source"
load_module replug
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
prompt "Close OBS."
run sh -c "grep -h '\\[gchd\\]' $HOME/.config/obs-studio/logs/\$(ls -t $HOME/.config/obs-studio/logs | head -1) | tail -20"
