# TASK T-065 — Analog robustness: console swap, start-up, signal loss

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** implemented (1904bc3, 743919d, dea13c8); hardware test with the test script
- **Type:** driver fixes
- **Branch:** `fix/analog-robustness` (from `dev` 0.5.5)
- **TODO items:** T-065, T-067, T-054 (stage 1)

## Why

The last small stage-1 issues with composite and S-video:

- **T-065:** swapping consoles while capturing without a detected signal
  drop (N64 → running PS1, `0x88:FA` stayed `6`): on S-video one restart
  of about 7 s, on composite two restarts (240p, then 576i).
- **T-067:** the firmware start-up failed twice (`state-change timeout:
  current=0x10 expected=0x11 state=0x0011 completion=0x0008`): IDLE is
  reached but the completion flag is not set; each time after a session
  that ended with a failed START.
- **T-054:** source switched off and on while capturing: worked by report,
  never checked in the logs.

## Implementation

1. T-065: the family change also waits out the 60 Hz state when
   `0x88:CA` is not locked at the trigger (console swap without a drop).
2. T-067: a state change whose state is reached without the completion
   flag is accepted after the wait (acknowledged, warning logged).
3. T-054: test steps (source off/on while capturing, NTSC and PAL).
4. 288p capture start as recorded (R1, dea13c8): wait 0.4 s and read
   `E7`/`F1` after the apply block.
5. From the new recordings (743919d): `system_rate` refit (all 15 values),
   GOP 8 for the NTSC scaled outputs, `v_vinpelclk` 1 for 50 Hz and 0 for
   60 Hz at every size.

## Hardware test (maintainer)

