# T-060 (settings): settings window gchd-settings-gui and OBS plugin.
# One global setting: the window, the terminal and the OBS plugin show and
# change the same driver settings, at any time (also while OBS captures,
# applied live); the window shows changes made elsewhere (refresh 1.5 s).
# Two groups: composite/S-video and HDMI/component keep their own bitrate
# mode, bitrates, H.264 profile and level; the driver keeps a bitrate per
# mode (CBR and VBR). Output size, stretch and bitrate are shown for
# composite/S-video only. The window applies a slider when it is
# released; OBS's properties apply 0.5 s after the last change (live
# preview), Cancel sets the earlier values again. Slider in Mbit/s (VBR: average 3.00-7.23,
# 480x360 2.00-7.50; CBR 7.00-11.25).
# Needs: OBS with the plugin, a console on composite.

INST="$REPO/install-driver.sh"
LOGS="$HOME/.config/obs-studio/logs"
D=/dev/elgato-gchd
# obs_log N: the plugin's last N lines from the newest OBS log (names have spaces).
obs_log() {
	local f
	f=$(ls -t "$LOGS" | head -1)
	grep -h '\[gchd\]' "$LOGS/$f" | tail -"$1"
}
# rate_near KBIT: "near" when the bitrate is within 60 kbit/s (slider positions).
rate_near() {
	local v
	v=$(v4l2-ctl -d $D -C video_bitrate | sed -n 's/.*: //p')
	if [ -n "$v" ] && [ $(( v / 1000 - $1 )) -le 60 ] && [ $(( $1 - v / 1000 )) -le 60 ]; then
		echo "near ($v)"
	else
		echo "video_bitrate $v"
	fi
}
# stretch_shot ON: 6 s with stretch_sd_input=ON; aspect ratio of the stream.
stretch_shot() {
	local f=/tmp/gchd-stretch-$1.ts
	v4l2-ctl -d $D -v pixelformat=MPEG
	v4l2-ctl -d $D -c stretch_sd_input="$1"
	timeout 6 cat $D > "$f"
	ffprobe -v error -select_streams v:0 -show_entries \
		stream=width,height,sample_aspect_ratio,display_aspect_ratio -of default=nw=1 "$f" | head -4 | tr '\n' ' '
	rm -f "$f"
}
irq_clean() {
	if sudo dmesg | grep -q "Disabling IRQ"; then
		sudo dmesg | grep -B2 -A2 "Disabling IRQ" | tail -8
	else
		echo clean
	fi
}

step "install the driver part and the plugin"
expect "no USB interrupt disabled by the kernel (else: reboot first)" "^clean" irq_clean
run sudo rmmod elgato_gchd
run bash "$INST" --all --yes --in-terminal < /dev/null
prompt "Unplug the Elgato and plug it in again; wait 5 s."
expect "v4l2-compliance: no failures" "Failed: 0" v4l2-compliance -d $D
# Mode and bitrate in separate calls: a mode change loads that mode's bitrate.
run v4l2-ctl -d $D -i 2
run v4l2-ctl -d $D -v width=720,height=480,pixelformat=MPEG
run v4l2-ctl -d $D -c h264_profile=4,stretch_sd_input=0
run v4l2-ctl -d $D -c video_bitrate_mode=0
run v4l2-ctl -d $D -c video_bitrate=10000000
run v4l2-ctl -d $D -c video_bitrate_mode=1
run v4l2-ctl -d $D -c video_bitrate=11250000
# 480x360 keeps its own bitrate: 7.50 (15000 kbit/s). One option per call:
# v4l2-ctl sets the controls (-c) before the input (-i) and format (-v).
run v4l2-ctl -d $D -v width=480,height=360
run v4l2-ctl -d $D -c video_bitrate=15000000
run v4l2-ctl -d $D -v width=720,height=480
expect "720x480: CBR 11250 again" "video_bitrate: 11250000" v4l2-ctl -d $D -C video_bitrate

