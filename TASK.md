# TASK T-080 — Picture and audio settings

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** composite/S-video picture settings in the decoder; second hardware run next
- **Type:** driver feature (stage 4)
- **Branch:** `feat/picture-audio` (from `dev` 0.5.12)
- **TODO items:** T-080 (stage 4)

## Why

The Windows software offers brightness, contrast, saturation and hue and
an analog audio gain. They change the device itself (recorded), so they
belong to the driver, remembered like the other settings, and to the
settings window, text menu and OBS plugin.

## Goal (acceptance criteria, draft)

1. Brightness, contrast, saturation (−100 … +100) and hue (−180° … +180°)
   as V4L2 controls (`V4L2_CID_BRIGHTNESS`, `_CONTRAST`, `_SATURATION`,
   `_HUE`), the recorded register values at 0, the ends and in between;
   applied while capturing without an encoder restart; for every input
   (HDMI RGB sources: combined with the RGB conversion).
2. Analog audio gain −12 … +12 dB, default 0 dB (maintainer 2026-11-27);
   the recorded register values; the positive half confirmed by a level
   measurement.
3. Remembered (module parameters, udev rule); shown in the settings
   window, text menu and OBS plugin.

## Evidence

- `traces/gchd_{brightness,contrast,saturation,hue}_small`: each slider
  0 → −100 → 0 → +100 → 0 (hue ±180°), component 576p, while capturing.
- `traces/gchd_analog_audio_gain_small`: 0 → −12 dB in 1 dB steps, back to
  0, then towards +12 dB (component).
- Analysis: `traces/ANALYSIS-2026-11-27-hdmi-picture-audio-bitrate.md`
  §1–§4: the colour conversion block `0x9C:92`–`AA` (header, 3 × 3 matrix
  of 15-bit coefficients, 4096 = 1.0, word `in × 3 + out`, outputs Cr, Y,
  Cb; offsets × 64); contrast = Y → Y coefficient v × 32 (v 0 … 255,
  128 neutral); saturation = Cr → Cr and Cb → Cb, same scale; hue = Cr/Cb
  rotation in 0.5° steps, ±61.5° at ±180°; brightness = Y offset
  16 + ⌊(v − 128) × 1.25⌋; audio: `0x94:41/4A/4B`, code XOR `0x08`, 2 codes
  per dB, `41 = 37` for 0 … −12 dB (`AF` 0 dB), `41 = 57` (+20 dB) for
  +1 … +12 dB.
- Driver: `gchd_csc_put()` / `gchd_csc_of()` (`src/driver/input.c`) build
  the block from a matrix; picture settings multiply into it.

## Plan

1. Driver controls and the matrix: adjustment (contrast, saturation, hue)
   × the source's conversion; brightness on the Y offset; written while
   capturing through the mode worker (as the colour range).
2. Audio gain control writing `0x94:41/4A/4B` (while capturing through the
   mode worker); every input setup uses it instead of the fixed values.
3. Parameters, uevent, udev rule; settings window, text menu, OBS plugin.
4. Emulator check against the recordings (each recorded block reproduced
   from its slider value); hardware test with a level measurement for the
   audio gain.

## Done so far

- Driver: controls `brightness`, `contrast`, `saturation` (−100 … 100),
  `hue` (−180 … 180) and "Analog Audio Gain (dB)" (−12 … 12, default 0);
  the colour conversion of the source times the picture adjustment
  (`gchd_csc_picture()`), the gain registers (`gchd_audio_put()`, also in
  the input setup instead of the fixed values); changes while capturing
  through the mode worker without an encoder restart; parameters
  `picture`, `audio_gain`, uevent, udev rule.
