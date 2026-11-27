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

## Maintainer comments

My PS3 can't connect to to the GCHD via HDMI. It struggles with HDCP the whole time. So I guess 1080i via HDMI is to be tested but we can assume that what we got from the C++ implementation for 1080i via HDMI is going to work.

gchd-test T-050 20261127-131803 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-050-20261127-131803.log
== build
$ git fetch -q origin
$ git checkout -q feat/hdmi
$ git pull -q --ff-only
commit: 4b76eda TASK: T-050 first run (31 PASS, 3 FAIL: test order, no composite signal, compliance on a second run)
commit: 4b76eda TASK: T-050 first run (31 PASS, 3 FAIL: test order, no composite signal, compliance on a second run)
build rc=0 warnings=0

=== step 1: install
$ sudo rmmod elgato_gchd
$ bash /home/ff5/elgato-gchd/install-driver.sh --core --yes --in-terminal
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
   - No original module exists within this kernel
 - Installation
   - Installing to /lib/modules/6.8.0-146-generic/updates/dkms/
depmod...
[32mInstalled with DKMS (rebuilt automatically after kernel updates).[0m
[32mudev rules installed (settings are remembered; device name /dev/elgato-gchd).[0m
[32mDriver loaded. Plug in the Game Capture HD; it appears as a /dev/video device.[0m
Watch: v4l2-ctl -d /dev/elgato-gchd -v pixelformat=MPEG && ffplay -f mpegts /dev/elgato-gchd

PROMPT: Laptop on HDMI at 1920x1080 60 Hz; PS3 on composite (on, showing its menu). Unplug the Elgato and plug it in again; wait 5 s.
--- dmesg step 1
[21802.229000] usbcore: deregistering interface driver elgato_gchd
[21812.626096] usbcore: registered new interface driver elgato_gchd
[21903.131815] usb 1-2: new high-speed USB device number 12 using xhci_hcd
[21903.509464] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[21903.509486] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[21903.509491] usb 1-2: Product: GameCapture HD
[21903.509496] usb 1-2: Manufacturer: Elgato
[21903.509500] usb 1-2: SerialNumber: 04107F1267
[21903.516958] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[21903.521262] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[21949.698712] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[21949.849118] elgato_gchd 1-2:1.0: transcoder defaults loaded
[21949.912677] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[21949.927630] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba94 samples6463=1 avg6463=0xb05c rgb=1
[21949.927636] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[21949.927638] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1280x720 fps=60/1 interlaced=0
[21949.927640] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1280x720 fps=60/1 interlaced=0
[21950.775101] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[21951.360271] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb05c rgb=1
[21951.360281] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1280x720 fps=60/1 interlaced=0 signal_present=1
[21951.360285] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1280x720 fps=60/1 interlaced=0
[21951.360288] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1280x720 fps=60/1 interlaced=0
[21952.564685] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[21952.588391] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[21952.590528] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[21952.591607] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[21952.592459] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1280x720 fps=60/1 interlaced=0
[21952.592943] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x000a
[21952.661218] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[21957.649283] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=53 bytes=3473408 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21962.673732] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=112 bytes=7340032 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21963.905842] elgato_gchd 1-2:1.0: HDMI source mode changed while capturing; detecting again
[21963.927608] elgato_gchd 1-2:1.0: signal present: input=0; first video TS packet on PID 0x1011
[21965.410728] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb6d7 rgb=1
[21965.410741] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1920x1080 fps=60/1 interlaced=0 signal_present=1
[21965.410743] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1920x1080 fps=60/1 interlaced=0
[21965.410745] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1920x1080 fps=60/1 interlaced=0
[21966.602840] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[21966.620358] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[21966.620724] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[21966.625433] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[21966.625993] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1920x1080 fps=60/1 interlaced=0
[21966.629885] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x000a
[21966.695066] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[21971.657534] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21973.230750] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=1 streaming=1
[21973.969210] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[21974.185174] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x26 masked_mode=0x6
[21974.187832] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[21974.261379] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x36 masked_mode=0x6
[21974.265559] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[21974.333330] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x36 masked_mode=0x6
[21974.335667] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[21974.335670] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[21974.335673] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[21974.335675] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[21974.482644] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[21974.510297] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[21974.510554] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[21974.540502] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[21974.541472] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[21974.545289] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[21974.649669] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[21979.574484] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21984.598696] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21989.624043] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21994.647486] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21999.669881] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[22004.691891] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=680 bytes=44564480 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[22009.715512] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=795 bytes=52101120 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[22014.104882] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=1 streaming=1
[22014.786006] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=904 bytes=59244544 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=8
[22014.943147] elgato_gchd 1-2:1.0: input selected: input=0; detection scheduled
[22014.970266] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb6d7 rgb=1
[22014.970275] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1920x1080 fps=60/1 interlaced=0 signal_present=1
[22014.970278] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1920x1080 fps=60/1 interlaced=0
[22014.970280] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1920x1080 fps=60/1 interlaced=0
[22016.592593] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[22016.625008] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[22016.625444] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[22016.626682] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[22016.627724] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1920x1080 fps=60/1 interlaced=0
[22016.628933] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x000a
[22016.699015] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[22021.662003] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[22026.697512] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[22031.732812] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[22034.257979] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 2: composite with a signal, then HDMI
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -v width=720,height=480,pixelformat=MPEG
PROMPT: A window 'Elgato composite' should show the PS3 (composite). If it stays black, check the PS3's composite cable and that the PS3 shows its menu. Press Enter when the window shows the PS3.
[PASS] The 'Elgato composite' window shows the PS3?
$ sh -c sudo dmesg | grep -E 'configuring|analog mode|signal detected' | tail -3
[22045.240364] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[22045.240370] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
$ v4l2-ctl -d /dev/video0 -v width=1920,height=1080,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ sh -c tr '\n' ' ' < /home/ff5/gchd-test-logs/T-050-samples/hdmi_after_composite_1080p.ts.ffprobe.txt; echo errors=$(wc -l < /home/ff5/gchd-test-logs/T-050-samples/hdmi_after_composite_1080p.ts.ffmpeg.txt)
codec_name=h264 profile=High width=1280 height=720 sample_aspect_ratio=1:1 level=41 color_primaries=bt709 field_order=progressive r_frame_rate=30/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 codec_name=h264 profile=High width=1280 height=720 sample_aspect_ratio=1:1 level=41 color_primaries=bt709 field_order=progressive r_frame_rate=30/1 codec_name=mp2 profile=unknown sample_rate=48000 channels=2 r_frame_rate=0/0 duration=13.333333 errors=0
[FAIL] after_composite_1080p: 1920 1080, progressive, level 40, (30/1|30000/1001) fps, decodes without errors (expected /width=1920 height=1080 .*level=40 .*field_order=progressive r_frame_rate=(30/1|30000/1001) .*errors=0/, got: codec_name=h264 profile=High width=1280 height=720 sample_aspect_ratio=1:1 level=41 color_primaries=bt709 field_order=progressive r_frame_rate=30/1 codec_name=m)
$ range_of
8000000-19980000 15000000
[FAIL] 1080p: bitrate range 10-30 Mbit/s, highest 30 (expected /^10000000-30000000 30000000$/, got: 8000000-19980000 15000000)
--- dmesg step 2
[22044.391186] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[22044.391224] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[22044.396559] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[22044.522979] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[22044.659747] elgato_gchd 1-2:1.0: transcoder defaults loaded
[22044.724082] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[22044.827020] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[22045.026522] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[22045.120605] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[22045.152161] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[22045.154623] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[22045.237874] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[22045.240364] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[22045.240367] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[22045.240370] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[22045.240373] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[22046.049447] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[22046.196344] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[22046.212898] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[22046.213114] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[22046.236490] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[22046.237324] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[22046.237819] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[22046.352206] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[22051.278945] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=23
[22056.300070] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[22061.322746] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[22066.346897] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[22071.370002] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[22075.300372] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[22078.328062] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=0 streaming=0
[22078.328090] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[22078.330614] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[22078.427785] elgato_gchd 1-2:1.0: transcoder defaults loaded
[22078.489449] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[22078.504090] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb6d7 rgb=1
[22078.504095] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1920x1080 fps=60/1 interlaced=0 signal_present=1
[22078.504098] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1920x1080 fps=60/1 interlaced=0
[22078.504100] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1920x1080 fps=60/1 interlaced=0
[22079.286755] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[22079.830996] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb6d7 rgb=1
[22079.831014] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1920x1080 fps=60/1 interlaced=0 signal_present=1
[22079.831023] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1920x1080 fps=60/1 interlaced=0
[22079.831029] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1920x1080 fps=60/1 interlaced=0
[22081.013406] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[22081.028373] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[22081.028574] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[22081.029725] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[22081.030469] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1920x1080 fps=60/1 interlaced=0
[22081.030985] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[22081.101319] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[22086.067194] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[22091.103633] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[22095.652322] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 3: v4l2-compliance (full output)
$ sh -c grep -E 'fail|FAIL|warn' /home/ff5/gchd-test-logs/T-050-compliance.txt | head -20; grep -E '^Total' /home/ff5/gchd-test-logs/T-050-compliance.txt
		fail: v4l2-test-io-config.cpp(226): fmt.fmt.pix.width < enumtimings.timings.bt.width
		fail: v4l2-test-io-config.cpp(386): Timings check failed for input 0.
	test VIDIOC_ENUM/G/S/QUERY_DV_TIMINGS: FAIL
		warn: v4l2-test-buffers.cpp(693): VIDIOC_CREATE_BUFS not supported
		warn: v4l2-test-buffers.cpp(693): VIDIOC_CREATE_BUFS not supported
		warn: v4l2-test-buffers.cpp(693): VIDIOC_CREATE_BUFS not supported
		warn: v4l2-test-buffers.cpp(693): VIDIOC_CREATE_BUFS not supported