PS1 and N64 on composite; the script tells you when to swap, switch off
and on. "signal lost" warnings in the off/on steps are expected.

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-065.sh > tests/T-065.sh
bash gchd-test.sh T-065 --branch fix/analog-robustness
```

Paste the summary; the log too if anything fails.

## Findings
- Maintainer recordings 2026-11-18 (now `traces/composite/`):
  `gchd_PAL_composite_bitrate_audio_small` (PS1, start with the source on,
  640×480, bitrate max/min/middle/max, audio gain) and
  `gchd_NTSC_composite_output_bitrate_audio_small` (PS2 480i, every output
  size with bitrate min/middle/max, audio gain). Results: `system_rate`
  refit for PAL and NTSC; the slider shows the VBR average, its top is
  CBR 11250; GOP 8 for NTSC scaled outputs; `v_vinpelclk` follows the
  family; audio gain = `0x94:4A/4B` (+ `41` 37/57), `composite.md` §11;
  480i/576i start writes the apply block (known differences). The PAL
  start was 576i, so R1 (288p start) is still open: start the capture
  while the game shows a 240p/288p screen.

## Maintainer comments

gchd-test T-065 20261118-142723 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-065-20261118-142723.log
== build
$ git fetch -q origin
$ git checkout -q fix/analog-robustness
$ git pull -q --ff-only
commit: c0c408e TASK: 288p start in the T-065 test
commit: c0c408e TASK: 288p start in the T-065 test
build rc=0 warnings=0

=== step 1: load module (fresh plug-in)
PROMPT: Unplug the Elgato from the PC (or detach it in VirtualBox), wait 3 s, plug it in again.
$ sudo insmod /home/ff5/elgato-gchd/dkms/elgato_gchd.ko
module: 0.5.5 
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
--- dmesg step 1
[73009.228146] usbcore: deregistering interface driver elgato_gchd
[73033.359778] usb 1-2: new high-speed USB device number 76 using xhci_hcd
[73033.771333] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[73033.771339] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[73033.771340] usb 1-2: Product: GameCapture HD
[73033.771342] usb 1-2: Manufacturer: Elgato
[73033.771343] usb 1-2: SerialNumber: 04107F1267
[73036.190603] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[73036.193967] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[73036.194139] usbcore: registered new interface driver elgato_gchd
[73036.204865] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[73036.204892] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[73036.204894] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0

=== step 2: PS1 standard size (PAL, v_vinpelclk now 1)
PROMPT: PS1 on composite, game running.
$ v4l2-ctl -d /dev/video0 -v width=720,height=576,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -c video_bitrate_mode=1,video_bitrate=11250000
capturing 10s to step2-21731.ts
capture: 10989568 bytes, ~8791 kbit/s; codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 ; decode errors 1
  codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 ; ~8791 kbit/s; decode errors 1
audio: mp2
[PASS] stream 720x576 50/1 tt, ~8791 kbit/s
--- dmesg step 2
[73080.758532] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[73080.898173] elgato_gchd 1-2:1.0: transcoder defaults loaded
[73080.958954] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[73081.248894] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[73081.358706] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[73081.435099] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[73081.436970] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 288p
[73081.436972] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[73081.436974] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=0
[73081.436975] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=0
[73082.190943] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[73082.869231] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[73082.883848] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[73082.884079] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[73082.889768] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[73082.890230] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=0
[73082.890708] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[73083.029739] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[73087.899365] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[73091.768310] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 3: PS1 640x480
$ v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
capturing 10s to step3-6126.ts
capture: 6279168 bytes, ~5023 kbit/s; codec_name=h264 width=640 height=480 field_order=progressive r_frame_rate=25/1 ; decode errors 1
  codec_name=h264 width=640 height=480 field_order=progressive r_frame_rate=25/1 ; ~5023 kbit/s; decode errors 1
audio: mp2
[PASS] stream 640x480 25/1 progressive, ~5023 kbit/s
--- dmesg step 3
[73092.341995] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[73092.341998] elgato_gchd 1-2:1.0: waiting 927 ms after the previous shutdown
[73093.436388] elgato_gchd 1-2:1.0: transcoder defaults loaded
[73093.504676] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[73093.640656] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[73093.823834] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[73093.826290] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[73093.905812] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[73093.938190] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[73093.940193] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 288p
[73093.940195] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[73093.940198] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=0
[73093.940199] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=0
[73094.731571] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[73095.607739] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[73095.622663] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[73095.622971] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[73095.628352] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[73095.629172] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=0
[73095.629638] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[73095.748413] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[73096.057987] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[73097.634012] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[73097.634221] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[73097.639783] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[73097.640412] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=0
[73097.640857] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[73097.759941] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[73097.759943] elgato_gchd 1-2:1.0: analog mode now 288p, output 1
[73098.152839] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[73102.674652] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=8
[73103.308353] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 4: N64 640x480 (NTSC: GOP 8, v_vinpelclk 0)
PROMPT: Replace the PS1 with the N64 on composite (menu).
capturing 10s to step4-18717.ts
[FAIL] capture: no data
--- dmesg step 4
[73326.469567] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[73326.909634] elgato_gchd 1-2:1.0: transcoder defaults loaded
[73326.978160] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[73327.100408] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[73336.541248] elgato_gchd 1-2:1.0: shutdown: device in STOP after RESET; IDLE and RESET again
[73336.557178] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0011 before, 0x0012 now)

=== step 5: N64 480x360 at 9400 kbit/s VBR
$ v4l2-ctl -d /dev/video0 -v width=480,height=360,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -c video_bitrate_mode=0,video_bitrate=9400000
capturing 10s to step5-16440.ts
capture: 782336 bytes, ~625 kbit/s; codec_name=h264 width=480 height=360 field_order=progressive r_frame_rate=30000/1001 ; decode errors 1
  codec_name=h264 width=480 height=360 field_order=progressive r_frame_rate=30000/1001 ; ~625 kbit/s; decode errors 1
audio: mp2
[PASS] stream 480x360 30000/1001 progressive, ~625 kbit/s
--- dmesg step 5
[73336.595963] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[73336.595966] elgato_gchd 1-2:1.0: waiting 1463 ms after the previous shutdown
[73338.220686] elgato_gchd 1-2:1.0: transcoder defaults loaded
[73338.280813] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[73338.406673] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[73338.546732] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[73338.582358] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[73338.584357] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[73338.668752] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[73338.671093] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[73338.768123] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[73338.854935] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[73338.940601] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[73338.942799] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[73338.942800] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[73338.942802] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[73338.942804] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[73339.715645] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[73339.839743] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[73339.854950] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[73339.855170] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[73339.877137] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[73339.877655] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[73339.878094] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[73339.977486] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[73340.231494] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 2
[73342.661357] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[73342.661612] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[73342.666986] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[73342.667519] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[73342.668055] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[73342.767323] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[73342.767327] elgato_gchd 1-2:1.0: analog mode now 480i, output 2
[73343.365780] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[73347.719690] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=14 bytes=917504 ep84_bytes=917504 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=2
[73348.152464] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 6: N64 standard size
$ v4l2-ctl -d /dev/video0 -v width=720,height=480,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -c video_bitrate_mode=1,video_bitrate=11250000
capturing 10s to step6-8254.ts
capture: 9355264 bytes, ~7484 kbit/s; codec_name=h264 width=720 height=480 field_order=tt r_frame_rate=60000/1001 ; decode errors 1
  codec_name=h264 width=720 height=480 field_order=tt r_frame_rate=60000/1001 ; ~7484 kbit/s; decode errors 1
audio: mp2
[PASS] stream 720x480 60000/1001 tt, ~7484 kbit/s
--- dmesg step 6
[73348.395529] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[73348.395532] elgato_gchd 1-2:1.0: waiting 1257 ms after the previous shutdown
[73349.803112] elgato_gchd 1-2:1.0: transcoder defaults loaded
[73349.865478] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[73349.971104] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[73350.172966] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[73350.175063] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[73350.251186] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[73350.282756] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[73350.284993] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[73350.373375] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[73350.375991] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[73350.461757] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[73350.558165] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[73350.560746] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[73350.560748] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[73350.560752] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[73350.560754] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[73351.330524] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[73351.656738] elgato_gchd 1-2:1.0: back-end not settled: status 0xf2
[73351.673035] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[73351.701517] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[73351.701872] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[73351.708327] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[73351.709102] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[73351.709644] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[73351.807541] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[73356.731644] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[73359.252177] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 7: N64 switched off and on while capturing (T-054)
PROMPT: Read first: ffplay opens next. When the picture shows, switch the N64 OFF, wait 5 s, switch it ON again, wait until the picture is back. ffplay closes by itself after 45 s. Press Enter to start.
ffplay for 45s
[h264 @ 0x7637642e96c0] error while decoding MB 3 4, bytestream -32
[mp2 @ 0x7637642c0040] Header missing
[PASS] Did the picture come back after the N64 was switched on again?
--- dmesg step 7
[73407.171854] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[73407.279877] elgato_gchd 1-2:1.0: transcoder defaults loaded
[73407.338736] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[73407.556589] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[73407.697213] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[73407.725010] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[73407.727230] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[73407.835976] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[73407.888463] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[73407.890899] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[73407.980302] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[73407.983018] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[73407.983021] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[73407.983025] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[73407.983029] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[73408.770784] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[73408.891339] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[73408.907360] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[73408.907663] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[73408.930909] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[73408.931515] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[73408.931990] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[73409.063394] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[73413.942049] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=22
[73415.873409] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=6 CA=fe E7=e9); configuring again
[73417.494414] elgato_gchd 1-2:1.0: back-end not settled: status 0xf2
[73417.518995] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[73417.519346] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[73417.525863] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[73417.526433] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[73417.527179] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[73417.624863] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[73417.624865] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[73422.548296] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[73427.574601] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[73432.095305] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=6 CA=fe E7=e9); configuring again
[73432.596918] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[73433.802908] elgato_gchd 1-2:1.0: back-end not settled: status 0xf2
[73433.818704] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[73433.818931] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[73433.824549] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[73433.825503] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[73433.826024] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[73433.924751] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[73433.924754] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[73438.847746] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[73443.873281] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[73448.896292] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[73452.804713] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 8: console swap N64 -> running PS1 while capturing (T-065)
PROMPT: Read first: start the PS1 game now on the side (not connected). ffplay opens next. When the N64 picture shows, move the composite cable from the N64 to the running PS1 (do not switch anything off). ffplay closes by itself after 45 s. Press Enter to start.
ffplay for 45s
[mp2 @ 0x787ca82c1380] Header missing
[PASS] Did the PS1 picture appear (correct PAL picture, maybe after a few seconds)?
$ sh -c sudo dmesg | grep -E 'decoder status changed|analog mode now|waiting up to' 
[73623.634243] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[73650.242375] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=f CA=ee E7=d9); configuring again
[73651.956540] elgato_gchd 1-2:1.0: analog mode now 288p, output 0
--- dmesg step 8
[73618.430060] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[73618.558140] elgato_gchd 1-2:1.0: transcoder defaults loaded
[73618.626410] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[73618.720564] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[73618.859660] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[73618.962981] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[73618.993384] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[73618.995734] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[73619.066197] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[73619.093809] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[73619.096390] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[73619.096391] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[73619.096393] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[73619.096398] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[73619.873980] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[73619.997929] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[73620.013730] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[73620.014088] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[73620.037128] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[73620.037825] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[73620.038243] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[73620.153117] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[73621.965268] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[73623.528242] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[73623.528448] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[73623.534027] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[73623.534576] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[73623.534972] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[73623.634240] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[73623.634243] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[73628.557022] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[73633.580899] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=43
[73638.604443] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=43
[73643.626694] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=43
[73648.649897] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=43
[73650.242375] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=f CA=ee E7=d9); configuring again
[73651.810718] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[73651.811031] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[73651.816293] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[73651.817904] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=0
[73651.818413] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[73651.956538] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[73651.956540] elgato_gchd 1-2:1.0: analog mode now 288p, output 0
[73656.821107] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[73661.848670] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[73664.130645] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 9: PS1 switched off and on while capturing (T-054)
PROMPT: Read first: ffplay opens next. When the PS1 picture shows, switch the PS1 OFF, wait 5 s, switch it ON again and wait until the game picture is back. ffplay closes by itself after 60 s. Press Enter to start.
ffplay for 60s
[h264 @ 0x74ea84305e40] error while decoding MB 5 33, bytestream -12
[mp2 @ 0x74ea842ea140] Header missing
[PASS] Did the PS1 picture come back after it was switched on again?
--- dmesg step 9
[73717.812844] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[73717.915811] elgato_gchd 1-2:1.0: transcoder defaults loaded
[73717.979369] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[73718.096721] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[73718.173882] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[73718.176616] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 288p
[73718.176619] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[73718.176622] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=0
[73718.176624] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=0
[73718.948672] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[73719.755504] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[73719.777214] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[73719.777535] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[73719.783091] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[73719.783599] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=0
[73719.784061] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[73719.923092] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[73724.791710] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=22
[73729.826476] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[73733.365668] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=e CA=ee E7=f9); configuring again
[73734.286524] elgato_gchd 1-2:1.0: decoder reports 60 Hz, standard 59 (0x88:E7=59); waiting up to 5000 ms
[73738.602858] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[73738.603204] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[73738.610720] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[73738.611300] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[73738.611851] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[73738.749179] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[73738.749183] elgato_gchd 1-2:1.0: analog mode now 576i, output 0
[73743.615699] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[73748.644981] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[73750.501159] elgato_gchd 1-2:1.0: analog mode change detected: 576i -> 288p
[73750.974474] elgato_gchd 1-2:1.0: analog mode now 288p
[73753.676508] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[73758.714430] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=448 bytes=29360128 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[73763.750991] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=563 bytes=36896768 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[73768.781551] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=678 bytes=44433408 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[73773.814855] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=793 bytes=51970048 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[73778.779091] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 10: capture start with a 288p source (R1)
PROMPT: Bring the PS1 to the same screen you used for the 288p recording (gchd_PAL_composite_small) and leave it there.
capturing 10s to step10-2336.ts
capture: 11382784 bytes, ~9106 kbit/s; codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 ; decode errors 1
  codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 ; ~9106 kbit/s; decode errors 1
audio: mp2
[PASS] stream 720x576 50/1 tt, ~9106 kbit/s
$ sudo dmesg
[73809.490002] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=36338 comm=bash
[73809.491711] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[73809.509788] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[73809.510255] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
[73809.576100] elgato_gchd 1-2:1.0: transcoder defaults loaded
[73809.576696] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[73809.577057] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x04
[73809.579258] elgato_gchd 1-2:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.580470] elgato_gchd 1-2:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.581830] elgato_gchd 1-2:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.583148] elgato_gchd 1-2:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.584342] elgato_gchd 1-2:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.585625] elgato_gchd 1-2:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.586904] elgato_gchd 1-2:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.588137] elgato_gchd 1-2:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.589315] elgato_gchd 1-2:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.590519] elgato_gchd 1-2:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.591850] elgato_gchd 1-2:1.0: firmware chunk offset=327680 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.593161] elgato_gchd 1-2:1.0: firmware chunk offset=360448 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.594440] elgato_gchd 1-2:1.0: firmware chunk offset=393216 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.595914] elgato_gchd 1-2:1.0: firmware chunk offset=425984 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.597468] elgato_gchd 1-2:1.0: firmware chunk offset=458752 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.598894] elgato_gchd 1-2:1.0: firmware chunk offset=491520 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.600355] elgato_gchd 1-2:1.0: firmware chunk offset=524288 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.601717] elgato_gchd 1-2:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.603115] elgato_gchd 1-2:1.0: firmware chunk offset=589824 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.604463] elgato_gchd 1-2:1.0: firmware chunk offset=622592 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.605804] elgato_gchd 1-2:1.0: firmware chunk offset=655360 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.607227] elgato_gchd 1-2:1.0: firmware chunk offset=688128 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.608507] elgato_gchd 1-2:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.609923] elgato_gchd 1-2:1.0: firmware chunk offset=753664 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.611402] elgato_gchd 1-2:1.0: firmware chunk offset=786432 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.612772] elgato_gchd 1-2:1.0: firmware chunk offset=819200 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.614259] elgato_gchd 1-2:1.0: firmware chunk offset=851968 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.615698] elgato_gchd 1-2:1.0: firmware chunk offset=884736 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.617051] elgato_gchd 1-2:1.0: firmware chunk offset=917504 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.618537] elgato_gchd 1-2:1.0: firmware chunk offset=950272 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.620026] elgato_gchd 1-2:1.0: firmware chunk offset=983040 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.621450] elgato_gchd 1-2:1.0: firmware chunk offset=1015808 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.622935] elgato_gchd 1-2:1.0: firmware chunk offset=1048576 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.624284] elgato_gchd 1-2:1.0: firmware chunk offset=1081344 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.625627] elgato_gchd 1-2:1.0: firmware chunk offset=1114112 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.627023] elgato_gchd 1-2:1.0: firmware chunk offset=1146880 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.628390] elgato_gchd 1-2:1.0: firmware chunk offset=1179648 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.629787] elgato_gchd 1-2:1.0: firmware chunk offset=1212416 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.631090] elgato_gchd 1-2:1.0: firmware chunk offset=1245184 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.632442] elgato_gchd 1-2:1.0: firmware chunk offset=1277952 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.633905] elgato_gchd 1-2:1.0: firmware chunk offset=1310720 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.635236] elgato_gchd 1-2:1.0: firmware chunk offset=1343488 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.636707] elgato_gchd 1-2:1.0: firmware chunk offset=1376256 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.638161] elgato_gchd 1-2:1.0: firmware chunk offset=1409024 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.639693] elgato_gchd 1-2:1.0: firmware chunk offset=1441792 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.641188] elgato_gchd 1-2:1.0: firmware chunk offset=1474560 size=26224 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[73809.649093] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[73809.755065] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[73809.810418] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x07 mode=0x7
[73809.818287] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[73809.820122] elgato_gchd 1-2:1.0: analog format status 0x6e not locked yet
[73809.820123] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[73809.886970] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[73809.905906] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[73809.913376] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[73809.915278] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 288p
[73809.915280] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[73809.915282] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=0
[73809.915284] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=0
[73810.665243] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[73810.665372] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[73811.316039] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[73811.332661] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[73811.332801] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[73811.332891] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[73811.339042] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[73811.339659] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=0
[73811.340086] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[73811.340186] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[73811.340431] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[73811.340824] elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x03d2 ack=1
[73811.341542] elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[73811.341824] elgato_gchd 1-2:1.0: START poll=1 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[73811.342090] elgato_gchd 1-2:1.0: START poll=2 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[73811.342446] elgato_gchd 1-2:1.0: START poll=3 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[73811.342726] elgato_gchd 1-2:1.0: START poll=4 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[73811.343002] elgato_gchd 1-2:1.0: START poll=5 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[73811.343289] elgato_gchd 1-2:1.0: START poll=6 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[73811.343595] elgato_gchd 1-2:1.0: START poll=7 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[73811.343881] elgato_gchd 1-2:1.0: START poll=8 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[73811.344186] elgato_gchd 1-2:1.0: START poll=9 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[73811.344471] elgato_gchd 1-2:1.0: START poll=10 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[73811.344760] elgato_gchd 1-2:1.0: START poll=11 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[73811.345040] elgato_gchd 1-2:1.0: START poll=12 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[73811.345316] elgato_gchd 1-2:1.0: START poll=13 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[73811.345607] elgato_gchd 1-2:1.0: START poll=14 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[73811.345884] elgato_gchd 1-2:1.0: START poll=15 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[73811.368741] elgato_gchd 1-2:1.0: START poll=100 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[73811.394493] elgato_gchd 1-2:1.0: START poll=200 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[73811.420828] elgato_gchd 1-2:1.0: START poll=300 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[73811.447776] elgato_gchd 1-2:1.0: START poll=400 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[73811.473728] elgato_gchd 1-2:1.0: START poll=500 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x03d2 ack=0
[73811.478199] elgato_gchd 1-2:1.0: START poll=514 state=0x0002 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x03d2 ack=1
[73811.478745] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[73811.876438] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[73811.922667] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[73811.967462] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[73812.011699] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[73812.057656] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[73816.346651] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[73819.493460] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[73819.493953] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[73820.206461] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[73820.207194] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[73820.253861] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[73820.254766] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[73820.254990] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
[73820.274416] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[PASS] capture started in 288p
--- dmesg step 10
[73809.491711] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[73809.576100] elgato_gchd 1-2:1.0: transcoder defaults loaded
[73809.649093] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[73809.755065] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[73809.818287] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[73809.820122] elgato_gchd 1-2:1.0: analog format status 0x6e not locked yet
[73809.886970] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[73809.913376] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[73809.915278] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 288p
[73809.915280] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[73809.915282] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=0
[73809.915284] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=0
[73810.665243] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[73811.316039] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[73811.332661] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[73811.332891] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[73811.339042] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[73811.339659] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=0
[73811.340086] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[73811.478745] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[73816.346651] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[73820.274416] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 11: start/close cycles (T-067)
capturing 5s to step11-9763.ts
capture: 2416640 bytes, ~3866 kbit/s; codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 ; decode errors 1
  codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 ; ~3866 kbit/s; decode errors 1
audio: mp2
[PASS] stream 720x576 50/1 tt, ~3866 kbit/s
capturing 5s to step11-24504.ts
capture: 2158592 bytes, ~3453 kbit/s; codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 ; decode errors 1
  codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 ; ~3453 kbit/s; decode errors 1
audio: mp2
[PASS] stream 720x576 50/1 tt, ~3453 kbit/s
capturing 5s to step11-25769.ts
capture: 1830912 bytes, ~2929 kbit/s; codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 ; decode errors 1
  codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 ; ~2929 kbit/s; decode errors 1
audio: mp2
[PASS] stream 720x576 50/1 tt, ~2929 kbit/s
--- dmesg step 11
[73820.881104] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[73820.881107] elgato_gchd 1-2:1.0: waiting 895 ms after the previous shutdown
[73821.929100] elgato_gchd 1-2:1.0: transcoder defaults loaded
[73821.991851] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[73822.092147] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[73822.152074] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[73822.154043] elgato_gchd 1-2:1.0: analog format status 0x6e not locked yet
[73822.222479] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[73822.253335] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[73822.255314] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 288p
[73822.255316] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[73822.255318] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=0
[73822.255320] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=0
[73823.037535] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[73823.716943] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[73823.731760] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[73823.732072] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[73823.737664] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[73823.738151] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=0
[73823.738564] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[73823.877735] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[73826.893803] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[73827.142118] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[73827.142120] elgato_gchd 1-2:1.0: waiting 1252 ms after the previous shutdown
[73828.478491] elgato_gchd 1-2:1.0: transcoder defaults loaded
[73828.540870] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[73828.636785] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[73828.692671] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[73828.694800] elgato_gchd 1-2:1.0: analog format status 0x6e not locked yet
[73828.758968] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[73828.785302] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[73828.787288] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 288p
[73828.787290] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[73828.787292] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=0
[73828.787294] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=0
[73829.536146] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[73830.187520] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[73830.203083] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[73830.203410] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[73830.208917] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[73830.209398] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=0
[73830.209893] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[73830.348919] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[73832.928779] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[73833.174072] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[73833.174076] elgato_gchd 1-2:1.0: waiting 1256 ms after the previous shutdown
[73834.587601] elgato_gchd 1-2:1.0: transcoder defaults loaded
[73834.650660] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[73834.751417] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[73834.814626] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[73834.816770] elgato_gchd 1-2:1.0: analog format status 0x6e not locked yet
[73834.890335] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[73834.939719] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[73834.941920] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 288p
[73834.941921] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[73834.941923] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=0
[73834.941925] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=0
[73835.732165] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[73836.426258] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[73836.448093] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[73836.448477] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[73836.455347] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[73836.456632] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=0
[73836.457218] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[73836.574037] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[73838.999123] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== summary (T-065, c0c408e TASK: 288p start in the T-065 test)
PASS | step 2 PS1 standard size (PAL, v_vinpelclk now 1) | stream 720x576 50/1 tt, ~8791 kbit/s
PASS | step 3 PS1 640x480 | stream 640x480 25/1 progressive, ~5023 kbit/s
FAIL | step 4 N64 640x480 (NTSC: GOP 8, v_vinpelclk 0) | capture: no data
PASS | step 5 N64 480x360 at 9400 kbit/s VBR | stream 480x360 30000/1001 progressive, ~625 kbit/s
PASS | step 6 N64 standard size | stream 720x480 60000/1001 tt, ~7484 kbit/s
PASS | step 7 N64 switched off and on while capturing (T-054) | Did the picture come back after the N64 was switched on again?
PASS | step 8 console swap N64 -> running PS1 while capturing (T-065) | Did the PS1 picture appear (correct PAL picture, maybe after a few seconds)?
PASS | step 9 PS1 switched off and on while capturing (T-054) | Did the PS1 picture come back after it was switched on again?
PASS | step 10 capture start with a 288p source (R1) | stream 720x576 50/1 tt, ~9106 kbit/s
PASS | step 10 capture start with a 288p source (R1) | capture started in 288p
PASS | step 11 start/close cycles (T-067) | stream 720x576 50/1 tt, ~3866 kbit/s
PASS | step 11 start/close cycles (T-067) | stream 720x576 50/1 tt, ~3453 kbit/s
PASS | step 11 start/close cycles (T-067) | stream 720x576 50/1 tt, ~2929 kbit/s
