# TASK T-050 — HDMI input, all modes

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** implemented (driver, settings window, text menu, OBS plugin); emulator: every recorded HDMI start and source change reproduced; hardware test next
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

- Driver: HDMI receiver setup once per selection, per-mode configuration
  and colour conversion as recorded, without the back-end blocks
  (`protocol_common.c` removed); modes 480p, 576p, 720p50/60, 1080i,
  1080p25/30/50/60 (`0x9C:64/63` lines, `66/65` field rate); encoder values
  per output mode as component with the HDMI differences (half rate
  signalled 60/1 or 50/1 on both streams, `v_vinpelclk` 1, `system_rate`
  constants, SD source type 3 and SMPTE 170M); output sizes, "Allow 50/60
  fps" and source mode changes while capturing as component; "HDMI Color
  Range" (`hdmi_range`, rewritten while capturing without a restart);
  BT.601 for SD RGB sources; colour conversion built from its matrix
  (`gchd_csc_put`).
- Bitrate as one slider for every input (driver ranges, window, text
  menu, OBS plugin): VBR below 10.00 (up to 19.98 Mbit/s), CBR from 10.00.
- Emulator (`tools/drvemu`, now with HDMI and an encoder field comparison):
  720p60 and 1080p60 first start, source changes to 576p, 720p50/60,
  1080p60/50/30/25: 110 of 110 encoder fields as recorded; receiver writes
  as recorded except the bank selects of the timing polls.
- Docs: `docs/hdmi.md` §3, §5, READMEs.

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
- The receiver delivers RGB from the source (a laptop's HDMI output in
  every HDMI recording; the PS3 is not used on HDMI); 480p needs the BT.601 matrix
  (the driver uses the BT.709 one for every RGB source).