Total for elgato-gchd device /dev/video0: 115, Succeeded: 114, Failed: 1, Warnings: 4
$ grep -E ^Total /home/ff5/gchd-test-logs/T-050-compliance.txt
Total for elgato-gchd device /dev/video0: 115, Succeeded: 114, Failed: 1, Warnings: 4
[FAIL] compliance run 1: 0 failures (expected /Failed: 0/, got: Total for elgato-gchd device /dev/video0: 115, Succeeded: 114, Failed: 1, Warnings: 4)
$ sh -c grep -E 'fail|FAIL|warn' /home/ff5/gchd-test-logs/T-050-compliance.txt.2 | head -20; grep -E '^Total' /home/ff5/gchd-test-logs/T-050-compliance.txt.2
		fail: v4l2-test-io-config.cpp(226): fmt.fmt.pix.width < enumtimings.timings.bt.width
		fail: v4l2-test-io-config.cpp(386): Timings check failed for input 0.
	test VIDIOC_ENUM/G/S/QUERY_DV_TIMINGS: FAIL
		warn: v4l2-test-buffers.cpp(693): VIDIOC_CREATE_BUFS not supported
		warn: v4l2-test-buffers.cpp(693): VIDIOC_CREATE_BUFS not supported
		warn: v4l2-test-buffers.cpp(693): VIDIOC_CREATE_BUFS not supported
		warn: v4l2-test-buffers.cpp(693): VIDIOC_CREATE_BUFS not supported
