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

## Maintainer comments

There were a couple of issues.

- I think Elgato recommends not to have 2 inputs plugged at the same time otherwise there could be problems. That's why I didn't have a composite input plugged in immediately.

My NTSC PS3 I think I can test via HDMI, so I'm going to prepare a trace for you.

Here is the log:
gchd-test T-050 20261127-121532 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-050-20261127-121532.log
== build
$ git fetch -q origin
$ git checkout -q feat/hdmi
$ git pull -q --ff-only
commit: 66d891a T-050 findings: HDMI source was a laptop; back end and HDMI (evidence, cold start and HDMI after composite in the test); 1080i and 24 Hz not covered
commit: 66d891a T-050 findings: HDMI source was a laptop; back end and HDMI (evidence, cold start and HDMI after composite in the test); 1080i and 24 Hz not covered
build rc=0 warnings=0

=== step 1: install the settings programs and the plugin of this branch
$ sudo rmmod elgato_gchd
$ bash /home/ff5/elgato-gchd/install-driver.sh --all --yes --in-terminal
[1mElgato Game Capture HD driver 0.5.11[0m

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
Signing module /var/lib/dkms/elgato-gchd/0.5.11/build/elgato_gchd.ko
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

[1m==> Settings programs[0m
[32mSettings installed: 'Elgato Game Capture HD Settings' in the application menu (gchd-settings-gui); text menu: gchd-settings[0m

[1m==> OBS plugin[0m
[32mOBS plugin installed in /home/ff5/.config/obs-studio/plugins/obs-elgato-gchd. Restart OBS and add the source 'Elgato Game Capture HD'.[0m

PROMPT: Connect the laptop's HDMI output to the Elgato (laptop on, the Elgato as a second display, mirrored) and the PS3 to composite (PS3 on). Unplug the Elgato and plug it in again; wait 5 s (the first capture is a cold start with HDMI selected).
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -c allow_50_60_fps=1,hdmi_color_range=0
--- dmesg step 1
[18054.439674] usbcore: deregistering interface driver elgato_gchd
[18063.309553] usbcore: registered new interface driver elgato_gchd
[18090.923137] usb 1-2: new high-speed USB device number 11 using xhci_hcd
[18091.339838] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[18091.339866] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[18091.339873] usb 1-2: Product: GameCapture HD
[18091.339879] usb 1-2: Manufacturer: Elgato
[18091.339885] usb 1-2: SerialNumber: 04107F1267
[18091.347272] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[18091.349986] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[18260.663919] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=0 streaming=0
[18260.663992] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open

=== step 2: HDMI source 1280x720 at 60 Hz
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
PROMPT: A window 'Elgato HDMI' shows the laptop's HDMI picture. In the laptop's display settings set the HDMI display (the Elgato) to 1280x720 at 60 Hz. If the picture freezes or disappears, close the window (it reopens). Press Enter when it shows the laptop in 1280x720 at 60 Hz.
$ v4l2-ctl -d /dev/video0 --list-framesizes MPEG
ioctl: VIDIOC_ENUM_FRAMESIZES
	Size: Discrete 1280x720
	Size: Discrete 640x480
	Size: Discrete 480x360
$ v4l2-ctl -d /dev/video0 --get-dv-timings
DV timings:
	Active width: 1280
	Active height: 720
	Total width: 1650
	Total height: 750
	Frame format: progressive
	Polarities: +vsync +hsync
	Pixelclock: 74250000 Hz (60.00 frames per second)
	Horizontal frontporch: 110
	Horizontal sync: 40
	Horizontal backporch: 220
	Vertical frontporch: 5
	Vertical sync: 5
	Vertical backporch: 20
	Standards: CTA-861
	CTA-861 VIC: 4
	Flags: framerate can be reduced by 1/1.001, CE-video, has CTA-861 VIC
--- dmesg step 2
[18260.705168] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18260.822425] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[18261.126538] elgato_gchd 1-2:1.0: transcoder defaults loaded
[18261.188833] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[18261.202561] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba94 samples6463=1 avg6463=0xb6d7 rgb=1
[18261.202565] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1920x1080 fps=60/1 interlaced=0 signal_present=1
[18261.202567] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1920x1080 fps=60/1 interlaced=0
[18261.202572] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1920x1080 fps=60/1 interlaced=0
[18261.959980] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[18263.155317] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[18263.176544] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18263.176901] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18263.178283] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18263.178832] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1920x1080 fps=60/1 interlaced=0
[18263.179450] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x000a
[18263.248934] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18268.191861] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=181 bytes=11862016 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=20
[18273.205387] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=381 bytes=24969216 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=20
[18278.219682] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=581 bytes=38076416 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=21
[18283.234645] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=781 bytes=51183616 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=21
[18285.437061] elgato_gchd 1-2:1.0: HDMI source mode changed while capturing; detecting again
[18285.439978] elgato_gchd 1-2:1.0: signal present: input=0; first video TS packet on PID 0x1011
[18286.656514] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb05c rgb=1
[18286.656520] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[18286.656522] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1280x720 fps=60/1 interlaced=0
[18286.656525] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1280x720 fps=60/1 interlaced=0
[18287.754765] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[18287.770988] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18287.771190] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18287.772325] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18287.772942] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1280x720 fps=60/1 interlaced=0
[18287.773408] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x000a (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x000a
[18287.843455] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18292.778363] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=221 bytes=14483456 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18297.785312] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=460 bytes=30146560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18300.809893] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 3: 720p
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ sh -c tr '\n' ' ' < /home/ff5/gchd-test-logs/T-050-samples/hdmi_720p.ts.ffprobe.txt; echo errors=$(wc -l < /home/ff5/gchd-test-logs/T-050-samples/hdmi_720p.ts.ffmpeg.txt)
codec_name=h264 profile=High width=1280 height=720 sample_aspect_ratio=1:1 level=40 color_primaries=bt709 field_order=progressive r_frame_rate=60/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 codec_name=h264 profile=High width=1280 height=720 sample_aspect_ratio=1:1 level=40 color_primaries=bt709 field_order=progressive r_frame_rate=60/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 duration=6.400000 errors=0
[PASS] 720p: 1280 720, progressive, level 40, (60/1|60000/1001) fps, decodes without errors
$ sh -c v4l2-ctl -d /dev/video0 --list-framesizes MPEG | tr '\n' ' '
ioctl: VIDIOC_ENUM_FRAMESIZES 	Size: Discrete 1280x720 	Size: Discrete 640x480 	Size: Discrete 480x360 
[PASS] 720p: 1280x720, 640x480, 480x360 offered (no scaled 1280x720)
$ v4l2-ctl -d /dev/video0 -c allow_50_60_fps=0
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ sh -c tr '\n' ' ' < /home/ff5/gchd-test-logs/T-050-samples/hdmi_720p_allow_off.ts.ffprobe.txt; echo errors=$(wc -l < /home/ff5/gchd-test-logs/T-050-samples/hdmi_720p_allow_off.ts.ffmpeg.txt)
codec_name=h264 profile=High width=1280 height=720 sample_aspect_ratio=1:1 level=31 color_primaries=bt709 field_order=progressive r_frame_rate=30/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 codec_name=h264 profile=High width=1280 height=720 sample_aspect_ratio=1:1 level=31 color_primaries=bt709 field_order=progressive r_frame_rate=30/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 duration=12.533333 errors=0
[PASS] 720p_allow_off: 1280 720, progressive, level 31, (30/1|30000/1001) fps, decodes without errors
$ v4l2-ctl -d /dev/video0 -c allow_50_60_fps=1
$ v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ sh -c tr '\n' ' ' < /home/ff5/gchd-test-logs/T-050-samples/hdmi_720p_640x480.ts.ffprobe.txt; echo errors=$(wc -l < /home/ff5/gchd-test-logs/T-050-samples/hdmi_720p_640x480.ts.ffmpeg.txt)
codec_name=h264 profile=High width=640 height=480 sample_aspect_ratio=4:3 level=30 color_primaries=bt709 field_order=progressive r_frame_rate=30/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 codec_name=h264 profile=High width=640 height=480 sample_aspect_ratio=4:3 level=30 color_primaries=bt709 field_order=progressive r_frame_rate=30/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 duration=14.933333 errors=0
[PASS] 720p_640x480: 640 480, progressive, level 30, (30/1|30000/1001) fps, decodes without errors
--- dmesg step 3
[18303.867966] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18303.869461] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[18304.088506] elgato_gchd 1-2:1.0: transcoder defaults loaded
[18304.151106] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[18304.165994] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb05c rgb=1
[18304.166000] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[18304.166003] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1280x720 fps=60/1 interlaced=0
[18304.166006] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1280x720 fps=60/1 interlaced=0
[18304.958576] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[18306.224067] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[18306.239221] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18306.239447] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18306.240717] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18306.241466] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1280x720 fps=60/1 interlaced=0
[18306.242031] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[18306.311789] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18311.245783] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=221 bytes=14483456 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18313.716586] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[18314.470251] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18314.472375] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[18314.472378] elgato_gchd 1-2:1.0: waiting 745 ms after the previous shutdown
[18315.361210] elgato_gchd 1-2:1.0: transcoder defaults loaded
[18315.424544] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[18315.439889] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb05c rgb=1
[18315.439893] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[18315.439895] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1280x720 fps=60/1 interlaced=0
[18315.439898] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1280x720 fps=60/1 interlaced=0
[18316.219700] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[18317.484141] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[18317.499988] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18317.500310] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18317.501487] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18317.502080] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1280x720 fps=60/1 interlaced=0
[18317.502555] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[18317.572575] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18322.503176] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=112 bytes=7340032 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18327.523189] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=234 bytes=15335424 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18331.306712] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[18332.241342] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18332.243185] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[18332.243189] elgato_gchd 1-2:1.0: waiting 563 ms after the previous shutdown
[18332.955098] elgato_gchd 1-2:1.0: transcoder defaults loaded
[18333.023754] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[18333.040747] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba94 samples6463=1 avg6463=0xb05c rgb=1
[18333.040752] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[18333.040754] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1280x720 fps=60/1 interlaced=0
[18333.040756] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1280x720 fps=60/1 interlaced=0
[18333.829672] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[18335.063104] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[18335.079377] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18335.079596] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18335.080743] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18335.081309] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1280x720 fps=60/1 interlaced=0
[18335.081774] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[18335.151944] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18335.494212] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[18340.106791] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=95 bytes=6225920 ep84_bytes=6225920 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18345.153507] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=198 bytes=12976128 ep84_bytes=12976128 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18350.198917] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=301 bytes=19726336 ep84_bytes=19726336 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18351.233299] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 4: HDMI source 1920x1080 at 60 Hz
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
PROMPT: A window 'Elgato HDMI' shows the laptop's HDMI picture. In the laptop's display settings set the HDMI display (the Elgato) to 1920x1080 at 60 Hz. If the picture freezes or disappears, close the window (it reopens). Press Enter when it shows the laptop in 1920x1080 at 60 Hz.
$ v4l2-ctl -d /dev/video0 --list-framesizes MPEG
ioctl: VIDIOC_ENUM_FRAMESIZES
	Size: Discrete 1920x1080
	Size: Discrete 1280x720
	Size: Discrete 640x480
	Size: Discrete 480x360
