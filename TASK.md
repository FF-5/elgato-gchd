# TASK T-051 — Component input, all modes

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** encoder values and the mode configuration (front end, back
  end), output sizes per source and "Allow 50/60 fps" accepted on
  hardware, also the size change while capturing and 480i/480p (NTSC
  PS3); next: mode changes while capturing,
  480i/480p, the settings window and OBS plugin for component
- **Type:** driver feature (stage 2)
- **Branch:** `feat/component` (from `dev` 0.5.10)
- **TODO items:** T-051 (stage 2)

## Why

Component input is selectable, but its configuration still follows older
code (the "C++ blocks"): front end, back end and encoder values were never
compared with a recording with a source, and component capture with a
source has not been confirmed (`docs/component.md §3`). Stage 2 brings
component to the level of composite and S-video (RULES D7: within an
input, the lowest mode first).

## Goal (acceptance criteria, draft)

1. Every mode the device offers on component is detected and captured
   with the recorded configuration: 480i, 480p, 576i, 576p, 720p, 1080i,
   1080p30 (USB 2.0 limit, maintainer).
2. Mode changes while capturing, as recorded.
3. Output sizes and the "Allow 50/60fps" option of the Windows software,
   as recorded.
4. Encoder settings of the HDMI/component group (bitrate, mode, profile)
   with the limits of the recordings; shown in the settings window and
   the OBS plugin.
5. V4L2 reporting (DV timings, size, frame rate, field order) right for
   each mode; `v4l2-compliance` without failures.

## Evidence

- `traces/component/gchd_component_modes_output-sizes_5060fps_small_part1`
  and `_part2` (2026-11-25, decoded `.ops.txt` next to them): 576i, then
  576p, 720p, 1080i, 1080p; output sizes; "Allow 50/60fps" on and off;
  order not noted (maintainer).
- No recording of 480i/480p yet (TODO R7).

## Plan

1. Split the recording into its change sequences (NULL … START), read the
   detection values (`0x9C:68/67`), the front-end and back-end writes and
   the encoder fields per sequence, and name each sequence (mode, output
   size, 50/60 fps option). Result: an analysis note on `traces` and a
   table in `docs/component.md`.
2. Ask for single-option recordings where the sequence is unclear.
3. Compare with the driver's component path; change one item at a time
   (RULES D5), 576i first.

## Done so far

- Analysis (`traces/ANALYSIS-2026-11-25-component.md`): 19 change
  sequences (source mode changes, output sizes, the 50/60 fps option),
  encoder fields per mode and size, `docs/component.md §4`.
- Detection values of the driver match the recording for all five modes;
  the first front-end block per mode (`9C:01/02/03/05/06/07/1E/1F`,
  `B2/B5`) matches as well.
- Encoder (`src/driver/transcoder.c`), component at the standard output
  size, now as recorded: bitrate 11250 / 15000 / 26250 / 30000 per mode
  (was 15.625 kbit/s per pixel of width: 20000 at 720p, 11250 at 576p),
  minimum as composite (0 / 10 % / 25 %) and 0.35·B from 26250, GOP 6 for
  576p (was 12), level 30 / 31 / 40
  (was the generic table: 31 / 32 / 42 / 41), `system_rate` from the
  composite-type formula with the progressive surcharge (was
  `1.075 × (B + 320) + 256`; 60 Hz constant 470.2 after the second
  upload), colour BT.709 for HD and SMPTE 170M for SD
  (was SMPTE 170M), `v_vinpelclk` 1 at 50 Hz (was 0), 1080p30 signalled
  60000/1001. All recorded values reproduced (checked by calculation;
  `system_rate` 14 of 14); W=1 clean.
- Second upload (8 single-option recordings, analysis appended to the
  note, `docs/component.md §4`): bitrate ranges of the slider, the 50/60
  fps option per mode (720p 60/30; 576p GOP 6/12 only; none on 1080i and
  1080p30), HD output sizes of HD sources (1280 × 720, 640 × 480,
  480 × 360), SD sources at "720"/"1080" scaled by the Windows software.
- Module version from `dkms.conf` (the Makefile passes it; it was still
  "0.5.8").

## Next

