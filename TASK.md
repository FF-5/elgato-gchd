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
- T-051 (dc20867), twelfth run: driver: the bitrate is kept per mode and
  across output sizes (20 Mbit/s CBR back after 640 × 480; CBR 20 / VBR 16
  each kept), so the 30.00 of the eleventh run was not the driver. FAIL:
  after the maintainer switched the window to Composite and back to
  Component, 1080 was no longer offered and the bitrate slider was gone;
  the window and OBS then disagreed. Cause: selecting an input clears the
  detected source mode (the next capture detects it), and without a
  capture the driver offered only its nominal sizes and no bitrate range.
  Now the driver keeps the component mode detected last for the sizes, the
  bitrate range and the frame rate until a capture detects again. OBS's
  slider stopped at 29.99 (OBS's step rounding with a range starting at
  18.67): the upper limit gets a 0.004 margin. "16 Mbit/s shown as the
  average" in the test meant: VBR 16 Mbit/s is shown as 8.00 (the
  average, as composite).

- T-051 (4747cd5), thirteenth run: 7 PASS, 2 FAIL. Window: CBR 18.67–30.00
  at 20.00, Composite → Component keeps 1080 and the slider, VBR 8.00–15.00,
  VBR 10.00 = 20.04 Mbit/s kept across 640 × 480, 480 × 360 VBR only
  4.00–7.50; OBS: CBR 18.67–30.00, the slider reaches 30.00. FAIL: OBS
  Variable (VBR) gave 30 Mbit/s and the window 15.00 instead of the kept
  20 Mbit/s. Cause: component kept one VBR bitrate for all its output
  sizes; the window's 480 × 360 step left 15000 kbit/s there, below the
  1080 VBR range, so the driver used the highest (30000) and the plugin
  showed and sent 15.00. Maintainer: the bitrate mode carried over to
  every output size; each output size should remember the mode it used
  last, and the settings of an output size are the same for every input.
  Agreed with the maintainer: settings are kept per output mode (the
  picture encoded), shared by every input: 480 × 360 (15000, VBR only);
  640 × 480 and 720 × 480i/576i (11250); 720 × 480p/576p (15000); 1280 × 720
  at 60 fps (26250); 1280 × 720 at 30 fps and scaled from 1080 (15000; own
  mode because the highest bitrate differs, following "Allow 50/60 fps" as
  the Windows software does); 1920 × 1080i/1080p (30000). Each keeps the
  bitrate mode last used and the last bitrate of each mode (parameters
  `out_bitrate_mode`, `out_bitrate_cbr`, `out_bitrate_vbr`, six values;
  the older per-group parameters are read once). The driver follows the
  input, output size, source mode and option, and shows that mode's values
  in the controls; `VIDIOC_G_DV_TIMINGS` gives the component mode detected
  last, from which the plugin predicts the output mode. The window reads
  the values back and no longer sends VBR itself at 480 × 360.

