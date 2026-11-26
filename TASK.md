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

## Maintainer comments

gchd-test T-051 20261126-131418 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-051-20261126-131418.log
== build
$ git fetch -q origin
$ git checkout -q feat/component
$ git pull -q --ff-only
commit: 05173eb Component: no duplicate size in ENUM_FRAMESIZES before a detection; TASK: T-051 fifth run (test file broken, PS3 stayed in 576p)
commit: 05173eb Component: no duplicate size in ENUM_FRAMESIZES before a detection; TASK: T-051 fifth run (test file broken, PS3 stayed in 576p)
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
--- dmesg step 1
[ 5690.890616] usbcore: deregistering interface driver elgato_gchd
[ 5696.761040] usb 1-2: new high-speed USB device number 11 using xhci_hcd
[ 5697.155678] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[ 5697.155695] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[ 5697.155700] usb 1-2: Product: GameCapture HD
[ 5697.155704] usb 1-2: Manufacturer: Elgato
[ 5697.155707] usb 1-2: SerialNumber: 04107F1267
[ 5697.180646] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[ 5697.183173] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[ 5697.183210] usbcore: registered new interface driver elgato_gchd
[ 5700.834533] usbcore: deregistering interface driver elgato_gchd
[ 5700.870420] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[ 5700.876050] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[ 5700.876076] usbcore: registered new interface driver elgato_gchd
[ 5700.891539] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[ 5700.891565] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 5700.896337] elgato_gchd 1-2:1.0: input switch requested: old=2 new=1 hw_initialized=0 streaming=0
[ 5700.896369] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open

=== step 2: component source 1080p
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
PROMPT: A window 'Elgato component' shows the PS3. Set the PS3's video output to 1080p. If the picture freezes or disappears, close the window (it reopens). Press Enter when it shows the PS3 in 1080p.
$ v4l2-ctl -d /dev/video0 --list-framesizes MPEG
ioctl: VIDIOC_ENUM_FRAMESIZES
	Size: Discrete 1920x1080
	Size: Discrete 1280x720
	Size: Discrete 640x480
	Size: Discrete 480x360
