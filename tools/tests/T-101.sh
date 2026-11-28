# T-101: --uninstall also removes the firmware after asking (No keeps it,
# Enter removes it, --yes removes it without asking); a new installation
# then extracts it again from Elgato's installer file in firmware/.
# Needs: Elgato's installer file (GameCaptureSetup*.msi) in the
# repository's firmware/ folder; the Elgato plugged in.

INST="$REPO/install-driver.sh"
FW=/lib/firmware/gchd
fw_state() { if [ -d $FW ]; then ls $FW | tr '\n' ' '; else echo "no folder"; fi; echo; }
module_state() { if modinfo elgato_gchd >/dev/null 2>&1; then echo installed; else echo removed; fi; }

step "installer file present"
expect "Elgato's installer file in firmware/" "\.msi" sh -c "ls $REPO/firmware/*.msi"
run bash "$INST" --all --yes --in-terminal < /dev/null
expect "firmware installed" "MB86H57_H58_ENC_H.*MB86M01_ASSP_NSEC_IDLE" fw_state

step "uninstall, answer No: firmware kept"
run sh -c "echo n | bash '$INST' --uninstall --in-terminal"
expect "driver removed" "^removed$" module_state
expect "firmware kept" "MB86H57_H58_ENC_H.*MB86M01_ASSP_NSEC_IDLE" fw_state
expect "profile command removed" "^gone$" sh -c "[ -e /usr/local/bin/gchd-profile ] && echo there || echo gone"

step "uninstall, Enter: firmware removed"
run bash "$INST" --core --yes --in-terminal < /dev/null
expect "core installed with the kept firmware" "^installed$" module_state
run sh -c "echo | bash '$INST' --uninstall --in-terminal"
expect "driver removed" "^removed$" module_state
expect "firmware removed, folder gone" "^no folder$" fw_state

step "install extracts the firmware again"
run bash "$INST" --core --yes --in-terminal < /dev/null
expect "firmware extracted again" "MB86H57_H58_ENC_H.*MB86M01_ASSP_NSEC_IDLE" fw_state
expect "driver installed" "^installed$" module_state

step "uninstall --yes: removed without asking"
run bash "$INST" --uninstall --yes --in-terminal < /dev/null
expect "firmware removed" "^no folder$" fw_state

step "reinstall everything"
run bash "$INST" --all --yes --in-terminal < /dev/null
expect "firmware back" "MB86H57_H58_ENC_H.*MB86M01_ASSP_NSEC_IDLE" fw_state
prompt "PS3 on composite. Unplug the Elgato and plug it in again; wait 5 s."
set_input 2
run v4l2-ctl -d $DEV -v width=720,height=480,pixelformat=MPEG
capture 8