$ v4l2-ctl -d /dev/video0 --get-dv-timings
DV timings:
	Active width: 1920
	Active height: 1080
	Total width: 2200
	Total height: 1125
	Frame format: progressive
	Polarities: +vsync +hsync
	Pixelclock: 148500000 Hz (60.00 frames per second)
	Horizontal frontporch: 88
	Horizontal sync: 44
	Horizontal backporch: 148
	Vertical frontporch: 4
	Vertical sync: 5
	Vertical backporch: 36
	Standards: CTA-861, DMT
	CTA-861 VIC: 16
	Flags: framerate can be reduced by 1/1.001, CE-video, has CTA-861 VIC
--- dmesg step 4
[18351.836015] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18351.952534] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[18351.952535] elgato_gchd 1-2:1.0: waiting 782 ms after the previous shutdown
[18352.875923] elgato_gchd 1-2:1.0: transcoder defaults loaded
[18352.934171] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[18352.946311] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb05c rgb=1
[18352.946316] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[18352.946318] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1280x720 fps=60/1 interlaced=0
[18352.946320] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1280x720 fps=60/1 interlaced=0
[18353.700021] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[18354.870711] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[18354.885721] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18354.885922] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18354.887341] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18354.888068] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1280x720 fps=60/1 interlaced=0
[18354.888724] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[18354.941882] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18359.894930] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=222 bytes=14548992 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=12
[18364.904813] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=461 bytes=30212096 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=11
[18368.028952] elgato_gchd 1-2:1.0: HDMI source mode changed while capturing; detecting again
[18368.048381] elgato_gchd 1-2:1.0: signal present: input=0; first video TS packet on PID 0x1011
[18368.944711] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb6d7 rgb=1
[18368.944724] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1920x1080 fps=60/1 interlaced=0 signal_present=1
[18368.944727] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1920x1080 fps=60/1 interlaced=0
[18368.944729] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1920x1080 fps=60/1 interlaced=0
[18370.138081] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[18370.153632] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18370.153926] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18370.155104] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18370.155901] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1920x1080 fps=60/1 interlaced=0
[18370.156395] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[18370.226291] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18375.170642] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=181 bytes=11862016 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18377.080701] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 5: 1080p
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ sh -c tr '\n' ' ' < /home/ff5/gchd-test-logs/T-050-samples/hdmi_1080p.ts.ffprobe.txt; echo errors=$(wc -l < /home/ff5/gchd-test-logs/T-050-samples/hdmi_1080p.ts.ffmpeg.txt)
codec_name=h264 profile=High width=1920 height=1080 sample_aspect_ratio=1:1 level=40 color_primaries=bt709 field_order=progressive r_frame_rate=30/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 codec_name=h264 profile=High width=1920 height=1080 sample_aspect_ratio=1:1 level=40 color_primaries=bt709 field_order=progressive r_frame_rate=30/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 duration=7.666666 errors=0
[PASS] 1080p: 1920 1080, progressive, level 40, (30/1|30000/1001) fps, decodes without errors
$ v4l2-ctl -d /dev/video0 -v width=1280,height=720,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ sh -c tr '\n' ' ' < /home/ff5/gchd-test-logs/T-050-samples/hdmi_1080p_1280x720.ts.ffprobe.txt; echo errors=$(wc -l < /home/ff5/gchd-test-logs/T-050-samples/hdmi_1080p_1280x720.ts.ffmpeg.txt)
codec_name=h264 profile=High width=1280 height=720 sample_aspect_ratio=1:1 level=31 color_primaries=bt709 field_order=progressive r_frame_rate=30/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 codec_name=h264 profile=High width=1280 height=720 sample_aspect_ratio=1:1 level=31 color_primaries=bt709 field_order=progressive r_frame_rate=30/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 duration=12.533333 errors=0
[PASS] 1080p_1280x720: 1280 720, progressive, level 31, (30/1|30000/1001) fps, decodes without errors
$ v4l2-ctl -d /dev/video0 -v width=480,height=360,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ sh -c tr '\n' ' ' < /home/ff5/gchd-test-logs/T-050-samples/hdmi_1080p_480x360.ts.ffprobe.txt; echo errors=$(wc -l < /home/ff5/gchd-test-logs/T-050-samples/hdmi_1080p_480x360.ts.ffmpeg.txt)
codec_name=h264 profile=High width=480 height=360 sample_aspect_ratio=4:3 level=22 color_primaries=bt709 field_order=progressive r_frame_rate=30/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 codec_name=h264 profile=High width=480 height=360 sample_aspect_ratio=4:3 level=22 color_primaries=bt709 field_order=progressive r_frame_rate=30/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 duration=16.533333 errors=0
[PASS] 1080p_480x360: 480 360, progressive, level 22, (30/1|30000/1001) fps, decodes without errors
$ range_of
4000000-15000000 15000000
[FAIL] 1080p: 16-30 Mbit/s range covers VBR from 8.00 and CBR to 30.00 (expected /^10000000-30000000 30000000$/, got: 4000000-15000000 15000000)
--- dmesg step 5
[18380.146403] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18380.153123] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[18380.365648] elgato_gchd 1-2:1.0: transcoder defaults loaded
[18380.429765] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[18380.443744] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb6d7 rgb=1
[18380.443750] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1920x1080 fps=60/1 interlaced=0 signal_present=1
[18380.443752] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1920x1080 fps=60/1 interlaced=0
[18380.443753] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1920x1080 fps=60/1 interlaced=0
[18381.234728] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[18382.430902] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[18382.445965] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18382.446195] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18382.447536] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18382.448339] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1920x1080 fps=60/1 interlaced=0
[18382.448914] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[18382.518617] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18387.462660] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=181 bytes=11862016 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18391.536840] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[18392.605270] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18392.607780] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[18392.607784] elgato_gchd 1-2:1.0: waiting 430 ms after the previous shutdown
[18393.185408] elgato_gchd 1-2:1.0: transcoder defaults loaded
[18393.251939] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[18393.265188] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb6d7 rgb=1
[18393.265190] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1920x1080 fps=60/1 interlaced=0 signal_present=1
[18393.265192] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1920x1080 fps=60/1 interlaced=0
[18393.265194] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1920x1080 fps=60/1 interlaced=0
[18394.051511] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[18395.275032] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[18395.291489] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18395.291707] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18395.292904] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18395.293800] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1920x1080 fps=60/1 interlaced=0
[18395.294271] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[18395.364016] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18400.301260] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=111 bytes=7274496 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18405.320868] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=233 bytes=15269888 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18409.321767] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[18410.207156] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18410.209584] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[18410.209587] elgato_gchd 1-2:1.0: waiting 612 ms after the previous shutdown
[18410.972049] elgato_gchd 1-2:1.0: transcoder defaults loaded
[18411.036370] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[18411.050985] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb6d7 rgb=1
[18411.050991] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1920x1080 fps=60/1 interlaced=0 signal_present=1
[18411.050994] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1920x1080 fps=60/1 interlaced=0
[18411.050996] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1920x1080 fps=60/1 interlaced=0
[18411.831494] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[18413.134330] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[18413.150588] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18413.150908] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18413.152649] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18413.153409] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1920x1080 fps=60/1 interlaced=0
[18413.153913] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[18413.223438] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18413.836247] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[18418.179022] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=18 bytes=1179648 ep84_bytes=1179648 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18423.280852] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=39 bytes=2555904 ep84_bytes=2555904 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18428.334651] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=60 bytes=3932160 ep84_bytes=3932160 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18431.660972] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 6: HDMI Color Range while capturing (1080p)
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
PROMPT: Watch the 'Elgato HDMI' window for 10 s, then press Enter.
$ v4l2-ctl -d /dev/video0 -c hdmi_color_range=1
[PASS] With Expanded the picture became more contrasty (blacks darker, whites brighter) at once, without the picture stopping?
$ v4l2-ctl -d /dev/video0 -c hdmi_color_range=0
[PASS] Back to Standard, the picture is as before?
$ count_new encoder restart
0
[PASS] no encoder restart for the colour range
--- dmesg step 6
[18432.120715] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18432.135365] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18432.253725] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[18432.253727] elgato_gchd 1-2:1.0: waiting 908 ms after the previous shutdown
[18433.305774] elgato_gchd 1-2:1.0: transcoder defaults loaded
[18433.368976] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[18433.383678] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba94 samples6463=1 avg6463=0xb6d7 rgb=1
[18433.383683] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1920x1080 fps=60/1 interlaced=0 signal_present=1
[18433.383685] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1920x1080 fps=60/1 interlaced=0
[18433.383687] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1920x1080 fps=60/1 interlaced=0
[18434.175239] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[18435.489155] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[18435.504861] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18435.505086] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18435.506135] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18435.506896] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1920x1080 fps=60/1 interlaced=0
[18435.507461] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[18435.577260] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18440.520183] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=181 bytes=11862016 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=20
[18445.533776] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=381 bytes=24969216 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=20
[18450.547882] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=581 bytes=38076416 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=20
[18455.561871] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=781 bytes=51183616 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=21
[18460.574361] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=981 bytes=64290816 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=20
[18465.588600] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1181 bytes=77398016 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=21
[18470.604591] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1381 bytes=90505216 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=20
[18475.613078] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1581 bytes=103612416 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=20
[18480.626833] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1781 bytes=116719616 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=20
[18485.639032] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1981 bytes=129826816 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=20
[18489.098250] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 7: HDMI source 1920x1080 at 50 Hz
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
PROMPT: A window 'Elgato HDMI' shows the laptop's HDMI picture. In the laptop's display settings set the HDMI display (the Elgato) to 1920x1080 at 50 Hz. If the laptop offers no 50 Hz, choose the lowest refresh it offers for 1920x1080 (30, 25 or 24 Hz) and note it. If the picture freezes or disappears, close the window (it reopens). Press Enter when it shows the laptop in 1920x1080 at 50 Hz.
$ v4l2-ctl -d /dev/video0 --list-framesizes MPEG
ioctl: VIDIOC_ENUM_FRAMESIZES
	Size: Discrete 1920x1080
	Size: Discrete 1280x720
	Size: Discrete 640x480
	Size: Discrete 480x360
