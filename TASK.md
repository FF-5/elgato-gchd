# TASK T-065 — Analog robustness: console swap, start-up, signal loss

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** implemented (1904bc3, 743919d, dea13c8); hardware test with the test script
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

## Implementation

1. T-065: the family change also waits out the 60 Hz state when
   `0x88:CA` is not locked at the trigger (console swap without a drop).
2. T-067: a state change whose state is reached without the completion
   flag is accepted after the wait (acknowledged, warning logged).
3. T-054: test steps (source off/on while capturing, NTSC and PAL).
4. 288p capture start as recorded (R1, dea13c8): wait 0.4 s and read
   `E7`/`F1` after the apply block.
5. From the new recordings (743919d): `system_rate` refit (all 15 values),
   GOP 8 for the NTSC scaled outputs, `v_vinpelclk` 1 for 50 Hz and 0 for
   60 Hz at every size.

## Hardware test (maintainer)

PS1 and N64 on composite; the script tells you when to swap, switch off
and on. "signal lost" warnings in the off/on steps are expected.

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-065.sh > tests/T-065.sh
bash gchd-test.sh T-065 --branch fix/analog-robustness
```

Paste the summary; the log too if anything fails.

## Findings
- Maintainer recordings 2026-11-18 (now `traces/composite/`):
  `gchd_PAL_composite_bitrate_audio_small` (PS1, start with the source on,
  640×480, bitrate max/min/middle/max, audio gain) and
  `gchd_NTSC_composite_output_bitrate_audio_small` (PS2 480i, every output
  size with bitrate min/middle/max, audio gain). Results: `system_rate`
  refit for PAL and NTSC; the slider shows the VBR average, its top is
  CBR 11250; GOP 8 for NTSC scaled outputs; `v_vinpelclk` follows the
  family; audio gain = `0x94:4A/4B` (+ `41` 37/57), `composite.md` §11;
  480i/576i start writes the apply block (known differences). The PAL
  start was 576i, so R1 (288p start) is still open: start the capture
  while the game shows a 240p/288p screen.

## Maintainer comments

