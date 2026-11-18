# TASK T-071 — Installer and firmware script

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** implemented (8c415cf); hardware test with the test script
- **Type:** scripts + docs
- **Branch:** `feat/installer` (from `dev`)
- **TODO items:** T-071

## Why

Installing today needs several manual commands (firmware extraction,
DKMS add/install, udev rule). Users without Linux experience should be
able to install the driver from the file manager.

## Implementation

`install-driver.sh` (top level), in the maintainer's order:

1. Firmware in `/lib/firmware/gchd/`? If not, `firmware/extract_firmware_windows`
   (finds `GameCaptureSetup*.msi` in `firmware/`, the current directory or
   `~/Downloads`, or takes its path); if that fails, the installer stops.
2. Build: sources copied to `build/objects/kernel/`, module to
   `build/elgato_gchd.ko`, log `build/build.log`.
3. Install: DKMS when available (offered when missing; otherwise the module
   goes to `/lib/modules/<kernel>/updates/`), the settings udev rule,
   `modprobe`.

Missing packages (headers, make/gcc, 7z, dkms) are named with the
distribution's package and installed on request (`--yes`: without asking).
From a file manager it reopens itself in a terminal. `--uninstall` removes
the driver, DKMS entry, udev rule and settings file (firmware kept).
README: install section. Not yet: download of the .msi from Elgato
(waits for the e-mail answer, TODO T-071).

Tested in the development container: no 7z → offered; no .msi → firmware
step fails, installer stops before building; firmware present → build in
`build/` succeeds. The install step needs a real system: hardware test.

## Hardware test (maintainer)

Needs `GameCaptureSetup*.msi` in `~/Downloads` (or `firmware/`): the test
moves your firmware aside so that the installer extracts it again (put
back if that fails). PS1 on composite for one capture. The installed
driver is removed again at the end.

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-071.sh > tests/T-071.sh
bash gchd-test.sh T-071 --branch feat/installer
```

Paste the summary (and the log if something fails). Optional: also start
`install-driver.sh` once by double-click in the file manager and say
whether the terminal window opened.

## Findings

- T-070 (`chore/src-layout`): smoke test 7/7 PASS with the new layout;
  merged. Licence files: TODO T-094 (blocked for the development
  session; commands in my reply of 2026-11-18).
- Maintainer (2026-11-18): licence files done by the maintainer (two
  files; README explains them, SPDX lines added, ea9f78f). Installer
  sequence: firmware present? else firmware script, stop on failure;
  then build; then install module and components.

## Maintainer comments

=== summary (T-071, 36127c1 TASK: T-071 implementation and test)
PASS | step 1 clean start: uninstall | no driver installed
PASS | step 2 install with the firmware missing (extraction from the .msi) | extracted firmware equals the previous one
PASS | step 3 installed parts | module built in build/
PASS | step 3 installed parts | objects in build/objects/kernel
PASS | step 3 installed parts | DKMS: installed
PASS | step 3 installed parts | module loaded
PASS | step 3 installed parts | udev rule installed
PASS | step 4 capture with the installed driver | stream 720x576 50/1 tt, ~8581 kbit/s
PASS | step 5 uninstall | driver removed
PASS | step 5 uninstall | udev rule removed

FAIL: 0  WARN: 0
