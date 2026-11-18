# T-065: analog robustness (console swap, start-up, source off/on) and the
# refined encoder values (v_vinpelclk per family, GOP 8 for NTSC small
# sizes, system_rate). Needs the PS1 and the N64, both on composite.
# "signal lost" WARN lines are expected in the source off/on steps.

step "load module (fresh plug-in)"
load_module replug
set_input 2

step "PS1 standard size (PAL, v_vinpelclk now 1)"
prompt "PS1 on composite, game running."
set_fmt 720 576 MPEG
set_ctrl video_bitrate_mode=1,video_bitrate=11250000
capture 10 720 576 50/1 tt

step "PS1 640x480"
set_fmt 640 480 MPEG
capture 10 640 480 25/1 progressive

step "N64 640x480 (NTSC: GOP 8, v_vinpelclk 0)"
prompt "Replace the PS1 with the N64 on composite (menu)."
capture 10 640 480 30000/1001 progressive

step "N64 480x360 at 9400 kbit/s VBR"
set_fmt 480 360 MPEG
set_ctrl video_bitrate_mode=0,video_bitrate=9400000
capture 10 480 360 30000/1001 progressive

step "N64 standard size"
set_fmt 720 480 MPEG
set_ctrl video_bitrate_mode=1,video_bitrate=11250000
capture 10 720 480 60000/1001 tt

step "N64 switched off and on while capturing (T-054)"
prompt "Read first: ffplay opens next. When the picture shows, switch the N64 OFF, wait 5 s, switch it ON again, wait until the picture is back. ffplay closes by itself after 45 s. Press Enter to start."
play 45 "Did the picture come back after the N64 was switched on again?"

step "console swap N64 -> running PS1 while capturing (T-065)"
prompt "Read first: start the PS1 game now on the side (not connected). ffplay opens next. When the N64 picture shows, move the composite cable from the N64 to the running PS1 (do not switch anything off). ffplay closes by itself after 45 s. Press Enter to start."
play 45 "Did the PS1 picture appear (correct PAL picture, maybe after a few seconds)?"
run sh -c "sudo dmesg | grep -E 'decoder status changed|analog mode now|waiting up to' "

step "PS1 switched off and on while capturing (T-054)"
prompt "Read first: ffplay opens next. When the PS1 picture shows, switch the PS1 OFF, wait 5 s, switch it ON again and wait until the game picture is back. ffplay closes by itself after 60 s. Press Enter to start."
play 60 "Did the PS1 picture come back after it was switched on again?"

step "capture start with a 288p source (R1)"
prompt "Bring the PS1 to the same screen you used for the 288p recording (gchd_PAL_composite_small) and leave it there."
capture 10 720 576 50/1 tt
expect "capture started in 288p" "configuring 288p" sudo dmesg

step "start/close cycles (T-067)"
capture 5 720 576 50/1 tt
capture 5 720 576 50/1 tt
capture 5 720 576 50/1 tt