$ v4l2-ctl -d /dev/video0 --get-dv-timings
DV timings:
	Active width: 1920
	Active height: 1080
	Total width: 2640
	Total height: 1125
	Frame format: progressive
	Polarities: +vsync +hsync
	Pixelclock: 148500000 Hz (50.00 frames per second)
	Horizontal frontporch: 528
	Horizontal sync: 44
	Horizontal backporch: 148
	Vertical frontporch: 4
	Vertical sync: 5
	Vertical backporch: 36
	Standards: CTA-861
	CTA-861 VIC: 31
	Flags: CE-video, has CTA-861 VIC
--- dmesg step 7
[18492.173050] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18492.290701] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[18492.393653] elgato_gchd 1-2:1.0: transcoder defaults loaded
[18492.454719] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[18492.469057] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb6d7 rgb=1
[18492.469060] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1920x1080 fps=60/1 interlaced=0 signal_present=1
[18492.469061] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1920x1080 fps=60/1 interlaced=0
[18492.469063] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1920x1080 fps=60/1 interlaced=0
[18493.233094] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[18494.414982] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[18494.431775] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18494.431979] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18494.434960] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18494.435710] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1920x1080 fps=60/1 interlaced=0
[18494.436181] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[18494.504408] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18499.447183] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=181 bytes=11862016 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=20
[18504.460707] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=381 bytes=24969216 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=20
[18509.475210] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=581 bytes=38076416 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=20
[18514.490067] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=781 bytes=51183616 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=21
[18515.022253] elgato_gchd 1-2:1.0: HDMI source mode changed while capturing; detecting again
[18515.039872] elgato_gchd 1-2:1.0: signal present: input=0; first video TS packet on PID 0x1011
[18516.312730] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xbb75 samples6463=1 avg6463=0xb6d7 rgb=1
[18516.312739] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1920x1080 fps=50/1 interlaced=0 signal_present=1
[18516.312742] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1920x1080 fps=50/1 interlaced=0
[18516.312744] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1920x1080 fps=50/1 interlaced=0
[18517.447372] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[18517.468541] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18517.468996] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18517.471066] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18517.471870] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1920x1080 fps=50/1 interlaced=0
[18517.472419] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[18517.554277] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18521.234785] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 8: 1080p at 50 Hz (or the lower rate)
$ v4l2-ctl -d /dev/video0 --get-dv-timings
DV timings:
	Active width: 1920
	Active height: 1080
	Total width: 2640
	Total height: 1125
	Frame format: progressive
	Polarities: +vsync +hsync
	Pixelclock: 148500000 Hz (50.00 frames per second)
	Horizontal frontporch: 528
	Horizontal sync: 44
	Horizontal backporch: 148
	Vertical frontporch: 4
	Vertical sync: 5
	Vertical backporch: 36
	Standards: CTA-861
	CTA-861 VIC: 31
	Flags: CE-video, has CTA-861 VIC
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ sh -c tr '\n' ' ' < /home/ff5/gchd-test-logs/T-050-samples/hdmi_1080p_low.ts.ffprobe.txt; echo errors=$(wc -l < /home/ff5/gchd-test-logs/T-050-samples/hdmi_1080p_low.ts.ffmpeg.txt)
codec_name=h264 profile=High width=1920 height=1080 sample_aspect_ratio=1:1 level=40 color_primaries=bt709 field_order=progressive r_frame_rate=25/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 codec_name=h264 profile=High width=1920 height=1080 sample_aspect_ratio=1:1 level=40 color_primaries=bt709 field_order=progressive r_frame_rate=25/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 duration=7.680000 errors=0
[PASS] 1080p_low: 1920 1080, progressive, level 40, (25/1|30/1|30000/1001|24/1|24000/1001) fps, decodes without errors
--- dmesg step 8
[18524.311154] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18524.313901] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[18524.527782] elgato_gchd 1-2:1.0: transcoder defaults loaded
[18524.590169] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[18524.604290] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xbb75 samples6463=1 avg6463=0xb6d7 rgb=1
[18524.604296] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1920x1080 fps=50/1 interlaced=0 signal_present=1
[18524.604299] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1920x1080 fps=50/1 interlaced=0
[18524.604302] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1920x1080 fps=50/1 interlaced=0
[18525.402424] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[18526.591508] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[18526.606643] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18526.606857] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18526.608920] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18526.609481] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1920x1080 fps=50/1 interlaced=0
[18526.609962] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[18526.692730] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18531.612225] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=179 bytes=11730944 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18535.659468] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 9: HDMI source 720x576 at 50 Hz (or 720x480 at 60 Hz)
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
PROMPT: A window 'Elgato HDMI' shows the laptop's HDMI picture. In the laptop's display settings set the HDMI display (the Elgato) to 720x576 at 50 Hz (or 720x480 at 60 Hz). If the laptop offers neither, choose 640x480 and note it. If the picture freezes or disappears, close the window (it reopens). Press Enter when it shows the laptop in 720x576 at 50 Hz (or 720x480 at 60 Hz).
$ v4l2-ctl -d /dev/video0 --list-framesizes MPEG
ioctl: VIDIOC_ENUM_FRAMESIZES
	Size: Discrete 720x576
	Size: Discrete 640x480
	Size: Discrete 480x360
