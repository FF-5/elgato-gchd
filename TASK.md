# TASK T-101 — Uninstall removes the firmware

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** implemented; hardware test next
- **Type:** installer (stage 5)
- **Branch:** `feat/uninstall-firmware` (from `dev` 0.5.14)
- **TODO items:** T-101 (stage 5)

## Why

`./install-driver.sh --uninstall` removed the driver but kept the firmware
in `/lib/firmware/gchd/`. A full uninstall should leave nothing behind
unless the user wants to keep it (maintainer, 2026-11-27).

## Goal (acceptance criteria)

1. `--uninstall` removes the extracted firmware too, always, without a
   question: it has no use without the driver (maintainer, 2026-11-28;
   during development the test runner reinstalls without uninstalling, and
   a new installation extracts the firmware again from the installer file
   in `firmware/`).
2. Only the four firmware files the installer extracted are removed; the
   folder goes when it is empty (other files stay).
3. A new installation extracts the firmware again.
4. Saved profiles are kept; the README says what the uninstall does.
5. The settings window has one menu entry, under the system settings
   (maintainer 2026-11-28: a shortcut was still under *Sound & Video*):
   the installer and the uninstall remove copies of its entry in the
   user's applications folder (`~/.local/share/applications`; a menu
   editor saves such a copy, which keeps the old categories).

## Done so far

- `install-driver.sh`: `uninstall_firmware()` after `uninstall_driver()`,
  no question; `remove_user_shortcuts()` (entries with
  `Exec=...gchd-settings-gui` in the user's applications folder) on
  install and uninstall of the settings programs; header comment and
  README.
- Checked with scratch folders: firmware files and the empty folder
  removed, an extra file stays; a stale user menu entry removed, other
  entries untouched.

## Findings

- Maintainer (2026-11-28): asking whether to keep the firmware is poor
  for users; the release always removes it.
- Maintainer (2026-11-28): the settings window still had a shortcut under
  *Applications > Sound & Video*. The repository installs only
  `/usr/local/share/applications/elgato-gchd-settings.desktop` (now
  `Categories=Settings;HardwareSettings;`); the other entry is outside
  it, most likely a copy in `~/.local/share/applications`. The test
  lists every entry before the install to confirm.

## Hardware test (maintainer)

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-101.sh > tests/T-101.sh
bash gchd-test.sh T-101 --branch feat/uninstall-firmware
```

Needs Elgato's installer file (`GameCaptureSetup*.msi`) in the
repository's `firmware/` folder and the PS3 on composite. The test
uninstalls and reinstalls.

## Maintainer comments

