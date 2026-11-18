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
confirm "the window shows Device /dev/elgato-gchd, Input Composite, Output size Source size, Bitrate 11250, Constant (CBR), High, Stretch off?"
prompt "In the window: Output size 640x480, Bitrate slider to about 8000, Bitrate mode Variable (VBR), H.264 profile Main, Stretch on."
expect "output size 640x480" "640/480" v4l2-ctl -d /dev/elgato-gchd -V
expect "bitrate about 8000 kbit/s" "video_bitrate: [78][0-9]{6}" v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
expect "bitrate mode VBR" "video_bitrate_mode: 0" v4l2-ctl -d /dev/elgato-gchd -C video_bitrate_mode
expect "profile Main" "h264_profile: 2" v4l2-ctl -d /dev/elgato-gchd -C h264_profile
expect "stretch on" "stretch_sd_input: 1" v4l2-ctl -d /dev/elgato-gchd -C stretch_sd_input
sleep 2
expect "settings remembered (modprobe.d)" "output=1.*bitrate=[78][0-9]{3}.*bitrate_mode=0.*stretch=1.*h264_profile=2" cat /etc/modprobe.d/elgato-gchd-settings.conf
prompt "In the window set them back: Source size, Bitrate 11250, Constant (CBR), High, Stretch off. Leave the window open."

step "window while OBS captures"
prompt "Start OBS with the source 'Elgato Game Capture HD' showing the picture. In the settings window choose Input 'S-Video'."
confirm "the window refused it (Input back on Composite) and explained that a program is capturing?"
prompt "In the settings window move the Bitrate slider to about 6000; wait 5 s."
expect "bitrate changed while capturing" "video_bitrate: [56][0-9]{6}" v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
confirm "OBS kept showing the picture (a short pause is normal)?"
prompt "Set the Bitrate back to 11250 in the window; close the window and OBS."