$ sh -c v4l2-ctl -d /dev/video0 --list-framesizes MPEG | tr '\n' ' '
ioctl: VIDIOC_ENUM_FRAMESIZES 	Size: Discrete 1920x1080 	Size: Discrete 1280x720 	Size: Discrete 640x480 	Size: Discrete 480x360 
[PASS] 1080p: four output sizes offered
$ v4l2-ctl -d /dev/video0 -v width=1280,height=720,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=1280 height=720 sample_aspect_ratio=1:1 level=31 color_primaries=bt709 field_order=progressive r_frame_rate=30000/1001 width=1280 height=720 sample_aspect_ratio=1:1 level=31 color_primaries=bt709 field_order=progressive r_frame_rate=30000/1001  ~11721 kbit/s errors=1
[PASS] 1080p -> 1280x720: stream 1280x720 progressive, level 31
$ v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=640 height=480 sample_aspect_ratio=4:3 level=30 color_primaries=bt709 field_order=progressive r_frame_rate=30000/1001 width=640 height=480 sample_aspect_ratio=4:3 level=30 color_primaries=bt709 field_order=progressive r_frame_rate=30000/1001  ~8005 kbit/s errors=1
[PASS] 1080p -> 640x480: stream 640x480 progressive, level 30
$ v4l2-ctl -d /dev/video0 -v width=480,height=360,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=480 height=360 sample_aspect_ratio=4:3 level=22 color_primaries=bt709 field_order=progressive r_frame_rate=30000/1001 width=480 height=360 sample_aspect_ratio=4:3 level=22 color_primaries=bt709 field_order=progressive r_frame_rate=30000/1001  ~9836 kbit/s errors=0
[PASS] 1080p -> 480x360: stream 480x360 progressive, level 22
--- dmesg step 2
[ 5700.931179] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5701.050065] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5701.175831] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5701.240284] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5701.249771] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb74 raw6867=0xa6b7
[ 5701.249775] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 5701.249778] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5701.249781] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5702.038572] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5703.110008] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5703.126647] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5703.126874] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5703.128190] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5703.128878] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5703.129530] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x020e
[ 5703.212617] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5708.149262] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=36
[ 5713.149220] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=291 bytes=19070976 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=35
[ 5718.150874] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=442 bytes=28966912 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=35
[ 5723.154772] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=593 bytes=38862848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=35
[ 5728.152117] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=744 bytes=48758784 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=35
[ 5733.153130] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=895 bytes=58654720 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=35
[ 5738.153874] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1046 bytes=68550656 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=35
[ 5743.154102] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1197 bytes=78446592 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=36
[ 5748.155269] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1348 bytes=88342528 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=36
[ 5753.154578] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1499 bytes=98238464 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=34
[ 5758.155439] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1650 bytes=108134400 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=35
[ 5763.155727] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1801 bytes=118030336 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=35
[ 5768.156074] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1952 bytes=127926272 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=35
[ 5773.161263] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2103 bytes=137822208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=34
[ 5778.171738] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2254 bytes=147718144 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=34
[ 5782.952916] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5783.986481] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5784.108272] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5784.108275] elgato_gchd 1-2:1.0: waiting 344 ms after the previous shutdown
[ 5784.589244] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5784.654483] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5784.664646] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbbfa
[ 5784.664649] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=30/1 interlaced=0 signal_present=1
[ 5784.664651] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=30/1 interlaced=0
[ 5784.664652] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=30/1 interlaced=0
[ 5785.460826] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5786.524117] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5786.540994] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5786.541249] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5786.542332] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5786.542937] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=30/1 interlaced=0
[ 5786.543426] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5786.613524] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5791.555409] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=269 bytes=17629184 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5796.555123] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=566 bytes=37093376 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=22
[ 5797.094809] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5800.115215] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5800.120211] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5800.300336] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5800.365171] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5800.375904] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbbf5
[ 5800.375907] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=30/1 interlaced=0 signal_present=1
[ 5800.375910] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=30/1 interlaced=0
[ 5800.375911] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=30/1 interlaced=0
[ 5801.162992] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5802.213744] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5802.233180] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5802.233430] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5802.234633] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5802.235341] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=30/1 interlaced=0
[ 5802.236152] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5802.305725] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5807.252138] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=138 bytes=9043968 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5811.071770] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5811.742902] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5811.748823] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5811.748827] elgato_gchd 1-2:1.0: waiting 824 ms after the previous shutdown
[ 5812.732497] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5812.796665] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5812.805706] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbbf4
[ 5812.805708] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=30/1 interlaced=0 signal_present=1
[ 5812.805711] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=30/1 interlaced=0
[ 5812.805713] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=30/1 interlaced=0
[ 5813.562597] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5814.592783] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5814.607692] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5814.607906] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5814.608965] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5814.609529] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=30/1 interlaced=0
[ 5814.610003] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5814.680234] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5815.068819] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[ 5819.619607] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5822.654069] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5822.997237] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5823.001640] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5823.001644] elgato_gchd 1-2:1.0: waiting 1154 ms after the previous shutdown
[ 5824.302237] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5824.378138] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5824.393647] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbbf5
[ 5824.393654] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=30/1 interlaced=0 signal_present=1
[ 5824.393658] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=30/1 interlaced=0
[ 5824.393660] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=30/1 interlaced=0
[ 5825.184516] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5826.327991] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5826.343558] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5826.343768] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5826.345036] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5826.345855] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=30/1 interlaced=0
[ 5826.346381] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5826.416573] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5826.789687] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[ 5831.366834] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=139 bytes=9109504 ep84_bytes=9109504 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5833.846189] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 3: component source 1080i
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
PROMPT: A window 'Elgato component' shows the PS3. Set the PS3's video output to 1080i. Untick 1080p. If the picture freezes or disappears, close the window (it reopens). Press Enter when it shows the PS3 in 1080i.
$ v4l2-ctl -d /dev/video0 --list-framesizes MPEG
ioctl: VIDIOC_ENUM_FRAMESIZES
	Size: Discrete 1920x1080
	Size: Discrete 1280x720
	Size: Discrete 640x480
	Size: Discrete 480x360