step "window: values, labels, slider, status line"
prompt "Open 'Elgato Game Capture HD Settings' from the application menu."
confirm "Input Composite, Output size '720x480 NTSC / 720x576 PAL', Constant (CBR), slider at '11.25 Mbit/s', High, Stretch off; no text next to the Close button?"
prompt "Open the Output size list (then close it with Escape)."
confirm "the list is '720x480 NTSC / 720x576 PAL', '640x480', '480x360' (no 'Source size')?"
prompt "Hold the slider and move it slowly back and forth for about 10 s, ending at about 9.00; watch the status text; then release it."
confirm "no 'Changing'/'changed' message while you held the slider; 'Bitrate changed.' once after the release, on the line of the Close button, gone after about 3 s?"
prompt "Wait 3 s."
expect "CBR about 9000 kbit/s" "^near" rate_near 9000
prompt "Bitrate mode Variable (VBR); wait 3 s."
confirm "the slider shows the last VBR bitrate, 5.00 Mbit/s (range 3.00 to 7.23)?"
expect "VBR 10000 kbit/s remembered" "^near" rate_near 10000
prompt "Move the slider to about 4.00; then Bitrate mode Constant (CBR); wait 3 s."
confirm "the slider is back at about 9.00 (the last CBR bitrate), not at 11.25?"
expect "CBR 9000 kbit/s remembered" "^near" rate_near 9000
prompt "Move the slider fully to the right and release it; wait 3 s."
confirm "the slider stays at 11.25 (not 11.24)?"
expect "CBR 11250 kbit/s exactly" "video_bitrate: 11250000" v4l2-ctl -d $D -C video_bitrate
prompt "Output size 480x360; wait 3 s."
confirm "only 'Output size changed.' (no 'Bitrate mode changed.'); mode Variable (VBR) and locked, slider range 2.00 to 7.50 at 7.50 (480x360 keeps its own bitrate)?"
prompt "Move the slider to about 2.00 and release it; then Output size 640x480; wait 3 s."
confirm "'Output size changed.'; Variable (VBR) at about 4.00 again (the VBR bitrate of the other sizes)?"
expect "VBR 8000 kbit/s for 640x480" "^near" rate_near 8000
prompt "Output size 480x360; wait 3 s."
confirm "back at about 2.00 (480x360's own bitrate)?"
expect "480x360 kept 4000 kbit/s" "^near" rate_near 4000
prompt "Output size '720x480 NTSC / 720x576 PAL'; Bitrate mode Constant (CBR); wait 3 s."
confirm "slider at 11.25 again (CBR remembered)?"
expect "CBR 11250 remembered" "video_bitrate: 11250000" v4l2-ctl -d $D -C video_bitrate

step "groups: composite/S-video share, HDMI/component have their own"
prompt "Stretch on; Bitrate mode Variable (VBR). Then Input S-Video; wait 5 s."
confirm "S-Video: Output size, VBR about 4.00 and Stretch on unchanged?"
expect "S-video: VBR kept" "video_bitrate_mode: 0" v4l2-ctl -d $D -C video_bitrate_mode
prompt "Input HDMI; wait 5 s."
confirm "only Input and H.264 profile are shown (no Output size, Bitrate mode, Bitrate, Stretch); status text on the Close button line?"
run v4l2-ctl -d $D -C video_bitrate_mode,video_bitrate,h264_profile
prompt "H.264 profile Main (HDMI). Then Input Component; wait 5 s."
confirm "Component also shows H.264 profile Main?"
prompt "Input Composite; wait 5 s."
confirm "Composite: profile High, Variable (VBR) about 4.00, Stretch on, all rows shown again?"
expect "composite group kept its VBR bitrate" "^near" rate_near 8000
expect "composite group kept High" "h264_profile: 4" v4l2-ctl -d $D -C h264_profile
run v4l2-ctl -d $D -i 0
expect "HDMI group kept Main" "h264_profile: 2" v4l2-ctl -d $D -C h264_profile
run v4l2-ctl -d $D -c h264_profile=4
run v4l2-ctl -d $D -i 3
run v4l2-ctl -d $D -c video_bitrate_mode=1
run v4l2-ctl -d $D -c video_bitrate=10000000
prompt "Leave the window open; wait 3 s."
confirm "the window now shows Input S-Video and CBR 10.00 by itself (changed from the terminal)?"
run v4l2-ctl -d $D -i 2
run v4l2-ctl -d $D -c stretch_sd_input=0

