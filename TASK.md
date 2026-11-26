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

## Maintainer comments

gchd-test T-051 20261126-201616 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-051-20261126-201616.log
== build
$ git fetch -q origin
$ git checkout -q feat/component
$ git pull -q --ff-only
commit: b4c5a7f Settings window, OBS plugin and text menu for component: output sizes the source allows, Allow 50/60 fps (T-051)
commit: b4c5a7f Settings window, OBS plugin and text menu for component: output sizes the source allows, Allow 50/60 fps (T-051)
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
[12795.724355] usbcore: deregistering interface driver elgato_gchd
[12809.889176] usbcore: registered new interface driver elgato_gchd
[13077.397180] usb 1-2: new high-speed USB device number 17 using xhci_hcd
[13077.799141] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[13077.799163] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[13077.799169] usb 1-2: Product: GameCapture HD
[13077.799174] usb 1-2: Manufacturer: Elgato
[13077.799178] usb 1-2: SerialNumber: 04107F1267
[13077.803923] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[13077.806714] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[13081.597445] elgato_gchd 1-2:1.0: input switch requested: old=2 new=1 hw_initialized=0 streaming=0
[13081.597513] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open

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
[13081.637723] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[13081.758941] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[13082.038001] elgato_gchd 1-2:1.0: transcoder defaults loaded
[13082.101842] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[13082.116888] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0x9abe
[13082.116891] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[13082.116893] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=1
[13082.116895] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=1
[13082.892937] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[13083.330905] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb74 raw6867=0x9abe
[13083.330914] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[13083.330916] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=1
[13083.330919] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=1
[13084.427627] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[13084.442347] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[13084.442541] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[13084.444329] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[13084.445319] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=1
[13084.445823] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x020e
[13084.588264] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[13089.460800] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=21
[13094.497931] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[13099.534744] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[13104.571920] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=448 bytes=29360128 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[13107.963727] elgato_gchd 1-2:1.0: component source mode changed while capturing; detecting again
[13107.988066] elgato_gchd 1-2:1.0: signal present: input=1; first video TS packet on PID 0x1011
[13108.720340] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b4
[13108.720346] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[13108.720349] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[13108.720351] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[13109.735832] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[13109.752173] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[13109.752476] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[13109.753629] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[13109.754359] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[13109.755159] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x020e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x020e
[13109.837930] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[13114.781642] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13119.782162] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=291 bytes=19070976 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13124.784059] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=442 bytes=28966912 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13129.784781] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=593 bytes=38862848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13134.785856] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=744 bytes=48758784 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13139.785972] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=895 bytes=58654720 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13144.787009] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1046 bytes=68550656 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13148.834843] elgato_gchd 1-2:1.0: component source mode changed while capturing; detecting again
[13148.862578] elgato_gchd 1-2:1.0: signal present: input=1; first video TS packet on PID 0x1011
[13149.877729] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03c
[13149.877735] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[13149.877738] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[13149.877740] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[13150.936176] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[13150.952319] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[13150.952539] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[13150.953765] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[13150.954517] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[13150.955031] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x020e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x020e
[13151.074845] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[13155.955199] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=267 bytes=17498112 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13160.968249] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13165.982177] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=863 bytes=56557568 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13171.053959] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1158 bytes=75890688 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=26
[13171.509500] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 3: settings window
PROMPT: Open 'Elgato Game Capture HD Settings'. Input Component.
[PASS] Output size lists 1920x1080, 1280x720, 640x480, 480x360; an 'Allow 50/60 fps' switch is shown; no Stretch, Bitrate mode or Bitrate? -- But I don't know why you omitted the bitrate slider. I gave you traces with bitrate changes.
PROMPT: Choose Output size 1280x720; wait 3 s.
$ v4l2-ctl -d /dev/video0 -V
Format Video Capture:
	Width/Height      : 1280/720
	Pixel Format      : 'MPEG' (MPEG-1/2/4)
	Field             : None
	Bytes per Line    : 0
	Size Image        : 65424
	Colorspace        : Rec. 709
	Transfer Function : Default (maps to Rec. 709)
	YCbCr/HSV Encoding: Default (maps to Rec. 709)
	Quantization      : Default (maps to Full Range)
	Flags             : 
