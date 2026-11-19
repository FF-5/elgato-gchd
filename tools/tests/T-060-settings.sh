# T-060 (settings): settings window gchd-settings-gui and OBS plugin.
# One global setting: the window, the terminal and the OBS plugin show and
# change the same driver settings, at any time (also while OBS captures,
# applied live); the window shows changes made elsewhere (refresh 1.5 s);
# bitrate slider in Mbit/s (VBR: average 3.00-7.23, 480x360 2.00-7.50;
# CBR 7.00-11.25).
# Needs: OBS with the plugin, a console on composite.

INST="$REPO/install-driver.sh"
LOGS="$HOME/.config/obs-studio/logs"
# obs_log N: the plugin's last N lines from the newest OBS log (names have spaces).
obs_log() {
	local f
	f=$(ls -t "$LOGS" | head -1)
	grep -h '\[gchd\]' "$LOGS/$f" | tail -"$1"
}

step "install the driver part and the plugin"
run sudo rmmod elgato_gchd
run bash "$INST" --all --yes --in-terminal < /dev/null
prompt "Unplug the Elgato and plug it in again; wait 5 s."
expect "v4l2-compliance: no failures" "Failed: 0" v4l2-compliance -d /dev/elgato-gchd
run v4l2-ctl -d /dev/elgato-gchd -i 2 -v width=720,height=480,pixelformat=MPEG -c video_bitrate=11250000,video_bitrate_mode=1,h264_profile=4,stretch_sd_input=0

step "window: values, slider, refresh"
prompt "Open 'Elgato Game Capture HD Settings' from the application menu."
confirm "Input Composite, Source size, Constant (CBR), slider at '11.25 Mbit/s' (no other words), High, Stretch off; no Refresh button?"
prompt "Bitrate mode Constant (CBR); move the slider fully to the right and release it; wait 3 s."
confirm "the slider stays at 11.25 (not 11.24)?"
expect "CBR 11250 kbit/s exactly" "video_bitrate: 11250000" v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
prompt "Bitrate mode Variable (VBR); move the slider to both ends, then to about 4.00."
confirm "VBR slider from 3.00 to 7.23 Mbit/s?"
expect "VBR about 8000 kbit/s (4.00 shown)" "video_bitrate: [78][0-9]{6}" v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
prompt "Output size 480x360."
confirm "mode Variable (VBR) and locked, slider 2.00 to 7.50, and the size reads just '480x360'?"
prompt "Output size Source size, Bitrate mode Constant (CBR), slider to the left end."
confirm "CBR slider from 7.00 to 11.25?"
expect "CBR 7000 kbit/s" "video_bitrate: 7000000" v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
run v4l2-ctl -d /dev/elgato-gchd -i 3 -c video_bitrate=11250000
prompt "Leave the window open; wait 3 s."
confirm "the window now shows Input S-Video and CBR 11.25 by itself (changed from the terminal)?"
run v4l2-ctl -d /dev/elgato-gchd -i 2

step "OBS: the same settings, changes in both directions"
prompt "In the settings window: Input Composite, Source size, Bitrate mode Variable (VBR), slider 5.00, Stretch off."
expect "window: VBR 10000 kbit/s (5.00 shown)" "video_bitrate: 10000000" v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
prompt "Close OBS if open. Start OBS (the scene with the source 'Elgato Game Capture HD'); open the source's properties."
run obs_log 5
expect "plugin read the device's settings (VBR, 5.00)" "properties: device settings read: input 2, output 0, mode 0, 5.00" obs_log 5
confirm "OBS's properties show Composite, Source size, Variable (VBR), Bitrate 5.00 Mbit/s, Stretch off?"
prompt "In OBS's properties move the Bitrate slider to 6.00 -> OK. Wait 3 s."
expect "OBS: VBR 12000 kbit/s (6.00 shown)" "video_bitrate: 12000000" v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
confirm "the settings window shows 6.00 by itself?"
prompt "In OBS's properties choose Output size 640x480 -> OK. Wait about 8 s."
expect "OBS's output size reached the device" "640/480" v4l2-ctl -d /dev/elgato-gchd -V
expect "bitrate not overwritten by the size change (still 12000)" "video_bitrate: 12000000" v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
confirm "OBS shows the picture at 640x480, and the settings window shows 640x480 by itself?"
prompt "In the settings window: Stretch on, Output size Source size, Bitrate slider 4.00; wait 8 s."
expect "window: stretch on" "stretch_sd_input: 1" v4l2-ctl -d /dev/elgato-gchd -C stretch_sd_input
prompt "In OBS open the source's properties again."
confirm "OBS's properties show Source size, Bitrate 4.00, Stretch on?"
prompt "Close the properties with Cancel. In the settings window choose Input 'S-Video'; wait 5 s."
confirm "no message box, OBS's picture went to S-Video (no signal) at once?"
prompt "In the settings window choose Input 'Composite' and Stretch off; wait 8 s."
confirm "OBS shows the console again?"
prompt "Close OBS completely; wait 5 s."
expect "settings stay after OBS closed (VBR 8000 kbit/s)" "video_bitrate: 8000000" v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
expect "VBR stays" "video_bitrate_mode: 0" v4l2-ctl -d /dev/elgato-gchd -C video_bitrate_mode
run obs_log 20
prompt "In the settings window choose Bitrate mode Constant (CBR), slider 11.25; close the window."
