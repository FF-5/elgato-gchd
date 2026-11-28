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

1. `--uninstall` asks whether to remove the firmware too (Enter = yes);
   No keeps it; `--yes` removes it without asking.
2. Only the four firmware files the installer extracted are removed; the
   folder goes when it is empty (other files stay).
3. A new installation extracts the firmware again from Elgato's installer
   file in `firmware/`.
4. Saved profiles are kept; the README says what the uninstall does.

## Done so far

- `install-driver.sh`: `uninstall_firmware()` after `uninstall_driver()`
  (only when one of the four files exists); header comment and README.
- Checked with a scratch folder: No keeps the files, Enter and `y` remove
  them and the empty folder, an extra file stays with its folder,
  `--yes` removes without asking, no question without firmware.

## Findings

## Hardware test (maintainer)

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-101.sh > tests/T-101.sh
bash gchd-test.sh T-101 --branch feat/uninstall-firmware
```

Needs Elgato's installer file (`GameCaptureSetup*.msi`) in the
repository's `firmware/` folder and the PS3 on composite. The test
uninstalls and reinstalls several times.

## Maintainer comments