[PASS] window: output 1280x720
PROMPT: Switch 'Allow 50/60 fps' off; wait 3 s.
$ v4l2-ctl -d /dev/video0 -C allow_50_60_fps
allow_50_60_fps: 0
[PASS] window: Allow 50/60 fps off
PROMPT: Switch it on again and choose Output size 1920x1080; wait 3 s; leave the window open.
$ v4l2-ctl -d /dev/video0 -V
Format Video Capture:
	Width/Height      : 1920/1080
	Pixel Format      : 'MPEG' (MPEG-1/2/4)
	Field             : Interlaced
	Bytes per Line    : 0
	Size Image        : 65424
	Colorspace        : Rec. 709
	Transfer Function : Default (maps to Rec. 709)
	YCbCr/HSV Encoding: Default (maps to Rec. 709)
	Quantization      : Default (maps to Full Range)
	Flags             : 
[PASS] window: back to the source size
--- dmesg step 3


=== step 4: OBS plugin
PROMPT: Start OBS (the source 'Elgato Game Capture HD' shown, input Component); open the source's properties.
[PASS] OBS: Output size lists 1920x1080, 1280x720, 640x480, 480x360 and 'Allow 50/60 fps' is shown; no Stretch, Bitrate mode or Bitrate? -- Still I don't know why you would omit bitrate change since that is a setting that can be changed in Windows. H264 profile on the other hand is not a setting that is available on Windows, but is handled automatically by the driver.
PROMPT: In OBS's properties choose 640x480; wait 5 s (the preview follows).
$ v4l2-ctl -d /dev/video0 -V
Format Video Capture:
	Width/Height      : 640/480
	Pixel Format      : 'MPEG' (MPEG-1/2/4)
	Field             : None
	Bytes per Line    : 0
	Size Image        : 65424
	Colorspace        : Rec. 709
	Transfer Function : Default (maps to Rec. 709)
	YCbCr/HSV Encoding: Default (maps to Rec. 709)
	Quantization      : Default (maps to Full Range)
	Flags             : 