$ v4l2-ctl -d /dev/video0 -v width=1280,height=720,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=1280 height=720 sample_aspect_ratio=1:1 level=31 color_primaries=bt709 field_order=progressive r_frame_rate=30000/1001 width=1280 height=720 sample_aspect_ratio=1:1 level=31 color_primaries=bt709 field_order=progressive r_frame_rate=30000/1001  ~11668 kbit/s errors=1
[PASS] 1080i -> 1280x720: stream 1280x720 progressive, level 31
$ v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=640 height=480 sample_aspect_ratio=4:3 level=30 color_primaries=bt709 field_order=progressive r_frame_rate=30000/1001 width=640 height=480 sample_aspect_ratio=4:3 level=30 color_primaries=bt709 field_order=progressive r_frame_rate=30000/1001  ~7484 kbit/s errors=1
[PASS] 1080i -> 640x480: stream 640x480 progressive, level 30
$ v4l2-ctl -d /dev/video0 -v width=480,height=360,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=480 height=360 sample_aspect_ratio=4:3 level=22 color_primaries=bt709 field_order=progressive r_frame_rate=30000/1001 width=480 height=360 sample_aspect_ratio=4:3 level=22 color_primaries=bt709 field_order=progressive r_frame_rate=30000/1001  ~9368 kbit/s errors=0
[PASS] 1080i -> 480x360: stream 480x360 progressive, level 22
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=1920 height=1080 sample_aspect_ratio=1:1 level=40 color_primaries=bt709 field_order=tt r_frame_rate=60000/1001 width=1920 height=1080 sample_aspect_ratio=1:1 level=40 color_primaries=bt709 field_order=tt r_frame_rate=60000/1001  ~18946 kbit/s errors=1
[PASS] 1080i source size: stream 1920x1080 tt, level 40
--- dmesg step 3
[ 5834.130389] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5834.245268] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5834.245271] elgato_gchd 1-2:1.0: waiting 1102 ms after the previous shutdown
[ 5835.496672] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5835.557017] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5835.564743] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbbf4
[ 5835.564746] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=30/1 interlaced=0 signal_present=1
[ 5835.564748] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=30/1 interlaced=0
[ 5835.564750] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=30/1 interlaced=0
[ 5836.313432] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5837.302526] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5837.318450] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5837.318657] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5837.319639] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5837.320177] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=30/1 interlaced=0
[ 5837.320647] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5837.391105] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5842.329020] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=269 bytes=17629184 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5847.326707] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=566 bytes=37093376 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5849.692777] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5850.720564] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5850.835393] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5850.835395] elgato_gchd 1-2:1.0: waiting 358 ms after the previous shutdown
[ 5851.339699] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5851.403401] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5851.415028] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b7
[ 5851.415031] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 5851.415034] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5851.415037] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5852.218978] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5853.295049] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5853.310280] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5853.310538] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5853.312253] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5853.312770] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 5853.313289] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5853.396509] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5858.331992] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=36
[ 5863.331264] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=291 bytes=19070976 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=35
[ 5868.337318] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=442 bytes=28966912 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=35
[ 5873.345250] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=593 bytes=38862848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=36
[ 5878.347420] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=744 bytes=48758784 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=35
[ 5881.639482] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5882.667147] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5882.779602] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5882.779603] elgato_gchd 1-2:1.0: waiting 360 ms after the previous shutdown
[ 5883.266608] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5883.329360] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5883.338847] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03d
[ 5883.338849] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[ 5883.338851] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5883.338852] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5884.134063] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5885.240009] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5885.259754] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5885.260011] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5885.261116] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5885.261728] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5885.262243] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5885.382814] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5890.266205] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=267 bytes=17498112 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5892.957595] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5895.978364] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5895.982369] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5896.151497] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5896.214488] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5896.223781] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03c
[ 5896.223784] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[ 5896.223786] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5896.223788] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5897.004685] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5898.091279] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5898.109579] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5898.110066] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5898.111878] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5898.112512] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5898.113296] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5898.232317] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5903.129689] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=137 bytes=8978432 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5906.987087] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5907.691944] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5907.696026] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5907.696029] elgato_gchd 1-2:1.0: waiting 791 ms after the previous shutdown
[ 5908.631592] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5908.697106] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5908.710135] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03d
[ 5908.710140] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[ 5908.710143] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5908.710145] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5909.469351] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5909.900569] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03c
[ 5909.900584] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[ 5909.900592] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5909.900596] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5910.989976] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5911.004727] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5911.004919] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5911.006272] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5911.006951] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5911.007348] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5911.110532] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5911.482040] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[ 5916.032187] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5918.518178] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5918.896407] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5918.901253] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5918.901256] elgato_gchd 1-2:1.0: waiting 1118 ms after the previous shutdown
[ 5920.165962] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5920.226494] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5920.235480] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba9a raw6867=0xa03c
[ 5920.235482] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[ 5920.235484] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5920.235486] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5920.991930] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5921.463270] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba9a raw6867=0xa03d
[ 5921.463280] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[ 5921.463284] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5921.463287] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5922.501166] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5922.515899] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5922.516221] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5922.517203] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5922.517812] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5922.518277] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5922.621722] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5922.977360] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[ 5927.520643] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=138 bytes=9043968 ep84_bytes=9043968 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5929.773992] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5930.018275] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5930.021493] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5930.021496] elgato_gchd 1-2:1.0: waiting 1252 ms after the previous shutdown
[ 5931.411208] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5931.477645] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5931.486824] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03d
[ 5931.486826] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[ 5931.486827] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5931.486829] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5932.280733] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5933.387525] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5933.405103] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5933.405347] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5933.406584] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5933.407306] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5933.407852] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5933.527860] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5938.412093] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=267 bytes=17498112 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5941.037926] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 4: 1080i: output size change while capturing
$ sh -c (sleep 5; v4l2-ctl -d /dev/video0 -v width=1280,height=720,pixelformat=MPEG) & sleep 0; true
$ comp_cap
width=1280 height=720 sample_aspect_ratio=1:1 level=31 color_primaries=bt709 field_order=progressive r_frame_rate=30000/1001 width=1280 height=720 sample_aspect_ratio=1:1 level=31 color_primaries=bt709 field_order=progressive r_frame_rate=30000/1001  ~10728 kbit/s errors=1
$ sh -c sudo dmesg | tail -80
[ 5947.370773] elgato_gchd 1-2:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.372002] elgato_gchd 1-2:1.0: firmware chunk offset=753664 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.373230] elgato_gchd 1-2:1.0: firmware chunk offset=786432 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.374474] elgato_gchd 1-2:1.0: firmware chunk offset=819200 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.375612] elgato_gchd 1-2:1.0: firmware chunk offset=851968 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.376710] elgato_gchd 1-2:1.0: firmware chunk offset=884736 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.377902] elgato_gchd 1-2:1.0: firmware chunk offset=917504 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.379011] elgato_gchd 1-2:1.0: firmware chunk offset=950272 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.380130] elgato_gchd 1-2:1.0: firmware chunk offset=983040 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.381456] elgato_gchd 1-2:1.0: firmware chunk offset=1015808 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.382754] elgato_gchd 1-2:1.0: firmware chunk offset=1048576 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.383908] elgato_gchd 1-2:1.0: firmware chunk offset=1081344 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.385212] elgato_gchd 1-2:1.0: firmware chunk offset=1114112 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.386499] elgato_gchd 1-2:1.0: firmware chunk offset=1146880 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.387669] elgato_gchd 1-2:1.0: firmware chunk offset=1179648 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.388818] elgato_gchd 1-2:1.0: firmware chunk offset=1212416 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.390137] elgato_gchd 1-2:1.0: firmware chunk offset=1245184 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.391420] elgato_gchd 1-2:1.0: firmware chunk offset=1277952 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.392665] elgato_gchd 1-2:1.0: firmware chunk offset=1310720 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.393990] elgato_gchd 1-2:1.0: firmware chunk offset=1343488 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.395409] elgato_gchd 1-2:1.0: firmware chunk offset=1376256 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.396743] elgato_gchd 1-2:1.0: firmware chunk offset=1409024 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.398067] elgato_gchd 1-2:1.0: firmware chunk offset=1441792 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.399463] elgato_gchd 1-2:1.0: firmware chunk offset=1474560 size=26224 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5947.404478] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5947.413907] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03d
[ 5947.413914] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[ 5947.413916] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5947.413918] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5948.199648] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5948.199868] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 5948.665816] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03c
[ 5948.665828] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[ 5948.665838] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5948.665842] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5949.838007] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5949.853606] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5949.853725] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 5949.853948] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5949.855427] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5949.855942] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5949.856773] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5949.856905] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[ 5949.857193] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[ 5949.859521] elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x03d2 ack=1
[ 5949.860273] elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[ 5949.860593] elgato_gchd 1-2:1.0: START poll=1 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[ 5949.860923] elgato_gchd 1-2:1.0: START poll=2 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[ 5949.861232] elgato_gchd 1-2:1.0: START poll=3 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[ 5949.861519] elgato_gchd 1-2:1.0: START poll=4 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[ 5949.861811] elgato_gchd 1-2:1.0: START poll=5 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[ 5949.862150] elgato_gchd 1-2:1.0: START poll=6 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[ 5949.862480] elgato_gchd 1-2:1.0: START poll=7 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[ 5949.862829] elgato_gchd 1-2:1.0: START poll=8 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[ 5949.863284] elgato_gchd 1-2:1.0: START poll=9 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[ 5949.863574] elgato_gchd 1-2:1.0: START poll=10 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[ 5949.863856] elgato_gchd 1-2:1.0: START poll=11 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[ 5949.864188] elgato_gchd 1-2:1.0: START poll=12 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[ 5949.864513] elgato_gchd 1-2:1.0: START poll=13 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[ 5949.864816] elgato_gchd 1-2:1.0: START poll=14 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[ 5949.865115] elgato_gchd 1-2:1.0: START poll=15 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[ 5949.890205] elgato_gchd 1-2:1.0: START poll=100 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[ 5949.924388] elgato_gchd 1-2:1.0: START poll=200 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[ 5949.960217] elgato_gchd 1-2:1.0: START poll=300 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[ 5949.975776] elgato_gchd 1-2:1.0: START poll=345 state=0x0002 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x03d2 ack=1
[ 5949.976242] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5950.358432] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[ 5950.390997] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[ 5950.428102] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[ 5950.461434] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[ 5950.495783] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[ 5954.874207] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=137 bytes=8978432 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5957.161686] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[ 5957.162485] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[ 5957.837082] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[ 5957.837860] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[ 5957.884179] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 5957.885543] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[ 5957.885871] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
[ 5957.905058] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[FAIL] encoder restarted for 1280x720 while capturing (expected /component output now 3/, got: [ 5947.370773] elgato_gchd 1-2:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0 [ 5947.372002] elgato_gchd 1-2:1.0: firmware)
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
--- dmesg step 4
[ 5947.164130] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5947.342798] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5947.404478] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5947.413907] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03d
[ 5947.413914] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[ 5947.413916] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5947.413918] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5948.199648] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5948.665816] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03c
[ 5948.665828] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[ 5948.665838] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5948.665842] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5949.838007] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5949.853606] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5949.853948] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5949.855427] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5949.855942] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5949.856773] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5949.976242] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5954.874207] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=137 bytes=8978432 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5957.905058] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 5: component source 720p
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
PROMPT: A window 'Elgato component' shows the PS3. Set the PS3's video output to 720p. Untick 1080i. If the picture freezes or disappears, close the window (it reopens). Press Enter when it shows the PS3 in 720p.
$ v4l2-ctl -d /dev/video0 --list-framesizes MPEG
ioctl: VIDIOC_ENUM_FRAMESIZES
	Size: Discrete 1280x720
	Size: Discrete 640x480
	Size: Discrete 480x360