$ v4l2-ctl -d /dev/video0 --get-dv-timings
DV timings:
	Active width: 720
	Active height: 576
	Total width: 864
	Total height: 625
	Frame format: progressive
	Polarities: -vsync -hsync
	Pixelclock: 27000000 Hz (50.00 frames per second)
	Horizontal frontporch: 12
	Horizontal sync: 64
	Horizontal backporch: 68
	Vertical frontporch: 5
	Vertical sync: 5
	Vertical backporch: 39
	Standards: CTA-861
	Picture aspect: 4:3
	CTA-861 VIC: 17
	Flags: CE-video, has picture aspect, has CTA-861 VIC
--- dmesg step 9
[18536.759596] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18536.874837] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[18536.874838] elgato_gchd 1-2:1.0: waiting 285 ms after the previous shutdown
[18537.309164] elgato_gchd 1-2:1.0: transcoder defaults loaded
[18537.374148] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[18537.389972] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xbb75 samples6463=1 avg6463=0xb6d7 rgb=1
[18537.389977] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1920x1080 fps=50/1 interlaced=0 signal_present=1
[18537.389979] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1920x1080 fps=50/1 interlaced=0
[18537.389981] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1920x1080 fps=50/1 interlaced=0
[18538.147595] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[18539.328298] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[18539.343043] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18539.343277] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18539.345098] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18539.345549] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1920x1080 fps=50/1 interlaced=0
[18539.346201] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[18539.428889] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18544.348354] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=179 bytes=11730944 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=20
[18549.361845] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=379 bytes=24838144 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=22
[18554.375760] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=579 bytes=37945344 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=22
[18559.386825] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=779 bytes=51052544 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=22
[18564.400446] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=979 bytes=64159744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=22
[18569.414897] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1179 bytes=77266944 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=22
[18572.526179] elgato_gchd 1-2:1.0: HDMI source mode changed while capturing; detecting again
[18572.548839] elgato_gchd 1-2:1.0: signal present: input=0; first video TS packet on PID 0x1011
[18573.442248] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb88a samples6463=1 avg6463=0xb6d3 rgb=1
[18573.442253] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1920x1080 fps=60/1 interlaced=0 signal_present=1
[18573.442255] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1920x1080 fps=60/1 interlaced=0
[18573.442257] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1920x1080 fps=60/1 interlaced=0
[18574.671006] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[18574.687402] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18574.687631] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18574.688722] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18574.689235] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=720x576 fps=50/1 interlaced=0
[18574.689729] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[18574.773508] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18579.738954] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=33 bytes=2162688 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18584.777344] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=70 bytes=4587520 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18587.752457] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 10: SD progressive
$ v4l2-ctl -d /dev/video0 --get-dv-timings
DV timings:
	Active width: 720
	Active height: 576
	Total width: 864
	Total height: 625
	Frame format: progressive
	Polarities: -vsync -hsync
	Pixelclock: 27000000 Hz (50.00 frames per second)
	Horizontal frontporch: 12
	Horizontal sync: 64
	Horizontal backporch: 68
	Vertical frontporch: 5
	Vertical sync: 5
	Vertical backporch: 39
	Standards: CTA-861
	Picture aspect: 4:3
	CTA-861 VIC: 17
	Flags: CE-video, has picture aspect, has CTA-861 VIC
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ sh -c tr '\n' ' ' < /home/ff5/gchd-test-logs/T-050-samples/hdmi_sd.ts.ffprobe.txt; echo errors=$(wc -l < /home/ff5/gchd-test-logs/T-050-samples/hdmi_sd.ts.ffmpeg.txt)
codec_name=h264 profile=High width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 codec_name=h264 profile=High width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 duration=17.160000 errors=0
[PASS] sd: 720 (480|576), progressive, level 30, (50/1|60/1|60000/1001) fps, decodes without errors
$ v4l2-ctl -d /dev/video0 -c allow_50_60_fps=0
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ sh -c tr '\n' ' ' < /home/ff5/gchd-test-logs/T-050-samples/hdmi_sd_allow_off.ts.ffprobe.txt; echo errors=$(wc -l < /home/ff5/gchd-test-logs/T-050-samples/hdmi_sd_allow_off.ts.ffmpeg.txt)
codec_name=h264 profile=High width=720 height=576 sample_aspect_ratio=12:11 level=31 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 codec_name=h264 profile=High width=720 height=576 sample_aspect_ratio=12:11 level=31 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 duration=16.600000 errors=0
[PASS] sd_allow_off: 720 (480|576), progressive, level 31, (50/1|60/1|60000/1001) fps, decodes without errors
$ v4l2-ctl -d /dev/video0 -c allow_50_60_fps=1
--- dmesg step 10
[18590.828070] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18590.831331] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[18591.046301] elgato_gchd 1-2:1.0: transcoder defaults loaded
[18591.114855] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[18591.131217] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xbb75 samples6463=1 avg6463=0xb0c3 rgb=1
[18591.131221] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[18591.131223] elgato_gchd 1-2:1.0: signal detected: input=0 mode=720x576 fps=50/1 interlaced=0
[18591.131225] elgato_gchd 1-2:1.0: capture setup: input=0 mode=720x576 fps=50/1 interlaced=0
[18591.947185] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[18593.226330] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[18593.246356] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18593.246593] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18593.247439] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18593.247884] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=720x576 fps=50/1 interlaced=0
[18593.248516] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[18593.332386] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18598.302495] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=33 bytes=2162688 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18603.335525] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=70 bytes=4587520 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18608.394072] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=107 bytes=7012352 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18612.320642] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[18613.151271] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18613.153972] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[18613.153975] elgato_gchd 1-2:1.0: waiting 668 ms after the previous shutdown
[18613.966744] elgato_gchd 1-2:1.0: transcoder defaults loaded
[18614.029879] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[18614.044501] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xbb75 samples6463=1 avg6463=0xb0c3 rgb=1
[18614.044503] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[18614.044504] elgato_gchd 1-2:1.0: signal detected: input=0 mode=720x576 fps=50/1 interlaced=0
[18614.044506] elgato_gchd 1-2:1.0: capture setup: input=0 mode=720x576 fps=50/1 interlaced=0
[18614.817947] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[18615.997564] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[18616.013447] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18616.013668] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18616.014880] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18616.015613] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=720x576 fps=50/1 interlaced=0
[18616.016031] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[18616.099784] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18621.113579] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=36 bytes=2359296 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18626.139580] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=77 bytes=5046272 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18631.185275] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=118 bytes=7733248 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18634.630923] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 11: source mode change while capturing
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
PROMPT: While the 'Elgato HDMI' window shows the laptop, set the laptop's HDMI display to 1280x720 at 60 Hz (the picture may go away for a few seconds and comes back). Press Enter when it shows 720p again.
$ count_new HDMI source mode changed while capturing
1
[PASS] the driver followed the source change
$ sh -c sudo dmesg | tail -n +4 | grep -E 'HDMI|detected|failed' | tail -12
[18636.366372] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xbb75 samples6463=1 avg6463=0xb0c3 rgb=1
[18636.366375] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[18636.366377] elgato_gchd 1-2:1.0: signal detected: input=0 mode=720x576 fps=50/1 interlaced=0
[18660.862386] elgato_gchd 1-2:1.0: HDMI source mode changed while capturing; detecting again
[18662.403319] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb05c rgb=1
[18662.403326] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[18662.403330] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1280x720 fps=60/1 interlaced=0
--- dmesg step 11
[18635.463685] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18635.476474] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18635.587518] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[18635.587520] elgato_gchd 1-2:1.0: waiting 543 ms after the previous shutdown
[18636.287035] elgato_gchd 1-2:1.0: transcoder defaults loaded
[18636.352590] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[18636.366372] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xbb75 samples6463=1 avg6463=0xb0c3 rgb=1
[18636.366375] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[18636.366377] elgato_gchd 1-2:1.0: signal detected: input=0 mode=720x576 fps=50/1 interlaced=0
[18636.366379] elgato_gchd 1-2:1.0: capture setup: input=0 mode=720x576 fps=50/1 interlaced=0
[18637.155866] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[18638.430255] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[18638.445918] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18638.446140] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18638.447164] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18638.447694] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=720x576 fps=50/1 interlaced=0
[18638.448139] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[18638.531678] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18643.498911] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=33 bytes=2162688 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18648.536379] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=70 bytes=4587520 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=19
[18653.574791] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=107 bytes=7012352 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=20
[18658.573194] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=144 bytes=9437184 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=19
[18660.862386] elgato_gchd 1-2:1.0: HDMI source mode changed while capturing; detecting again
[18660.868109] elgato_gchd 1-2:1.0: signal present: input=0; first video TS packet on PID 0x1011
[18662.403319] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb05c rgb=1
[18662.403326] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[18662.403330] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1280x720 fps=60/1 interlaced=0
[18662.403340] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1280x720 fps=60/1 interlaced=0
[18663.667988] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[18663.690784] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18663.691147] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18663.692234] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18663.692810] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1280x720 fps=60/1 interlaced=0
[18663.693413] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[18663.766233] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18668.696900] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=221 bytes=14483456 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18671.509264] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 12: bitrate as one slider (driver, 720p)
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -c video_bitrate_mode=1,video_bitrate=20000000
$ rates
video_bitrate_mode: 1 video_bitrate: 20000000 
[PASS] 720p CBR 20 Mbit/s
$ v4l2-ctl -d /dev/video0 -c video_bitrate_mode=0,video_bitrate=19000000
$ rates
video_bitrate_mode: 0 video_bitrate: 19000000 
[PASS] 720p VBR 19 Mbit/s (9.50 shown)
$ v4l2-ctl -d /dev/video0 -c video_bitrate_mode=0,video_bitrate=25000000
$ rates
video_bitrate_mode: 0 video_bitrate: 19980000 
[PASS] VBR above 19.98 Mbit/s gives 19.98
$ v4l2-ctl -d /dev/video0 -c video_bitrate_mode=1,video_bitrate=10000000
$ rates
video_bitrate_mode: 1 video_bitrate: 10000000 
[PASS] CBR 10 Mbit/s (10.00, the first CBR value)
--- dmesg step 12


