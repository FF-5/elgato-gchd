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

## Maintainer comments

There's a behavior I would like to change. Everytime I select a bitrate mode and change output size, the bitrate mode carries over. I think the ideal behavior is for the output size to remember what bitrate mode it was using last time. Maybe you need to better group how everything is going to be stored. Because clearly every output setting seems to have the same settings regardless of input source, so there's a clear easy pattern to store settings for every output size.

This is the log:
gchd-test T-051 20261126-212251 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-051-20261126-212251.log
== build
$ git fetch -q origin
$ git checkout -q feat/component
$ git pull -q --ff-only
commit: 4747cd5 Component: sizes, bitrate range and frame rate from the mode detected last while no capture has detected since the input was selected (window lost 1080 and the bitrate after Composite -> Component); OBS bitrate slider reaches its highest value (T-051)
commit: 4747cd5 Component: sizes, bitrate range and frame rate from the mode detected last while no capture has detected since the input was selected (window lost 1080 and the bitrate after Composite -> Component); OBS bitrate slider reaches its highest value (T-051)
build rc=0 warnings=0

=== step 1: install the settings programs and the plugin of this branch
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
--- dmesg step 1
[16780.526969] usbcore: deregistering interface driver elgato_gchd
[16783.967388] usb 1-2: new high-speed USB device number 20 using xhci_hcd
[16784.347314] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[16784.347321] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[16784.347323] usb 1-2: Product: GameCapture HD
[16784.347325] usb 1-2: Manufacturer: Elgato
[16784.347326] usb 1-2: SerialNumber: 04107F1267
[16784.366354] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[16784.368007] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[16784.368030] usbcore: registered new interface driver elgato_gchd
[16785.854304] usbcore: deregistering interface driver elgato_gchd
[16787.973965] usb 1-2: USB disconnect, device number 20
[16791.480160] usbcore: registered new interface driver elgato_gchd
[16799.143755] usb 1-2: new high-speed USB device number 21 using xhci_hcd
[16799.522152] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[16799.522172] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[16799.522178] usb 1-2: Product: GameCapture HD
[16799.522183] usb 1-2: Manufacturer: Elgato
[16799.522187] usb 1-2: SerialNumber: 04107F1267
[16799.527062] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[16799.529402] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[16800.590202] elgato_gchd 1-2:1.0: input switch requested: old=2 new=1 hw_initialized=0 streaming=0
[16800.590253] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open

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
[16800.628061] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[16800.746993] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[16800.879281] elgato_gchd 1-2:1.0: transcoder defaults loaded
[16800.944512] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[16800.955343] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03d
[16800.955346] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[16800.955348] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[16800.955351] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[16801.736050] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[16802.888126] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[16802.904818] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[16802.905073] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[16802.906472] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[16802.907232] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[16802.907911] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x020e
[16803.010899] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[16807.907807] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=268 bytes=17563648 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[16810.338149] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 3: settings
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -c video_bitrate_mode=0
$ v4l2-ctl -d /dev/video0 -c video_bitrate=16000000
$ v4l2-ctl -d /dev/video0 -c video_bitrate_mode=1
$ v4l2-ctl -d /dev/video0 -c video_bitrate=20000000
--- dmesg step 3


=== step 4: settings window
PROMPT: Open 'Elgato Game Capture HD Settings' (Input Component).
[PASS] Bitrate mode is shown; Constant (CBR) with the slider from 18.67 to 30.00 at 20.00?
PROMPT: Choose Input Composite, wait 3 s, then Input Component again; wait 3 s.
[PASS] Component again: Output size still offers 1920x1080, 1280x720, 640x480, 480x360; Bitrate mode and the slider (18.67 to 30.00 at 20.00) are shown?
PROMPT: Choose Variable (VBR); wait 3 s.
[PASS] slider from 8.00 to 15.00 at 8.00 (VBR 16 Mbit/s is shown as its average, 8.00)?
PROMPT: Move the slider to about 10.00 and release it; wait 3 s.
$ rate
video_bitrate: 20040000
[PASS] window: VBR about 20 Mbit/s (10.00 average)
PROMPT: Choose Output size 640x480; wait 3 s; then 1920x1080 again; wait 3 s.
[PASS] back at 1920x1080 the slider stands at about 10.00 again (not at the highest)?
PROMPT: Choose Output size 480x360; wait 3 s.
[PASS] Bitrate mode locked to Variable (VBR), slider 4.00 to 7.50?
PROMPT: Output size 1920x1080, Bitrate mode Constant (CBR); wait 3 s; leave the window open.
--- dmesg step 4
[16847.893378] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[16847.893458] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[16853.379450] elgato_gchd 1-2:1.0: input switch requested: old=2 new=1 hw_initialized=0 streaming=0
[16853.379506] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open