$ sh -c v4l2-ctl -d /dev/video0 --list-framesizes MPEG | grep -c 1280x720
1
[PASS] 720p: 1280x720 not offered as a scaled size
$ v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=640 height=480 sample_aspect_ratio=4:3 level=30 color_primaries=bt709 field_order=progressive r_frame_rate=30000/1001 width=640 height=480 sample_aspect_ratio=4:3 level=30 color_primaries=bt709 field_order=progressive r_frame_rate=30000/1001  ~9053 kbit/s errors=1
[PASS] 720p -> 640x480: stream 640x480 progressive, level 30
$ v4l2-ctl -d /dev/video0 -v width=480,height=360,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=480 height=360 sample_aspect_ratio=4:3 level=22 color_primaries=bt709 field_order=progressive r_frame_rate=30000/1001 width=480 height=360 sample_aspect_ratio=4:3 level=22 color_primaries=bt709 field_order=progressive r_frame_rate=30000/1001  ~9315 kbit/s errors=0
[PASS] 720p -> 480x360: stream 480x360 progressive, level 22
$ v4l2-ctl -d /dev/video0 -c allow_50_60_fps=0
$ v4l2-ctl -d /dev/video0 -v width=1280,height=720,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=1280 height=720 sample_aspect_ratio=1:1 level=31 color_primaries=bt709 field_order=progressive r_frame_rate=60000/1001 width=1280 height=720 sample_aspect_ratio=1:1 level=31 color_primaries=bt709 field_order=progressive r_frame_rate=60000/1001  ~9627 kbit/s errors=1
[PASS] 720p, Allow 50/60 fps off: stream 1280x720 progressive, level 31
$ v4l2-ctl -d /dev/video0 -c allow_50_60_fps=1
$ v4l2-ctl -d /dev/video0 -v width=1280,height=720,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=1280 height=720 sample_aspect_ratio=1:1 level=40 color_primaries=bt709 field_order=progressive r_frame_rate=60000/1001 width=1280 height=720 sample_aspect_ratio=1:1 level=40 color_primaries=bt709 field_order=progressive r_frame_rate=60000/1001  ~19051 kbit/s errors=1
[PASS] 720p, Allow 50/60 fps on: stream 1280x720 progressive, level 40
--- dmesg step 5
[ 5958.629507] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5958.744185] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5958.744187] elgato_gchd 1-2:1.0: waiting 661 ms after the previous shutdown
[ 5959.551921] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5959.617568] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5959.627172] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03c
[ 5959.627176] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[ 5959.627178] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5959.627180] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5960.422103] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5961.553048] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5961.570122] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5961.570492] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5961.571665] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5961.572518] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[ 5961.573013] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5961.692760] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5966.577320] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=267 bytes=17498112 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5971.574531] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5976.589948] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=862 bytes=56492032 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5981.601630] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1160 bytes=76021760 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5986.598928] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1457 bytes=95485952 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5991.612252] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1755 bytes=115015680 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5996.609209] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2052 bytes=134479872 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6001.619735] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2350 bytes=154009600 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6004.708919] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 6005.736641] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 6005.852646] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 6005.852648] elgato_gchd 1-2:1.0: waiting 357 ms after the previous shutdown
[ 6006.357202] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 6006.417991] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 6006.426365] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb74 raw6867=0xa6b7
[ 6006.426367] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 6006.426369] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 6006.426371] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 6007.186543] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 6008.198099] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 6008.213504] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 6008.213702] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 6008.215021] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 6008.215757] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 6008.216365] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 6008.299346] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 6013.234682] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=36
[ 6018.236301] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=291 bytes=19070976 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=35
[ 6023.245964] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=442 bytes=28966912 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=35
[ 6028.292678] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=587 bytes=38469632 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=48
[ 6028.371385] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 6029.396731] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 6029.514875] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 6029.514877] elgato_gchd 1-2:1.0: waiting 357 ms after the previous shutdown
[ 6030.022144] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 6030.086499] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 6030.095639] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba9a raw6867=0xbf59
[ 6030.095641] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[ 6030.095649] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 6030.095650] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 6030.892666] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 6031.329710] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbf59
[ 6031.329715] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[ 6031.329718] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 6031.329719] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 6032.342084] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 6032.357548] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 6032.357775] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 6032.359464] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 6032.360199] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 6032.360730] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 6032.413584] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 6037.357848] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=242 bytes=15859712 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=4
[ 6042.363433] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=503 bytes=32964608 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=4
[ 6046.127911] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 6049.156823] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 6049.160670] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 6049.318889] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 6049.382841] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 6049.391723] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbf59
[ 6049.391726] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[ 6049.391728] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 6049.391730] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 6050.162321] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 6051.182377] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 6051.199226] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 6051.199646] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 6051.201269] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 6051.201964] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 6051.202595] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 6051.271969] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 6051.611097] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[ 6056.205148] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=106 bytes=6946816 ep84_bytes=6946816 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6059.928091] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 6060.323109] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 6060.329156] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 6060.329159] elgato_gchd 1-2:1.0: waiting 1099 ms after the previous shutdown
[ 6061.552802] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 6061.628166] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 6061.640136] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbf5e
[ 6061.640142] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[ 6061.640146] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 6061.640149] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 6062.441939] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 6062.898737] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbf58
[ 6062.898742] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[ 6062.898745] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 6062.898746] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 6064.024902] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 6064.042867] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 6064.043259] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 6064.044404] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 6064.044919] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 6064.045426] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 6064.115746] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 6064.437709] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[ 6069.047509] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=9175040 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6071.123927] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 6071.381031] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 6071.385195] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 6071.385200] elgato_gchd 1-2:1.0: waiting 1240 ms after the previous shutdown
[ 6072.942983] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 6073.004670] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 6073.013055] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbf58
[ 6073.013058] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[ 6073.013059] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 6073.013061] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 6073.789590] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 6074.881474] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 6074.907105] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 6074.907322] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 6074.908370] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 6074.908957] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 6074.909498] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 6074.979751] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 6079.917040] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6082.077676] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 6082.944777] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 6082.948175] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 6082.948178] elgato_gchd 1-2:1.0: waiting 629 ms after the previous shutdown
[ 6083.722002] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 6083.783984] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 6083.791613] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba9a raw6867=0xbf59
[ 6083.791614] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[ 6083.791616] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 6083.791618] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 6084.539358] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 6085.556763] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 6085.571576] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 6085.571780] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 6085.572979] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 6085.573540] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 6085.573928] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 6085.644082] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 6090.569323] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=241 bytes=15794176 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6093.743861] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 6: component source 576p
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
PROMPT: A window 'Elgato component' shows the PS3. Set the PS3's video output to 576p. Untick 720p. If the picture freezes or disappears, close the window (it reopens). Press Enter when it shows the PS3 in 576p.
$ v4l2-ctl -d /dev/video0 --list-framesizes MPEG
ioctl: VIDIOC_ENUM_FRAMESIZES
	Size: Discrete 720x576
