# TASK T-080 — Picture and audio settings

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** composite/S-video picture settings in the decoder; third hardware run (composite, S-video only) next
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
- Recordings R15/R16 (2026-11-27): see Done so far.
- T-080 second run (2026-11-27): built 6d94540, not 74d6482: that commit
  had carried the build file `src/driver/.fops.o.d`, the runner's
  `git pull` aborted on the untracked copy and the test went on with the
  old code (fixed: file removed, `.*.o.d` ignored, the runner stops when
  the update fails). So the composite and S-video picture FAILs (no effect,
  saturation −100 not kept after the signal returned) are of the old code.
  PASS: component, all four picture settings, no encoder restart; audio
  with a steady tone, mean volume 0 dB −18.3, −6 −24.3, −12 −30.2, +1
  −17.3, +6 −12.2, +12 −6.0, 0 again −18.2: exactly 1 dB per step on both
  sides, the positive half confirmed (`41 = 57` is +20 dB; the first run's
  −3.1 dB came from the music).
- Maintainer (2026-11-27): `master` holds only the product (sources, docs,
  READMEs); contributor guides and agent files stay on `dev` (RULES B2,
  TODO T-100). The test suite lives on its own branch `tests` (RULES B3,
  TODO T-102).

## Hardware test (maintainer)

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-080.sh > tests/T-080.sh
bash gchd-test.sh T-080 --branch feat/picture-audio
```

## Maintainer comments

There was 1 FAIL which I don't know if it's a symptom of the test or the driver.

gchd-test T-080 20261127-221845 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-080-20261127-221845.log
== build
$ git fetch -q origin
$ git checkout -q feat/picture-audio
$ git pull -q --ff-only
commit: 6df6492 T-080 second run: audio gain confirmed in 1 dB steps (+20 dB boost right); remove a committed build file; master holds only the product, tests on their own branch
commit: 6df6492 T-080 second run: audio gain confirmed in 1 dB steps (+20 dB boost right); remove a committed build file; master holds only the product, tests on their own branch
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
[ 6409.778336] usbcore: deregistering interface driver elgato_gchd
[ 6419.920152] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[ 6419.924646] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[ 6419.924676] usbcore: registered new interface driver elgato_gchd
[ 6425.917293] usb 1-2: USB disconnect, device number 7
[ 6432.824486] usb 1-2: new high-speed USB device number 8 using xhci_hcd
[ 6433.218756] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[ 6433.218772] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[ 6433.218777] usb 1-2: Product: GameCapture HD
[ 6433.218781] usb 1-2: Manufacturer: Elgato
[ 6433.218784] usb 1-2: SerialNumber: 04107F1267
[ 6433.235541] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[ 6433.236763] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[ 6436.961878] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0

=== step 2: picture settings: composite
PROMPT: A window 'Elgato composite' shows the PS3. Watch it during the next changes (3 s each).
$ v4l2-ctl -d /dev/video0 -c brightness=-100
$ v4l2-ctl -d /dev/video0 -c brightness=100
$ v4l2-ctl -d /dev/video0 -c brightness=0
[PASS] composite brightness: dark, then bright, then normal, without stopping?
$ v4l2-ctl -d /dev/video0 -c contrast=-100
$ v4l2-ctl -d /dev/video0 -c contrast=100
$ v4l2-ctl -d /dev/video0 -c contrast=0
[PASS] composite contrast: flat, then hard, then normal?
$ v4l2-ctl -d /dev/video0 -c saturation=-100
$ v4l2-ctl -d /dev/video0 -c saturation=100
$ v4l2-ctl -d /dev/video0 -c saturation=0
[PASS] composite saturation: black and white, then very colourful, then normal?
$ v4l2-ctl -d /dev/video0 -c hue=-180
$ v4l2-ctl -d /dev/video0 -c hue=180
$ v4l2-ctl -d /dev/video0 -c hue=0
[PASS] composite hue: colours shifted one way, then the other way, then normal?
$ count_new encoder restart
0
[PASS] composite: no encoder restart for the picture changes
$ v4l2-ctl -d /dev/video0 -c saturation=-100
PROMPT: Saturation is -100 (black and white). Switch the PS3 off and on again (or pull the yellow cable for 5 s and put it back); wait until the picture is back.
[FAIL] Composite after the signal came back: still black and white? -- Not really, it was in color even before I unplugged the yellow connector.
$ v4l2-ctl -d /dev/video0 -c saturation=0
--- dmesg step 2
[ 6437.009661] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 6437.129660] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 6437.269404] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 6437.336229] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 6437.767683] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 6437.769805] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[ 6437.769810] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 6437.769813] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 6437.769815] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 6438.556943] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 6438.671868] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 6438.686821] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 6438.687019] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 6438.709413] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 6438.710040] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 6438.711035] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 6438.826087] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 6443.748753] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=23
[ 6448.773340] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 6453.797361] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 6458.821562] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 6463.846113] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 6468.870979] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=680 bytes=44564480 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 6473.895534] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=795 bytes=52101120 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 6478.919965] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=910 bytes=59637760 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 6483.943695] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1025 bytes=67174400 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 6488.967813] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1140 bytes=74711040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 6493.992114] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1255 bytes=82247680 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 6499.017314] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1370 bytes=89784320 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 6504.041484] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1485 bytes=97320960 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 6509.066619] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1600 bytes=104857600 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 6510.731271] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 6513.769068] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 6513.890402] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 6513.986169] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 6514.048420] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 6514.142077] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 6514.328914] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 6514.412613] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 6514.442611] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 6514.444604] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 6514.503740] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 6514.529495] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[ 6514.531654] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 6514.597102] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 6514.599278] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 6514.668829] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 6514.670724] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[ 6514.735560] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 6514.737960] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[ 6514.737964] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 6514.737967] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 6514.737970] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 6515.491300] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 6515.597789] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 6515.612807] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 6515.612988] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 6515.632769] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 6515.633495] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 6515.633891] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 6515.735521] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 6520.657881] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=23
[ 6525.680688] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=47
[ 6530.704396] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 6535.728120] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 6540.753596] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 6542.400674] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=e CA=ee E7=e9); configuring again
[ 6544.324107] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 6544.324361] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 6544.330123] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 6544.330642] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 6544.331128] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 6544.430413] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 6544.430417] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[ 6549.352715] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6554.375707] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6559.398682] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6564.420110] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6569.443643] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6574.467442] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=680 bytes=44564480 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6579.492159] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=795 bytes=52101120 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6584.512669] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=910 bytes=59637760 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6589.536626] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1025 bytes=67174400 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6594.560805] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1140 bytes=74711040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6599.583453] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1255 bytes=82247680 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6604.607108] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1370 bytes=89784320 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6609.630674] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1485 bytes=97320960 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6614.654159] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1600 bytes=104857600 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6619.677601] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1715 bytes=112394240 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 6624.187120] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 3: picture settings: S-video
PROMPT: Connect the PS3 with the S-video cable.
$ v4l2-ctl -d /dev/video0 -i 3
Video input set to 3 (S-Video: Camera, no signal)
PROMPT: A window 'Elgato S-video' shows the PS3. Watch it during the next changes (3 s each).
$ v4l2-ctl -d /dev/video0 -c brightness=-100
$ v4l2-ctl -d /dev/video0 -c brightness=100
$ v4l2-ctl -d /dev/video0 -c brightness=0
[PASS] S-video brightness: dark, then bright, then normal, without stopping?
$ v4l2-ctl -d /dev/video0 -c contrast=-100
$ v4l2-ctl -d /dev/video0 -c contrast=100
$ v4l2-ctl -d /dev/video0 -c contrast=0
[PASS] S-video contrast: flat, then hard, then normal?
$ v4l2-ctl -d /dev/video0 -c saturation=-100
$ v4l2-ctl -d /dev/video0 -c saturation=100
$ v4l2-ctl -d /dev/video0 -c saturation=0
[PASS] S-video saturation: black and white, then very colourful, then normal?
$ v4l2-ctl -d /dev/video0 -c hue=-180
$ v4l2-ctl -d /dev/video0 -c hue=180
$ v4l2-ctl -d /dev/video0 -c hue=0
[PASS] S-video hue: colours shifted one way, then the other way, then normal?
$ count_new encoder restart
0
[PASS] S-video: no encoder restart for the picture changes
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
--- dmesg step 3
[ 6692.614739] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 6692.614820] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 6692.632564] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[ 6692.748924] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 6692.836342] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 6692.916321] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 6693.057511] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 6693.188767] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 6693.244683] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x26 masked_mode=0x6
[ 6693.247171] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=89); waiting up to 5000 ms
[ 6693.372302] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x16 masked_mode=0x6
[ 6693.456534] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[ 6693.534811] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x16 masked_mode=0x6
[ 6693.537293] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[ 6693.537295] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 6693.537297] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=1
[ 6693.537299] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=1
[ 6694.326615] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 6694.444261] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 6694.463366] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 6694.463576] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 6694.485226] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 6694.485800] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=1
[ 6694.486255] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 6694.586133] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 6699.508900] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=23
[ 6704.533522] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 6709.557232] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 6714.580619] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 6719.604911] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 6724.629740] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=680 bytes=44564480 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 6729.654138] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=795 bytes=52101120 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 6734.679508] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=910 bytes=59637760 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 6739.703018] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1025 bytes=67174400 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 6744.726816] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1140 bytes=74711040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[ 6749.751312] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1255 bytes=82247680 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 6754.776503] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1370 bytes=89784320 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 6759.800484] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1485 bytes=97320960 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[ 6762.874739] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 6765.908439] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[ 6765.908474] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== summary (T-080, 6df6492 T-080 second run: audio gain confirmed in 1 dB steps (+20 dB boost right); remove a committed build file; master holds only the product, tests on their own branch)
PASS | step 2 picture settings: composite | composite brightness: dark, then bright, then normal, without stopping?
PASS | step 2 picture settings: composite | composite contrast: flat, then hard, then normal?
PASS | step 2 picture settings: composite | composite saturation: black and white, then very colourful, then normal?
PASS | step 2 picture settings: composite | composite hue: colours shifted one way, then the other way, then normal?
PASS | step 2 picture settings: composite | composite: no encoder restart for the picture changes
FAIL | step 2 picture settings: composite | Composite after the signal came back: still black and white? -- Not really, it was in color even before I unplugged the yellow connector.
PASS | step 3 picture settings: S-video | S-video brightness: dark, then bright, then normal, without stopping?
PASS | step 3 picture settings: S-video | S-video contrast: flat, then hard, then normal?
PASS | step 3 picture settings: S-video | S-video saturation: black and white, then very colourful, then normal?
PASS | step 3 picture settings: S-video | S-video hue: colours shifted one way, then the other way, then normal?
PASS | step 3 picture settings: S-video | S-video: no encoder restart for the picture changes
