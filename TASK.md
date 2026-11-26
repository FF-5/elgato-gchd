# TASK T-051 — Component input, all modes

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** encoder values and the mode configuration (front end, back
  end) accepted on hardware; output sizes and "Allow 50/60 fps"
  implemented (hardware test open); next: mode changes while capturing,
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

## Maintainer comments

The test didn't allow me to change the output of the PS3.

gchd-test T-051 20261126-130559 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-051-20261126-130559.log
== build
$ git fetch -q origin
$ git checkout -q feat/component
$ git pull -q --ff-only
commit: baf7a66 TASK: T-051 output sizes and Allow 50/60 fps implemented
commit: baf7a66 TASK: T-051 output sizes and Allow 50/60 fps implemented
build rc=0 warnings=0

=== step 1: load the test build (fresh plug-in)
PROMPT: Unplug the Elgato from the PC (or detach it in VirtualBox), wait 3 s, plug it in again.
$ sudo insmod /home/ff5/elgato-gchd/src/driver/elgato_gchd.ko
H.264 device: /dev/video0
module: 0.5.10 
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -v width=720,height=480,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -c allow_50_60_fps=1
$ sh -c v4l2-ctl -d /dev/video0 --list-framesizes MPEG | tr '\n' ' '
ioctl: VIDIOC_ENUM_FRAMESIZES 	Size: Discrete 1280x720 	Size: Discrete 1280x720 	Size: Discrete 640x480 	Size: Discrete 480x360 
[FAIL] 1080p: four output sizes offered (expected /1920x1080.*1280x720.*640x480.*480x360/, got: ioctl: VIDIOC_ENUM_FRAMESIZES 	Size: Discrete 1280x720 	Size: Discrete 1280x720 	Size: Discrete 640x480 	Size: Discrete 480x360 )
$ v4l2-ctl -d /dev/video0 -v width=1280,height=720,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~11930 kbit/s errors=1
[FAIL] 1080p -> 1280x720: stream 1280x720 progressive, level 31 (expected /width=1280 height=720 .*level=31 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
$ v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~10937 kbit/s errors=1
[FAIL] 1080p -> 640x480: stream 640x480 progressive, level 30 (expected /width=640 height=480 .*level=30 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
$ v4l2-ctl -d /dev/video0 -v width=480,height=360,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~10728 kbit/s errors=1
[FAIL] 1080p -> 480x360: stream 480x360 progressive, level 22 (expected /width=480 height=360 .*level=22 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
$ v4l2-ctl -d /dev/video0 -v width=1280,height=720,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~10623 kbit/s errors=1
[FAIL] 1080i -> 1280x720: stream 1280x720 progressive, level 31 (expected /width=1280 height=720 .*level=31 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
$ v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~10046 kbit/s errors=1
[FAIL] 1080i -> 640x480: stream 640x480 progressive, level 30 (expected /width=640 height=480 .*level=30 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
$ v4l2-ctl -d /dev/video0 -v width=480,height=360,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~10833 kbit/s errors=1
[FAIL] 1080i -> 480x360: stream 480x360 progressive, level 22 (expected /width=480 height=360 .*level=22 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~10623 kbit/s errors=1
[FAIL] 1080i source size: stream 1920x1080 tt, level 40 (expected /width=1920 height=1080 .*level=40 .*field_order=tt/, got: width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
--- dmesg step 1
[ 5194.426581] usbcore: deregistering interface driver elgato_gchd
[ 5199.761639] workqueue: hub_event hogged CPU for >10000us 8 times, consider switching to WQ_UNBOUND
[ 5199.877522] usb 1-2: new high-speed USB device number 10 using xhci_hcd
[ 5200.256062] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[ 5200.256084] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[ 5200.256091] usb 1-2: Product: GameCapture HD
[ 5200.256097] usb 1-2: Manufacturer: Elgato
[ 5200.256102] usb 1-2: SerialNumber: 04107F1267
[ 5200.273399] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[ 5200.276229] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[ 5200.276273] usbcore: registered new interface driver elgato_gchd
[ 5218.477438] usbcore: deregistering interface driver elgato_gchd
[ 5218.510507] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[ 5218.515776] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[ 5218.515803] usbcore: registered new interface driver elgato_gchd
[ 5218.531801] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[ 5218.531830] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 5218.537907] elgato_gchd 1-2:1.0: input switch requested: old=2 new=1 hw_initialized=0 streaming=0
[ 5218.537930] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 5218.556515] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5218.560367] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5218.765179] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5218.831704] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5218.842828] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b4
[ 5218.842831] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 5218.842834] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5218.842837] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5219.592896] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5220.603470] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5220.618125] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5220.618323] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5220.619866] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5220.620691] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5220.621129] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x020e
[ 5220.703833] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5225.641226] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5229.258891] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5230.064388] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5230.068977] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5230.068980] elgato_gchd 1-2:1.0: waiting 691 ms after the previous shutdown
[ 5230.905732] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5230.966457] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5230.976895] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b4
[ 5230.976898] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 5230.976901] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5230.976904] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5231.734620] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5232.744109] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5232.758998] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5232.759198] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5232.760560] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5232.761496] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5232.761935] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5232.844681] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5237.780077] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5240.885801] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5241.630176] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5241.633282] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5241.633285] elgato_gchd 1-2:1.0: waiting 753 ms after the previous shutdown
[ 5242.518190] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5242.578954] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5242.587685] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b4
[ 5242.587688] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 5242.587690] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5242.587691] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5243.356336] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5244.434044] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5244.449212] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5244.449462] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5244.450915] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5244.451877] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5244.452310] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5244.535059] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5249.470569] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5252.355063] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5253.125164] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5253.130433] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5253.130436] elgato_gchd 1-2:1.0: waiting 724 ms after the previous shutdown
[ 5254.017081] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5254.083950] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5254.094531] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b7
[ 5254.094535] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 5254.094538] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5254.094540] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5254.906868] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5255.985333] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5256.000663] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5256.000904] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5256.002484] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5256.003052] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5256.003575] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5256.086851] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5261.023006] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5263.920875] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5264.610535] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5264.614305] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5264.614308] elgato_gchd 1-2:1.0: waiting 807 ms after the previous shutdown
[ 5265.764932] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5265.828723] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5265.839856] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b7
[ 5265.839860] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 5265.839862] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5265.839864] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5266.637089] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5267.826856] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5267.842114] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5267.842332] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5267.843623] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5267.844169] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5267.844792] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5267.927841] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5272.864215] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5275.316694] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5276.026365] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5276.030000] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5276.030004] elgato_gchd 1-2:1.0: waiting 787 ms after the previous shutdown
[ 5276.963268] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5277.019027] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5277.026733] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b4
[ 5277.026735] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 5277.026737] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5277.026739] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5277.776416] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5278.762353] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5278.778196] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5278.778585] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5278.780066] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5278.781132] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5278.781769] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5278.864196] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5283.798796] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5286.787144] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5287.480318] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5287.483542] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5287.483545] elgato_gchd 1-2:1.0: waiting 804 ms after the previous shutdown
[ 5288.415484] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5288.493859] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5288.505950] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b4
[ 5288.505954] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 5288.505957] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5288.505959] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5289.274962] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5290.333226] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5290.350429] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5290.350746] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5290.351766] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5290.352293] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5290.352761] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5290.436391] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5295.375062] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5298.258621] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 2: 1080i: output size change while capturing
$ sh -c (sleep 5; v4l2-ctl -d /dev/video0 -v width=1280,height=720,pixelformat=MPEG) & sleep 0; true
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~11462 kbit/s errors=1
$ sh -c sudo dmesg | tail -80
[ 5304.224748] elgato_gchd 1-2:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.225950] elgato_gchd 1-2:1.0: firmware chunk offset=589824 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.227142] elgato_gchd 1-2:1.0: firmware chunk offset=622592 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.228393] elgato_gchd 1-2:1.0: firmware chunk offset=655360 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.229623] elgato_gchd 1-2:1.0: firmware chunk offset=688128 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.230869] elgato_gchd 1-2:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.232131] elgato_gchd 1-2:1.0: firmware chunk offset=753664 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.233445] elgato_gchd 1-2:1.0: firmware chunk offset=786432 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.234741] elgato_gchd 1-2:1.0: firmware chunk offset=819200 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.236012] elgato_gchd 1-2:1.0: firmware chunk offset=851968 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.237317] elgato_gchd 1-2:1.0: firmware chunk offset=884736 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.238658] elgato_gchd 1-2:1.0: firmware chunk offset=917504 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.239977] elgato_gchd 1-2:1.0: firmware chunk offset=950272 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.241319] elgato_gchd 1-2:1.0: firmware chunk offset=983040 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.242623] elgato_gchd 1-2:1.0: firmware chunk offset=1015808 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.243940] elgato_gchd 1-2:1.0: firmware chunk offset=1048576 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.245307] elgato_gchd 1-2:1.0: firmware chunk offset=1081344 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.246670] elgato_gchd 1-2:1.0: firmware chunk offset=1114112 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.248008] elgato_gchd 1-2:1.0: firmware chunk offset=1146880 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.249273] elgato_gchd 1-2:1.0: firmware chunk offset=1179648 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.250644] elgato_gchd 1-2:1.0: firmware chunk offset=1212416 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.252001] elgato_gchd 1-2:1.0: firmware chunk offset=1245184 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.253335] elgato_gchd 1-2:1.0: firmware chunk offset=1277952 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.254709] elgato_gchd 1-2:1.0: firmware chunk offset=1310720 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.256129] elgato_gchd 1-2:1.0: firmware chunk offset=1343488 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.257664] elgato_gchd 1-2:1.0: firmware chunk offset=1376256 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.259068] elgato_gchd 1-2:1.0: firmware chunk offset=1409024 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.260491] elgato_gchd 1-2:1.0: firmware chunk offset=1441792 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.261832] elgato_gchd 1-2:1.0: firmware chunk offset=1474560 size=26224 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5304.271006] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5304.281192] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb74 raw6867=0xa6b7
[ 5304.281195] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 5304.281197] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5304.281199] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5305.079272] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5305.079447] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 5306.314415] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5306.329568] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5306.329682] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 5306.329782] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5306.330970] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5306.331606] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5306.332051] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5306.332302] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[ 5306.332571] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[ 5306.335492] elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0150 ack=1
[ 5306.336280] elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 5306.336608] elgato_gchd 1-2:1.0: START poll=1 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 5306.336911] elgato_gchd 1-2:1.0: START poll=2 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 5306.337219] elgato_gchd 1-2:1.0: START poll=3 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 5306.337527] elgato_gchd 1-2:1.0: START poll=4 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 5306.337833] elgato_gchd 1-2:1.0: START poll=5 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 5306.338127] elgato_gchd 1-2:1.0: START poll=6 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 5306.338422] elgato_gchd 1-2:1.0: START poll=7 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 5306.338704] elgato_gchd 1-2:1.0: START poll=8 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 5306.338975] elgato_gchd 1-2:1.0: START poll=9 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 5306.339266] elgato_gchd 1-2:1.0: START poll=10 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 5306.339552] elgato_gchd 1-2:1.0: START poll=11 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 5306.339857] elgato_gchd 1-2:1.0: START poll=12 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 5306.340128] elgato_gchd 1-2:1.0: START poll=13 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 5306.340428] elgato_gchd 1-2:1.0: START poll=14 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 5306.340696] elgato_gchd 1-2:1.0: START poll=15 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 5306.363450] elgato_gchd 1-2:1.0: START poll=100 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 5306.391877] elgato_gchd 1-2:1.0: START poll=200 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 5306.415214] elgato_gchd 1-2:1.0: START poll=281 state=0x0002 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0150 ack=1
[ 5306.415668] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5306.744191] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[ 5306.778763] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[ 5306.812254] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[ 5306.846121] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[ 5306.878853] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[ 5311.351864] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5313.997655] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[ 5313.998721] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[ 5314.661296] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[ 5314.661731] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[ 5314.719654] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 5314.721071] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[ 5314.721411] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
[ 5314.740346] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[FAIL] encoder restarted for 1280x720 while capturing (expected /component output now 3/, got: [ 5304.224748] elgato_gchd 1-2:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0 [ 5304.225950] elgato_gchd 1-2:1.0: firmware)
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
$ v4l2-compliance -d /dev/video0
v4l2-compliance 1.26.1, 64 bits, 64-bit time_t

