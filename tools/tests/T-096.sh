# T-096: stretch SD input as in the Windows software; installer with the
# core always and the settings programs and the OBS plugin as options.
# Needs: a console on composite with a picture of known shape (a menu, a
# circle), OBS installed from the distribution, the firmware installed.

INST="$REPO/install-driver.sh"
D=/dev/elgato-gchd
PLUG="$HOME/.config/obs-studio/plugins/obs-elgato-gchd/bin/64bit/obs-elgato-gchd.so"
have() { [ -e "$1" ] && echo present || echo absent; }

step "installer: uninstall everything, then the core only (answers: no, no)"
run sudo rmmod elgato_gchd
run bash "$INST" --uninstall --in-terminal < /dev/null
expect "settings programs removed" "^absent$" have /usr/local/bin/gchd-settings-gui
expect "plugin removed" "^absent$" have "$PLUG"
run sh -c "printf 'n\nn\n' | bash '$INST' --in-terminal"
expect "core: driver installed" "elgato_gchd" sh -c "modinfo -n elgato_gchd"
expect "core: udev rules installed" "55-elgato-gchd-device.rules.*56-elgato-gchd-settings.rules" sh -c "ls /etc/udev/rules.d | tr '\n' ' '"
expect "no settings programs" "^absent$" have /usr/local/bin/gchd-settings-gui
expect "no plugin" "^absent$" have "$PLUG"
prompt "Unplug the Elgato and plug it in again; wait 5 s."
expect "/dev/elgato-gchd present" "elgato-gchd" ls -l $D

step "installer: optional parts by option"
run bash "$INST" --settings --yes --in-terminal < /dev/null
expect "settings window installed" "^present$" have /usr/local/bin/gchd-settings-gui
expect "text menu installed" "^present$" have /usr/local/bin/gchd-settings
expect "still no plugin" "^absent$" have "$PLUG"
run bash "$INST" --uninstall --settings --in-terminal < /dev/null
expect "settings programs removed" "^absent$" have /usr/local/bin/gchd-settings-gui
expect "driver kept" "elgato_gchd" sh -c "modinfo -n elgato_gchd"
run bash "$INST" --obs --yes --in-terminal < /dev/null
expect "plugin installed" "^present$" have "$PLUG"
expect "still no settings programs" "^absent$" have /usr/local/bin/gchd-settings-gui

step "installer: questions answered with Enter (both optional parts)"
run sh -c "printf '\n\n' | bash '$INST' --in-terminal"
expect "settings window installed" "^present$" have /usr/local/bin/gchd-settings-gui
expect "plugin installed" "^present$" have "$PLUG"
expect "the installer printed both questions" "Install the OBS plugin.*\\[Y/n\\]" sh -c "printf '\\n\\n' | bash '$INST' --in-terminal 2>&1 | grep 'Install the'"
prompt "Unplug the Elgato and plug it in again; wait 5 s."

step "stretch in ffplay"
run v4l2-ctl -d $D -i 2
run v4l2-ctl -d $D -v width=720,height=480,pixelformat=MPEG
run v4l2-ctl -d $D -c stretch_sd_input=0
play 12 "stretch off: normal 4:3 picture?"
run v4l2-ctl -d $D -c stretch_sd_input=1
play 12 "stretch on: the same picture horizontally stretched (wider window), as in the Windows software's preview?"

step "stretch in OBS"
prompt "Start OBS with the source 'Elgato Game Capture HD' shown; stretch is on; wait 5 s."
confirm "the plugin's picture is horizontally stretched (wider than 4:3), as in the Windows software?"
prompt "Add a Media Source (untick Local File, input /dev/elgato-gchd, input format mpegts) next to it, or open your existing one; wait 5 s. (Hide the plugin's source first: only one program can capture.)"
confirm "the Media Source shows the picture NOT stretched (OBS ignores the aspect ratio; documented)?"
prompt "Remove or hide the Media Source, close OBS."
run v4l2-ctl -d $D -c stretch_sd_input=0