- T-051 (4fe6471), fourteenth run: every bitrate mode and bitrate read
  from the driver was the one expected (per output mode across output
  sizes, composite and component, 720p with "Allow 50/60 fps" off and on,
  576p, 576i, 1080i again, after a replug; settings file), but 18 checks
  reported FAIL: the test's pattern did not allow v4l2-ctl's "(Variable
  Bitrate)" after the mode value (test fixed). Window 6/6 and OBS 4/4
  confirmations PASS. Seen in the OBS log: choosing 1280 × 720 applied
  30.00 (the dialog still used the range of 1080 read from the driver
  before, so the kept 12.00 was outside it; nothing was sent and the
  driver kept 12). Now the plugin takes the range from the output mode
  (the driver's highest bitrates), not from the last read range.

- T-051 (e5f99f5), fifteenth run: 6/6 PASS: OBS shows the range and kept
  bitrate of the output mode at once (1280 × 720 9.33–15.00 at 12.00,
  640 × 480 7.00–11.25 at 10.00, source 18.67–30.00 at 20.00) and applied
  12.00 for 1280 × 720. Bitrate settings per output mode accepted. WARN:
  one encoder restart for a bitrate change at 1280 × 720 while capturing
  failed after 27 s with -110 (no step logged between "encoder restart" and
  the failure); the driver detected again and the capture resumed after
  about 20 s. Now the message names the restart and the failing step
  (stream stop, configuration, mode check, start). Still open for the
  merge: TS samples per mode (RULES F3); the next run saves them and repeats
  eight bitrate changes while capturing at 1280 × 720.

## Maintainer comments

There were still some fails in the test.

gchd-test T-051 20261126-225600 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-051-20261126-225600.log
== build
$ git fetch -q origin
$ git checkout -q feat/component
$ git pull -q --ff-only
commit: 7fcad33 Mode worker: a failed restart names itself (encoder restart or analog mode change) and the failing step (an encoder restart at 1280x720 timed out once with -110, logged as an analog mode change) (T-051)
commit: 7fcad33 Mode worker: a failed restart names itself (encoder restart or analog mode change) and the failing step (an encoder restart at 1280x720 timed out once with -110, logged as an analog mode change) (T-051)
build rc=0 warnings=0

=== step 1: install
$ sudo rmmod elgato_gchd
$ bash /home/ff5/elgato-gchd/install-driver.sh --all --yes --in-terminal
[1mElgato Game Capture HD driver 0.5.10[0m

[1m==> 1/3 Firmware[0m
[32mFirmware found in /lib/firmware/gchd.[0m

[1m==> 2/3 Building the driver[0m
[32mBuilt /home/ff5/elgato-gchd/build/elgato_gchd.ko.[0m

[1m==> 3/3 Installing[0m
Sign command: /usr/bin/kmodsign
Signing key: /var/lib/shim-signed/mok/MOK.priv
Public certificate (MOK): /var/lib/shim-signed/mok/MOK.der

Building module:
Cleaning build area...
make -j4 KERNELRELEASE=6.8.0-146-generic KDIR=/lib/modules/6.8.0-146-generic/build...
Signing module /var/lib/dkms/elgato-gchd/0.5.10/build/elgato_gchd.ko
Cleaning build area...

elgato_gchd.ko.zst:
Running module version sanity check.
 - Original module
   - No original module exists within this kernel
 - Installation
   - Installing to /lib/modules/6.8.0-146-generic/updates/dkms/
depmod...
[32mInstalled with DKMS (rebuilt automatically after kernel updates).[0m
[32mudev rules installed (settings are remembered; device name /dev/elgato-gchd).[0m
[32mDriver loaded. Plug in the Game Capture HD; it appears as a /dev/video device.[0m
Watch: v4l2-ctl -d /dev/elgato-gchd -v pixelformat=MPEG && ffplay -f mpegts /dev/elgato-gchd

[1m==> Settings programs[0m
[32mSettings installed: 'Elgato Game Capture HD Settings' in the application menu (gchd-settings-gui); text menu: gchd-settings[0m

[1m==> OBS plugin[0m
[32mOBS plugin installed in /home/ff5/.config/obs-studio/plugins/obs-elgato-gchd. Restart OBS and add the source 'Elgato Game Capture HD'.[0m

PROMPT: Unplug the Elgato and plug it in again; wait 5 s.
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -c allow_50_60_fps=1
--- dmesg step 1
[ 1916.083650] usbcore: deregistering interface driver elgato_gchd
[ 1926.969877] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[ 1926.974728] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[ 1926.974747] usbcore: registered new interface driver elgato_gchd
[ 1937.478684] usb 1-2: USB disconnect, device number 6
[ 1943.252172] usb 1-2: new high-speed USB device number 7 using xhci_hcd
[ 1943.651597] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[ 1943.651608] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[ 1943.651611] usb 1-2: Product: GameCapture HD
[ 1943.651614] usb 1-2: Manufacturer: Elgato
[ 1943.651616] usb 1-2: SerialNumber: 04107F1267
[ 1943.653934] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[ 1943.657663] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[ 1945.814295] elgato_gchd 1-2:1.0: input switch requested: old=2 new=1 hw_initialized=0 streaming=0
[ 1945.814380] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open

=== step 2: component source 1080i
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
PROMPT: A window 'Elgato component' shows the PS3. Set the PS3's video output to 1080i. Tick 1080i (and untick 1080p) so the PS3 sends 1080i. If the picture freezes or disappears, close the window (it reopens). Press Enter when it shows the PS3 in 1080i.
$ v4l2-ctl -d /dev/video0 --list-framesizes MPEG
ioctl: VIDIOC_ENUM_FRAMESIZES
	Size: Discrete 1920x1080
	Size: Discrete 1280x720
	Size: Discrete 640x480
	Size: Discrete 480x360
--- dmesg step 2
[ 1945.857869] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 1945.979011] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 1946.697352] elgato_gchd 1-2:1.0: state 0x11 reached without completion flag (completion=0x0008); accepted
[ 1946.903541] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 1946.970835] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 1946.993419] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03d
[ 1946.993439] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[ 1946.993446] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 1946.993452] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 1947.791938] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 1949.008676] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 1949.030464] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1949.030683] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1949.032023] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1949.032804] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 1949.033229] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x020e
[ 1949.136538] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1954.030874] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=180 bytes=11796480 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=22
[ 1957.880946] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 3: samples 1080i
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ sh -c tr '\n' ' ' < /home/ff5/gchd-test-logs/T-051-samples/component_1080i.ts.ffprobe.txt; echo errors=$(wc -l < /home/ff5/gchd-test-logs/T-051-samples/component_1080i.ts.ffmpeg.txt)
codec_name=h264 profile=High width=1920 height=1080 sample_aspect_ratio=1:1 level=40 color_primaries=bt709 field_order=tt r_frame_rate=60000/1001 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 codec_name=h264 profile=High width=1920 height=1080 sample_aspect_ratio=1:1 level=40 color_primaries=bt709 field_order=tt r_frame_rate=60000/1001 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 errors=1
[FAIL] 1080i: 1920x1080 tt, level 40, decodes without errors (expected /width=1920 height=1080 .*level=40 .*field_order=tt .*errors=0/, got: codec_name=h264 profile=High width=1920 height=1080 sample_aspect_ratio=1:1 level=40 color_primaries=bt709 field_order=tt r_frame_rate=60000/1001 codec_name=mp2)
$ v4l2-ctl -d /dev/video0 -v width=1280,height=720,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ sh -c tr '\n' ' ' < /home/ff5/gchd-test-logs/T-051-samples/component_1080i_1280x720.ts.ffprobe.txt; echo errors=$(wc -l < /home/ff5/gchd-test-logs/T-051-samples/component_1080i_1280x720.ts.ffmpeg.txt)
/home/ff5/gchd-test-logs/T-051-samples/component_1080i_1280x720.ts: Invalid data found when processing input errors=3
[FAIL] 1080i_1280x720: 1280x720 progressive, level 31, decodes without errors (expected /width=1280 height=720 .*level=31 .*field_order=progressive .*errors=0/, got: /home/ff5/gchd-test-logs/T-051-samples/component_1080i_1280x720.ts: Invalid data found when processing input errors=3)
--- dmesg step 3
[ 1960.936974] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 1960.952708] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 1961.063953] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 1961.135295] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 1961.148078] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03d
[ 1961.148090] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[ 1961.148094] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 1961.148098] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 1961.933965] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 1962.377933] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba9a raw6867=0xa03d
[ 1962.377944] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[ 1962.377950] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 1962.377954] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 1963.448136] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 1963.465036] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1963.465246] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1963.467038] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1963.468138] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 1963.468641] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 1963.587506] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1965.964762] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 1966.255620] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 1966.258564] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 1966.258570] elgato_gchd 1-2:1.0: waiting 1206 ms after the previous shutdown
[ 1967.621334] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 1967.697513] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 1967.711646] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03c
[ 1967.711648] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[ 1967.711650] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 1967.711652] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 1968.504820] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 1969.009480] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03d
[ 1969.009497] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[ 1969.009506] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 1969.009511] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 1970.190585] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 1970.215009] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1970.215315] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1970.216537] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1970.217089] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 1970.217828] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 1970.337745] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1972.147690] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 4: bitrate changes while capturing at 1280x720
$ v4l2-ctl -d /dev/video0 -v width=1280,height=720,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -c video_bitrate=9500000
$ v4l2-ctl -d /dev/video0 -c video_bitrate=14000000
$ v4l2-ctl -d /dev/video0 -c video_bitrate=10000000
$ v4l2-ctl -d /dev/video0 -c video_bitrate=15000000
$ v4l2-ctl -d /dev/video0 -c video_bitrate=12000000
$ v4l2-ctl -d /dev/video0 -c video_bitrate=9330000
$ v4l2-ctl -d /dev/video0 -c video_bitrate=13000000
$ v4l2-ctl -d /dev/video0 -c video_bitrate=12000000
$ count_new component output now 3
8
[PASS] 8 encoder restarts while capturing
$ count_new restart failed|change failed
0
[PASS] no failed restart
$ restarts
[ 1977.285777] elgato_gchd 1-2:1.0: encoder restart: output 3 -> 3, settings changed
[ 1978.205061] elgato_gchd 1-2:1.0: component output now 3
[ 1981.300230] elgato_gchd 1-2:1.0: encoder restart: output 3 -> 3, settings changed
[ 1982.206684] elgato_gchd 1-2:1.0: component output now 3
[ 1985.317203] elgato_gchd 1-2:1.0: encoder restart: output 3 -> 3, settings changed
[ 1986.156134] elgato_gchd 1-2:1.0: component output now 3
[ 1989.333978] elgato_gchd 1-2:1.0: encoder restart: output 3 -> 3, settings changed
[ 1990.322231] elgato_gchd 1-2:1.0: component output now 3
[ 1993.344498] elgato_gchd 1-2:1.0: encoder restart: output 3 -> 3, settings changed
[ 1994.292603] elgato_gchd 1-2:1.0: component output now 3
[ 1997.354384] elgato_gchd 1-2:1.0: encoder restart: output 3 -> 3, settings changed
[ 1998.391080] elgato_gchd 1-2:1.0: component output now 3
[ 2001.365506] elgato_gchd 1-2:1.0: encoder restart: output 3 -> 3, settings changed
[ 2002.362672] elgato_gchd 1-2:1.0: component output now 3
[ 2005.390680] elgato_gchd 1-2:1.0: encoder restart: output 3 -> 3, settings changed
[ 2006.405350] elgato_gchd 1-2:1.0: component output now 3
--- dmesg step 4
[ 1972.258059] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 1972.273678] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 1972.273679] elgato_gchd 1-2:1.0: waiting 1375 ms after the previous shutdown
[ 1973.973157] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 1974.044668] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 1974.057554] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03d
[ 1974.057557] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[ 1974.057559] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 1974.057561] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 1974.849830] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 1975.977146] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 1975.994014] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1975.994339] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1975.995857] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1975.996516] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 1975.997083] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 1976.099935] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1977.285777] elgato_gchd 1-2:1.0: encoder restart: output 3 -> 3, settings changed
[ 1978.098875] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1978.099539] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1978.101110] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1978.102088] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 1978.102929] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 1978.205058] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1978.205061] elgato_gchd 1-2:1.0: component output now 3
[ 1981.300230] elgato_gchd 1-2:1.0: encoder restart: output 3 -> 3, settings changed
[ 1982.083971] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1982.084320] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1982.085787] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1982.086726] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 1982.087662] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 1982.206681] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1982.206684] elgato_gchd 1-2:1.0: component output now 3
[ 1985.317203] elgato_gchd 1-2:1.0: encoder restart: output 3 -> 3, settings changed
[ 1986.033394] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1986.033661] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1986.034777] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1986.035344] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 1986.035867] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 1986.156132] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1986.156134] elgato_gchd 1-2:1.0: component output now 3
[ 1989.333978] elgato_gchd 1-2:1.0: encoder restart: output 3 -> 3, settings changed
[ 1990.199220] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1990.199449] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1990.200664] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1990.201429] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 1990.202008] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 1990.322226] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1990.322231] elgato_gchd 1-2:1.0: component output now 3
[ 1993.344498] elgato_gchd 1-2:1.0: encoder restart: output 3 -> 3, settings changed
[ 1994.186491] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1994.186707] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1994.187927] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1994.188587] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 1994.189019] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 1994.292601] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1994.292603] elgato_gchd 1-2:1.0: component output now 3
[ 1997.354384] elgato_gchd 1-2:1.0: encoder restart: output 3 -> 3, settings changed
[ 1998.268403] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1998.268628] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1998.269905] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1998.270745] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 1998.271398] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 1998.391076] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1998.391080] elgato_gchd 1-2:1.0: component output now 3
[ 2001.365506] elgato_gchd 1-2:1.0: encoder restart: output 3 -> 3, settings changed
[ 2002.256595] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 2002.256843] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 2002.258107] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 2002.258761] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 2002.259295] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 2002.362670] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 2002.362672] elgato_gchd 1-2:1.0: component output now 3
[ 2005.390680] elgato_gchd 1-2:1.0: encoder restart: output 3 -> 3, settings changed
[ 2006.299346] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 2006.299563] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 2006.301029] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 2006.301563] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 2006.302135] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 2006.405347] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 2006.405350] elgato_gchd 1-2:1.0: component output now 3
[ 2011.324325] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=111 bytes=7274496 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 2015.145415] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 5: component source 720p
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
PROMPT: A window 'Elgato component' shows the PS3. Set the PS3's video output to 720p. Tick only 720p. If the picture freezes or disappears, close the window (it reopens). Press Enter when it shows the PS3 in 720p.
$ v4l2-ctl -d /dev/video0 --list-framesizes MPEG
ioctl: VIDIOC_ENUM_FRAMESIZES
	Size: Discrete 1280x720
	Size: Discrete 640x480
	Size: Discrete 480x360