$ v4l2-ctl -d /dev/video0 -c allow_50_60_fps=0
$ v4l2-ctl -d /dev/video0 -v width=720,height=576,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=31 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=31 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~11825 kbit/s errors=1
[PASS] 576p, Allow 50/60 fps off: stream 720x576 progressive, level 31
$ v4l2-ctl -d /dev/video0 -c allow_50_60_fps=1
$ v4l2-ctl -d /dev/video0 -v width=720,height=576,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~10780 kbit/s errors=1
[PASS] 576p, Allow 50/60 fps on: stream 720x576 progressive, level 30
$ v4l2-ctl -d /dev/video0 -v width=720,height=576,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~10728 kbit/s errors=1
[PASS] 576p asked for 640x480: source size: stream 720x576 progressive, level 30
--- dmesg step 6
[ 6094.883054] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 6095.017636] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 6095.017638] elgato_gchd 1-2:1.0: waiting 225 ms after the previous shutdown
[ 6095.388438] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 6095.447347] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 6095.455887] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbf58
[ 6095.455891] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[ 6095.455893] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 6095.455894] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 6096.228634] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 6097.238346] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 6097.253744] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 6097.253948] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 6097.255368] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 6097.256103] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1280x720 fps=60/1 interlaced=0
[ 6097.256555] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 6097.326241] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 6102.269270] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=242 bytes=15859712 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=4
[ 6107.275465] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=503 bytes=32964608 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=4
[ 6112.281608] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=764 bytes=50069504 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=4
[ 6117.287204] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1025 bytes=67174400 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=4
[ 6122.293460] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1286 bytes=84279296 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=6
[ 6127.299513] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1547 bytes=101384192 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=6
[ 6132.305774] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1808 bytes=118489088 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=5
[ 6137.312049] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2069 bytes=135593984 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=8
[ 6142.317911] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2330 bytes=152698880 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=5
[ 6147.323459] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2591 bytes=169803776 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=7
[ 6152.329916] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2852 bytes=186908672 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=4
[ 6157.335170] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=3113 bytes=204013568 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=6
[ 6162.341733] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=3374 bytes=221118464 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=3
[ 6167.347223] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=3635 bytes=238223360 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=5
[ 6172.353471] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=3896 bytes=255328256 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=5
[ 6177.359361] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=4157 bytes=272433152 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=6
[ 6182.367105] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=4418 bytes=289538048 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=7
[ 6187.371404] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=4679 bytes=306642944 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=5
[ 6192.378053] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=4940 bytes=323747840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=6
[ 6197.384927] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=5201 bytes=340852736 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=5
[ 6202.391230] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=5462 bytes=357957632 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=6
[ 6207.397442] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=5723 bytes=375062528 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=6
[ 6212.394940] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=5960 bytes=390594560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=28
[ 6216.827406] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 6217.856667] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 6217.970190] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 6217.970192] elgato_gchd 1-2:1.0: waiting 366 ms after the previous shutdown
[ 6218.474352] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 6218.539381] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 6218.549724] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b7
[ 6218.549727] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 6218.549729] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 6218.549731] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 6219.350762] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 6220.433546] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 6220.449180] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 6220.449415] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 6220.450811] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 6220.451406] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 6220.451875] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 6220.535195] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 6225.470088] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=36
[ 6230.471981] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=291 bytes=19070976 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=35
[ 6235.473086] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=442 bytes=28966912 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=35
[ 6240.558077] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 6243.579556] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 6243.583308] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 6243.782014] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 6243.848208] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 6243.859292] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb74 raw6867=0xa6b7
[ 6243.859296] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 6243.859298] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 6243.859300] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 6244.654542] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 6245.687550] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 6245.709338] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 6245.709723] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 6245.711481] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 6245.712449] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 6245.712949] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 6245.795373] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 6250.730113] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6254.434344] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 6255.156778] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 6255.161663] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 6255.161666] elgato_gchd 1-2:1.0: waiting 773 ms after the previous shutdown
[ 6256.064121] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 6256.125723] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 6256.133889] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb74 raw6867=0xa6b7
[ 6256.133892] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 6256.133894] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 6256.133895] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 6256.891799] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 6257.933509] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 6257.951650] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 6257.951861] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 6257.953612] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 6257.954209] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 6257.954621] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 6258.017844] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 6262.953043] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6266.068850] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 6266.752781] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 6266.757331] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 6266.757334] elgato_gchd 1-2:1.0: waiting 812 ms after the previous shutdown
[ 6267.720802] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 6267.780384] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 6267.789324] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b4
[ 6267.789329] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 6267.789331] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 6267.789333] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 6268.549995] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 6269.592309] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 6269.607063] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 6269.607388] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 6269.608766] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 6269.609382] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 6269.609780] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 6269.692871] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 6274.629379] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6277.648690] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 7: component source 576i
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
PROMPT: A window 'Elgato component' shows the PS3. Set the PS3's video output to 576i. Untick 576p. If the picture freezes or disappears, close the window (it reopens). Press Enter when it shows the PS3 in 576i.
$ v4l2-ctl -d /dev/video0 --list-framesizes MPEG
ioctl: VIDIOC_ENUM_FRAMESIZES
	Size: Discrete 720x576
	Size: Discrete 640x480