Compliance test for elgato-gchd device /dev/video0:

Driver Info:
	Driver name      : elgato-gchd
	Card type        : Elgato Game Capture HD
	Bus info         : usb-0000:00:0c.0-2
	Driver version   : 6.8.12
	Capabilities     : 0x85200001
		Video Capture
		Read/Write
		Streaming
		Extended Pix Format
		Device Capabilities
	Device Caps      : 0x05200001
		Video Capture
		Read/Write
		Streaming
		Extended Pix Format

Required ioctls:
	test VIDIOC_QUERYCAP: OK
	test invalid ioctls: OK

Allow for multiple opens:
	test second /dev/video0 open: OK
	test VIDIOC_QUERYCAP: OK
	test VIDIOC_G/S_PRIORITY: OK
	test for unlimited opens: OK

Debug ioctls:
	test VIDIOC_DBG_G/S_REGISTER: OK (Not Supported)
	test VIDIOC_LOG_STATUS: OK (Not Supported)

Input ioctls:
	test VIDIOC_G/S_TUNER/ENUM_FREQ_BANDS: OK (Not Supported)
	test VIDIOC_G/S_FREQUENCY: OK (Not Supported)
	test VIDIOC_S_HW_FREQ_SEEK: OK (Not Supported)
	test VIDIOC_ENUMAUDIO: OK (Not Supported)
	test VIDIOC_G/S/ENUMINPUT: OK
	test VIDIOC_G/S_AUDIO: OK (Not Supported)
	Inputs: 4 Audio Inputs: 0 Tuners: 0