Total for elgato-gchd device /dev/video0: 115, Succeeded: 114, Failed: 1, Warnings: 4
$ grep -E ^Total /home/ff5/gchd-test-logs/T-050-compliance.txt.2
Total for elgato-gchd device /dev/video0: 115, Succeeded: 114, Failed: 1, Warnings: 4
[FAIL] compliance run 2 (right after): 0 failures (expected /Failed: 0/, got: Total for elgato-gchd device /dev/video0: 115, Succeeded: 114, Failed: 1, Warnings: 4)
--- dmesg step 3
[22096.627413] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[22096.627417] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[22096.627452] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[22096.627458] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[22096.627470] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[22096.627474] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[22096.627485] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[22096.627488] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[22096.627499] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[22096.627502] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[22096.627504] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[22096.627513] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[22096.627516] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[22096.627524] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[22096.627527] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[22096.627536] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[22096.627550] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[22096.627562] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[22096.627567] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[22096.627576] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[22096.627580] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[22096.627589] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[22096.627596] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[22096.627605] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[22096.627612] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[22096.627621] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[22096.627630] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[22096.627632] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[22096.627641] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[22096.627644] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[22096.627654] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[22096.627656] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[22096.627668] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[22096.627671] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[22096.627680] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[22096.627690] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[22096.627700] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[22096.627703] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[22096.627714] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[22096.627717] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[22096.627727] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[22096.627731] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[22096.627741] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[22096.631266] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[22096.631283] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[22096.633466] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[22096.633483] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[22096.635303] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[22096.635317] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[22096.639000] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[22096.639015] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[22096.639051] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[22096.639058] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[22096.655124] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[22096.655127] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[22096.655174] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[22096.655178] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[22096.655188] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[22096.655191] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[22096.655198] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[22096.655201] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[22096.655208] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[22096.655210] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[22096.655212] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[22096.655219] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[22096.655221] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[22096.655229] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[22096.655231] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[22096.655238] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[22096.655248] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[22096.655256] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[22096.655267] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[22096.655274] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[22096.655277] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[22096.655284] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[22096.655290] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[22096.655297] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[22096.655302] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[22096.655309] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[22096.655315] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[22096.655317] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[22096.655324] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[22096.655327] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[22096.655337] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[22096.655339] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[22096.655346] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[22096.655349] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[22096.655356] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[22096.655366] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[22096.655373] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[22096.655376] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[22096.655385] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[22096.655388] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[22096.655402] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[22096.655406] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[22096.655414] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[22096.657230] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[22096.657239] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[22096.658757] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[22096.658765] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[22096.660744] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[22096.660754] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[22096.663063] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[22096.663072] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[22096.663096] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[22096.663101] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0