- Encoder values per output mode are the component ones (same tables).
- Back end and HDMI (maintainer's question, 2026-11-27): both start
  recordings switch from component. Evidence that HDMI does not use the
  back end: the device initialization writes one fixed back-end block
  (the driver writes the same), Windows selects HDMI after it without
  further back-end writes (initialization recording, no source), and HDMI
  source mode changes never write the back end while analog mode changes
  rewrite its per-mode tables. Not recorded with a source: a cold start
  with HDMI selected and HDMI after composite/S-video; the test covers
  both (if they fail: a Windows recording of composite → HDMI).
- Not covered: 1080i over HDMI (no source offered it; detection value from
  the older code), 24 Hz sources (no field-rate value known).

## Hardware test (maintainer)

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-050.sh > tests/T-050.sh
bash gchd-test.sh T-050 --branch feat/hdmi
```

- T-050 (66d891a), first run (laptop on HDMI): cold start with HDMI
  selected PASS; samples (on `traces/hdmi/samples/`, all decode without
  errors): 720p60 1280 × 720 level 40 at 60, "Allow 50/60 fps" off level 31
  at 30, 640 × 480; 1080p60 at 30 (level 40), its 1280 × 720 and 480 × 360;
  1080p50 at 25; 720 × 576p50 level 30 / 31 (option off); colour range
  Standard/Expanded while capturing without a restart PASS; source change
  1080/576 → 720p followed PASS; bitrate as one slider in the driver, the
  window (9.50 → VBR 18.96, 15.00 → CBR 15.07) and OBS (8.00 → VBR 16.04)
  PASS; composite slider ranges PASS. FAIL (3): the 1080p range check ran
  while the output was still 480 × 360 (test order; the range shown,
  4–15 Mbit/s, is right for 480 × 360); the composite capture before HDMI
  got no data in 20 s (no composite detection logged: no signal from the
  PS3 on composite?), so HDMI after a configured composite was not tested
  (HDMI right after the attempt PASS); v4l2-compliance 0 failures on the
  first run, 1 failure on the second run right after it (which test: not
  in the log). Maintainer: the NTSC PS3 can be tested on HDMI; a
  recording with it follows.

- T-050 (4b76eda), second run: composite with the PS3 (480i detected and
  captured) PASS; then HDMI 1080p60: 1280 × 720 at level 41 and the
  1280 × 720 bitrate range. Cause: the test set the output size while
  composite was still selected (the size belongs to the selected input),
  so HDMI kept its 1280 × 720 from the run before; level 41: the previous
  run's v4l2-compliance had set the H.264 level control, which the driver
  then kept (no way back to automatic). HDMI after a configured composite
  capture worked (the back end set up for composite did not disturb HDMI).
  v4l2-compliance: 1 failure, "fmt.fmt.pix.width < enumtimings width"
  (DV timings with a scaled output size). Now: the H.264 profile and level
  controls are removed (maintainer, 2026-11-27: High profile and the level
  of each output, as recorded; the old parameters are ignored); a DV
  timings change selects the source size. Maintainer: the PS3 does not
  work on HDMI (HDCP); 1080i over HDMI stays as the older code had it
  (detection `9C:64/63 = B081`), not testable here.

## Maintainer comments

gchd-test T-050 20261127-134225 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-050-20261127-134225.log
== build
$ git fetch -q origin
$ git checkout -q dev
$ git pull -q --ff-only
commit: 47469ac TASK: T-080 (picture and audio settings) after T-050
commit: 47469ac TASK: T-080 (picture and audio settings) after T-050
build rc=0 warnings=0

=== step 1: install
$ sudo rmmod elgato_gchd
$ bash /home/ff5/elgato-gchd/install-driver.sh --core --yes --in-terminal
[1mElgato Game Capture HD driver 0.5.12[0m

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
Signing module /var/lib/dkms/elgato-gchd/0.5.12/build/elgato_gchd.ko
Cleaning build area...

elgato_gchd.ko.zst:
Running module version sanity check.
 - Original module
   - This kernel never originally had a module by this name
 - Installation
   - Installing to /lib/modules/6.8.0-146-generic/updates/dkms/
depmod...
[32mInstalled with DKMS (rebuilt automatically after kernel updates).[0m
[32mudev rules installed (settings are remembered; device name /dev/elgato-gchd).[0m
[32mDriver loaded. Plug in the Game Capture HD; it appears as a /dev/video device.[0m
Watch: v4l2-ctl -d /dev/elgato-gchd -v pixelformat=MPEG && ffplay -f mpegts /dev/elgato-gchd

PROMPT: Laptop on HDMI. Unplug the Elgato and plug it in again; wait 5 s.
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -c allow_50_60_fps=1
--- dmesg step 1
[23264.375035] usbcore: deregistering interface driver elgato_gchd
[23273.483258] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[23273.490781] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[23273.490806] usbcore: registered new interface driver elgato_gchd
[23276.550279] usb 1-2: USB disconnect, device number 13
[23284.378445] usb 1-2: new high-speed USB device number 14 using xhci_hcd
[23284.757729] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[23284.757751] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[23284.757756] usb 1-2: Product: GameCapture HD
[23284.757762] usb 1-2: Manufacturer: Elgato
[23284.757766] usb 1-2: SerialNumber: 04107F1267
[23284.763098] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[23284.767822] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[23293.098535] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0

=== step 2: HDMI source 720x480 at 60 Hz (480p)
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
PROMPT: A window 'Elgato HDMI' shows the laptop's HDMI picture. In the laptop's display settings set the HDMI display (the Elgato) to 720x480 at 60 Hz (480p). If the picture freezes or disappears, close the window (it reopens). Press Enter when it shows the laptop in 720x480 at 60 Hz (480p).
$ v4l2-ctl -d /dev/video0 --list-framesizes MPEG
ioctl: VIDIOC_ENUM_FRAMESIZES
	Size: Discrete 720x480
	Size: Discrete 640x480
	Size: Discrete 480x360
$ v4l2-ctl -d /dev/video0 --get-dv-timings
DV timings:
	Active width: 720
	Active height: 480
	Total width: 858
	Total height: 525
	Frame format: progressive
	Polarities: -vsync -hsync
	Pixelclock: 27000000 Hz (59.94 frames per second)
	Horizontal frontporch: 16
	Horizontal sync: 62
	Horizontal backporch: 60
	Vertical frontporch: 9
	Vertical sync: 6
	Vertical backporch: 30
	Standards: CTA-861
	Picture aspect: 4:3
	CTA-861 VIC: 2
	Flags: CE-video, has picture aspect, has CTA-861 VIC
--- dmesg step 2
[23293.138348] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[23293.260530] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[23293.466497] elgato_gchd 1-2:1.0: transcoder defaults loaded
[23293.528542] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[23293.543220] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb6d7 rgb=1
[23293.543224] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1920x1080 fps=60/1 interlaced=0 signal_present=1
[23293.543226] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1920x1080 fps=60/1 interlaced=0
[23293.543228] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1920x1080 fps=60/1 interlaced=0
[23294.311462] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[23295.514970] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[23295.537402] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[23295.537771] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[23295.538857] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[23295.539545] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1920x1080 fps=60/1 interlaced=0
[23295.540070] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x000a
[23295.609997] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[23300.568571] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=20
[23303.101521] elgato_gchd 1-2:1.0: HDMI source mode changed while capturing; detecting again
[23303.109407] elgato_gchd 1-2:1.0: signal present: input=0; first video TS packet on PID 0x1011
[23304.033416] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb0bf rgb=1
[23304.033423] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[23304.033426] elgato_gchd 1-2:1.0: signal detected: input=0 mode=720x480 fps=60/1 interlaced=0
[23304.033428] elgato_gchd 1-2:1.0: capture setup: input=0 mode=720x480 fps=60/1 interlaced=0
[23305.128362] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[23305.147811] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[23305.148148] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[23305.151449] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[23305.152705] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=720x480 fps=60/1 interlaced=0
[23305.153339] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x000a (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x000a
[23305.220386] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[23310.193102] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=38 bytes=2490368 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[23315.242181] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=81 bytes=5308416 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[23317.894134] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 3: 480p
$ sh -c v4l2-ctl -d /dev/video0 --get-dv-timings | tr '\n' ' '
DV timings: 	Active width: 720 	Active height: 480 	Total width: 858 	Total height: 525 	Frame format: progressive 	Polarities: -vsync -hsync 	Pixelclock: 27000000 Hz (59.94 frames per second) 	Horizontal frontporch: 16 	Horizontal sync: 62 	Horizontal backporch: 60 	Vertical frontporch: 9 	Vertical sync: 6 	Vertical backporch: 30 	Standards: CTA-861 	Picture aspect: 4:3 	CTA-861 VIC: 2 	Flags: CE-video, has picture aspect, has CTA-861 VIC 
[PASS] 480p detected (720x480 progressive)
$ sh -c v4l2-ctl -d /dev/video0 --list-framesizes MPEG | tr '\n' ' '
ioctl: VIDIOC_ENUM_FRAMESIZES 	Size: Discrete 720x480 	Size: Discrete 640x480 	Size: Discrete 480x360 
[PASS] 480p: 720x480, 640x480, 480x360 offered
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ sh -c tr '\n' ' ' < /home/ff5/gchd-test-logs/T-050-samples/hdmi_480p.ts.ffprobe.txt; echo errors=$(wc -l < /home/ff5/gchd-test-logs/T-050-samples/hdmi_480p.ts.ffmpeg.txt)
codec_name=h264 profile=High width=720 height=480 sample_aspect_ratio=10:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=60000/1001 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 codec_name=h264 profile=High width=720 height=480 sample_aspect_ratio=10:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=60000/1001 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 duration=17.350656 errors=0
[PASS] 480p: 720 480, progressive, level 30, (60/1|60000/1001) fps, decodes without errors
$ v4l2-ctl -d /dev/video0 -c allow_50_60_fps=0
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ sh -c tr '\n' ' ' < /home/ff5/gchd-test-logs/T-050-samples/hdmi_480p_allow_off.ts.ffprobe.txt; echo errors=$(wc -l < /home/ff5/gchd-test-logs/T-050-samples/hdmi_480p_allow_off.ts.ffmpeg.txt)
/home/ff5/gchd-test-logs/T-050-samples/hdmi_480p_allow_off.ts: Invalid data found when processing input errors=3
[FAIL] 480p_allow_off: 720 480, progressive, level 31, (60/1|60000/1001) fps, decodes without errors (expected /width=720 height=480 .*level=31 .*field_order=progressive r_frame_rate=(60/1|60000/1001) .*errors=0/, got: /home/ff5/gchd-test-logs/T-050-samples/hdmi_480p_allow_off.ts: Invalid data found when processing input errors=3)
$ v4l2-ctl -d /dev/video0 -c allow_50_60_fps=1
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ sh -c tr '\n' ' ' < /home/ff5/gchd-test-logs/T-050-samples/hdmi_480p_640x480.ts.ffprobe.txt; echo errors=$(wc -l < /home/ff5/gchd-test-logs/T-050-samples/hdmi_480p_640x480.ts.ffmpeg.txt)
codec_name=h264 profile=High width=640 height=480 sample_aspect_ratio=1:1 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=30/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 codec_name=h264 profile=High width=640 height=480 sample_aspect_ratio=1:1 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=30/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 duration=13.400000 errors=0
[PASS] 480p_640x480: 640 480, progressive, level 30, (30/1|30000/1001) fps, decodes without errors
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -v width=480,height=360,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ sh -c tr '\n' ' ' < /home/ff5/gchd-test-logs/T-050-samples/hdmi_480p_480x360.ts.ffprobe.txt; echo errors=$(wc -l < /home/ff5/gchd-test-logs/T-050-samples/hdmi_480p_480x360.ts.ffmpeg.txt)
codec_name=h264 profile=High width=480 height=360 sample_aspect_ratio=1:1 level=22 color_primaries=smpte170m field_order=progressive r_frame_rate=30/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 codec_name=h264 profile=High width=480 height=360 sample_aspect_ratio=1:1 level=22 color_primaries=smpte170m field_order=progressive r_frame_rate=30/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 duration=15.800000 errors=0
[PASS] 480p_480x360: 480 360, progressive, level 22, (30/1|30000/1001) fps, decodes without errors
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
$ ls -l /home/ff5/gchd-test-logs/T-050-samples
total 404484
-rw-rw-r-- 1 ff5 ff5 19956576 Nov 27 12:21 hdmi_1080p_1280x720.ts
-rw-rw-r-- 1 ff5 ff5        0 Nov 27 12:21 hdmi_1080p_1280x720.ts.ffmpeg.txt
-rw-rw-r-- 1 ff5 ff5      469 Nov 27 12:21 hdmi_1080p_1280x720.ts.ffprobe.txt
-rw-rw-r-- 1 ff5 ff5 20000000 Nov 27 12:21 hdmi_1080p_1280x720.ts.raw
-rw-rw-r-- 1 ff5 ff5  4438492 Nov 27 12:22 hdmi_1080p_480x360.ts
-rw-rw-r-- 1 ff5 ff5        0 Nov 27 12:22 hdmi_1080p_480x360.ts.ffmpeg.txt
-rw-rw-r-- 1 ff5 ff5      467 Nov 27 12:22 hdmi_1080p_480x360.ts.ffprobe.txt
-rw-rw-r-- 1 ff5 ff5  4448256 Nov 27 12:22 hdmi_1080p_480x360.ts.raw
-rw-rw-r-- 1 ff5 ff5 19963720 Nov 27 12:23 hdmi_1080p_low.ts
-rw-rw-r-- 1 ff5 ff5        0 Nov 27 12:23 hdmi_1080p_low.ts.ffmpeg.txt
-rw-rw-r-- 1 ff5 ff5      470 Nov 27 12:23 hdmi_1080p_low.ts.ffprobe.txt
-rw-rw-r-- 1 ff5 ff5 20000000 Nov 27 12:23 hdmi_1080p_low.ts.raw
-rw-rw-r-- 1 ff5 ff5 19945484 Nov 27 12:21 hdmi_1080p.ts
-rw-rw-r-- 1 ff5 ff5        0 Nov 27 12:21 hdmi_1080p.ts.ffmpeg.txt
-rw-rw-r-- 1 ff5 ff5      470 Nov 27 12:21 hdmi_1080p.ts.ffprobe.txt
-rw-rw-r-- 1 ff5 ff5 20000000 Nov 27 12:21 hdmi_1080p.ts.raw
-rw-rw-r-- 1 ff5 ff5  4508240 Nov 27 13:44 hdmi_480p_480x360.ts
-rw-rw-r-- 1 ff5 ff5        0 Nov 27 13:44 hdmi_480p_480x360.ts.ffmpeg.txt
-rw-rw-r-- 1 ff5 ff5      475 Nov 27 13:44 hdmi_480p_480x360.ts.ffprobe.txt
-rw-rw-r-- 1 ff5 ff5  4513792 Nov 27 13:44 hdmi_480p_480x360.ts.raw
-rw-rw-r-- 1 ff5 ff5 19983836 Nov 27 13:44 hdmi_480p_640x480.ts
-rw-rw-r-- 1 ff5 ff5        0 Nov 27 13:44 hdmi_480p_640x480.ts.ffmpeg.txt
-rw-rw-r-- 1 ff5 ff5      475 Nov 27 13:44 hdmi_480p_640x480.ts.ffprobe.txt
-rw-rw-r-- 1 ff5 ff5 20000000 Nov 27 13:44 hdmi_480p_640x480.ts.raw
-rw-rw-r-- 1 ff5 ff5        0 Nov 27 13:44 hdmi_480p_allow_off.ts
-rw-rw-r-- 1 ff5 ff5      242 Nov 27 13:44 hdmi_480p_allow_off.ts.ffmpeg.txt
-rw-rw-r-- 1 ff5 ff5      104 Nov 27 13:44 hdmi_480p_allow_off.ts.ffprobe.txt
-rw-rw-r-- 1 ff5 ff5        0 Nov 27 13:43 hdmi_480p_allow_off.ts.raw
-rw-rw-r-- 1 ff5 ff5  9606236 Nov 27 13:43 hdmi_480p.ts
-rw-rw-r-- 1 ff5 ff5        0 Nov 27 13:43 hdmi_480p.ts.ffmpeg.txt
-rw-rw-r-- 1 ff5 ff5      491 Nov 27 13:43 hdmi_480p.ts.ffprobe.txt
-rw-rw-r-- 1 ff5 ff5  9613312 Nov 27 13:43 hdmi_480p.ts.raw
-rw-rw-r-- 1 ff5 ff5 19968044 Nov 27 12:20 hdmi_720p_640x480.ts
-rw-rw-r-- 1 ff5 ff5        0 Nov 27 12:20 hdmi_720p_640x480.ts.ffmpeg.txt
-rw-rw-r-- 1 ff5 ff5      467 Nov 27 12:20 hdmi_720p_640x480.ts.ffprobe.txt
-rw-rw-r-- 1 ff5 ff5 20000000 Nov 27 12:20 hdmi_720p_640x480.ts.raw
-rw-rw-r-- 1 ff5 ff5 19947552 Nov 27 12:20 hdmi_720p_allow_off.ts
-rw-rw-r-- 1 ff5 ff5        0 Nov 27 12:20 hdmi_720p_allow_off.ts.ffmpeg.txt
-rw-rw-r-- 1 ff5 ff5      469 Nov 27 12:20 hdmi_720p_allow_off.ts.ffprobe.txt
-rw-rw-r-- 1 ff5 ff5 20000000 Nov 27 12:20 hdmi_720p_allow_off.ts.raw
-rw-rw-r-- 1 ff5 ff5 19948868 Nov 27 12:20 hdmi_720p.ts
-rw-rw-r-- 1 ff5 ff5        0 Nov 27 12:20 hdmi_720p.ts.ffmpeg.txt
-rw-rw-r-- 1 ff5 ff5      468 Nov 27 12:20 hdmi_720p.ts.ffprobe.txt
-rw-rw-r-- 1 ff5 ff5 20000000 Nov 27 12:20 hdmi_720p.ts.raw
-rw-rw-r-- 1 ff5 ff5 19945484 Nov 27 13:35 hdmi_after_composite_1080p.ts
-rw-rw-r-- 1 ff5 ff5        0 Nov 27 13:35 hdmi_after_composite_1080p.ts.ffmpeg.txt
-rw-rw-r-- 1 ff5 ff5      470 Nov 27 13:35 hdmi_after_composite_1080p.ts.ffprobe.txt
-rw-rw-r-- 1 ff5 ff5 20000000 Nov 27 13:35 hdmi_after_composite_1080p.ts.raw
-rw-rw-r-- 1 ff5 ff5 11599976 Nov 27 12:31 hdmi_after_composite.ts
-rw-rw-r-- 1 ff5 ff5        0 Nov 27 12:31 hdmi_after_composite.ts.ffmpeg.txt
-rw-rw-r-- 1 ff5 ff5      469 Nov 27 12:31 hdmi_after_composite.ts.ffprobe.txt
-rw-rw-r-- 1 ff5 ff5 11710464 Nov 27 12:31 hdmi_after_composite.ts.raw
-rw-rw-r-- 1 ff5 ff5  8754972 Nov 27 12:25 hdmi_sd_allow_off.ts
-rw-rw-r-- 1 ff5 ff5        0 Nov 27 12:25 hdmi_sd_allow_off.ts.ffmpeg.txt
-rw-rw-r-- 1 ff5 ff5      479 Nov 27 12:25 hdmi_sd_allow_off.ts.ffprobe.txt
-rw-rw-r-- 1 ff5 ff5  8765440 Nov 27 12:25 hdmi_sd_allow_off.ts.raw
-rw-rw-r-- 1 ff5 ff5  8226316 Nov 27 12:25 hdmi_sd.ts
-rw-rw-r-- 1 ff5 ff5        0 Nov 27 12:25 hdmi_sd.ts.ffmpeg.txt
-rw-rw-r-- 1 ff5 ff5      479 Nov 27 12:25 hdmi_sd.ts.ffprobe.txt
-rw-rw-r-- 1 ff5 ff5  8241152 Nov 27 12:25 hdmi_sd.ts.raw
PROMPT: Upload the hdmi_480p* files from /home/ff5/gchd-test-logs/T-050-samples to traces/hdmi/samples/.
--- dmesg step 3
[23320.961072] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[23320.971328] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[23320.975428] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[23321.181651] elgato_gchd 1-2:1.0: transcoder defaults loaded
[23321.237780] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[23321.250447] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb0bf rgb=1
[23321.250453] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[23321.250455] elgato_gchd 1-2:1.0: signal detected: input=0 mode=720x480 fps=60/1 interlaced=0
[23321.250457] elgato_gchd 1-2:1.0: capture setup: input=0 mode=720x480 fps=60/1 interlaced=0
[23321.998752] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[23323.130233] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[23323.145765] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[23323.145957] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[23323.146956] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[23323.147540] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=720x480 fps=60/1 interlaced=0
[23323.148289] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[23323.218408] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[23328.200811] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=38 bytes=2490368 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[23333.240981] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=81 bytes=5308416 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[23338.296686] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=124 bytes=8126464 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[23342.389266] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[23343.335306] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[23343.339946] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[23343.341584] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[23343.341587] elgato_gchd 1-2:1.0: waiting 548 ms after the previous shutdown
[23344.375882] elgato_gchd 1-2:1.0: transcoder defaults loaded
[23344.439275] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[23344.456092] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23344.747434] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23345.027712] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23345.319568] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23345.605536] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23345.882476] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23346.162751] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23346.452470] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23346.742825] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23347.029830] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23347.328064] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23347.617112] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23347.916738] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23348.186339] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23348.489022] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23348.771735] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23349.039279] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23349.330657] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23349.626974] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23349.901135] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23350.187657] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23350.475507] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23350.753885] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23351.031732] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23351.322806] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23351.620922] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23351.901549] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23352.176325] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23352.461283] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23352.729490] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23353.015007] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23353.299885] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23353.584019] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23353.886153] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23354.173758] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23354.456231] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23354.749038] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23355.019915] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23355.311078] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23355.600763] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23355.896234] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23356.184521] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23356.467061] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23356.752165] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23357.043007] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23357.331751] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23357.614982] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23357.919746] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23358.219731] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23358.496353] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23358.779934] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23359.063432] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23359.366121] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23359.646346] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23359.940246] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23360.221811] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23360.501888] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23360.783400] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23361.058791] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23361.332046] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23361.629667] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23361.909107] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23362.179110] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23362.461014] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23362.747893] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23363.024991] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23363.315124] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb2b0 rgb=0
[23363.423101] elgato_gchd 1-2:1.0: shutdown: device in STOP after RESET; IDLE and RESET again
[23363.438949] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0011 before, 0x0012 now)
[23363.562681] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[23363.568517] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[23363.570945] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[23363.570948] elgato_gchd 1-2:1.0: waiting 1369 ms after the previous shutdown
[23365.312275] elgato_gchd 1-2:1.0: transcoder defaults loaded
[23365.372660] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[23365.386050] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb0bf rgb=1
[23365.386053] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[23365.386055] elgato_gchd 1-2:1.0: signal detected: input=0 mode=720x480 fps=60/1 interlaced=0
[23365.386057] elgato_gchd 1-2:1.0: capture setup: input=0 mode=720x480 fps=60/1 interlaced=0
[23366.145729] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[23367.287560] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[23367.302282] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[23367.302482] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[23367.303779] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[23367.304419] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=720x480 fps=60/1 interlaced=0
[23367.304788] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[23367.374715] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[23367.711660] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[23372.319329] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=106 bytes=6946816 ep84_bytes=6946816 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[23377.357947] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=221 bytes=14483456 ep84_bytes=14483456 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[23381.913537] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[23382.457027] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[23382.462769] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[23382.466118] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[23382.466123] elgato_gchd 1-2:1.0: waiting 948 ms after the previous shutdown
[23384.121236] elgato_gchd 1-2:1.0: transcoder defaults loaded
[23384.186572] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[23384.203796] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb0bf rgb=1
[23384.203802] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[23384.203805] elgato_gchd 1-2:1.0: signal detected: input=0 mode=720x480 fps=60/1 interlaced=0
[23384.203806] elgato_gchd 1-2:1.0: capture setup: input=0 mode=720x480 fps=60/1 interlaced=0
[23384.989643] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[23386.269046] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[23386.284246] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[23386.284474] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[23386.286070] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[23386.286692] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=720x480 fps=60/1 interlaced=0
[23386.287113] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[23386.356831] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[23386.914628] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[23391.296446] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=19 bytes=1245184 ep84_bytes=1245184 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[23396.402706] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=42 bytes=2752512 ep84_bytes=2752512 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[23401.435450] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=64 bytes=4194304 ep84_bytes=4194304 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[23403.993148] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== summary (T-050, 47469ac TASK: T-080 (picture and audio settings) after T-050)
PASS | step 3 480p | 480p detected (720x480 progressive)
PASS | step 3 480p | 480p: 720x480, 640x480, 480x360 offered
PASS | step 3 480p | 480p: 720 480, progressive, level 30, (60/1|60000/1001) fps, decodes without errors
FAIL | step 3 480p | 480p_allow_off: 720 480, progressive, level 31, (60/1|60000/1001) fps, decodes without errors (expected /width=720 height=480 .*level=31 .*field_order=progressive r_frame_rate=(60/1|60000/1001) .*errors=0/, got: /home/ff5/gchd-test-logs/T-050-samples/hdmi_480p_allow_off.ts: Invalid data found when processing input errors=3)
PASS | step 3 480p | 480p_640x480: 640 480, progressive, level 30, (30/1|30000/1001) fps, decodes without errors
PASS | step 3 480p | 480p_480x360: 480 360, progressive, level 22, (30/1|30000/1001) fps, decodes without errors