Output ioctls:
	test VIDIOC_G/S_MODULATOR: OK (Not Supported)
	test VIDIOC_G/S_FREQUENCY: OK (Not Supported)
	test VIDIOC_ENUMAUDOUT: OK (Not Supported)
	test VIDIOC_G/S/ENUMOUTPUT: OK (Not Supported)
	test VIDIOC_G/S_AUDOUT: OK (Not Supported)
	Outputs: 0 Audio Outputs: 0 Modulators: 0

Input/Output configuration ioctls:
	test VIDIOC_ENUM/G/S/QUERY_STD: OK
	test VIDIOC_ENUM/G/S/QUERY_DV_TIMINGS: OK
	test VIDIOC_DV_TIMINGS_CAP: OK
	test VIDIOC_G/S_EDID: OK (Not Supported)

Control ioctls (Input 0):
	test VIDIOC_QUERY_EXT_CTRL/QUERYMENU: OK
	test VIDIOC_QUERYCTRL: OK
	test VIDIOC_G/S_CTRL: OK
	test VIDIOC_G/S/TRY_EXT_CTRLS: OK
	test VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: OK
	test VIDIOC_G/S_JPEGCOMP: OK (Not Supported)
	Standard Controls: 8 Private Controls: 2

Format ioctls (Input 0):
	test VIDIOC_ENUM_FMT/FRAMESIZES/FRAMEINTERVALS: OK
	test VIDIOC_G/S_PARM: OK
	test VIDIOC_G_FBUF: OK (Not Supported)
	test VIDIOC_G_FMT: OK
	test VIDIOC_TRY_FMT: OK
	test VIDIOC_S_FMT: OK
	test VIDIOC_G_SLICED_VBI_CAP: OK (Not Supported)
	test Cropping: OK (Not Supported)
	test Composing: OK (Not Supported)
	test Scaling: OK (Not Supported)

