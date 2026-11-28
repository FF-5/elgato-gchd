# T-103: release logging. Without the debug parameter a capture start logs
# only the main events (device ready, source detected, mode changes); with
# debug on (switched while loaded) the detailed messages appear again.
# Needs: the PS3 on composite.
# The runner switches debug on after each step's commands; the quiet checks
# below therefore switch it off themselves right before capturing.

INST="$REPO/install-driver.sh"
P=/sys/module/elgato_gchd/parameters/debug
quiet() { echo 0 | sudo tee $P >/dev/null; sudo dmesg -C; }
# grab SECONDS: read the stream (no runner helper, so debug stays as set).
grab() { timeout -k 3 "$1" head -c 20000000 < "$DEV" > /dev/null 2>&1; sleep 1; }
kdmesg() { sudo dmesg | grep elgato_gchd | sed 's/^\[[^]]*\] //'; }

step "install"
run sudo rmmod elgato_gchd
run bash "$INST" --core --yes --in-terminal < /dev/null
prompt "PS3 on composite. Unplug the Elgato and plug it in again; wait 5 s."
expect "debug parameter exists" "Log detailed messages" sh -c "modinfo -p elgato_gchd | grep '^debug'"
expect "debug off after loading" "^N$" sh -c "sudo modprobe -r elgato_gchd && sudo modprobe elgato_gchd && sleep 3 && cat $P"
expect "device ready message" "Elgato Game Capture HD ready as /dev/video" sh -c "sudo dmesg | grep elgato_gchd"

step "quiet capture (debug off)"
quiet
v4l2-ctl -d $DEV -i 2 >/dev/null; v4l2-ctl -d $DEV -v width=720,height=480,pixelformat=MPEG
quiet
grab 8
expect "source detected logged" "signal detected: input=2" kdmesg
expect "no detailed messages" "^0$" sh -c "sudo dmesg | grep elgato_gchd | grep -cE 'capture setup|gchd-diag|bulk-IN|lifecycle|input switch|transcoder defaults|START'"
run sh -c "sudo dmesg | grep elgato_gchd | sed 's/^\[[^]]*\] //'"
confirm "The quiet log above: only a few lines (source detected and perhaps a mode change), no step-by-step messages?"

step "detailed capture (debug switched on while loaded)"
quiet
echo 1 | sudo tee $P >/dev/null
grab 8
expect "detailed messages again" "capture setup: encoder start completed" kdmesg
expect "transfer statistics" "async bulk-IN" kdmesg

step "switched off again"
quiet
grab 6
expect "quiet again" "^0$" sh -c "sudo dmesg | grep elgato_gchd | grep -cE 'capture setup|gchd-diag|bulk-IN'"