$ v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=640 height=480 sample_aspect_ratio=1:1 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=25/1 width=640 height=480 sample_aspect_ratio=1:1 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=25/1  ~8372 kbit/s errors=1
[PASS] 576i -> 640x480: stream 640x480 progressive, level 30
$ v4l2-ctl -d /dev/video0 -v width=720,height=576,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=31 color_primaries=smpte170m field_order=tt r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=31 color_primaries=smpte170m field_order=tt r_frame_rate=50/1  ~6697 kbit/s errors=1
[PASS] 576i source size: stream 720x576 tt, level 31
--- dmesg step 7
[ 6278.382411] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 6278.500776] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 6278.500778] elgato_gchd 1-2:1.0: waiting 648 ms after the previous shutdown
[ 6279.303318] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 6279.363567] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 6279.372630] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b7
[ 6279.372634] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[ 6279.372636] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[ 6279.372638] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[ 6280.157981] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 6281.241478] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 6281.257483] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 6281.257717] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 6281.259016] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 6281.259881] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[ 6281.260354] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 6281.343547] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 6286.277751] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=37
[ 6291.278802] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=291 bytes=19070976 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=35
[ 6296.280085] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=442 bytes=28966912 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=35
[ 6301.283973] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=593 bytes=38862848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=35
[ 6306.284705] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=744 bytes=48758784 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=36
[ 6310.513386] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 6311.546543] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 6311.664000] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 6311.664002] elgato_gchd 1-2:1.0: waiting 349 ms after the previous shutdown
[ 6312.146692] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 6312.208246] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 6312.216996] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb74 raw6867=0x9ab9
[ 6312.217007] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[ 6312.217009] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=1
[ 6312.217011] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=1
[ 6313.017123] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 6314.143523] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 6314.166002] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 6314.166315] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 6314.167258] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 6314.167943] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=1
[ 6314.168570] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 6314.292078] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 6319.163630] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=21
[ 6324.200207] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 6327.228216] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 6330.251001] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 6330.254905] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 6330.421734] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 6330.481415] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 6330.490006] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0x9abe
[ 6330.490010] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[ 6330.490013] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=1
[ 6330.490014] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=1
[ 6331.253373] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 6331.698042] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb74 raw6867=0x9abe
[ 6331.698070] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[ 6331.698083] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=1
[ 6331.698092] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=1
[ 6332.730366] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 6332.746685] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 6332.746992] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 6332.748689] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 6332.749476] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=1
[ 6332.749880] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 6332.892353] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 6333.285733] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[ 6337.761997] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6341.078112] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 6341.502255] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 6341.507397] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 6341.507399] elgato_gchd 1-2:1.0: waiting 1070 ms after the previous shutdown
[ 6342.903128] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 6342.965669] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 6342.975481] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb74 raw6867=0x9abe
[ 6342.975483] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[ 6342.975485] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=1
[ 6342.975487] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=1
[ 6343.771253] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 6344.254192] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb74 raw6867=0x9abe
[ 6344.254202] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[ 6344.254206] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=1
[ 6344.254210] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=1
[ 6345.383770] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 6345.402217] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 6345.402597] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 6345.404034] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 6345.405044] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=1
[ 6345.405629] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 6345.528198] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 6350.400274] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6352.354664] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 8: v4l2-compliance on component (last: it leaves its own control values)
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
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
--- dmesg step 8
[ 6352.731026] elgato_gchd 1-2:1.0: input switch requested: old=1 new=0 hw_initialized=0 streaming=0
[ 6352.731056] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 6352.731058] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 6352.731065] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 6352.731067] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 6352.731074] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 6352.731077] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 6352.731084] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 6352.731086] elgato_gchd 1-2:1.0: input switch requested: old=3 new=1 hw_initialized=0 streaming=0
[ 6352.731092] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 6352.731094] elgato_gchd 1-2:1.0: input switch requested: old=1 new=0 hw_initialized=0 streaming=0
[ 6352.731100] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 6352.731102] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 6352.731109] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 6352.731111] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 6352.731117] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 6352.731119] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 6352.731125] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 6352.731135] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 6352.731142] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 6352.731145] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 6352.731152] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 6352.731154] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 6352.731160] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 6352.731165] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 6352.731172] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 6352.731177] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 6352.731184] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 6352.731196] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 6352.731202] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 6352.731214] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 6352.731221] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 6352.731223] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 6352.731230] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 6352.731233] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 6352.731239] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 6352.731241] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 6352.731247] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 6352.731249] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 6352.731255] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 6352.731257] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 6352.731263] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 6352.731265] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 6352.731272] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 6352.731278] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 6352.731285] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 6352.731287] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 6352.731293] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 6352.731296] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 6352.731302] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 6352.731304] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 6352.731311] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 6352.735061] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 6352.735070] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 6352.737680] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 6352.737725] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 6352.740635] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 6352.740645] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 6352.741541] elgato_gchd 1-2:1.0: input switch requested: old=3 new=1 hw_initialized=0 streaming=0
[ 6352.741548] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 6352.741568] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 6352.741573] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 6352.749510] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 6352.749531] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== summary (T-051, 05173eb Component: no duplicate size in ENUM_FRAMESIZES before a detection; TASK: T-051 fifth run (test file broken, PS3 stayed in 576p))
PASS | step 2 component source 1080p | 1080p: four output sizes offered
PASS | step 2 component source 1080p | 1080p -> 1280x720: stream 1280x720 progressive, level 31
PASS | step 2 component source 1080p | 1080p -> 640x480: stream 640x480 progressive, level 30
PASS | step 2 component source 1080p | 1080p -> 480x360: stream 480x360 progressive, level 22
PASS | step 3 component source 1080i | 1080i -> 1280x720: stream 1280x720 progressive, level 31
PASS | step 3 component source 1080i | 1080i -> 640x480: stream 640x480 progressive, level 30
PASS | step 3 component source 1080i | 1080i -> 480x360: stream 480x360 progressive, level 22
PASS | step 3 component source 1080i | 1080i source size: stream 1920x1080 tt, level 40
FAIL | step 4 1080i: output size change while capturing | encoder restarted for 1280x720 while capturing (expected /component output now 3/, got: [ 5947.370773] elgato_gchd 1-2:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0 [ 5947.372002] elgato_gchd 1-2:1.0: firmware)
PASS | step 5 component source 720p | 720p: 1280x720 not offered as a scaled size
PASS | step 5 component source 720p | 720p -> 640x480: stream 640x480 progressive, level 30
PASS | step 5 component source 720p | 720p -> 480x360: stream 480x360 progressive, level 22
PASS | step 5 component source 720p | 720p, Allow 50/60 fps off: stream 1280x720 progressive, level 31
PASS | step 5 component source 720p | 720p, Allow 50/60 fps on: stream 1280x720 progressive, level 40
PASS | step 6 component source 576p | 576p, Allow 50/60 fps off: stream 720x576 progressive, level 31
PASS | step 6 component source 576p | 576p, Allow 50/60 fps on: stream 720x576 progressive, level 30
PASS | step 6 component source 576p | 576p asked for 640x480: source size: stream 720x576 progressive, level 30
PASS | step 7 component source 576i | 576i -> 640x480: stream 640x480 progressive, level 30
PASS | step 7 component source 576i | 576i source size: stream 720x576 tt, level 31
PASS | step 8 v4l2-compliance on component (last: it leaves its own control values) | v4l2-compliance on component: no failures