Codec ioctls (Input 0):
	test VIDIOC_(TRY_)ENCODER_CMD: OK (Not Supported)
	test VIDIOC_G_ENC_INDEX: OK (Not Supported)
	test VIDIOC_(TRY_)DECODER_CMD: OK (Not Supported)

Buffer ioctls (Input 0):
		warn: v4l2-test-buffers.cpp(693): VIDIOC_CREATE_BUFS not supported
	test VIDIOC_REQBUFS/CREATE_BUFS/QUERYBUF: OK
	test CREATE_BUFS maximum buffers: OK
	test VIDIOC_EXPBUF: OK (Not Supported)
	test Requests: OK (Not Supported)

Control ioctls (Input 1):
	test VIDIOC_QUERY_EXT_CTRL/QUERYMENU: OK
	test VIDIOC_QUERYCTRL: OK
	test VIDIOC_G/S_CTRL: OK
	test VIDIOC_G/S/TRY_EXT_CTRLS: OK
	test VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: OK
	test VIDIOC_G/S_JPEGCOMP: OK (Not Supported)
	Standard Controls: 8 Private Controls: 2

Format ioctls (Input 1):
	test VIDIOC_ENUM_FMT/FRAMESIZES/FRAMEINTERVALS: OK
	test VIDIOC_G/S_PARM: OK
	test VIDIOC_G_FBUF: OK (Not Supported)
	test VIDIOC_G_FMT: OK
	test VIDIOC_TRY_FMT: OK
	test VIDIOC_S_FMT: OK
	test VIDIOC_G_SLICED_VBI_CAP: OK (Not Supported)
	test Cropping: OK (Not Supported)
	test Composing: OK (Not Supported)
	test Scaling: OK (Not Supported)