=== step 13: settings window
PROMPT: Open 'Elgato Game Capture HD Settings' (Input HDMI).
[PASS] Rows: Output size (1920... source size, 640x480, 480x360), Bitrate (one slider, no Bitrate mode row), Allow 50/60 fps, HDMI color range; the slider ends at 26.25?
PROMPT: Move the slider to about 9.50 and release it; wait 3 s.
$ rates
video_bitrate_mode: 0 video_bitrate: 18960000 
[PASS] window 9.50: VBR 19 Mbit/s
PROMPT: Move the slider to about 15.00 and release it; wait 3 s.
$ rates
video_bitrate_mode: 1 video_bitrate: 15070000 
[PASS] window 15.00: CBR 15 Mbit/s
PROMPT: Choose HDMI color range Expanded; wait 3 s.
$ v4l2-ctl -d /dev/video0 -C hdmi_color_range
hdmi_color_range: 1 (Expanded)
[PASS] window: Expanded
PROMPT: Choose Standard again; choose Output size 480x360; wait 3 s.
[PASS] 480x360: the slider goes from 2.00 to 7.50 (VBR)?
PROMPT: Choose the source size again; close the window.
--- dmesg step 13


=== step 14: OBS plugin
PROMPT: Start OBS (source 'Elgato Game Capture HD', input HDMI); open the source's properties.
[PASS] OBS: Output size, Bitrate (one slider, at about 15.00), Allow 50/60 fps and HDMI color range are shown, no Bitrate mode?
PROMPT: Move the bitrate slider to about 8.00 -> OK; wait 3 s.
$ rates
video_bitrate_mode: 0 video_bitrate: 16040000 
[PASS] OBS 8.00: VBR 16 Mbit/s
PROMPT: Close OBS.
$ obs_log 6
12:30:03.705: [gchd] properties: device settings read: input 0, output 0, mode 0, 8.07 Mbit/s
12:30:11.246: [gchd] settings applied: input 0, output 0, mode 0, 7.76 Mbit/s, profile 4, stretch 0
12:30:12.253: [gchd] settings applied: input 0, output 0, mode 0, 8.11 Mbit/s, profile 4, stretch 0
12:30:13.057: [gchd] settings applied: input 0, output 0, mode 0, 7.98 Mbit/s, profile 4, stretch 0
12:30:14.367: [gchd] settings applied: input 0, output 0, mode 0, 8.02 Mbit/s, profile 4, stretch 0
12:30:41.549: [gchd] capture stopped (source hidden or settings changed)
--- dmesg step 14
[18867.796824] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[18867.893038] elgato_gchd 1-2:1.0: transcoder defaults loaded
[18867.965716] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[18867.984375] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba94 samples6463=1 avg6463=0xb05c rgb=1
[18867.984382] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[18867.984384] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1280x720 fps=60/1 interlaced=0
[18867.984386] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1280x720 fps=60/1 interlaced=0
[18868.870717] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[18870.337394] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[18870.358611] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18870.358928] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18870.359505] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18870.359998] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1280x720 fps=60/1 interlaced=0
[18870.360469] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[18870.431848] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18875.381020] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=141 bytes=9240576 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18880.391545] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=293 bytes=19202048 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18885.402110] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=445 bytes=29163520 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18890.412484] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=597 bytes=39124992 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18895.422901] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=749 bytes=49086464 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18900.433765] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=901 bytes=59047936 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18905.443618] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1053 bytes=69009408 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18910.452846] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1205 bytes=78970880 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18912.713205] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[18913.637274] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18913.638691] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18913.639605] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18913.640353] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1280x720 fps=60/1 interlaced=0
[18913.641269] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[18913.710373] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18913.710376] elgato_gchd 1-2:1.0: HDMI output now 0
[18918.695327] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=53 bytes=3473408 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18920.250821] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[18921.987115] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18921.987472] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18921.988506] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18921.989080] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1280x720 fps=60/1 interlaced=0
[18921.989636] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[18922.061794] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18922.061797] elgato_gchd 1-2:1.0: HDMI output now 0
[18922.061799] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[18924.559082] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18924.559500] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18924.560719] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18924.561381] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1280x720 fps=60/1 interlaced=0
[18924.561862] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[18924.633755] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18924.633760] elgato_gchd 1-2:1.0: HDMI output now 0
[18929.619529] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=53 bytes=3473408 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18934.645336] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=112 bytes=7340032 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18939.649150] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=172 bytes=11272192 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18944.653439] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=232 bytes=15204352 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18949.674341] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=287 bytes=18808832 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=3
[18950.537889] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 15: composite: bitrate ranges of the slider, then HDMI after composite
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -v width=720,height=480,pixelformat=MPEG
$ range_of
6000000-19980000 11250000
[PASS] composite source size: 6 to 19.98 Mbit/s, highest 11.25
$ v4l2-ctl -d /dev/video0 -v width=480,height=360,pixelformat=MPEG
$ range_of
4000000-15000000 15000000
[PASS] composite 480x360: 4 to 15 Mbit/s
$ v4l2-ctl -d /dev/video0 -v width=720,height=480,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ sh -c timeout -k 5 20 head -c 6000000 < /dev/video0 > /tmp/gchd-c.ts; ffprobe -v error -select_streams v:0 -show_entries stream=width,height -of default=nw=1 /tmp/gchd-c.ts | tr '\n' ' '
/tmp/gchd-c.ts: Invalid data found when processing input
[FAIL] composite capture (PS3) before HDMI (expected /width=720 height=(480|576)/, got: /tmp/gchd-c.ts: Invalid data found when processing input)
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ sh -c tr '\n' ' ' < /home/ff5/gchd-test-logs/T-050-samples/hdmi_after_composite.ts.ffprobe.txt; echo errors=$(wc -l < /home/ff5/gchd-test-logs/T-050-samples/hdmi_after_composite.ts.ffmpeg.txt)
codec_name=h264 profile=High width=1280 height=720 sample_aspect_ratio=1:1 level=40 color_primaries=bt709 field_order=progressive r_frame_rate=60/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 codec_name=h264 profile=High width=1280 height=720 sample_aspect_ratio=1:1 level=40 color_primaries=bt709 field_order=progressive r_frame_rate=60/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 duration=15.166667 errors=0
[PASS] after_composite: 1280 720, progressive, level 40, (60/1|60000/1001) fps, decodes without errors
--- dmesg step 15
[18952.564539] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[18952.564573] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[18952.588939] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[18952.591591] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[18952.727175] elgato_gchd 1-2:1.0: transcoder defaults loaded
[18952.785837] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[18972.658575] elgato_gchd 1-2:1.0: shutdown: device in STOP after RESET; IDLE and RESET again
[18972.674530] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0011 before, 0x0012 now)
[18972.728955] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=0 streaming=0
[18972.728999] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[18972.737222] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[18972.737258] elgato_gchd 1-2:1.0: waiting 1438 ms after the previous shutdown
[18974.922805] elgato_gchd 1-2:1.0: transcoder defaults loaded
[18974.989303] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[18975.006499] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb05c rgb=1
[18975.006504] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[18975.006506] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1280x720 fps=60/1 interlaced=0
[18975.006508] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1280x720 fps=60/1 interlaced=0
[18975.805333] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[18977.023813] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[18977.040446] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18977.040653] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18977.041545] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18977.042010] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1280x720 fps=60/1 interlaced=0
[18977.042466] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[18977.113126] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18982.101599] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=53 bytes=3473408 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18987.128560] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=112 bytes=7340032 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18992.128106] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=171 bytes=11206656 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18994.186923] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 16: v4l2-compliance
$ sh -c v4l2-compliance -d /dev/video0 2>&1 | tail -3
	test Requests: OK (Not Supported)

