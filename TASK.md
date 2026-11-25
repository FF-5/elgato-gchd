# TASK T-051 — Component input, all modes

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** recording analysed; encoder values per mode in the driver;
  hardware test T-051 (first step) open
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

1. Hardware test of the encoder values (below).
2. Front end and back end per mode: compare the rest of the recorded
   sequence (front-end colour tables, back end `98`, bridge `0900:0018`)
   with the driver's component path.
3. Output sizes (1280 × 720, 640 × 480) and the 50/60 fps option for
   HD inputs: V4L2 interface to be decided (output sizes like composite;
   the option as a control).
4. Mode changes while capturing (the Windows software restarts with the
   new mode's values, sequence #17/#18).

Wanted recordings: 480i and 480p (maintainer: after the tests with the
PAL PS3).

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

## Maintainer comments

The only problem I saw while doing the test is that the loaded module could not be unloaded. If you feel that most of the things are right and whenever you need me to get traces and test 480i/480p on component let me know. Also make sure to remove all tests that have already passed so that we only test the things that we haven't tested.

gchd-test T-051 20261126-000206 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-051-20261126-000206.log
== build
$ git fetch -q origin
$ git checkout -q feat/component
$ git pull -q --ff-only
commit: ae9c635 Component: system_rate 60 Hz constant and minimum bitrate as in the single-option recordings; module version from dkms.conf; docs/component.md §4 extended (T-051)
commit: ae9c635 Component: system_rate 60 Hz constant and minimum bitrate as in the single-option recordings; module version from dkms.conf; docs/component.md §4 extended (T-051)
build rc=0 warnings=0

=== step 1: load module (fresh plug-in)
PROMPT: Unplug the Elgato from the PC (or detach it in VirtualBox), wait 3 s, plug it in again.
$ sudo insmod /home/ff5/elgato-gchd/src/driver/elgato_gchd.ko
insmod: ERROR: could not insert module /home/ff5/elgato-gchd/src/driver/elgato_gchd.ko: File exists
[FAIL] insmod 
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -v width=720,height=480,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
--- dmesg step 1
[  572.267740] workqueue: hub_event hogged CPU for >10000us 4 times, consider switching to WQ_UNBOUND
[  572.408612] usb 1-2: new high-speed USB device number 6 using xhci_hcd
[  572.784957] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[  572.784972] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[  572.784976] usb 1-2: Product: GameCapture HD
[  572.784979] usb 1-2: Manufacturer: Elgato
[  572.784982] usb 1-2: SerialNumber: 04107F1267
[  572.811088] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[  572.813443] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[  572.813461] usbcore: registered new interface driver elgato_gchd
[  572.974739] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[  572.974773] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[  572.979903] elgato_gchd 1-2:1.0: input switch requested: old=2 new=1 hw_initialized=0 streaming=0
[  572.979926] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open

=== step 2: component 576i
PROMPT: A window 'Elgato component' shows the PS3. In the PS3's menu set its video output to 576i. If the picture freezes or disappears after the change, close the window: it reopens with a fresh detection. Press Enter when the window shows the PS3 in 576i.
[PASS] 576i: is the picture right (no colour cast, sharp, motion smooth)?
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=31 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=31 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~7900 kbit/s errors=1
[FAIL] 576i: stream 720x576, field order tt (expected /width=720 height=576 .*field_order=tt/, got: width=720 height=576 sample_aspect_ratio=12:11 level=31 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
$ sh -c sudo dmesg | grep 'signal detected: input=1' | tail -1
[  597.077788] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
--- dmesg step 2
[  573.009923] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[  573.127918] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  573.343036] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  573.408466] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  573.423014] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb74 raw6867=0xa6b7
[  573.423025] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[  573.423081] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[  573.423086] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[  574.224801] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  576.194760] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  576.211813] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  576.212049] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  576.213321] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  576.213927] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[  576.214654] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x020e
[  576.277548] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  581.252133] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=107 bytes=7012352 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=25
[  586.278512] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=222 bytes=14548992 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=47
[  591.303848] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=337 bytes=22085632 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[  593.816076] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[  596.831165] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[  596.838688] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  597.003958] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  597.066471] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  597.077780] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b4
[  597.077785] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[  597.077788] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[  597.077790] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[  597.887903] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  599.819082] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  599.836175] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  599.836528] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  599.837660] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  599.838313] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[  599.838938] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[  599.922702] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  604.854735] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=106 bytes=6946816 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  607.700561] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 3: component 576p
PROMPT: A window 'Elgato component' shows the PS3. In the PS3's menu set its video output to 576p. If the picture freezes or disappears after the change, close the window: it reopens with a fresh detection. Press Enter when the window shows the PS3 in 576p.
[PASS] 576p: is the picture right (no colour cast, sharp, motion smooth)?
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=31 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=31 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~7847 kbit/s errors=1
[PASS] 576p: stream 720x576, field order progressive
$ sh -c sudo dmesg | grep 'signal detected: input=1' | tail -1
[  681.486738] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
--- dmesg step 3
[  608.272124] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[  608.381690] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  608.381691] elgato_gchd 1-2:1.0: waiting 818 ms after the previous shutdown
[  609.353534] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  609.416040] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  609.424863] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b7
[  609.424866] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[  609.424868] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[  609.424870] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[  610.201831] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  612.095061] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  612.111994] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  612.112223] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  612.113590] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  612.114180] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[  612.114747] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[  612.198733] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  617.129442] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=106 bytes=6946816 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=24
[  622.154117] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=221 bytes=14483456 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  627.177918] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=336 bytes=22020096 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  632.201075] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=451 bytes=29556736 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  637.226390] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=566 bytes=37093376 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  642.250447] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=681 bytes=44630016 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  647.275652] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=796 bytes=52166656 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[  652.300562] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=911 bytes=59703296 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  657.325225] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1026 bytes=67239936 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=44
[  662.351578] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1141 bytes=74776576 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=44
[  667.374247] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1256 bytes=82313216 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  672.398040] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1371 bytes=89849856 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=44
[  677.423216] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1486 bytes=97386496 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  678.205654] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[  681.217024] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[  681.224502] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  681.415631] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  681.477449] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  681.486732] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b4
[  681.486736] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[  681.486738] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[  681.486740] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[  682.273663] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  684.257713] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  684.277214] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  684.277599] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  684.279061] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  684.279920] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[  684.280452] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[  684.363099] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  689.294078] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=106 bytes=6946816 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  691.987437] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 4: component 720p
PROMPT: A window 'Elgato component' shows the PS3. In the PS3's menu set its video output to 720p. If the picture freezes or disappears after the change, close the window: it reopens with a fresh detection. Press Enter when the window shows the PS3 in 720p.
[PASS] 720p: is the picture right (no colour cast, sharp, motion smooth)?
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=1280 height=720 sample_aspect_ratio=1:1 level=32 color_primaries=smpte170m field_order=progressive r_frame_rate=60000/1001 width=1280 height=720 sample_aspect_ratio=1:1 level=32 color_primaries=smpte170m field_order=progressive r_frame_rate=60000/1001  ~13136 kbit/s errors=1
[PASS] 720p: stream 1280x720, field order progressive
$ sh -c sudo dmesg | grep 'signal detected: input=1' | tail -1
[  779.624711] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1280x720 fps=60/1 interlaced=0
--- dmesg step 4
[  692.541205] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[  692.662967] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  692.662969] elgato_gchd 1-2:1.0: waiting 825 ms after the previous shutdown
[  693.638181] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  693.703658] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  693.719930] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b7
[  693.719932] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[  693.719934] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[  693.719936] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[  694.510436] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  696.461296] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  696.476910] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  696.477120] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  696.478389] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  696.479159] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[  696.479640] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[  696.563127] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  701.495246] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=106 bytes=6946816 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=25
[  706.519355] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=221 bytes=14483456 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  711.543858] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=336 bytes=22020096 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  716.566111] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=451 bytes=29556736 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  721.592177] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=566 bytes=37093376 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[  726.617463] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=681 bytes=44630016 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[  731.643543] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=796 bytes=52166656 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[  736.682027] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=911 bytes=59703296 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=44
[  741.720803] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1026 bytes=67239936 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  746.758959] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1141 bytes=74776576 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=44
[  751.796781] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1256 bytes=82313216 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  756.645152] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[  757.667281] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[  757.780728] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  757.780729] elgato_gchd 1-2:1.0: waiting 365 ms after the previous shutdown
[  758.300368] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  758.383279] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  758.402119] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbf59
[  758.402124] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[  758.402127] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1280x720 fps=60/1 interlaced=0
[  758.402130] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1280x720 fps=60/1 interlaced=0
[  759.191883] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  761.106971] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  761.122495] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  761.122805] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  761.123953] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  761.124663] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1280x720 fps=60/1 interlaced=0
[  761.125151] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[  761.195195] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  766.127669] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=185 bytes=12124160 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=21
[  771.133116] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=385 bytes=25231360 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=21
[  775.147773] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[  778.160969] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[  778.174007] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  778.348907] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  778.408797] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  778.417810] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbf59
[  778.417812] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[  778.417813] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1280x720 fps=60/1 interlaced=0
[  778.417815] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1280x720 fps=60/1 interlaced=0
[  779.208081] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  779.624697] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbf58
[  779.624707] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[  779.624711] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1280x720 fps=60/1 interlaced=0
[  779.624715] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1280x720 fps=60/1 interlaced=0
[  781.492176] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  781.507340] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  781.507555] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  781.508630] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  781.509162] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1280x720 fps=60/1 interlaced=0
[  781.509649] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[  781.579988] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  786.512782] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=185 bytes=12124160 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  788.960897] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 5: component 1080i
PROMPT: A window 'Elgato component' shows the PS3. In the PS3's menu set its video output to 1080i. If the picture freezes or disappears after the change, close the window: it reopens with a fresh detection. Press Enter when the window shows the PS3 in 1080i.
[PASS] 1080i: is the picture right (no colour cast, sharp, motion smooth)?
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=1920 height=1080 sample_aspect_ratio=1:1 level=42 color_primaries=smpte170m field_order=tt r_frame_rate=60000/1001 width=1920 height=1080 sample_aspect_ratio=1:1 level=42 color_primaries=smpte170m field_order=tt r_frame_rate=60000/1001  ~19519 kbit/s errors=1
[PASS] 1080i: stream 1920x1080, field order tt
$ sh -c sudo dmesg | grep 'signal detected: input=1' | tail -1
[  886.868195] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
--- dmesg step 5
[  789.900781] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[  790.013799] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  790.013800] elgato_gchd 1-2:1.0: waiting 447 ms after the previous shutdown
[  790.607257] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  790.667991] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  790.676945] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbf58
[  790.676948] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[  790.676950] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1280x720 fps=60/1 interlaced=0
[  790.676952] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1280x720 fps=60/1 interlaced=0
[  791.430943] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  791.840656] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbf59
[  791.840663] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[  791.840665] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1280x720 fps=60/1 interlaced=0
[  791.840667] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1280x720 fps=60/1 interlaced=0
[  793.655508] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  793.670666] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  793.670880] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  793.671808] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  793.672407] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1280x720 fps=60/1 interlaced=0
[  793.672892] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[  793.743178] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  798.673631] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=185 bytes=12124160 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=21
[  803.684766] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=385 bytes=25231360 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=21
[  808.695787] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=585 bytes=38338560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=21
[  813.706598] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=785 bytes=51445760 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=21
[  815.520085] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[  816.544828] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[  816.660551] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  816.660552] elgato_gchd 1-2:1.0: waiting 359 ms after the previous shutdown
[  817.177871] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  817.238865] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  817.249382] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b7
[  817.249385] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[  817.249387] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[  817.249388] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[  818.013920] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  819.798955] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  819.814190] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  819.814428] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  819.816238] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  819.817094] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[  819.817700] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[  819.880251] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  824.811499] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=106 bytes=6946816 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=24
[  829.836792] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=221 bytes=14483456 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[  834.861101] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=336 bytes=22020096 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  839.886232] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=451 bytes=29556736 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[  844.924276] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=566 bytes=37093376 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[  849.962659] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=681 bytes=44630016 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=47
[  855.001494] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=796 bytes=52166656 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[  860.034908] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=904 bytes=59244544 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=56
[  860.095770] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[  861.117595] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[  861.242153] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  861.242155] elgato_gchd 1-2:1.0: waiting 353 ms after the previous shutdown
[  861.740977] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  861.798584] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  861.808279] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03d
[  861.808282] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[  861.808284] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[  861.808287] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[  862.560745] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  862.965636] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03c
[  862.965642] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[  862.965645] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[  862.965646] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[  864.751847] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  864.766533] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  864.766748] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  864.768688] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  864.769435] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[  864.769861] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[  864.889195] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  869.768686] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=267 bytes=17498112 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  874.767436] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  879.780280] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=862 bytes=56492032 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  882.427541] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[  885.437126] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[  885.444367] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  885.622169] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  885.681559] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  885.691770] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba9a raw6867=0xa03c
[  885.691774] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[  885.691776] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[  885.691779] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[  886.450465] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  886.868176] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03c
[  886.868188] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[  886.868195] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[  886.868199] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[  888.649849] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  888.664289] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  888.664599] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  888.666299] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  888.667119] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[  888.667505] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[  888.770345] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  893.672170] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=268 bytes=17563648 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  896.288238] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 6: component 1080p
PROMPT: A window 'Elgato component' shows the PS3. In the PS3's menu set its video output to 1080p. If the picture freezes or disappears after the change, close the window: it reopens with a fresh detection. Press Enter when the window shows the PS3 in 1080p.
[PASS] 1080p: is the picture right (no colour cast, sharp, motion smooth)?
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ comp_cap
width=1920 height=1080 sample_aspect_ratio=1:1 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=30000/1001 width=1920 height=1080 sample_aspect_ratio=1:1 level=41 color_primaries=smpte170m field_order=progressive r_frame_rate=30000/1001  ~18998 kbit/s errors=1
[PASS] 1080p: stream 1920x1080, field order progressive
$ sh -c sudo dmesg | grep 'signal detected: input=1' | tail -1
[  998.395566] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=30/1 interlaced=0
--- dmesg step 6
[  897.372574] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[  897.489551] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  897.489552] elgato_gchd 1-2:1.0: waiting 298 ms after the previous shutdown
[  897.932805] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  897.991240] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  898.000623] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03c
[  898.000627] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[  898.000630] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[  898.000633] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[  898.760534] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  899.185791] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa03d
[  899.185810] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=60/1 interlaced=1 signal_present=1
[  899.185820] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=60/1 interlaced=1
[  899.185827] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=60/1 interlaced=1
[  901.026157] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  901.041773] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  901.041974] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  901.043220] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  901.043710] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=60/1 interlaced=1
[  901.044088] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[  901.147608] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  906.048294] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=268 bytes=17563648 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  911.055876] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=566 bytes=37093376 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=19
[  911.686310] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[  912.711204] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[  912.831405] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  912.831408] elgato_gchd 1-2:1.0: waiting 354 ms after the previous shutdown
[  913.333840] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  913.395471] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  913.404725] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b7
[  913.404732] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[  913.404735] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[  913.404738] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[  914.203428] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  916.110325] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  916.130752] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  916.131050] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  916.132243] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  916.132976] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[  916.133483] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[  916.216753] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  921.147550] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=106 bytes=6946816 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=25
[  926.173076] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=221 bytes=14483456 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  931.198068] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=336 bytes=22020096 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  936.220498] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=451 bytes=29556736 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  941.258323] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=566 bytes=37093376 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  946.296545] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=681 bytes=44630016 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  951.335071] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=796 bytes=52166656 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  956.373622] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=911 bytes=59703296 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=44
[  960.713919] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[  961.740824] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[  961.856138] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  961.856141] elgato_gchd 1-2:1.0: waiting 357 ms after the previous shutdown
[  962.362929] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  962.421749] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  962.431022] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbbf5
[  962.431025] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=30/1 interlaced=0 signal_present=1
[  962.431027] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=30/1 interlaced=0
[  962.431030] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=30/1 interlaced=0
[  963.204799] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  963.627843] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbbf5
[  963.627851] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=30/1 interlaced=0 signal_present=1
[  963.627854] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=30/1 interlaced=0
[  963.627857] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=30/1 interlaced=0
[  965.542936] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  965.560099] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  965.560328] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  965.561288] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  965.561796] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=30/1 interlaced=0
[  965.562393] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[  965.632915] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  970.567212] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=269 bytes=17629184 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  975.568643] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=566 bytes=37093376 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  980.568818] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=863 bytes=56557568 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  985.569002] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1160 bytes=76021760 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  990.569663] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1457 bytes=95485952 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  993.915445] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[  996.924312] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[  996.928024] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  997.098254] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  997.164514] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  997.175355] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbbf5
[  997.175358] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=30/1 interlaced=0 signal_present=1
[  997.175361] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=30/1 interlaced=0
[  997.175363] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=30/1 interlaced=0
[  997.960237] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  998.395547] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xbbf5
[  998.395560] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=1920x1080 fps=30/1 interlaced=0 signal_present=1
[  998.395566] elgato_gchd 1-2:1.0: signal detected: input=1 mode=1920x1080 fps=30/1 interlaced=0
[  998.395570] elgato_gchd 1-2:1.0: capture setup: input=1 mode=1920x1080 fps=30/1 interlaced=0
[ 1000.305233] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 1000.320151] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1000.320479] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1000.322121] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1000.322809] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=1920x1080 fps=30/1 interlaced=0
[ 1000.323278] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 1000.392820] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1005.326403] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=269 bytes=17629184 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1007.791242] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 7: 480i / 480p (if the source offers them)
480 modes available: n
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
--- dmesg step 7
[ 1015.602801] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 1015.602868] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== summary (T-051, ae9c635 Component: system_rate 60 Hz constant and minimum bitrate as in the single-option recordings; module version from dkms.conf; docs/component.md §4 extended (T-051))
FAIL | step 1 load module (fresh plug-in) | insmod 
PASS | step 2 component 576i | 576i: is the picture right (no colour cast, sharp, motion smooth)?
FAIL | step 2 component 576i | 576i: stream 720x576, field order tt (expected /width=720 height=576 .*field_order=tt/, got: width=720 height=576 sample_aspect_ratio=12:11 level=31 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
PASS | step 3 component 576p | 576p: is the picture right (no colour cast, sharp, motion smooth)?
PASS | step 3 component 576p | 576p: stream 720x576, field order progressive
PASS | step 4 component 720p | 720p: is the picture right (no colour cast, sharp, motion smooth)?
PASS | step 4 component 720p | 720p: stream 1280x720, field order progressive
PASS | step 5 component 1080i | 1080i: is the picture right (no colour cast, sharp, motion smooth)?
PASS | step 5 component 1080i | 1080i: stream 1920x1080, field order tt
PASS | step 6 component 1080p | 1080p: is the picture right (no colour cast, sharp, motion smooth)?
PASS | step 6 component 1080p | 1080p: stream 1920x1080, field order progressive