Codec ioctls (Input 1):
	test VIDIOC_(TRY_)ENCODER_CMD: OK (Not Supported)
	test VIDIOC_G_ENC_INDEX: OK (Not Supported)
	test VIDIOC_(TRY_)DECODER_CMD: OK (Not Supported)

Buffer ioctls (Input 1):
		warn: v4l2-test-buffers.cpp(693): VIDIOC_CREATE_BUFS not supported
	test VIDIOC_REQBUFS/CREATE_BUFS/QUERYBUF: OK
	test CREATE_BUFS maximum buffers: OK
	test VIDIOC_EXPBUF: OK (Not Supported)
	test Requests: OK (Not Supported)

Control ioctls (Input 2):
	test VIDIOC_QUERY_EXT_CTRL/QUERYMENU: OK
	test VIDIOC_QUERYCTRL: OK
	test VIDIOC_G/S_CTRL: OK
	test VIDIOC_G/S/TRY_EXT_CTRLS: OK
	test VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: OK
	test VIDIOC_G/S_JPEGCOMP: OK (Not Supported)
	Standard Controls: 8 Private Controls: 2

Format ioctls (Input 2):
	test VIDIOC_ENUM_FMT/FRAMESIZES/FRAMEINTERVALS: OK
	test VIDIOC_G/S_PARM: OK
	test VIDIOC_G_FBUF: OK (Not Supported)
	test VIDIOC_G_FMT: OK
	test VIDIOC_TRY_FMT: OK
	test VIDIOC_S_FMT: OK
	test VIDIOC_G_SLICED_VBI_CAP: OK (Not Supported)
	test Cropping: OK (Not Supported)
	test Composing: OK (Not Supported)
	test Scaling: OK (Not Supported)

Codec ioctls (Input 2):
	test VIDIOC_(TRY_)ENCODER_CMD: OK (Not Supported)
	test VIDIOC_G_ENC_INDEX: OK (Not Supported)
	test VIDIOC_(TRY_)DECODER_CMD: OK (Not Supported)

Buffer ioctls (Input 2):
		warn: v4l2-test-buffers.cpp(693): VIDIOC_CREATE_BUFS not supported
	test VIDIOC_REQBUFS/CREATE_BUFS/QUERYBUF: OK
	test CREATE_BUFS maximum buffers: OK
	test VIDIOC_EXPBUF: OK (Not Supported)
	test Requests: OK (Not Supported)

Control ioctls (Input 3):
	test VIDIOC_QUERY_EXT_CTRL/QUERYMENU: OK
	test VIDIOC_QUERYCTRL: OK
	test VIDIOC_G/S_CTRL: OK
	test VIDIOC_G/S/TRY_EXT_CTRLS: OK
	test VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: OK
	test VIDIOC_G/S_JPEGCOMP: OK (Not Supported)
	Standard Controls: 8 Private Controls: 2

Format ioctls (Input 3):
	test VIDIOC_ENUM_FMT/FRAMESIZES/FRAMEINTERVALS: OK
	test VIDIOC_G/S_PARM: OK
	test VIDIOC_G_FBUF: OK (Not Supported)
	test VIDIOC_G_FMT: OK
	test VIDIOC_TRY_FMT: OK
	test VIDIOC_S_FMT: OK
	test VIDIOC_G_SLICED_VBI_CAP: OK (Not Supported)
	test Cropping: OK (Not Supported)
	test Composing: OK (Not Supported)
	test Scaling: OK (Not Supported)

Codec ioctls (Input 3):
	test VIDIOC_(TRY_)ENCODER_CMD: OK (Not Supported)
	test VIDIOC_G_ENC_INDEX: OK (Not Supported)
	test VIDIOC_(TRY_)DECODER_CMD: OK (Not Supported)

Buffer ioctls (Input 3):
		warn: v4l2-test-buffers.cpp(693): VIDIOC_CREATE_BUFS not supported
	test VIDIOC_REQBUFS/CREATE_BUFS/QUERYBUF: OK
	test CREATE_BUFS maximum buffers: OK
	test VIDIOC_EXPBUF: OK (Not Supported)
	test Requests: OK (Not Supported)

