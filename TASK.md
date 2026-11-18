# TASK T-065 — Analog robustness: console swap, start-up, signal loss

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** analysis
- **Type:** driver fixes
- **Branch:** `fix/analog-robustness` (from `dev` 0.5.5)
- **TODO items:** T-065, T-067, T-054 (stage 1)

## Why

The last small stage-1 issues with composite and S-video:

- **T-065:** swapping consoles while capturing without a detected signal
  drop (N64 → running PS1, `0x88:FA` stayed `6`): on S-video one restart
  of about 7 s, on composite two restarts (240p, then 576i).
- **T-067:** the firmware start-up failed twice (`state-change timeout:
  current=0x10 expected=0x11 state=0x0011 completion=0x0008`): IDLE is
  reached but the completion flag is not set; each time after a session
  that ended with a failed START.
- **T-054:** source switched off and on while capturing: worked by report,
  never checked in the logs.

## Implementation (plan)

1. T-065: treat `0x88:E7` bit 7 together with an unlocked `0x88:CA` like a
   signal drop (wait for the 50 Hz reading as after a reset).
2. T-067: when IDLE is requested and the state reads `0x11` without the
   completion flag, accept IDLE as reached (log it); check the other
   start-up state changes for the same pattern.
3. T-054: test steps for source off/on while capturing (both families),
   with the expected log lines.
4. Test file `tools/tests/T-065.sh` (console swaps, source off/on,
   repeated start/close cycles).

## Hardware test (maintainer)

Follows with the implementation.

## Findings

## Maintainer comments

