# T-071: install-driver.sh (firmware extraction, build, DKMS install,
# udev rule, uninstall). Your firmware is moved aside during the test and
# put back if the extraction fails.

FW=/lib/firmware/gchd
BAK=/lib/firmware/gchd.test-backup
INST="$REPO/install-driver.sh"

step "clean start: uninstall"
run sudo rmmod elgato_gchd
run bash "$INST" --uninstall --yes
expect "no driver installed" "not found|ERROR" sh -c "modinfo elgato_gchd 2>&1"

step "install with the firmware missing (extraction from the .msi)"
prompt "Put Elgato's GameCaptureSetup*.msi into $REPO/firmware/ (it is needed now)."
run sudo rm -rf "$BAK"
run sudo mv "$FW" "$BAK"
run bash "$INST" --yes
if ! sudo test -s "$FW/MB86M01_ASSP_NSEC_ENC_H"; then
	result FAIL "firmware not extracted; your firmware is restored"
	run sudo rm -rf "$FW"
	run sudo mv "$BAK" "$FW"
else
	expect "extracted firmware equals the previous one" "^same$" sh -c "sudo diff -r '$FW' '$BAK' >/dev/null && echo same || echo different"
	run sudo rm -rf "$BAK"
fi

step "installed parts"
expect "module built in build/" "elgato_gchd.ko" ls "$REPO/build"
expect "objects in build/objects/kernel" "core.o" ls "$REPO/build/objects/kernel"
expect "DKMS: installed" "installed" sh -c "dkms status elgato-gchd 2>&1"
expect "module loaded" "elgato_gchd" sh -c "lsmod | grep elgato"
expect "udev rule installed" "56-elgato-gchd-settings.rules" ls /etc/udev/rules.d

step "capture with the installed driver"
prompt "Unplug the Elgato and plug it in again (the installed driver loads by itself). PS1 on composite, game running."
for i in $(seq 1 20); do [ -e "$DEV" ] && break; sleep 0.5; done
set_input 2
set_fmt 720 576 MPEG
capture 10 720 576 50/1 tt

step "uninstall"
run bash "$INST" --uninstall --yes
expect "driver removed" "not found|ERROR" sh -c "modinfo elgato_gchd 2>&1"
expect "udev rule removed" "^gone$" sh -c "test -e /etc/udev/rules.d/56-elgato-gchd-settings.rules && echo present || echo gone"