Total for elgato-gchd device /dev/video0: 115, Succeeded: 115, Failed: 0, Warnings: 4
[PASS] v4l2-compliance on component: no failures
$ sh -c v4l2-ctl -d /dev/video0 --list-framesizes MPEG | grep -c 1280x720
0
[FAIL] 720p: 1280x720 not offered as a scaled size (expected /^1$/, got: 0)
$ v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~8110 kbit/s errors=1
[FAIL] 720p -> 640x480: stream 640x480 progressive, level 30 (expected /width=640 height=480 .*level=30 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
$ v4l2-ctl -d /dev/video0 -v width=480,height=360,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~8005 kbit/s errors=1
[FAIL] 720p -> 480x360: stream 480x360 progressive, level 22 (expected /width=480 height=360 .*level=22 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
$ v4l2-ctl -d /dev/video0 -c allow_50_60_fps=0
$ v4l2-ctl -d /dev/video0 -v width=1280,height=720,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~7690 kbit/s errors=1
[FAIL] 720p, Allow 50/60 fps off: stream 1280x720 progressive, level 31 (expected /width=1280 height=720 .*level=31 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
$ v4l2-ctl -d /dev/video0 -c allow_50_60_fps=1
$ v4l2-ctl -d /dev/video0 -v width=1280,height=720,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~8057 kbit/s errors=1
[FAIL] 720p, Allow 50/60 fps on: stream 1280x720 progressive, level 40 (expected /width=1280 height=720 .*level=40 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
$ v4l2-ctl -d /dev/video0 -c allow_50_60_fps=0
$ v4l2-ctl -d /dev/video0 -v width=720,height=576,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~7952 kbit/s errors=1
[FAIL] 576p, Allow 50/60 fps off: stream 720x576 progressive, level 31 (expected /width=720 height=576 .*level=31 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
$ v4l2-ctl -d /dev/video0 -c allow_50_60_fps=1
$ v4l2-ctl -d /dev/video0 -v width=720,height=576,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~7847 kbit/s errors=1
[FAIL] 576p, Allow 50/60 fps on: stream 720x576 progressive, level 30 (expected /width=720 height=576 .*level=30 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
$ v4l2-ctl -d /dev/video0 -v width=720,height=576,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~7900 kbit/s errors=1
[FAIL] 576p asked for 640x480: source size: stream 720x576 progressive, level 30 (expected /width=720 height=576 .*level=30 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
$ v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~8110 kbit/s errors=1
[FAIL] 576i -> 640x480: stream 640x480 progressive, level 30 (expected /width=640 height=480 .*level=30 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
$ v4l2-ctl -d /dev/video0 -v width=720,height=576,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~8005 kbit/s errors=1
[FAIL] 576i source size: stream 720x576 tt, level 31 (expected /width=720 height=576 .*level=31 .*field_order=tt/, got: width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
--- dmesg step 2
[ 5303.999821] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5304.202075] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5304.271006] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5304.281192] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb74 raw6867=0xa6b7
[ 5304.281195] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 5304.281197] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5304.281199] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5305.079272] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5306.314415] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5306.329568] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5306.329782] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5306.330970] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5306.331606] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5306.332051] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5306.415668] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5311.351864] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5314.740346] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5315.542197] elgato_gchd 1-2:1.0: input switch requested: old=1 new=0 hw_initialized=0 streaming=0
[ 5315.542247] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 5315.542251] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 5315.542265] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 5315.542267] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 5315.542278] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 5315.542283] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 5315.542294] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 5315.542297] elgato_gchd 1-2:1.0: input switch requested: old=3 new=1 hw_initialized=0 streaming=0
[ 5315.542307] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 5315.542309] elgato_gchd 1-2:1.0: input switch requested: old=1 new=0 hw_initialized=0 streaming=0
[ 5315.542325] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 5315.542328] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 5315.542338] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 5315.542341] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 5315.542353] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 5315.542356] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 5315.542365] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 5315.542397] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 5315.542406] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 5315.542410] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 5315.542417] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 5315.542420] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 5315.542429] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 5315.542442] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 5315.542451] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 5315.542457] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 5315.542492] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 5315.542509] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 5315.542518] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 5315.542536] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 5315.542549] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 5315.542552] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 5315.542561] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 5315.542564] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 5315.542585] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 5315.542588] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 5315.542597] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 5315.542599] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 5315.542610] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 5315.542612] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 5315.542622] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 5315.542625] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 5315.542635] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 5315.542650] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 5315.542660] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 5315.542664] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 5315.542674] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 5315.542677] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 5315.542687] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 5315.542695] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 5315.542704] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 5315.544093] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 5315.544107] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 5315.545940] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 5315.545949] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 5315.546799] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 5315.546807] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 5315.547683] elgato_gchd 1-2:1.0: input switch requested: old=3 new=1 hw_initialized=0 streaming=0
[ 5315.547690] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 5315.547707] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5315.547712] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5315.565234] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5315.570395] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5315.570398] elgato_gchd 1-2:1.0: waiting 671 ms after the previous shutdown
[ 5316.390554] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5316.464184] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5316.475847] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b7
[ 5316.475849] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 5316.475851] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5316.475852] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5317.266099] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5318.375307] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5318.393660] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5318.394040] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5318.396258] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5318.396944] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5318.397456] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5318.479747] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5323.412579] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=106 bytes=6946816 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5326.508560] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5327.213193] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5327.216927] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5327.216929] elgato_gchd 1-2:1.0: waiting 792 ms after the previous shutdown
[ 5328.143528] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5328.203191] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5328.212343] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b4
[ 5328.212345] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 5328.212347] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5328.212349] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5328.988648] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5330.125742] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5330.141458] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5330.141688] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5330.142946] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5330.143533] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5330.143998] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5330.227619] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5335.158530] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=106 bytes=6946816 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5338.070202] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5338.735412] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5338.739325] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5338.739328] elgato_gchd 1-2:1.0: waiting 832 ms after the previous shutdown
[ 5339.893661] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5339.960499] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5339.970127] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b7
[ 5339.970130] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 5339.970132] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5339.970134] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5340.748847] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5341.880213] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5341.897690] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5341.898049] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5341.899926] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5341.900735] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5341.901206] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5341.983507] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5346.915125] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=106 bytes=6946816 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5349.454586] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5350.040467] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5350.044461] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5350.044464] elgato_gchd 1-2:1.0: waiting 911 ms after the previous shutdown
[ 5351.101945] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5351.168358] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5351.178181] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb74 raw6867=0xa6b7
[ 5351.178184] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 5351.178187] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5351.178190] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5351.919035] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5352.940521] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5352.955136] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5352.955332] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5352.956360] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5352.956908] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5352.957275] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5353.040920] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5357.970942] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=106 bytes=6946816 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5360.948991] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5361.593405] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5361.597268] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5361.597273] elgato_gchd 1-2:1.0: waiting 853 ms after the previous shutdown
[ 5362.578029] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5362.640703] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5362.651531] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb74 raw6867=0xa6b4
[ 5362.651536] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 5362.651538] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5362.651540] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5363.439031] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5364.537854] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5364.554628] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5364.554862] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5364.556214] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5364.556949] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5364.557613] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5364.640654] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5369.569456] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=106 bytes=6946816 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5372.364989] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5372.932795] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5372.936016] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5372.936019] elgato_gchd 1-2:1.0: waiting 928 ms after the previous shutdown
[ 5374.013882] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5374.076969] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5374.094793] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b7
[ 5374.094799] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 5374.094802] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5374.094804] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5374.884649] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5375.940370] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5375.958081] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5375.958475] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5375.959734] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5375.960448] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5375.961522] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5376.044071] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5380.975600] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=106 bytes=6946816 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5383.676550] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5384.310460] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5384.313602] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5384.313606] elgato_gchd 1-2:1.0: waiting 863 ms after the previous shutdown
[ 5385.330731] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5385.401054] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5385.410186] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b7
[ 5385.410188] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 5385.410191] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5385.410192] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5386.188701] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5387.299223] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5387.314293] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5387.314614] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5387.316171] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5387.316897] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5387.317307] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5387.400197] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5392.330320] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=106 bytes=6946816 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5395.142372] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5395.792263] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5395.797360] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5395.797364] elgato_gchd 1-2:1.0: waiting 846 ms after the previous shutdown
[ 5396.790826] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5396.853758] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5396.864043] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b7
[ 5396.864053] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 5396.864059] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5396.864063] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5397.611627] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5398.626732] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5398.641716] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5398.642108] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5398.643070] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5398.643601] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5398.644135] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5398.727661] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5403.659255] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=106 bytes=6946816 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5406.591479] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5407.270997] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5407.274517] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5407.274520] elgato_gchd 1-2:1.0: waiting 817 ms after the previous shutdown
[ 5408.241233] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5408.318530] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5408.330797] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b4
[ 5408.330801] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 5408.330855] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5408.330858] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5409.124576] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5410.157546] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5410.173199] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5410.173418] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5410.174812] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5410.175486] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5410.176004] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5410.259159] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5415.191127] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=106 bytes=6946816 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5418.085362] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5418.714023] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 5418.714098] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== summary (T-051, baf7a66 TASK: T-051 output sizes and Allow 50/60 fps implemented)
FAIL | step 1 load the test build (fresh plug-in) | 1080p: four output sizes offered (expected /1920x1080.*1280x720.*640x480.*480x360/, got: ioctl: VIDIOC_ENUM_FRAMESIZES 	Size: Discrete 1280x720 	Size: Discrete 1280x720 	Size: Discrete 640x480 	Size: Discrete 480x360 )
FAIL | step 1 load the test build (fresh plug-in) | 1080p -> 1280x720: stream 1280x720 progressive, level 31 (expected /width=1280 height=720 .*level=31 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
FAIL | step 1 load the test build (fresh plug-in) | 1080p -> 640x480: stream 640x480 progressive, level 30 (expected /width=640 height=480 .*level=30 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
FAIL | step 1 load the test build (fresh plug-in) | 1080p -> 480x360: stream 480x360 progressive, level 22 (expected /width=480 height=360 .*level=22 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
FAIL | step 1 load the test build (fresh plug-in) | 1080i -> 1280x720: stream 1280x720 progressive, level 31 (expected /width=1280 height=720 .*level=31 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
FAIL | step 1 load the test build (fresh plug-in) | 1080i -> 640x480: stream 640x480 progressive, level 30 (expected /width=640 height=480 .*level=30 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
FAIL | step 1 load the test build (fresh plug-in) | 1080i -> 480x360: stream 480x360 progressive, level 22 (expected /width=480 height=360 .*level=22 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
FAIL | step 1 load the test build (fresh plug-in) | 1080i source size: stream 1920x1080 tt, level 40 (expected /width=1920 height=1080 .*level=40 .*field_order=tt/, got: width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
FAIL | step 2 1080i: output size change while capturing | encoder restarted for 1280x720 while capturing (expected /component output now 3/, got: [ 5304.224748] elgato_gchd 1-2:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0 [ 5304.225950] elgato_gchd 1-2:1.0: firmware)
PASS | step 2 1080i: output size change while capturing | v4l2-compliance on component: no failures
FAIL | step 2 1080i: output size change while capturing | 720p: 1280x720 not offered as a scaled size (expected /^1$/, got: 0)
FAIL | step 2 1080i: output size change while capturing | 720p -> 640x480: stream 640x480 progressive, level 30 (expected /width=640 height=480 .*level=30 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
FAIL | step 2 1080i: output size change while capturing | 720p -> 480x360: stream 480x360 progressive, level 22 (expected /width=480 height=360 .*level=22 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
FAIL | step 2 1080i: output size change while capturing | 720p, Allow 50/60 fps off: stream 1280x720 progressive, level 31 (expected /width=1280 height=720 .*level=31 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
FAIL | step 2 1080i: output size change while capturing | 720p, Allow 50/60 fps on: stream 1280x720 progressive, level 40 (expected /width=1280 height=720 .*level=40 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
FAIL | step 2 1080i: output size change while capturing | 576p, Allow 50/60 fps off: stream 720x576 progressive, level 31 (expected /width=720 height=576 .*level=31 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
FAIL | step 2 1080i: output size change while capturing | 576p, Allow 50/60 fps on: stream 720x576 progressive, level 30 (expected /width=720 height=576 .*level=30 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
FAIL | step 2 1080i: output size change while capturing | 576p asked for 640x480: source size: stream 720x576 progressive, level 30 (expected /width=720 height=576 .*level=30 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
FAIL | step 2 1080i: output size change while capturing | 576i -> 640x480: stream 640x480 progressive, level 30 (expected /width=640 height=480 .*level=30 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
FAIL | step 2 1080i: output size change while capturing | 576i source size: stream 720x576 tt, level 31 (expected /width=720 height=576 .*level=31 .*field_order=tt/, got: width=720 height=576 sample_aspect_ratio=12:11 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
