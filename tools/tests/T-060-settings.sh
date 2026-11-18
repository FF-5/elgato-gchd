# T-060 (settings window): gchd-settings-gui ("Elgato Game Capture HD
# Settings" in the application menu): installed with the driver part,
# shows the device's settings, changes are applied and remembered, input
# change refused with an explanation while OBS captures.
# Needs: OBS with the plugin, a console on composite.

INST="$REPO/install-driver.sh"

step "install the driver part (window, menu entry)"
run sudo rmmod elgato_gchd
run bash "$INST" --driver --yes --in-terminal < /dev/null
expect "settings window installed" "gchd-settings-gui" ls /usr/local/bin/gchd-settings-gui
expect "menu entry installed" "elgato-gchd-settings.desktop" ls /usr/local/share/applications/elgato-gchd-settings.desktop
prompt "Unplug the Elgato and plug it in again; wait 5 s."
run v4l2-ctl -d /dev/elgato-gchd -i 2 -v width=720,height=480 -c video_bitrate=11250000,video_bitrate_mode=1,h264_profile=4,stretch_sd_input=0

step "window: show and change the settings"
prompt "Open 'Elgato Game Capture HD Settings' from the application menu (or run gchd-settings-gui)."
confirm "the window shows Device /dev/elgato-gchd, Input Composite, Output size Source size, Bitrate mode Constant (CBR) 11250, the slider greyed out ('11250 kbit/s constant'), High, Stretch off?"
prompt "In the window: Bitrate mode Variable (VBR); try to move the slider to both ends."
confirm "the slider goes from 6000 to 14460 kbit/s and shows the average (half)?"
prompt "Output size 480x360."
confirm "Bitrate mode is now Variable (VBR) and cannot be changed, and the slider goes from 4000 to 15000?"
prompt "In the window: Output size 640x480, Bitrate mode Variable (VBR), Bitrate slider to about 8000, H.264 profile Main, Stretch on."
expect "output size 640x480" "640/480" v4l2-ctl -d /dev/elgato-gchd -V
expect "bitrate about 8000 kbit/s" "video_bitrate: [78][0-9]{6}" v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
expect "bitrate mode VBR" "video_bitrate_mode: 0" v4l2-ctl -d /dev/elgato-gchd -C video_bitrate_mode
expect "profile Main" "h264_profile: 2" v4l2-ctl -d /dev/elgato-gchd -C h264_profile
expect "stretch on" "stretch_sd_input: 1" v4l2-ctl -d /dev/elgato-gchd -C stretch_sd_input
sleep 2
expect "settings remembered (modprobe.d)" "output=1.*bitrate=[78][0-9]{3}.*bitrate_mode=0.*stretch=1.*h264_profile=2" cat /etc/modprobe.d/elgato-gchd-settings.conf
prompt "In the window set them back: Source size, Bitrate mode Constant (CBR), High, Stretch off. Leave the window open."
expect "CBR 11250 again" "video_bitrate: 11250000" v4l2-ctl -d /dev/elgato-gchd -C video_bitrate

step "window while OBS captures"
prompt "Close OBS completely if it is open. Then start OBS; the scene with the source 'Elgato Game Capture HD' must show the PS1/N64 picture. Wait until the picture moves."
expect "OBS captures (capture device started)" "starting hardware boot" sudo dmesg
run sh -c "grep -h '\\[gchd\\]' $HOME/.config/obs-studio/logs/\$(ls -t $HOME/.config/obs-studio/logs | head -1) | tail -5"
prompt "In the settings window choose Input 'S-Video'."
confirm "a message box said 'Input not changed' (a program is capturing), and after OK the Input shows Composite again?"
expect "input still composite" "Video input : 2" v4l2-ctl -d /dev/elgato-gchd -I
prompt "In the settings window choose Bitrate mode Variable (VBR) and move the slider to about 6000; wait 5 s."
expect "bitrate changed while capturing" "video_bitrate: 6[0-9]{6}" v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
confirm "OBS kept showing the moving picture (a short pause is normal)?"
prompt "In the settings window choose Bitrate mode Constant (CBR); close the settings window."
prompt "In OBS open the properties of the source 'Elgato Game Capture HD': choose Bitrate mode Variable (VBR), then Output size 480x360 and back to the source size."
confirm "OBS: the bitrate slider appears only for VBR (6000-14460), the mode is hidden for 480x360 (slider 4000-15000)?"
prompt "Set Bitrate mode Constant (CBR), Output size Source size -> OK; close OBS."
run sh -c "grep -h '\\[gchd\\]' $HOME/.config/obs-studio/logs/\$(ls -t $HOME/.config/obs-studio/logs | head -1) | tail -20"