--- dmesg step 5
[ 2015.216262] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 2015.334808] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 2015.334810] elgato_gchd 1-2:1.0: waiting 1312 ms after the previous shutdown
[ 2016.791227] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 2016.854177] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 2016.865841] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03d
[ 2016.865847] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[ 2016.865851] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 2016.865855] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 2017.627116] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 2018.073159] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba9a raw6867=0xa03d
[ 2018.073174] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[ 2018.073182] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 2018.073187] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 2019.130664] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 2019.145405] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 2019.145606] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 2019.147140] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 2019.148022] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 2019.148471] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 2019.251315] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 2024.150235] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=180 bytes=11796480 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=22
[ 2029.157644] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=380 bytes=24903680 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=21
[ 2031.266140] elgato_gchd 1-2:1.0: component source mode changed while capturing; detecting again
[ 2031.285456] elgato_gchd 1-2:1.0: signal present: input=1; first video TS packet on PID 0x1011
[ 2032.404044] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb74 raw6867=0xa6b4
[ 2032.404052] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 2032.404055] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 2032.404058] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 2033.472724] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 2033.496850] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 2033.497064] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 2033.498284] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 2033.498849] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 2033.499297] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 2033.582640] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 2038.499178] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=49 bytes=3211264 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 2041.701155] elgato_gchd 1-2:1.0: component source mode changed while capturing; detecting again
[ 2041.978486] elgato_gchd 1-2:1.0: signal present: input=1; first video TS packet on PID 0x1011
[ 2043.160126] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbf58
[ 2043.160133] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[ 2043.160135] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 2043.160137] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 2044.175291] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 2044.196918] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 2044.197262] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 2044.198579] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 2044.199425] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 2044.200200] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 2044.269536] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 2049.201848] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=221 bytes=14483456 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 2054.206615] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=460 bytes=30146560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 2057.881843] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 6: sample 720p
$ v4l2-ctl -d /dev/video0 -v width=1280,height=720,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ sh -c tr '\n' ' ' < /home/ff5/gchd-test-logs/T-051-samples/component_720p.ts.ffprobe.txt; echo errors=$(wc -l < /home/ff5/gchd-test-logs/T-051-samples/component_720p.ts.ffmpeg.txt)
codec_name=h264 profile=High width=1280 height=720 sample_aspect_ratio=1:1 level=40 color_primaries=bt709 field_order=progressive r_frame_rate=60000/1001 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 codec_name=h264 profile=High width=1280 height=720 sample_aspect_ratio=1:1 level=40 color_primaries=bt709 field_order=progressive r_frame_rate=60000/1001 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 errors=1
[FAIL] 720p: 1280x720 progressive, level 40, decodes without errors (expected /width=1280 height=720 .*level=40 .*field_order=progressive .*errors=0/, got: codec_name=h264 profile=High width=1280 height=720 sample_aspect_ratio=1:1 level=40 color_primaries=bt709 field_order=progressive r_frame_rate=60000/1001 codec_)
--- dmesg step 6
[ 2060.946033] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 2060.949493] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 2061.067875] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 2061.128425] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 2061.138225] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbf59
[ 2061.138227] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[ 2061.138229] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 2061.138231] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 2061.892497] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 2062.327844] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbf59
[ 2062.327857] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[ 2062.327863] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 2062.327867] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 2063.330073] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 2063.344880] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 2063.345071] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 2063.346488] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 2063.347515] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 2063.347903] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 2063.417484] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 2065.823230] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 7: component source 576p
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
PROMPT: A window 'Elgato component' shows the PS3. Set the PS3's video output to 576p. Tick only 576p. If the picture freezes or disappears, close the window (it reopens). Press Enter when it shows the PS3 in 576p.
$ v4l2-ctl -d /dev/video0 --list-framesizes MPEG
ioctl: VIDIOC_ENUM_FRAMESIZES
	Size: Discrete 720x576
	Size: Discrete 640x480
	Size: Discrete 480x360