step "OBS: the same settings, changes in both directions"
prompt "In the settings window: Bitrate mode Variable (VBR), slider 5.00."
expect "window: VBR about 10000 kbit/s (5.00 shown)" "^near" rate_near 10000
prompt "Close OBS if open. Start OBS (the scene with the source 'Elgato Game Capture HD'); open the source's properties."
confirm "OBS: Composite, '720x480 NTSC / 720x576 PAL', Variable (VBR), about 5.00, Stretch off; mode list says just 'Constant (CBR)' / 'Variable (VBR)'?"
expect "plugin read the device's settings (VBR)" "properties: device settings read: input 2, output 0, mode 0" obs_log 8
prompt "In OBS's properties choose Bitrate mode Constant (CBR)."
confirm "OBS's slider jumps to 10.00 (the last CBR bitrate), not 11.25?"
prompt "Back to Variable (VBR); hold the slider and move it back and forth for about 10 s, ending at about 6.00; release; wait 3 s (properties still open)."
expect "applied while the properties are open (VBR about 12000)" "^near" rate_near 12000
run obs_log 6
confirm "the preview in the properties kept running while you moved the slider (short pause after each stop is fine)?"
prompt "Press OK. Wait 3 s."
expect "OBS: VBR about 12000 kbit/s (6.00 shown)" "^near" rate_near 12000
confirm "the settings window shows about 6.00 by itself?"
prompt "In OBS's properties choose Output size 640x480 -> OK. Wait about 8 s."
expect "OBS's output size reached the device" "640/480" v4l2-ctl -d $D -V
expect "bitrate not overwritten by the size change" "^near" rate_near 12000
confirm "OBS shows the picture at 640x480, and the settings window shows 640x480 by itself?"
prompt "In OBS's properties choose Input HDMI (do not press OK); wait 3 s."
confirm "OBS hides Output size, Stretch, Bitrate mode and Bitrate; the preview turned black (HDMI, no signal)?"
prompt "Press Cancel. Wait 5 s."
expect "Cancel set Composite again" "Video input : 2" v4l2-ctl -d $D -I
expect "Cancel kept 640x480" "640/480" v4l2-ctl -d $D -V
expect "Cancel kept the bitrate" "^near" rate_near 12000
prompt "In the settings window: Stretch on, Output size '720x480 NTSC / 720x576 PAL', slider 4.00; wait 8 s."
expect "window: stretch on" "stretch_sd_input: 1" v4l2-ctl -d $D -C stretch_sd_input
prompt "In OBS open the source's properties again."
confirm "OBS's properties show '720x480 NTSC / 720x576 PAL', about 4.00, Stretch on?"
prompt "Close the properties with Cancel. In the settings window choose Input 'S-Video'; wait 5 s."
confirm "no message box; OBS's picture turned black (same size) within about 1 s?"
prompt "In the settings window choose Input 'Composite' and Stretch off; wait 8 s."
confirm "OBS shows the console again?"
prompt "Close OBS completely; wait 5 s."
expect "settings stay after OBS closed (VBR about 8000 kbit/s)" "^near" rate_near 8000
expect "VBR stays" "video_bitrate_mode: 0" v4l2-ctl -d $D -C video_bitrate_mode
run obs_log 20

step "stretch SD input"
prompt "The console on composite with a picture with a clear shape (a menu, a circle). OBS closed; settings window: Composite, '720x480 NTSC / 720x576 PAL', Stretch off."
expect "stretch off: 4:3 aspect ratio in the stream" "sample_aspect_ratio=(10:11|12:11|8:9|16:15)" stretch_shot 0
expect "stretch on: 16:9 aspect ratio in the stream" "sample_aspect_ratio=(40:33|16:11|32:27|64:45)" stretch_shot 1
run v4l2-ctl -d $D -c stretch_sd_input=0
prompt "Start OBS (source 'Elgato Game Capture HD' shown). In the settings window: Stretch on; wait 5 s."
expect "OBS widens the stretched picture" "shown (8[0-9][0-9]x480|10[0-9][0-9]x576)" obs_log 3
confirm "the picture in OBS is wider (16:9 shape; the source's red frame wider)?"
prompt "Stretch off; wait 5 s."
expect "OBS shows the normal picture" "shown 720x(480|576)" obs_log 3
confirm "the picture is back to its normal shape and size?"
prompt "Close OBS."
expect "no USB interrupt disabled by the kernel" "^clean" irq_clean
prompt "In the settings window choose Bitrate mode Constant (CBR), slider 11.25; close the window."