=== step 5: OBS plugin
PROMPT: Start OBS (source 'Elgato Game Capture HD', input Component); open the source's properties.
[PASS] OBS: Bitrate mode Constant (CBR), Bitrate from 18.67 to 30.00 at about 20.00; dragging the slider fully to the right reaches 30.00 (not 29.99)?
PROMPT: In OBS's properties choose Variable (VBR) -> OK; wait 3 s.
$ rate
video_bitrate: 30000000
[FAIL] OBS: VBR with the kept VBR bitrate (about 20 Mbit/s) (expected /video_bitrate: (19[5-9]|20[0-5])[0-9]{5}/, got: video_bitrate: 30000000)
[FAIL] the settings window shows Variable (VBR) at about 10.00 by itself? -- It opened at 15.00.
PROMPT: Back to Constant (CBR), slider 30.00 -> OK. Close OBS and the settings window.
$ obs_log 8
21:27:29.234: [gchd] settings applied: input 1, output 0, mode 1, 30.00 Mbit/s, profile 4, stretch 0
21:27:46.777: [gchd] settings applied: input 1, output 0, mode 1, 19.97 Mbit/s, profile 4, stretch 0
21:27:47.484: [gchd] settings applied: input 1, output 0, mode 1, 20.00 Mbit/s, profile 4, stretch 0
21:28:00.597: [gchd] settings applied: input 1, output 0, mode 0, 15.00 Mbit/s, profile 4, stretch 0
21:28:51.895: [gchd] properties: device settings read: input 1, output 0, mode 0, 15.00 Mbit/s
21:28:55.490: [gchd] settings applied: input 1, output 0, mode 1, 20.00 Mbit/s, profile 4, stretch 0
21:29:02.739: [gchd] settings applied: input 1, output 0, mode 1, 30.00 Mbit/s, profile 4, stretch 0
21:29:10.858: [gchd] capture stopped (source hidden or settings changed)
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
--- dmesg step 5
[17022.828716] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[17023.051816] elgato_gchd 1-2:1.0: transcoder defaults loaded
[17023.128712] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[17023.139131] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03d
[17023.139139] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[17023.139143] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[17023.139146] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[17024.157138] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[17025.579295] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[17025.610370] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[17025.610956] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[17025.612502] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[17025.613305] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[17025.614267] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[17025.733565] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[17030.625283] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=180 bytes=11796480 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17035.629073] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=380 bytes=24903680 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17040.633060] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=580 bytes=38010880 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17045.636652] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=780 bytes=51118080 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17046.146735] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[17046.984395] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[17046.984851] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[17046.989007] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[17046.989626] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[17046.990192] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[17047.090579] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[17047.090582] elgato_gchd 1-2:1.0: component output now 0
[17047.753770] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[17049.188574] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[17049.188921] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[17049.189802] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[17049.191464] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[17049.191990] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[17049.294812] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[17049.294815] elgato_gchd 1-2:1.0: component output now 0
[17054.196354] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=268 bytes=17563648 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17059.210120] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=566 bytes=37093376 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=1
[17064.223912] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=864 bytes=56623104 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17065.298983] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[17066.348655] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[17066.348995] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[17066.350004] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[17066.350576] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[17066.351083] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[17066.471587] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[17066.471592] elgato_gchd 1-2:1.0: component output now 0
[17071.371212] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=180 bytes=11796480 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17076.377269] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=380 bytes=24903680 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17079.116680] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[17080.127681] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[17080.128036] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[17080.131609] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[17080.132396] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[17080.133311] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[17080.233867] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[17080.233871] elgato_gchd 1-2:1.0: component output now 0
[17085.171504] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=126 bytes=8257536 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17090.190617] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=264 bytes=17301504 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17095.192952] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=403 bytes=26411008 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17100.206655] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=545 bytes=35717120 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17105.254808] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=686 bytes=44957696 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17110.259167] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=824 bytes=54001664 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17115.263448] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=965 bytes=63242240 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17120.270337] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1105 bytes=72417280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17125.289122] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1247 bytes=81723392 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17130.292818] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1387 bytes=90898432 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17134.011281] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[17135.362280] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1503 bytes=98500608 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17135.699091] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[17135.699540] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[17135.700721] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[17135.701331] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[17135.701944] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[17135.821887] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[17135.821890] elgato_gchd 1-2:1.0: component output now 0
[17140.721149] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=180 bytes=11796480 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17141.260570] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[17142.155320] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[17142.155955] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[17142.157378] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[17142.158029] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[17142.158605] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[17142.261648] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[17142.261652] elgato_gchd 1-2:1.0: component output now 0
[17147.163013] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=268 bytes=17563648 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17149.379185] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[17161.416966] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[17161.416996] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== summary (T-051, 4747cd5 Component: sizes, bitrate range and frame rate from the mode detected last while no capture has detected since the input was selected (window lost 1080 and the bitrate after Composite -> Component); OBS bitrate slider reaches its highest value (T-051))
PASS | step 4 settings window | Bitrate mode is shown; Constant (CBR) with the slider from 18.67 to 30.00 at 20.00?
PASS | step 4 settings window | Component again: Output size still offers 1920x1080, 1280x720, 640x480, 480x360; Bitrate mode and the slider (18.67 to 30.00 at 20.00) are shown?
PASS | step 4 settings window | slider from 8.00 to 15.00 at 8.00 (VBR 16 Mbit/s is shown as its average, 8.00)?
PASS | step 4 settings window | window: VBR about 20 Mbit/s (10.00 average)
PASS | step 4 settings window | back at 1920x1080 the slider stands at about 10.00 again (not at the highest)?
PASS | step 4 settings window | Bitrate mode locked to Variable (VBR), slider 4.00 to 7.50?
PASS | step 5 OBS plugin | OBS: Bitrate mode Constant (CBR), Bitrate from 18.67 to 30.00 at about 20.00; dragging the slider fully to the right reaches 30.00 (not 29.99)?
FAIL | step 5 OBS plugin | OBS: VBR with the kept VBR bitrate (about 20 Mbit/s) (expected /video_bitrate: (19[5-9]|20[0-5])[0-9]{5}/, got: video_bitrate: 30000000)
FAIL | step 5 OBS plugin | the settings window shows Variable (VBR) at about 10.00 by itself? -- It opened at 15.00.