Total for elgato-gchd device /dev/video0: 115, Succeeded: 115, Failed: 0, Warnings: 4
$ sh -c v4l2-compliance -d /dev/video0 2>&1 | grep -E '^Total'
Total for elgato-gchd device /dev/video0: 115, Succeeded: 114, Failed: 1, Warnings: 4
[FAIL] compliance: 0 failures (expected /Failed: 0/, got: Total for elgato-gchd device /dev/video0: 115, Succeeded: 114, Failed: 1, Warnings: 4)
$ ls -l /home/ff5/gchd-test-logs/T-050-samples
total 298816
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
PROMPT: Upload the folder /home/ff5/gchd-test-logs/T-050-samples to the traces branch as traces/hdmi/samples/ (the .ts, .raw, .ffprobe.txt, .ffmpeg.txt files).
--- dmesg step 16
[18995.452669] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18995.452677] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[18995.452719] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[18995.452728] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[18995.452741] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[18995.452744] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[18995.452755] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[18995.452759] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[18995.452813] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[18995.452818] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18995.452823] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[18995.452841] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[18995.452846] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[18995.452862] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[18995.452866] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[18995.452878] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[18995.452931] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[18995.452942] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[18995.452949] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[18995.452959] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[18995.452964] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[18995.452974] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[18995.452983] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[18995.452994] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[18995.453002] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[18995.453012] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[18995.453042] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[18995.453053] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[18995.453082] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[18995.453094] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[18995.453098] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[18995.453110] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[18995.453114] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[18995.453127] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[18995.453130] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[18995.453141] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[18995.453145] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[18995.453155] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[18995.453159] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[18995.453169] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[18995.453173] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[18995.453183] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[18995.453198] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[18995.453209] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[18995.453214] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[18995.453228] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[18995.453232] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[18995.453243] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[18995.453248] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[18995.453283] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[18995.454691] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[18995.454742] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[18995.455795] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[18995.455810] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[18995.456875] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[18995.456883] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[18995.457793] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[18995.457807] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[18995.457840] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18995.457845] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18995.463138] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18995.463141] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[18995.463169] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[18995.463173] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[18995.463182] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[18995.463184] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[18995.463193] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[18995.463196] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[18995.463204] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[18995.463206] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18995.463208] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[18995.463216] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[18995.463219] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[18995.463227] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[18995.463229] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[18995.463236] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[18995.463242] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[18995.463261] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[18995.463266] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[18995.463274] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[18995.463277] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[18995.463284] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[18995.463295] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[18995.463302] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[18995.463307] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[18995.463314] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[18995.463321] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18995.463323] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[18995.463330] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[18995.463332] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[18995.463339] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[18995.463341] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[18995.463348] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[18995.463351] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[18995.463359] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[18995.463367] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[18995.463375] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[18995.463378] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[18995.463386] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[18995.463388] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[18995.463396] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[18995.463399] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[18995.463406] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[18995.466404] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[18995.466414] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[18995.469048] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[18995.469058] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[18995.471073] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[18995.471082] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[18995.472859] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[18995.472868] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[18995.472897] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[18995.472902] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0

