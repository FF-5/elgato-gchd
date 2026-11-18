# smoke: quick regression of the accepted analog features (composite, PS1).
# Sourced by gchd-test.sh; uses its step/prompt/confirm/capture helpers.

step "load module (fresh plug-in)"
load_module replug

step "composite, standard size, PS1"
prompt "Connect the PS1 to composite and start a game (PAL)."
set_input 2
set_fmt 720 576
capture 10 720 576 25/1 tt
report

step "composite 640x480"
set_fmt 640 480
capture 10 640 480 25/1 progressive

step "composite 480x360"
set_fmt 480 360
capture 10 480 360 25/1 progressive

step "picture check"
set_fmt 720 576
play 15 "ffplay showed the PS1 picture with sound, correct aspect?"

step "reopen right after close"
capture 5 720 576 25/1 tt
capture 5 720 576 25/1 tt

step "N64 on composite"
prompt "Replace the PS1 with the N64 on composite (menu)."
capture 10 720 480 30000/1001 tt
report
