# T-066: V4L2 compliance (DV timings, level menu, power-present control,
# input control while another process captures).

step "load module (fresh plug-in)"
load_module replug

step "v4l2-compliance without capture"
run sh -c "v4l2-compliance -d $DEV 2>&1 | grep -E 'fail:|warn:|Total for'"
expect "v4l2-compliance: no failures" "Failed: 0" v4l2-compliance -d "$DEV"

step "DV timings on HDMI (no signal needed)"
set_input 0
expect "G_DV_TIMINGS works without a signal" "1280x720p60|Active width: 1280" v4l2-ctl -d "$DEV" --get-dv-timings
expect "DV timings list" "1920x1080" v4l2-ctl -d "$DEV" --list-dv-timings
expect "power present 0 without a signal" "power_present.*value=0" v4l2-ctl -d "$DEV" -l

step "composite capture, input control refused while capturing"
prompt "PS1 on composite, game running."
set_input 2
set_fmt 720 576 MPEG
capture 10 720 576 50/1 tt "during=5:v4l2-ctl -d $DEV -c input_source=0; v4l2-ctl -d $DEV --get-input"
run sh -c "grep -A2 'during capture' '$LOG' | tail -3"
expect "input still composite after the refused change" "Video input : 2" v4l2-ctl -d "$DEV" --get-input
