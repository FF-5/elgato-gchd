# TASK T-071 — Installer and firmware script

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** plan
- **Type:** scripts + docs
- **Branch:** `feat/installer` (from `dev`)
- **TODO items:** T-071

## Why

Installing today needs several manual commands (firmware extraction,
DKMS add/install, udev rule). Users without Linux experience should be
able to install the driver from the file manager.

## Plan

1. `install-driver.sh` (top level, executable, runnable from a file
   manager: opens a terminal when started without one, uses `pkexec` or
   `sudo` for root steps):
   - checks the requirements (kernel headers, `dkms`, `make`, `p7zip` for
     the firmware) and names the package to install when one is missing;
   - firmware: if `/lib/firmware/gchd/` lacks the files, runs the firmware
     step (asks for the `GameCaptureSetup*.msi` the user downloaded;
     download from Elgato only after the e-mail answer, TODO);
   - builds in `build/objects/kernel/` (sources copied there), module to
     `build/`; installs with DKMS; installs the udev rule; loads the
     module;
   - `--uninstall`: DKMS remove, udev rule and settings file removed
     (firmware kept unless asked).
2. `firmware/extract_firmware_windows` reworked: checksum check of the
   extracted files, clear messages, usable by the installer.
3. README: install section points to the script; the manual steps stay
   documented.
4. Test file `tools/tests/T-071.sh`: install, module loads, capture,
   uninstall, reinstall.

## Hardware test (maintainer)

Follows with the implementation.

## Findings

- T-070 (`chore/src-layout`): smoke test 7/7 PASS with the new layout;
  merged. Licence files: TODO T-094 (blocked for the development
  session; commands in my reply of 2026-11-18).

## Maintainer comments