=== summary (T-050, 66d891a T-050 findings: HDMI source was a laptop; back end and HDMI (evidence, cold start and HDMI after composite in the test); 1080i and 24 Hz not covered)
PASS | step 3 720p | 720p: 1280 720, progressive, level 40, (60/1|60000/1001) fps, decodes without errors
PASS | step 3 720p | 720p: 1280x720, 640x480, 480x360 offered (no scaled 1280x720)
PASS | step 3 720p | 720p_allow_off: 1280 720, progressive, level 31, (30/1|30000/1001) fps, decodes without errors
PASS | step 3 720p | 720p_640x480: 640 480, progressive, level 30, (30/1|30000/1001) fps, decodes without errors
PASS | step 5 1080p | 1080p: 1920 1080, progressive, level 40, (30/1|30000/1001) fps, decodes without errors
PASS | step 5 1080p | 1080p_1280x720: 1280 720, progressive, level 31, (30/1|30000/1001) fps, decodes without errors
PASS | step 5 1080p | 1080p_480x360: 480 360, progressive, level 22, (30/1|30000/1001) fps, decodes without errors
FAIL | step 5 1080p | 1080p: 16-30 Mbit/s range covers VBR from 8.00 and CBR to 30.00 (expected /^10000000-30000000 30000000$/, got: 4000000-15000000 15000000)
PASS | step 6 HDMI Color Range while capturing (1080p) | With Expanded the picture became more contrasty (blacks darker, whites brighter) at once, without the picture stopping?
PASS | step 6 HDMI Color Range while capturing (1080p) | Back to Standard, the picture is as before?
PASS | step 6 HDMI Color Range while capturing (1080p) | no encoder restart for the colour range
PASS | step 8 1080p at 50 Hz (or the lower rate) | 1080p_low: 1920 1080, progressive, level 40, (25/1|30/1|30000/1001|24/1|24000/1001) fps, decodes without errors
PASS | step 10 SD progressive | sd: 720 (480|576), progressive, level 30, (50/1|60/1|60000/1001) fps, decodes without errors
PASS | step 10 SD progressive | sd_allow_off: 720 (480|576), progressive, level 31, (50/1|60/1|60000/1001) fps, decodes without errors
PASS | step 11 source mode change while capturing | the driver followed the source change
PASS | step 12 bitrate as one slider (driver, 720p) | 720p CBR 20 Mbit/s
PASS | step 12 bitrate as one slider (driver, 720p) | 720p VBR 19 Mbit/s (9.50 shown)
PASS | step 12 bitrate as one slider (driver, 720p) | VBR above 19.98 Mbit/s gives 19.98
PASS | step 12 bitrate as one slider (driver, 720p) | CBR 10 Mbit/s (10.00, the first CBR value)
PASS | step 13 settings window | Rows: Output size (1920... source size, 640x480, 480x360), Bitrate (one slider, no Bitrate mode row), Allow 50/60 fps, HDMI color range; the slider ends at 26.25?
PASS | step 13 settings window | window 9.50: VBR 19 Mbit/s
PASS | step 13 settings window | window 15.00: CBR 15 Mbit/s
PASS | step 13 settings window | window: Expanded
PASS | step 13 settings window | 480x360: the slider goes from 2.00 to 7.50 (VBR)?
PASS | step 14 OBS plugin | OBS: Output size, Bitrate (one slider, at about 15.00), Allow 50/60 fps and HDMI color range are shown, no Bitrate mode?
PASS | step 14 OBS plugin | OBS 8.00: VBR 16 Mbit/s
PASS | step 15 composite: bitrate ranges of the slider, then HDMI after composite | composite source size: 6 to 19.98 Mbit/s, highest 11.25
PASS | step 15 composite: bitrate ranges of the slider, then HDMI after composite | composite 480x360: 4 to 15 Mbit/s
FAIL | step 15 composite: bitrate ranges of the slider, then HDMI after composite | composite capture (PS3) before HDMI (expected /width=720 height=(480|576)/, got: /tmp/gchd-c.ts: Invalid data found when processing input)
PASS | step 15 composite: bitrate ranges of the slider, then HDMI after composite | after_composite: 1280 720, progressive, level 40, (60/1|60000/1001) fps, decodes without errors
FAIL | step 16 v4l2-compliance | compliance: 0 failures (expected /Failed: 0/, got: Total for elgato-gchd device /dev/video0: 115, Succeeded: 114, Failed: 1, Warnings: 4)
