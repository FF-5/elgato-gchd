# T-064: remembered settings (module parameters, uevent, udev rule).
# The saved options are passed to insmod by hand (modprobe would read them).

CONF=/etc/modprobe.d/elgato-gchd-settings.conf
saved_opts() { sed -n 's/^options elgato_gchd //p' "$CONF" 2>/dev/null; }

step "install the udev rule, forget saved settings (first use)"
run sudo cp "$REPO/udev-rules/56-elgato-gchd-settings.rules" /etc/udev/rules.d/
run sudo udevadm control --reload
run sudo rm -f "$CONF"

step "first use: HDMI and defaults"
load_module replug
expect "input HDMI on first use" "Video input : 0" v4l2-ctl -d "$DEV" --get-input
expect "format H.264 on first use" "H264" v4l2-ctl -d "$DEV" --get-fmt-video
expect "bitrate default 11.25 Mbit/s" "video_bitrate .*value=11250000" v4l2-ctl -d "$DEV" -l

step "change settings"
set_input 2
set_fmt 640 480 MPEG
set_ctrl video_bitrate_mode=0,video_bitrate=6000000,stretch_sd_input=1
sleep 1
expect "settings file written" "input=2 output=1 format=1 bitrate=6000 bitrate_mode=0 stretch=1" cat "$CONF"

step "module reload with the saved settings"
load_module $(saved_opts)
expect "input composite after reload" "Video input : 2" v4l2-ctl -d "$DEV" --get-input
expect "640x480 MPEG after reload" "640/480" v4l2-ctl -d "$DEV" --get-fmt-video
expect "MPEG after reload" "MPEG" v4l2-ctl -d "$DEV" --get-fmt-video
expect "bitrate 6000 VBR, stretch after reload" "video_bitrate .*value=6000000" v4l2-ctl -d "$DEV" -l
expect "VBR after reload" "video_bitrate_mode .*value=0" v4l2-ctl -d "$DEV" -l
expect "stretch after reload" "stretch_sd_input .*value=1" v4l2-ctl -d "$DEV" -l

step "replug with the saved settings, capture"
load_module replug $(saved_opts)
prompt "PS1 on composite, game running."
capture 10 640 480 25/1 progressive

step "back to defaults; forget the settings file"
set_fmt 720 576 H264
set_input 0
set_ctrl video_bitrate_mode=1,video_bitrate=11250000,stretch_sd_input=0
sleep 1
run cat "$CONF"
run sudo rm -f "$CONF"
