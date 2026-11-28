# TASK T-103 — Release logging

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** implemented; hardware test next
- **Type:** driver (stage 5), before T-102
- **Branch:** `feat/release-logging` (from `dev` 0.5.15)
- **TODO items:** T-103 (stage 5)

## Why

The driver logged every step of a capture start to the kernel log (65
informational messages): useful for troubleshooting and for the tests,
too much for users. One build for everybody, the details behind a switch
(maintainer, 2026-11-28; T-102 decision).

## Goal (acceptance criteria)

1. Always logged: warnings, errors, and the main events: the device ready
   (with its video device), the source detected with its mode, a lost
   signal, mode changes while capturing.
2. Everything else only with the `debug` module parameter (default off,
   changeable while the driver is loaded); `timing` and `trace_mailbox`
   stay separate switches; per-buffer messages stay `dev_dbg` (the
   kernel's dynamic debug).
3. The test runner switches `debug` on for its tests and off at the end.
4. Documented: parameter table, troubleshooting in the README.

## Done so far

- `gchd_dbg()` / `gchd_dbg_ratelimited()` (`elgato_gchd.h`): `dev_info`
  while `debug` is on. 59 messages moved to them; kept as `dev_info`:
  "signal detected", "analog mode now …", "… output now …", "input N:
  signal lost; detecting again" (was `pr_info`), "Elgato Game Capture HD
  ready as /dev/videoN" (was the probe's lifecycle line).
- Build without warnings; `debug` in the module's parameters.
- `tools/gchd-test.sh`: `debug_on` at each step, after prompts and
  commands (a reinstall or replug loads the module again), `debug_off` at
  the end and on Ctrl-C.
- Driver README (parameter table), README (Troubleshooting).

## Findings

## Hardware test (maintainer)

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-103.sh > tests/T-103.sh
bash gchd-test.sh T-103 --branch feat/release-logging
```

Needs the PS3 on composite. Note: `gchd-test.sh` changed (it switches
`debug` on); fetch it again as above.

## Maintainer comments