--- dmesg step 7
[ 2066.154811] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 2066.274421] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 2066.274423] elgato_gchd 1-2:1.0: waiting 1050 ms after the previous shutdown
[ 2067.474977] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 2067.544181] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 2067.565591] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbf59
[ 2067.565595] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[ 2067.565598] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 2067.565601] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 2068.348207] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 2069.531843] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 2069.550874] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 2069.551108] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 2069.552386] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 2069.553425] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 2069.553986] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 2069.623436] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 2074.553053] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=221 bytes=14483456 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=11
[ 2079.557667] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=460 bytes=30146560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=11
[ 2081.055324] elgato_gchd 1-2:1.0: component source mode changed while capturing; detecting again
[ 2081.063635] elgato_gchd 1-2:1.0: signal present: input=1; first video TS packet on PID 0x1011
[ 2081.811732] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b4
[ 2081.811741] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 2081.811745] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 2081.811752] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 2083.080126] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 2083.095390] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 2083.095602] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 2083.096805] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 2083.097466] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 2083.098135] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 2083.181251] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 2088.124792] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=49 bytes=3211264 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 2093.185145] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=95 bytes=6225920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 2098.279399] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=142 bytes=9306112 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 2102.371968] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 8: sample 576p
$ v4l2-ctl -d /dev/video0 -v width=720,height=576,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ sh -c tr '\n' ' ' < /home/ff5/gchd-test-logs/T-051-samples/component_576p.ts.ffprobe.txt; echo errors=$(wc -l < /home/ff5/gchd-test-logs/T-051-samples/component_576p.ts.ffmpeg.txt)
codec_name=h264 profile=High width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 codec_name=h264 profile=High width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 errors=1
[FAIL] 576p: 720x576 progressive, level 30, decodes without errors (expected /width=720 height=576 .*level=30 .*field_order=progressive .*errors=0/, got: codec_name=h264 profile=High width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 codec_n)
--- dmesg step 8
[ 2105.440293] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 2105.443984] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 2105.556966] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 2105.626995] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 2105.637370] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb74 raw6867=0xa6b4
[ 2105.637375] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 2105.637378] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 2105.637381] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 2106.441450] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 2107.598826] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 2107.616650] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 2107.616881] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 2107.618082] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 2107.618629] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 2107.619130] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 2107.702611] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 2110.882573] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 9: component source 576i
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
PROMPT: A window 'Elgato component' shows the PS3. Set the PS3's video output to 576i. Untick all HD resolutions and 576p. If the picture freezes or disappears, close the window (it reopens). Press Enter when it shows the PS3 in 576i.
$ v4l2-ctl -d /dev/video0 --list-framesizes MPEG
ioctl: VIDIOC_ENUM_FRAMESIZES
	Size: Discrete 720x576
	Size: Discrete 640x480
	Size: Discrete 480x360
