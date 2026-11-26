# TASK T-050 — HDMI input, all modes

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** not started; waiting for the HDMI recordings (TODO R8, R13)
- **Type:** driver feature (stage 3)
- **Branch:** `feat/hdmi` (from `dev` 0.5.11)
- **TODO items:** T-050 (stage 3)

## Why

HDMI is the last input. Its configuration still follows older code (the
"C++ blocks", TODO "Known differences": back end pulses, `0900:0018`,
encoder timing fields, GOP and bitrate formula); only a 1080p60 source
with output size changes is recorded (`docs/hdmi.md §4`). Stage 3 brings
HDMI to the level of component (RULES D7: lowest mode first).

## Goal (acceptance criteria, draft)

1. Every mode the device offers on HDMI is detected and captured with the
   recorded configuration (480p, 576p, 720p, 1080i, 1080p; the modes the
   recordings show; USB 2.0 limit as for component).
2. Source mode changes while capturing, as recorded.
3. Output sizes and "Allow 50/60 fps" as recorded, also while capturing.
4. Bitrate mode and bitrate per output mode, as for component (the output
   modes are shared with the other inputs), with the ranges of the
   recordings; the CBR/VBR switch point from R13 also sets the component
   ranges (now placeholders). Settings window and OBS plugin.
5. V4L2 reporting (DV timings, size, frame rate, field order) right for
   each mode; `v4l2-compliance` without failures; a TS sample per mode
   on `traces` (RULES F3).

## Evidence

- `traces/hdmi/gchd_output_switch_hdmi` (1080p60 source, output sizes;
  `docs/hdmi.md §4`).
- Needed: R8 (every HDMI mode, mode changes, output sizes, bitrate,
  "Allow 50/60 fps") and R13 (bitrate slider from the highest to the
  lowest and back, values noted).
- Component analysis for comparison:
  `traces/ANALYSIS-2026-11-25-component.md`, `docs/component.md §4–§6`.

## Plan

1. Split the recordings into their change sequences, read the detection
   values, front-end and back-end writes and encoder fields per sequence;
   analysis note on `traces`, table in `docs/hdmi.md`.
2. R13: find the bitrate where the Windows software switches between CBR
   and VBR; set the CBR minimum and VBR maximum (HDMI and component).
3. Compare with the driver's HDMI path; change one item at a time
   (RULES D5), the lowest mode first.
4. The emulator on the `tools` branch (`tools/drvemu/`) reproduces each
   recorded sequence before the hardware test.

## Done so far

## Findings

## Hardware test (maintainer)

Not yet (after the recordings and the analysis).

## Maintainer comments

