# T-061: bitrate, bitrate mode and stretch on composite (PS1, PAL).
# Expected encoder values per setting are in docs/composite.md §10; the
# data rate printed per capture follows the bitrate (VBR below it).

step "load module (fresh plug-in), controls listed"
load_module replug
set_input 2
set_fmt 720 576
run v4l2-ctl -d "$DEV" -l

step "default: 11250 kbit/s CBR"
prompt "PS1 on composite, game running (PAL)."
capture 10 720 576 25/1 tt

step "6000 kbit/s VBR (set while stopped)"
set_ctrl video_bitrate_mode=0,video_bitrate=6000000
capture 10 720 576 25/1 tt

step "14040 kbit/s VBR, changed while capturing"
capture 14 720 576 25/1 tt "during=5:v4l2-ctl -d $DEV -c video_bitrate=14040000"

step "back to 11250 CBR while capturing"
capture 14 720 576 25/1 tt "during=5:v4l2-ctl -d $DEV -c video_bitrate_mode=1,video_bitrate=11250000"

step "stretch"
set_ctrl stretch_sd_input=1
play 15 "Is the picture different from normal (stretched)? Describe in the note how."
set_ctrl stretch_sd_input=0

step "mobile 480x360 at 4000 kbit/s"
set_fmt 480 360
set_ctrl video_bitrate=4000000
capture 10 480 360 25/1 progressive

step "mobile 480x360 at 15000 kbit/s"
set_ctrl video_bitrate=15000000
capture 10 480 360 25/1 progressive

step "picture check at the end"
set_fmt 720 576
set_ctrl video_bitrate=11250000
play 10 "Normal PS1 picture with sound?"