--- dmesg step 9
[ 2111.088051] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 2111.206204] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 2111.206205] elgato_gchd 1-2:1.0: waiting 1176 ms after the previous shutdown
[ 2112.529779] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 2112.590533] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 2112.601457] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b7
[ 2112.601462] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 2112.601465] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 2112.601467] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 2113.349106] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 2114.451779] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 2114.466614] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 2114.466848] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 2114.468495] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 2114.469283] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 2114.469702] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 2114.533142] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 2119.540748] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=45 bytes=2949120 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 2124.632772] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=92 bytes=6029312 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=25
[ 2129.661216] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=132 bytes=8650752 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=22
[ 2134.744777] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=180 bytes=11796480 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=28
[ 2139.833799] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=233 bytes=15269888 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=27
[ 2140.826817] elgato_gchd 1-2:1.0: component source mode changed while capturing; detecting again
[ 2141.030736] elgato_gchd 1-2:1.0: signal present: input=1; first video TS packet on PID 0x1011
[ 2142.238308] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0x9ab9
[ 2142.238315] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[ 2142.238318] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=1
[ 2142.238319] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=1
[ 2143.353400] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 2143.374841] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 2143.375198] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 2143.376373] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 2143.376882] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=1
[ 2143.377413] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 2143.520923] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 2148.389285] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=92 bytes=6029312 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 2153.392838] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=194 bytes=12713984 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 2154.818227] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 10: sample 576i
$ v4l2-ctl -d /dev/video0 -v width=720,height=576,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ sh -c tr '\n' ' ' < /home/ff5/gchd-test-logs/T-051-samples/component_576i.ts.ffprobe.txt; echo errors=$(wc -l < /home/ff5/gchd-test-logs/T-051-samples/component_576i.ts.ffmpeg.txt)
codec_name=h264 profile=High width=720 height=576 sample_aspect_ratio=12:11 level=31 color_primaries=smpte170m field_order=tt r_frame_rate=50/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 codec_name=h264 profile=High width=720 height=576 sample_aspect_ratio=12:11 level=31 color_primaries=smpte170m field_order=tt r_frame_rate=50/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 errors=0
[PASS] 576i: 720x576 tt, level 31, decodes without errors
--- dmesg step 10
[ 2157.876585] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 2157.880698] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 2158.011128] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 2158.070564] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 2158.079636] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0x9ab9
[ 2158.079642] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[ 2158.079646] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=1
[ 2158.079648] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=1
[ 2158.835837] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 2159.275066] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb74 raw6867=0x9abe
[ 2159.275076] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[ 2159.275082] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=1
[ 2159.275086] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=1
[ 2160.523502] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 2160.538763] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 2160.538961] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 2160.539968] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 2160.540468] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=1
[ 2160.540865] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 2160.684653] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 2162.685473] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 11: samples
$ ls -l /home/ff5/gchd-test-logs/T-051-samples
total 8076
-rw-rw-r-- 1 ff5 ff5       0 Nov 26 22:57 component_1080i_1280x720.ts
-rw-rw-r-- 1 ff5 ff5     247 Nov 26 22:57 component_1080i_1280x720.ts.ffmpeg.txt
-rw-rw-r-- 1 ff5 ff5     109 Nov 26 22:57 component_1080i_1280x720.ts.ffprobe.txt
-rw-rw-r-- 1 ff5 ff5 2482176 Nov 26 22:56 component_1080i.ts
-rw-rw-r-- 1 ff5 ff5      70 Nov 26 22:57 component_1080i.ts.ffmpeg.txt
-rw-rw-r-- 1 ff5 ff5     446 Nov 26 22:57 component_1080i.ts.ffprobe.txt
-rw-rw-r-- 1 ff5 ff5 1110016 Nov 26 23:00 component_576i.ts
-rw-rw-r-- 1 ff5 ff5       0 Nov 26 23:00 component_576i.ts.ffmpeg.txt
-rw-rw-r-- 1 ff5 ff5     442 Nov 26 23:00 component_576i.ts.ffprobe.txt
-rw-rw-r-- 1 ff5 ff5  847872 Nov 26 22:59 component_576p.ts
-rw-rw-r-- 1 ff5 ff5      70 Nov 26 22:59 component_576p.ts.ffmpeg.txt
-rw-rw-r-- 1 ff5 ff5     460 Nov 26 22:59 component_576p.ts.ffprobe.txt
-rw-rw-r-- 1 ff5 ff5 3792896 Nov 26 22:58 component_720p.ts
-rw-rw-r-- 1 ff5 ff5      70 Nov 26 22:58 component_720p.ts.ffmpeg.txt
-rw-rw-r-- 1 ff5 ff5     462 Nov 26 22:58 component_720p.ts.ffprobe.txt
PROMPT: Upload the folder /home/ff5/gchd-test-logs/T-051-samples to the traces branch as traces/component/samples/ (or tell me where it is).
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
--- dmesg step 11
[ 2177.437071] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 2177.437141] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== summary (T-051, 7fcad33 Mode worker: a failed restart names itself (encoder restart or analog mode change) and the failing step (an encoder restart at 1280x720 timed out once with -110, logged as an analog mode change) (T-051))
FAIL | step 3 samples 1080i | 1080i: 1920x1080 tt, level 40, decodes without errors (expected /width=1920 height=1080 .*level=40 .*field_order=tt .*errors=0/, got: codec_name=h264 profile=High width=1920 height=1080 sample_aspect_ratio=1:1 level=40 color_primaries=bt709 field_order=tt r_frame_rate=60000/1001 codec_name=mp2)
FAIL | step 3 samples 1080i | 1080i_1280x720: 1280x720 progressive, level 31, decodes without errors (expected /width=1280 height=720 .*level=31 .*field_order=progressive .*errors=0/, got: /home/ff5/gchd-test-logs/T-051-samples/component_1080i_1280x720.ts: Invalid data found when processing input errors=3)
PASS | step 4 bitrate changes while capturing at 1280x720 | 8 encoder restarts while capturing
PASS | step 4 bitrate changes while capturing at 1280x720 | no failed restart
FAIL | step 6 sample 720p | 720p: 1280x720 progressive, level 40, decodes without errors (expected /width=1280 height=720 .*level=40 .*field_order=progressive .*errors=0/, got: codec_name=h264 profile=High width=1280 height=720 sample_aspect_ratio=1:1 level=40 color_primaries=bt709 field_order=progressive r_frame_rate=60000/1001 codec_)
FAIL | step 8 sample 576p | 576p: 720x576 progressive, level 30, decodes without errors (expected /width=720 height=576 .*level=30 .*field_order=progressive .*errors=0/, got: codec_name=h264 profile=High width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 codec_n)
PASS | step 10 sample 576i | 576i: 720x576 tt, level 31, decodes without errors