=== summary (T-050, 4b76eda TASK: T-050 first run (31 PASS, 3 FAIL: test order, no composite signal, compliance on a second run))
PASS | step 2 composite with a signal, then HDMI | The 'Elgato composite' window shows the PS3?
FAIL | step 2 composite with a signal, then HDMI | after_composite_1080p: 1920 1080, progressive, level 40, (30/1|30000/1001) fps, decodes without errors (expected /width=1920 height=1080 .*level=40 .*field_order=progressive r_frame_rate=(30/1|30000/1001) .*errors=0/, got: codec_name=h264 profile=High width=1280 height=720 sample_aspect_ratio=1:1 level=41 color_primaries=bt709 field_order=progressive r_frame_rate=30/1 codec_name=m)
FAIL | step 2 composite with a signal, then HDMI | 1080p: bitrate range 10-30 Mbit/s, highest 30 (expected /^10000000-30000000 30000000$/, got: 8000000-19980000 15000000)
FAIL | step 3 v4l2-compliance (full output) | compliance run 1: 0 failures (expected /Failed: 0/, got: Total for elgato-gchd device /dev/video0: 115, Succeeded: 114, Failed: 1, Warnings: 4)
FAIL | step 3 v4l2-compliance (full output) | compliance run 2 (right after): 0 failures (expected /Failed: 0/, got: Total for elgato-gchd device /dev/video0: 115, Succeeded: 114, Failed: 1, Warnings: 4)