[PASS] OBS: output 640x480
[PASS] the preview shows the PS3 at 640x480, and the settings window shows 640x480 by itself?
PROMPT: Choose 1920x1080 again -> OK. Close OBS and the settings window.
$ obs_log 8
20:26:43.880: [gchd] properties: device settings read: input 1, output 0, mode 1, 11.25 Mbit/s
20:26:46.731: [gchd] capturing from /dev/video0 (video yes, audio yes)
20:26:46.834: [gchd] picture 1920x1080, sample aspect ratio 1:1, shown 1920x1080
20:28:40.314: [gchd] settings applied: input 1, output 1, mode 1, 11.25 Mbit/s, profile 4, stretch 0
20:28:46.442: [gchd] picture 640x480, sample aspect ratio 4:3, shown 854x480
20:30:30.554: [gchd] properties: device settings read: input 1, output 0, mode 1, 11.25 Mbit/s
20:30:31.448: [gchd] picture 1920x1080, sample aspect ratio 1:1, shown 1920x1080
20:30:49.154: [gchd] capture stopped (source hidden or settings changed)
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
--- dmesg step 4
[13402.866315] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[13402.992439] elgato_gchd 1-2:1.0: transcoder defaults loaded
[13403.056064] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[13403.064443] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03c
[13403.064448] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[13403.064450] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[13403.064457] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[13403.905641] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[13405.072198] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[13405.089575] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[13405.090216] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[13405.091470] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[13405.092013] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[13405.092875] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[13405.196024] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[13410.091985] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=268 bytes=17563648 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=1
[13415.106131] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=566 bytes=37093376 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=1
[13420.119031] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=864 bytes=56623104 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13425.132550] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1162 bytes=76152832 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=1
[13430.145986] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1460 bytes=95682560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13435.159768] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1758 bytes=115212288 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13440.172922] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2056 bytes=134742016 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=1
[13445.187041] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2354 bytes=154271744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13450.183866] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2651 bytes=173735936 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=1
[13455.197923] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2949 bytes=193265664 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13460.211748] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=3247 bytes=212795392 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13465.225349] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=3545 bytes=232325120 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13470.222248] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=3842 bytes=251789312 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13475.236119] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=4140 bytes=271319040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13480.248872] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=4438 bytes=290848768 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=1
[13485.246037] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=4735 bytes=310312960 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=1
[13490.259835] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=5033 bytes=329842688 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13495.274364] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=5331 bytes=349372416 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13500.287453] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=5629 bytes=368902144 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13505.300976] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=5927 bytes=388431872 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13510.314391] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=6225 bytes=407961600 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13515.311271] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=6522 bytes=427425792 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13519.432276] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[13520.396644] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=6791 bytes=445054976 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13524.894010] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[13524.894365] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[13524.895322] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[13524.895918] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[13524.896514] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[13525.000258] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[13525.000261] elgato_gchd 1-2:1.0: component output now 1
[13525.369889] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[13529.920274] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13534.945414] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=14417920 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13539.969101] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=21954560 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13544.994153] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=29491200 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13550.019374] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=37027840 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13555.043061] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=680 bytes=44564480 ep84_bytes=44564480 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13560.068250] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=795 bytes=52101120 ep84_bytes=52101120 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13565.092030] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=910 bytes=59637760 ep84_bytes=59637760 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13570.117132] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1025 bytes=67174400 ep84_bytes=67174400 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13575.140694] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1140 bytes=74711040 ep84_bytes=74711040 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13580.165719] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1255 bytes=82247680 ep84_bytes=82247680 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13585.190963] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1370 bytes=89784320 ep84_bytes=89784320 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13590.214468] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1485 bytes=97320960 ep84_bytes=97320960 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13595.239799] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1600 bytes=104857600 ep84_bytes=104857600 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13600.263345] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1715 bytes=112394240 ep84_bytes=112394240 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13605.288393] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1830 bytes=119930880 ep84_bytes=119930880 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13610.313647] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1945 bytes=127467520 ep84_bytes=127467520 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13615.337441] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2060 bytes=135004160 ep84_bytes=135004160 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13620.362425] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2175 bytes=142540800 ep84_bytes=142540800 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13625.385988] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2290 bytes=150077440 ep84_bytes=150077440 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13628.699726] elgato_gchd 1-2:1.0: encoder restart: output 1 -> 0
[13629.761100] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[13629.761459] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[13629.763359] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[13629.763882] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[13629.764549] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[13629.867856] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[13629.867861] elgato_gchd 1-2:1.0: component output now 0
[13634.769221] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=268 bytes=17563648 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13639.782885] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=566 bytes=37093376 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13644.779597] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=863 bytes=56557568 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13648.207995] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[13653.216968] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[13653.216997] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== summary (T-051, b4c5a7f Settings window, OBS plugin and text menu for component: output sizes the source allows, Allow 50/60 fps (T-051))
PASS | step 3 settings window | Output size lists 1920x1080, 1280x720, 640x480, 480x360; an 'Allow 50/60 fps' switch is shown; no Stretch, Bitrate mode or Bitrate? -- But I don't know why you omitted the bitrate slider. I gave you traces with bitrate changes.
PASS | step 3 settings window | window: output 1280x720
PASS | step 3 settings window | window: Allow 50/60 fps off
PASS | step 3 settings window | window: back to the source size
PASS | step 4 OBS plugin | OBS: Output size lists 1920x1080, 1280x720, 640x480, 480x360 and 'Allow 50/60 fps' is shown; no Stretch, Bitrate mode or Bitrate? -- Still I don't know why you would omit bitrate change since that is a setting that can be changed in Windows. H264 profile on the other hand is not a setting that is available on Windows, but is handled automatically by the driver.
PASS | step 4 OBS plugin | OBS: output 640x480
PASS | step 4 OBS plugin | the preview shows the PS3 at 640x480, and the settings window shows 640x480 by itself?