- Check against the recordings (the driver's code in userspace): brightness
  ends exact (Y offset words `5C01`, `2B81`), hue ±61.5° in 0.5° steps
  (cos/sin within 2 of the recorded), contrast/saturation ends 8160 (the
  recordings 8159; Windows writes some values one lower), every recorded
  gain code (`A1` −1, `B7` −12, `C5` +1, `C1` +3, `C9` +7, `B1` +11,
  `BF` +12).
- Settings window (sliders, "Reset picture"), text menu (Picture, Audio
  gain), OBS plugin (sliders; audio gain hidden for HDMI).
- Composite and S-video: the picture settings are the decoder's registers
  `0x88:10`–`15` (recordings R15, analysis §8): `gchd_dec_picture()`,
  written as the third tuning pass of the input setup and while capturing
  (mode worker); every recorded end value reproduced except contrast +100
  (driver 162, the recording's highest 155).
- Audio: the recording of every step (R16) has exactly the driver's codes;
  the second run measures the levels with a steady sound.

## Findings

- T-080 (6d94540), first run: 15 PASS, 5 FAIL. PASS: controls present;
  audio gain while capturing (audible, no encoder restart); −12 dB measured
  13.0 dB quieter than 0 dB; settings kept after a replug; HDMI RGB
  (laptop): saturation −100 black and white, contrast +50 right colours,
  with Expanded too; settings window (sliders, Reset picture, gain hidden
  for HDMI), OBS (hue −60), text menu (contrast 25); v4l2-compliance 0
  failures. FAIL: (1) the picture settings had no effect on composite
  (maintainer). The Windows software never writes the colour conversion
  block for composite/S-video (none of the composite and S-video
  recordings has it): the analog decoder path does not pass through it;
  its picture settings are elsewhere (decoder `0x88`?), not recorded.
  The recordings of the picture settings were made on component, which the
  test did not check. (2) +6 dB measured 3.1 dB quieter than 0 dB (0 dB
  −10.1, −12 dB −23.1, +6 dB −13.2 dB mean volume, music, 8 s each): the
  "+20 dB" reading of `0x94:41 = 57` is wrong (from this one value about
  +11 dB, ±2 for the changing music); the positive half needs a recording
  with single steps and a steady sound.
- Recordings R15/R16 (2026-11-27): see Done so far; the doc comment of
  `0x94:41` no longer claims +20 dB.

## Hardware test (maintainer)

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-080.sh > tests/T-080.sh
bash gchd-test.sh T-080 --branch feat/picture-audio
```

## Maintainer comments

I need to correct maybe 2 things:
- You mentioned before something about an AGENTS.md file and master. All of those files are development only. Master will have only the delivered product source, documentation and READMEs. All other development files will live in dev and branches created out of dev.
- Regarding the test suite, this should live in a test branch. Tests should be separate from development and releases.

This is the log from the test:
gchd-test T-080 20261127-215013 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-080-20261127-215013.log
== build
$ git fetch -q origin
$ git checkout -q feat/picture-audio
$ git pull -q --ff-only
error: The following untracked working tree files would be overwritten by merge:
	src/driver/.fops.o.d
Please move or remove them before you merge.
Aborting
commit: 6d94540 TASK: T-080 done so far, hardware test
commit: 6d94540 TASK: T-080 done so far, hardware test
build rc=0 warnings=0

=== step 1: install
$ sudo rmmod elgato_gchd
$ bash /home/ff5/elgato-gchd/install-driver.sh --all --yes --in-terminal
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

PROMPT: PS3 on composite (video and audio). Unplug the Elgato and plug it in again; wait 5 s.
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -v width=720,height=480,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -c brightness=0,contrast=0,saturation=0,hue=0,analog_audio_gain_db=0
--- dmesg step 1
[ 4697.778231] usbcore: deregistering interface driver elgato_gchd
[ 4707.808819] usbcore: registered new interface driver elgato_gchd
[ 4718.973441] usb 1-2: new high-speed USB device number 7 using xhci_hcd
[ 4719.376988] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[ 4719.377013] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[ 4719.377020] usb 1-2: Product: GameCapture HD
[ 4719.377025] usb 1-2: Manufacturer: Elgato
[ 4719.377030] usb 1-2: SerialNumber: 04107F1267
[ 4719.385562] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[ 4719.388796] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[ 4795.807623] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0

=== step 2: picture settings: composite
PROMPT: A window 'Elgato composite' shows the PS3. Watch it during the next changes (3 s each).
$ v4l2-ctl -d /dev/video0 -c brightness=-100
$ v4l2-ctl -d /dev/video0 -c brightness=100
$ v4l2-ctl -d /dev/video0 -c brightness=0
[FAIL] composite brightness: dark, then bright, then normal, without stopping?
$ v4l2-ctl -d /dev/video0 -c contrast=-100
$ v4l2-ctl -d /dev/video0 -c contrast=100
$ v4l2-ctl -d /dev/video0 -c contrast=0
[FAIL] composite contrast: flat, then hard, then normal?
$ v4l2-ctl -d /dev/video0 -c saturation=-100
$ v4l2-ctl -d /dev/video0 -c saturation=100
$ v4l2-ctl -d /dev/video0 -c saturation=0
[FAIL] composite saturation: black and white, then very colourful, then normal?
$ v4l2-ctl -d /dev/video0 -c hue=-180
$ v4l2-ctl -d /dev/video0 -c hue=180
$ v4l2-ctl -d /dev/video0 -c hue=0
[FAIL] composite hue: colours shifted one way, then the other way, then normal?
$ count_new encoder restart
0
[PASS] composite: no encoder restart for the picture changes
$ v4l2-ctl -d /dev/video0 -c saturation=-100
PROMPT: Saturation is -100 (black and white). Switch the PS3 off and on again (or pull the yellow cable for 5 s and put it back); wait until the picture is back.
[FAIL] Composite after the signal came back: still black and white? -- Signal came back but picture has remained in color all throughout.
$ v4l2-ctl -d /dev/video0 -c saturation=0
--- dmesg step 2
[ 4795.859845] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 4795.980273] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 4796.099826] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 4796.164556] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 4796.602642] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 4796.604783] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[ 4796.604785] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 4796.604787] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 4796.604788] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 4797.401156] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 4797.524934] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 4797.545621] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 4797.545831] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 4797.570294] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 4797.570960] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 4797.571476] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 4797.668274] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 4802.591941] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=23
[ 4807.615481] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 4812.639710] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 4817.661744] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 4822.683930] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 4827.708394] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=680 bytes=44564480 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 4832.731330] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=795 bytes=52101120 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 4837.754293] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=910 bytes=59637760 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 4842.779126] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1025 bytes=67174400 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 4847.802503] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1140 bytes=74711040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 4852.825640] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1255 bytes=82247680 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 4857.848480] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1370 bytes=89784320 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 4862.870554] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1485 bytes=97320960 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 4867.895097] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1600 bytes=104857600 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 4872.917684] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1715 bytes=112394240 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 4877.941174] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1830 bytes=119930880 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 4882.388723] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 4885.424694] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 4885.543923] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 4885.630648] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 4885.691102] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 4885.775898] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 4886.006979] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 4886.094076] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 4886.096031] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 4886.151196] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 4886.177452] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[ 4886.179557] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 4886.257177] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 4886.260303] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[ 4886.329910] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 4886.332501] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[ 4886.332504] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 4886.332507] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 4886.332510] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 4887.111782] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 4887.230941] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 4887.248696] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 4887.248923] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 4887.272866] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 4887.273638] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 4887.274148] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 4887.388010] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 4892.314194] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=23
[ 4897.333751] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 4902.356010] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 4907.380918] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 4909.007637] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=e CA=fe E7=e9); configuring again
[ 4911.042942] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 4911.043292] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 4911.051119] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 4911.051863] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 4911.052746] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 4911.149969] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 4911.149972] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[ 4916.071302] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 4921.093469] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 4926.115709] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 4931.139733] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 4936.162579] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 4941.186097] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=680 bytes=44564480 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 4946.212320] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=795 bytes=52101120 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 4951.233122] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=910 bytes=59637760 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 4956.162541] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 3: picture settings: S-video
PROMPT: Connect the PS3 with the S-video cable.
$ v4l2-ctl -d /dev/video0 -i 3
Video input set to 3 (S-Video: Camera, no signal)
PROMPT: A window 'Elgato S-video' shows the PS3. Watch it during the next changes (3 s each).
$ v4l2-ctl -d /dev/video0 -c brightness=-100
$ v4l2-ctl -d /dev/video0 -c brightness=100
$ v4l2-ctl -d /dev/video0 -c brightness=0
[FAIL] S-video brightness: dark, then bright, then normal, without stopping?
$ v4l2-ctl -d /dev/video0 -c contrast=-100
$ v4l2-ctl -d /dev/video0 -c contrast=100
$ v4l2-ctl -d /dev/video0 -c contrast=0
[FAIL] S-video contrast: flat, then hard, then normal?
$ v4l2-ctl -d /dev/video0 -c saturation=-100
$ v4l2-ctl -d /dev/video0 -c saturation=100
$ v4l2-ctl -d /dev/video0 -c saturation=0
[FAIL] S-video saturation: black and white, then very colourful, then normal?
$ v4l2-ctl -d /dev/video0 -c hue=-180
$ v4l2-ctl -d /dev/video0 -c hue=180
$ v4l2-ctl -d /dev/video0 -c hue=0
[FAIL] S-video hue: colours shifted one way, then the other way, then normal?
$ count_new encoder restart
0
[PASS] S-video: no encoder restart for the picture changes
--- dmesg step 3
[ 5085.821152] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 5085.821252] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 5085.838197] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[ 5085.956276] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5086.180566] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5086.241479] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5086.689477] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[ 5086.692135] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 5086.784343] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[ 5086.787485] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 5086.860728] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[ 5086.863022] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[ 5086.863024] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 5086.863027] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=1
[ 5086.863029] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=1
[ 5087.620121] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5087.730306] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5087.745156] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5087.745367] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5087.768611] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5087.769370] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=1
[ 5087.769958] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 5087.884322] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5092.807507] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=23
[ 5097.831699] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 5102.855571] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 5107.879448] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 5112.903618] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 5117.928198] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=680 bytes=44564480 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 5122.952479] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=795 bytes=52101120 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 5127.976491] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=910 bytes=59637760 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 5133.000265] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1025 bytes=67174400 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 5138.024418] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1140 bytes=74711040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 5143.048658] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1255 bytes=82247680 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 5148.073382] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1370 bytes=89784320 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 5151.019209] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 4: picture settings: component
PROMPT: Connect the PS3 with the component cable (480p or 720p).
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
PROMPT: A window 'Elgato component' shows the PS3. Watch it during the next changes (3 s each).
$ v4l2-ctl -d /dev/video0 -c brightness=-100
$ v4l2-ctl -d /dev/video0 -c brightness=100
$ v4l2-ctl -d /dev/video0 -c brightness=0
[PASS] component brightness: dark, then bright, then normal, without stopping?
$ v4l2-ctl -d /dev/video0 -c contrast=-100
$ v4l2-ctl -d /dev/video0 -c contrast=100
$ v4l2-ctl -d /dev/video0 -c contrast=0
[PASS] component contrast: flat, then hard, then normal?
$ v4l2-ctl -d /dev/video0 -c saturation=-100
$ v4l2-ctl -d /dev/video0 -c saturation=100
$ v4l2-ctl -d /dev/video0 -c saturation=0
[PASS] component saturation: black and white, then very colourful, then normal?
$ v4l2-ctl -d /dev/video0 -c hue=-180
$ v4l2-ctl -d /dev/video0 -c hue=180
$ v4l2-ctl -d /dev/video0 -c hue=0
[PASS] component hue: colours shifted one way, then the other way, then normal?
$ count_new encoder restart
0
[PASS] component: no encoder restart for the picture changes
--- dmesg step 4
[ 5289.696219] elgato_gchd 1-2:1.0: input switch requested: old=3 new=1 hw_initialized=0 streaming=0
[ 5289.696267] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 5289.711370] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[ 5289.832127] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5290.044945] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5290.110352] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5290.126531] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0x9576
[ 5290.126540] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 5290.126546] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x480 fps=60/1 interlaced=1
[ 5290.126549] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x480 fps=60/1 interlaced=1
[ 5290.939181] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5291.410049] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0x9576
[ 5291.410074] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 5291.410164] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x480 fps=60/1 interlaced=1
[ 5291.410171] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x480 fps=60/1 interlaced=1
[ 5292.512735] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5292.529025] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5292.529374] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5292.531656] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5292.532248] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x480 fps=60/1 interlaced=1
[ 5292.532781] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5292.651983] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5297.531016] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=22
[ 5302.554812] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 5307.579589] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 5312.603714] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 5317.627371] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 5322.650895] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=679 bytes=44498944 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 5327.674331] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=794 bytes=52035584 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 5332.697240] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=909 bytes=59572224 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 5337.721186] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1024 bytes=67108864 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 5342.745201] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1139 bytes=74645504 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 5347.768327] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1254 bytes=82182144 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 5352.793595] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1369 bytes=89718784 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 5357.816124] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1484 bytes=97255424 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 5362.839948] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1599 bytes=104792064 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 5367.864532] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1714 bytes=112328704 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 5372.888614] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1829 bytes=119865344 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 5377.912426] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1944 bytes=127401984 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 5382.936360] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2059 bytes=134938624 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 5387.534176] elgato_gchd 1-2:1.0: component source mode changed while capturing; detecting again
[ 5387.567622] elgato_gchd 1-2:1.0: signal present: input=1; first video TS packet on PID 0x1011
[ 5387.961661] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2174 bytes=142475264 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 5388.486379] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xba95 raw6867=0xa150
[ 5388.486386] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[ 5388.486391] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x480 fps=60/1 interlaced=0
[ 5388.486393] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x480 fps=60/1 interlaced=0
[ 5389.579198] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5389.594205] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5389.595682] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5389.599453] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5389.600060] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x480 fps=60/1 interlaced=0
[ 5389.600516] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[ 5389.666933] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5394.600161] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=43 bytes=2818048 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5399.696311] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=88 bytes=5767168 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5404.744175] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=132 bytes=8650752 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5409.817501] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=171 bytes=11206656 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5414.876173] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5419.882245] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=254 bytes=16646144 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5424.956544] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=302 bytes=19791872 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5430.016063] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=350 bytes=22937600 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5435.043896] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=397 bytes=26017792 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5440.080765] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=445 bytes=29163520 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5445.153296] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=494 bytes=32374784 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5450.240501] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=544 bytes=35651584 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5454.408440] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 5: audio gain measured in steps (steady sound)
PROMPT: Connect the steady sound (continuous tone, medium volume, not changing) to the Elgato's analog audio input (red/white); composite video can stay. Start the sound now.
mean volume (dB): 0: -18.3, -6: -24.3, -12: -30.2, +1: -17.3, +6: -12.2, +12: -6.0, 0 again: -18.2
mean volume (dB): 0: -18.3, -6: -24.3, -12: -30.2, +1: -17.3, +6: -12.2, +12: -6.0, 0 again: -18.2
$ louder -18.3 -18.2 -1 1
ok 0,1
[PASS] sound steady (0 dB twice within 1 dB)
$ louder -24.3 -18.3 4 8
ok 6
[PASS] -6 dB: 6 dB quieter (4-8)
$ louder -30.2 -18.3 10 14
ok 11,9
[PASS] -12 dB: 12 dB quieter (10-14)
$ louder -18.3 -17.3 0 2
ok 1
[PASS] +1 dB: 1 dB louder (0-2)
$ louder -18.3 -12.2 4 8
ok 6,1
[PASS] +6 dB: 6 dB louder (4-8)
$ louder -18.3 -6.0 10 14
ok 12,3
[PASS] +12 dB: 12 dB louder (10-14; less if the tone clips)
$ v4l2-ctl -d /dev/video0 -c analog_audio_gain_db=0
PROMPT: Stop the sound; reconnect the PS3 audio if you like.
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
--- dmesg step 5
[ 5576.576110] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 5576.576166] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 5576.582486] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5576.697426] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5576.764093] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5576.861603] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 5577.057566] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 5577.229053] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 5577.275775] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[ 5577.278684] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 5577.391120] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 5577.394582] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[ 5577.492361] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 5577.497042] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[ 5577.497046] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 5577.497051] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5577.497055] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5578.276601] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5578.397970] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5578.413522] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5578.413758] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5578.435982] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5578.436580] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5578.437714] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 5578.586210] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5583.464980] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5588.488149] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5593.511914] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5598.534073] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5599.950387] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5600.024814] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 5600.029564] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5600.029568] elgato_gchd 1-2:1.0: waiting 1422 ms after the previous shutdown
[ 5601.619199] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5601.698413] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5601.825110] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 5602.035135] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x36 masked_mode=0x6
[ 5602.045125] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=89); waiting up to 5000 ms
[ 5602.128745] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 5602.172470] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 5602.366829] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[ 5602.371925] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[ 5602.371933] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 5602.371939] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5602.371943] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5603.175324] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5603.297853] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5603.313563] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5603.313770] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5603.336294] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5603.336786] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5603.337429] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 5603.436085] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5608.359995] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5613.382851] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5618.407400] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5623.431370] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5624.572944] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5624.653772] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 5624.658667] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5624.658671] elgato_gchd 1-2:1.0: waiting 1415 ms after the previous shutdown
[ 5626.225757] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5626.283883] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5626.406402] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 5626.459908] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 5626.544536] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 5626.567167] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x26 masked_mode=0x6
[ 5626.569047] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 5626.643799] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 5626.645806] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[ 5626.707687] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 5626.787121] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[ 5626.866645] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 5626.942405] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 5626.944488] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[ 5626.944490] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 5626.944493] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5626.944495] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5627.687886] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5627.796161] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5627.811788] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5627.812015] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5627.832955] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5627.833658] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5627.834090] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 5627.967819] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5632.846620] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5637.870048] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5642.894073] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5647.916463] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5649.062339] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5649.138262] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 5649.143578] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5649.143582] elgato_gchd 1-2:1.0: waiting 1420 ms after the previous shutdown
[ 5650.788454] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5650.854943] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5650.956512] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 5651.146870] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 5651.260627] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 5651.292320] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 5651.294203] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 5651.386915] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 5651.389453] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 5651.470730] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[ 5651.473077] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[ 5651.556207] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 5651.558418] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[ 5651.558421] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 5651.558423] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5651.558425] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5652.301933] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5652.416816] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5652.432386] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5652.432602] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5652.453981] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5652.454696] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5652.455214] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 5652.554901] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5657.478321] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5662.501511] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5667.525379] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5672.548468] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5673.960608] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5674.031954] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 5674.037092] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5674.037095] elgato_gchd 1-2:1.0: waiting 1424 ms after the previous shutdown
[ 5675.709564] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5675.783335] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5675.926915] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 5676.129143] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x36 masked_mode=0x6
[ 5676.131892] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 5676.192664] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 5676.237191] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[ 5676.239523] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 5676.321185] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 5676.323390] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[ 5676.421559] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 5676.424478] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[ 5676.424483] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 5676.424487] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5676.424490] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5677.208126] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5677.333282] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5677.350626] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5677.350834] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5677.373854] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5677.374785] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5677.375355] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 5677.489842] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5682.413255] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5687.437357] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5692.460174] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5697.483599] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5698.703322] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5698.778478] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 5698.784592] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5698.784595] elgato_gchd 1-2:1.0: waiting 1419 ms after the previous shutdown
[ 5700.355467] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5700.415468] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5700.541276] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 5700.609641] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 5700.612304] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[ 5700.683015] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 5700.716692] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 5700.829590] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[ 5700.832783] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[ 5700.832787] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 5700.832791] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5700.832832] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5701.614378] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5701.732514] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5701.748149] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5701.748369] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5701.772608] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5701.773420] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5701.773955] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 5701.870806] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5706.793538] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5711.817002] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5716.840788] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5721.863046] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5723.094345] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5723.169213] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 5723.174074] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5723.174077] elgato_gchd 1-2:1.0: waiting 1422 ms after the previous shutdown
[ 5724.685823] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 5724.745596] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 5724.856108] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 5724.991102] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x36 masked_mode=0x6
[ 5724.994017] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=89); waiting up to 5000 ms
[ 5725.062230] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 5725.094574] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x36 masked_mode=0x6
[ 5725.096696] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[ 5725.096698] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 5725.096700] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5725.096701] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5725.872393] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 5725.992005] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 5726.007392] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 5726.007598] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5726.028947] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5726.029639] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5726.030454] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 5726.130124] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 5731.052732] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5736.076703] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5741.100123] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5746.122179] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 5747.458057] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 5790.798287] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0

=== summary (T-080, 6d94540 TASK: T-080 done so far, hardware test)
FAIL | step 2 picture settings: composite | composite brightness: dark, then bright, then normal, without stopping?
FAIL | step 2 picture settings: composite | composite contrast: flat, then hard, then normal?
FAIL | step 2 picture settings: composite | composite saturation: black and white, then very colourful, then normal?
FAIL | step 2 picture settings: composite | composite hue: colours shifted one way, then the other way, then normal?
PASS | step 2 picture settings: composite | composite: no encoder restart for the picture changes
FAIL | step 2 picture settings: composite | Composite after the signal came back: still black and white? -- Signal came back but picture has remained in color all throughout.
FAIL | step 3 picture settings: S-video | S-video brightness: dark, then bright, then normal, without stopping?
FAIL | step 3 picture settings: S-video | S-video contrast: flat, then hard, then normal?
FAIL | step 3 picture settings: S-video | S-video saturation: black and white, then very colourful, then normal?
FAIL | step 3 picture settings: S-video | S-video hue: colours shifted one way, then the other way, then normal?
PASS | step 3 picture settings: S-video | S-video: no encoder restart for the picture changes
PASS | step 4 picture settings: component | component brightness: dark, then bright, then normal, without stopping?
PASS | step 4 picture settings: component | component contrast: flat, then hard, then normal?
PASS | step 4 picture settings: component | component saturation: black and white, then very colourful, then normal?
PASS | step 4 picture settings: component | component hue: colours shifted one way, then the other way, then normal?
PASS | step 4 picture settings: component | component: no encoder restart for the picture changes
PASS | step 5 audio gain measured in steps (steady sound) | sound steady (0 dB twice within 1 dB)
PASS | step 5 audio gain measured in steps (steady sound) | -6 dB: 6 dB quieter (4-8)
PASS | step 5 audio gain measured in steps (steady sound) | -12 dB: 12 dB quieter (10-14)
PASS | step 5 audio gain measured in steps (steady sound) | +1 dB: 1 dB louder (0-2)
PASS | step 5 audio gain measured in steps (steady sound) | +6 dB: 6 dB louder (4-8)
PASS | step 5 audio gain measured in steps (steady sound) | +12 dB: 12 dB louder (10-14; less if the tone clips)
