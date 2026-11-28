# T-101: --uninstall also removes the extracted firmware (always, no
# question); a new installation extracts it again from Elgato's installer
# file in firmware/. The settings window has only the installed menu entry
# (stale copies in the user's applications folder removed).
# Needs: Elgato's installer file (GameCaptureSetup*.msi) in the
# repository's firmware/ folder; the PS3 on composite.

INST="$REPO/install-driver.sh"
FW=/lib/firmware/gchd
fw_state() { if [ -d $FW ]; then ls $FW | tr '\n' ' '; else echo "no folder"; fi; echo; }
module_state() { if modinfo elgato_gchd >/dev/null 2>&1; then echo installed; else echo removed; fi; }
# Every menu entry that starts the settings window, with its categories.
entries() {
	grep -l "^Exec=.*gchd-settings-gui" \
		"${XDG_DATA_HOME:-$HOME/.local/share}"/applications/*.desktop \
		/usr/local/share/applications/*.desktop /usr/share/applications/*.desktop 2>/dev/null |
		while read -r f; do echo "$f: $(grep '^Categories=' "$f")"; done
}

step "menu entries before"
run entries
log "(the entries above: where the 'Sound & Video' shortcut came from)"

step "install: one menu entry"
expect "Elgato's installer file in firmware/" "\.msi" sh -c "ls $REPO/firmware/*.msi"
run bash "$INST" --all --yes --in-terminal < /dev/null
expect "firmware installed" "MB86H57_H58_ENC_H.*MB86M01_ASSP_NSEC_IDLE" fw_state
expect "only the installed menu entry" "^/usr/local/share/applications/elgato-gchd-settings.desktop: Categories=Settings;HardwareSettings;$" bash -c "$(declare -f entries); entries | tr '\n' '|' | sed 's/|$//'"
confirm "Application menu: 'Elgato Game Capture HD' only under System > Preferences > Hardware, no longer under Sound & Video? (If the menu has not refreshed, log out and in once.)"

step "uninstall: driver and firmware removed without a question"
run bash "$INST" --uninstall --in-terminal < /dev/null
expect "driver removed" "^removed$" module_state
expect "firmware removed, folder gone" "^no folder$" fw_state
expect "profile command removed" "^gone$" sh -c "[ -e /usr/local/bin/gchd-profile ] && echo there || echo gone"
expect "no menu entry left" "^$" bash -c "$(declare -f entries); entries"

step "install extracts the firmware again"
run bash "$INST" --all --yes --in-terminal < /dev/null
expect "firmware extracted again" "MB86H57_H58_ENC_H.*MB86M01_ASSP_NSEC_IDLE" fw_state
expect "driver installed" "^installed$" module_state
prompt "PS3 on composite. Unplug the Elgato and plug it in again; wait 5 s."
set_input 2
run v4l2-ctl -d $DEV -v width=720,height=480,pixelformat=MPEG
capture 8