1. ~~Hardware test of the encoder values~~ accepted (third run).
2. ~~Front end and back end per mode~~ accepted (fourth run).
3. Output sizes and the 50/60 fps option. Recorded per source (encoder
   size; "app" = the Windows software scales, the encoder keeps the
   source size):

   | Source | 1920 × 1080 | 1280 × 720 | 640 × 480 | 480 × 360 | "Allow 50/60fps" |
   |---|---|---|---|---|---|
   | 576i | app | app | 640 × 480 (with the software's SD option) | – | – |
   | 576p | app | app | not recorded with the SD option | – | GOP 6 / 12 only |
   | 720p | app? | 1280 × 720 | 640 × 480 (30) | not recorded | 60 / 30 at 1280 × 720 |
   | 1080i | 1920 × 1080 | 1280 × 720 (60) | 640 × 480 | 480 × 360 | no effect |
   | 1080p30 | 1920 × 1080 | 1280 × 720 | 640 × 480 | 480 × 360 | no effect (30) |

   Planned interface, like composite: `VIDIOC_S_FMT` chooses the output
   size among the sizes the current source allows (source size, 1280 ×
   720, 640 × 480, 480 × 360; `VIDIOC_ENUM_FRAMESIZES` lists them), also
   while capturing (encoder restart); a boolean control "Allow 50/60 fps"
   (default on). Today `S_FMT` on HDMI/component selects the source mode
   instead; the source mode belongs to `S_DV_TIMINGS`/detection.
4. Mode changes while capturing (the Windows software restarts with the
   new mode's values, sequence #17/#18).

Wanted recordings (maintainer, with the NTSC PS3): 480i, 480p and 720p
input, 480 × 360 output.

## Hardware test (maintainer)

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-051.sh > tests/T-051.sh
bash gchd-test.sh T-051 --branch feat/component
```

## Findings

- T-051 (a9f1ff0), first run: the PS3 stayed in 576p for the whole run:
  without a picture its menu could not be used (maintainer). The 576p
  capture was right (720 × 576 progressive, 50 frames per second, level
  30); the other mode steps captured 576p as well. The test now shows a
  live viewer while the source's output is changed (restarted when its
  window is closed, with the input selected again for a fresh
  detection), then captures.
- T-051 (ae9c635), second run (live viewer): 10 PASS, 2 FAIL. The test
  ran on the installed driver of `dev`, not on the test build: on
  plug-in udev loaded the DKMS module, and the runner's `insmod` failed
  (maintainer: the module could not be unloaded). The streams show the
  old values (720p level 32, 1080i 42, 1080p 41, SMPTE 170M colour for
  HD). With that driver 576p, 720p, 1080i and 1080p were detected and
  captured with a right picture: baseline for the new values. During the
  576i step the device reported 576p (`0x9C:68/67 = A6B7`): the PS3 still
  sent 576p (it sends the highest resolution ticked in its menu). The
  runner now unloads the auto-loaded module after the plug-in, stops when
  the driver is in use (and names the programs) or when the loaded module
  is not the test build; the test checks level and colour per mode; the
  480 step is left out until the maintainer records 480i/480p. Rule G7:
  test files contain only what is not yet accepted.
- T-051 (29c5497), third run, test build loaded: 10/10 PASS. All five
  modes detected and captured with the recorded encoder values (576i
  720×576 tt level 31 SMPTE 170M; 576p progressive level 30; 720p
  level 40 BT.709; 1080i tt level 40 BT.709; 1080p progressive level 40
  BT.709), picture right in each (maintainer).
- Front end and back end (item 2): the driver's component configuration
  run in userspace (`tools/drvemu`, the driver's own code with logged
  mailbox writes, reads answered from the recording) and diffed with the
  recorded mode changes. Front end: matched except `0x9C:17` (C6, was CE)
  and the input-selection writes (`0x9C:21 = C5`, `0x54 = FC`, bank 4C
  read instead of a second `0xB7 = CE`). Back end: the driver ran four
  blocks of another input path (about 170 writes: `0x98:10`–`0x15` loops,
  `0x58`–`0x65`, `0xE0`–`0xE4`, `0x68`–`0x6D`, `0xC1`/`0xC6`, …) that the
  Windows software never writes for component, wrote `0x98:C0 = 89`
  (recorded 88) and `0x0C = 89` (not recorded), and pulsed `75/74`
  (recorded `05/04` then `15/14`, repeated until `0x98:F5 = 72`, as the
  driver already does for composite); the back-end block came after the
  colour block (recorded before), and the final per-mode front-end values
  (`0xB2`, `0xB5`, `0x03`, `0x04`, `0xE0`) were missing. Now as recorded
  (`docs/component.md §5`); drvemu: all five modes match apart from the
  input selection (not in mode-change recordings) and the number of
  pulses (depends on the status reads). Not done: the back-end reset and
  the `0x9C:04` sweep the Windows software runs at a source mode change
  while capturing (item 4).
- T-051 (ecbbec5), fourth run: 10/10 PASS with the mode configuration as
  recorded (back end without the other input path's blocks, pulses until
  settled): all five modes detected, right picture, encoder values in the
  stream. The maintainer will record 480i, 480p and 720p input and the
  480 × 360 output with an NTSC PS3.
- Output sizes and "Allow 50/60 fps" (item 3): `S_FMT` on component now
  chooses the output size (source, 1280 × 720 for 1080i/1080p, 640 × 480,
  480 × 360 for HD sources, 640 × 480 for 480i/576i), no longer the source
  mode (the mode comes from the detection); `ENUM_FRAMESIZES`,
  `ENUM_FRAMEINTERVALS`, `G_PARM` and the field order follow; a change
  while capturing restarts the encoder (mode worker, as composite);
  encoder settings on component can now be changed while capturing
  (restart; before: refused). Control "Allow 50/60 fps" (default on);
  module parameters `hd_output`, `allow_5060` (uevent, udev rule).
  Encoder values per source, output and option checked with the driver's
  encoder programming run in userspace: all 15 recorded combinations
  reproduced. The settings window and the OBS plugin do not show the
  component output sizes and the option yet (after the hardware test).
  480i/480p values follow the same rules until the NTSC recordings.
- T-051 (baf7a66), fifth run (output sizes): the test file was broken:
  a rename in it had turned the steps that show the live picture and ask
  for the PS3's output into an unknown command, so the PS3 stayed in
  576p for the whole run (maintainer: no way to change it). With 576p the
  driver kept 720 × 576 for every requested size, as intended (576p
  offers only the source size; level 30 until the next point).
  v4l2-compliance (115 tests, 0 failures, now also with the new control
  and sizes) ran in the middle and left its own control values (level
  41, 8000 kbit/s), which the later captures showed. Before a detection
  the size list showed 1280 × 720 twice (nominal source size and the
  preset): fixed. Test: steps restored, output at the source size while
  watching, size list after the viewer's detection, compliance last.
- T-051 (05173eb), sixth run: 20 PASS, 1 FAIL. Accepted: the output
  sizes per source (1080p and 1080i: 1280 × 720 level 31, 640 × 480 level
  30, 480 × 360 level 22; 1080i source size tt level 40; 720p: 1280 × 720
  not offered as a scaled size, 640 × 480, 480 × 360; 576p: source size
  only; 576i: 640 × 480 and source size), "Allow 50/60 fps" (720p level
  31 off / 40 on; 576p level 31 off / 30 on), the size list per source,
  v4l2-compliance on component (0 failures). FAIL: the size change while
  capturing was not tested: the runner waits for a command's output to
  close, so the background change ran before the capture started (the
  capture began at 1280 × 720). The test now starts the change from the
  test itself, 6 s into the capture, in both directions.
- T-051 (fb2b097), seventh run: 2/2 PASS: output size change while
  capturing (1080i: source size → 1280 × 720 and back), encoder restarted
  each time. Next: the maintainer records 480i/480p (sizes, 50/60 fps
  option), 720p with 480 × 360 and a source mode change while capturing
  with the NTSC PS3.
- NTSC PS3 recordings (2026-11-26, analysis appended to
  `traces/ANALYSIS-2026-11-25-component.md`): 480i and 480p offer
  640 × 480 and 480 × 360 (the driver offered the source size only for
  480p, and no 480 × 360 for 480i); scaled outputs of 480p are encoded at
  30 frames per second (timing 60000/1001), like 720p and 1080p30; the
  50/60 fps option on 480p switches GOP 8/14 and level 30/31, also at
  640 × 480; `system_rate` of 480p 16614 (one more than the former +51
  rule): now one constant per case (461.7 / 470.2, progressive 50/60
  frames per second 504.7 / 521.5). Driver changed accordingly (576p
  follows 480p: scaled sizes offered); all 23 recorded combinations of
  source, output and option reproduced in the emulator. Source mode
  changes: the Windows software starts each new mode once with the
  previous mode's values and restarts 3–4 s later; from the new
  detection value to NULL 4.3 s (720p → 1080i).
- T-051 (ec3f0d5), eighth run, NTSC PS3: 9/9 PASS: 480p offers three
  sizes; 720 × 480 progressive level 30 (option on) / 31 (off), 640 × 480
  level 30 / 31 by the option, 480 × 360 level 22; 480i 720 × 480 tt
  level 31, 640 × 480 level 30, 480 × 360 level 22. Not tested on
  hardware: 576p with a scaled size (no recording; follows 480p).
- Source mode change while capturing (item 4), offline: the driver
  followed a component mode change only through the 0.5 s stream-loss
  check (and the mode worker never ran for component after a capture
  start). Now the mode worker runs for component too and reads the
  timing measurement every 0.75 s, as the Windows software does; another
  mode or no signal on two reads in a row hands over to the detection
  (stop, detect, configure, START). Not done: the Windows software's
  neutral back-end values and `0x9C:04` sweep at the change. Hardware
  test together with 576p's scaled outputs (PAL PS3, no new recordings,
  maintainer).
- T-051 (d29fbe6), ninth run, PAL PS3: 8/8 PASS: 576p offers three sizes;
  640 × 480 level 30 (option on) / 31 (off), 480 × 360 level 22, picture
  right at both; source mode changes while capturing (576p → 720p →
  1080i → 576p) followed by the driver, the picture came back by itself
  each time. All driver parts of T-051 accepted; open: the settings
  window and the OBS plugin for component (output sizes, the option).
- Settings window, OBS plugin and text menu for component (offline):
  output size list from the driver's `ENUM_FRAMESIZES` (resolutions,
  source size first; the source size is requested as 1920 × 1080, which
  is never a scaled size), switch/property "Allow 50/60 fps"; stretch and
  the bitrate stay hidden for component. Checked here: the window with a
  stand-in v4l2-ctl (list, switch, commands sent), the plugin's
  properties with libobs (lists per input, visibility), shellcheck.
- T-051 (b4c5a7f), tenth run: 7/7 PASS (component output sizes and
  "Allow 50/60 fps" in the settings window and the OBS plugin). Maintainer:
  the bitrate slider is missing for component (the Windows software has
  it, recorded); the H.264 profile is not a setting in the Windows
  software (handled by the driver). Now: the recordings show component
  encoded CBR at the highest bitrate of the output and VBR below it
  (`v_rate_mode 1`, delay 10), 480 × 360 always VBR; the driver had
  component always CBR (480 × 360 too). Driver: the mode follows the
  value; the bitrate control's range follows the output (lowest 8/15 of
  the highest, as recorded: 16000/30000, 14000/26250, 8000/15000); a
  bitrate outside the range gives the highest. Emulator: the recorded
  lower-bitrate sequences reproduced. Settings window, OBS plugin, text
  menu: component bitrate slider in the driver's range; the H.264 profile
  removed from all three (the driver keeps High).
- T-051 (1988a40), eleventh run: 9/9 PASS (component bitrate range per
  output in the driver, window and plugin; no H.264 profile). Maintainer:
  (1) the bitrate should work as for composite (bitrate mode menu, CBR
  and VBR ranges); the CBR/VBR switch point will come from a bitrate
  recording with the HDMI recordings (TODO R13). Now: component has the
  bitrate mode like composite (driver: the mode set, 480 × 360 VBR only,
  a bitrate kept per mode); window and plugin: mode menu, CBR from 7/11.25
  of the output's highest to the highest, VBR as the average from 8/15 of
  the highest to half of it (placeholders as composite's until R13).
  (2) After 20.00 at 1080i, 640 × 480 and back, the window showed 30.00
  (maintainer). With a stand-in modelling the driver the window shows
  20.00 again; the next test checks the value in the driver itself after
  the same steps.

## Maintainer comments

