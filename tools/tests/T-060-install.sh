# T-060 (installer): install-driver.sh with three install types: kernel
# driver only, OBS plugin only, uninstall per part, everything.
# Needs: the firmware installed (or GameCaptureSetup*.msi in firmware/).

INST="$REPO/install-driver.sh"
PLUG="$HOME/.config/obs-studio/plugins/obs-elgato-gchd/bin/64bit/obs-elgato-gchd.so"

step "uninstall everything first"
run sudo rmmod elgato_gchd
run bash "$INST" --uninstall --in-terminal < /dev/null
expect "plugin removed" "^0$" sh -c "ls '$PLUG' 2>/dev/null | wc -l"
expect "settings tool removed" "^0$" sh -c "ls /usr/local/bin/gchd-settings 2>/dev/null | wc -l"

step "kernel driver only (--driver)"
run bash "$INST" --driver --yes --in-terminal < /dev/null
expect "driver installed" "elgato_gchd" sh -c "modinfo -n elgato_gchd"
expect "udev rules installed" "55-elgato-gchd-device.rules.*56-elgato-gchd-settings.rules|56-elgato-gchd-settings.rules.*55-elgato-gchd-device.rules" sh -c "ls /etc/udev/rules.d | tr '\n' ' '"
expect "settings tool installed" "gchd-settings" ls /usr/local/bin/gchd-settings
expect "no plugin installed by --driver" "^0$" sh -c "ls '$PLUG' 2>/dev/null | wc -l"
prompt "Unplug the Elgato and plug it in again; wait 5 s."
expect "/dev/elgato-gchd present" "elgato-gchd" ls -l /dev/elgato-gchd

step "OBS plugin only (--obs)"
run bash "$INST" --obs --yes --in-terminal < /dev/null
expect "plugin installed" "obs-elgato-gchd.so" ls "$PLUG"

step "uninstall the plugin only (--uninstall --obs)"
run bash "$INST" --uninstall --obs --in-terminal < /dev/null
expect "plugin removed" "^0$" sh -c "ls '$PLUG' 2>/dev/null | wc -l"
expect "driver still installed" "elgato_gchd" sh -c "modinfo -n elgato_gchd"

step "menu: everything (choice 1)"
run sh -c "printf '1\n' | bash '$INST' --in-terminal"
expect "driver installed" "elgato_gchd" sh -c "modinfo -n elgato_gchd"
expect "plugin installed" "obs-elgato-gchd.so" ls "$PLUG"
prompt "Start OBS and check that the source 'Elgato Game Capture HD' is in the list (add it, PS1 or N64 running), then close OBS."
confirm "OBS lists the source and shows the picture?"
