# TASK T-050 — HDMI input, all modes

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** recordings analysed (2026-11-27); implementation next
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

- `traces/hdmi/gchd_hdmi_{480p60,720p60,1080p60}_input_small` (every output
  size, "Allow 50/60fps", "HDMI Color Range" on some outputs),
  `gchd_hdmi_input_switching{,_extra}_small` (576p, 720p50/60, 1080p50/60),
  `gchd_output_switch_hdmi` (1080p60, older); decoded `.ops.txt` next to
  them.
- `traces/component/gchd_*_output_bitrate_change_small` (bitrate slider,
  R13).
- Analysis: `traces/ANALYSIS-2026-11-27-hdmi-picture-audio-bitrate.md`
  (colour block, HDMI encoder values per mode, bitrate slider).
- `traces/component/gchd_{1920x1080i,1280x720p60}_output_bitrate_change_2_small`
  (slider through the VBR/CBR change), `traces/hdmi/gchd_hdmi_{720p60,1080p60}_input_start_small`
  (first HDMI start; analysis §7).

## Decisions (maintainer, 2026-11-27)

- Bitrate: one slider like the Windows software for every input; its lower
  part VBR (shown as the average), its upper part CBR; the bitrate mode
  menu goes (driver control kept for V4L2 clients, set by the driver).
- Default analog audio gain 0 dB (T-080).
- Bitrate slider: VBR for a shown value below 10.00 (average, B = 2 ×
  shown), CBR from 10.00 (B = shown) up to the output's highest, for every
  output; 480 × 360 VBR only (no gap between the two parts).

## Plan

1. HDMI configuration per mode from the recordings (front end, back end,
   encoder values as for component: the encoder tables are the same per
   output mode); compare with the driver's HDMI path, one item at a time;
   the emulator on `tools` reproduces each recorded sequence first.
2. HDMI colour conversion: BT.601 for 480p/576p RGB, BT.709 for HD;
   "HDMI Color Range" Standard/Expanded as a control (remembered).
3. Bitrate as one slider (driver: ranges per output mode, VBR below the
   CBR start; window, text menu, OBS plugin).
4. HDMI source mode changes while capturing, output sizes, Allow 50/60 fps,
   V4L2 reporting, compliance, TS samples.

## Done so far

## Findings

- HDMI recordings 2026-11-27 (analysis note on traces): output size
  changes and source mode changes do not touch the back end (`0x98`): an
  output size change is STOP, NULL, encoder setup, INIT A0, START; a source
  mode change adds front-end writes only (`9C:AB = 4C`, timing polls until
  stable, per-mode `B2/B5`, `9C:40 = CC`, colour block in the `B0 = E8` …
  `E9` window, `AB = CC`). The driver's HDMI path still runs the back-end
  calibration blocks for every configuration. No recording contains the
  HDMI selection with a source and the first capture start (all start
  while capturing): needed to replace the back-end part (TODO R14).
- First HDMI start recorded (720p60, 1080p60): no back-end writes at all;
  the front-end sequence is in the analysis §7. The driver's HDMI path gets
  that sequence without the back-end blocks.
- The receiver delivers RGB from the PS3; 480p needs the BT.601 matrix
  (the driver uses the BT.709 one for every RGB source).
- Encoder values per output mode are the component ones (same tables).

## Hardware test (maintainer)

Not yet (after the recordings and the analysis).

## Maintainer comments

