# TASK T-080 — Picture and audio settings

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** implemented (driver, settings window, text menu, OBS plugin); hardware test next
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

## Findings

## Hardware test (maintainer)

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-080.sh > tests/T-080.sh
bash gchd-test.sh T-080 --branch feat/picture-audio
```

## Maintainer comments

Picture control didn't work at all on composite.

gchd-test T-080 20261127-204129 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-080-20261127-204129.log
== build
$ git fetch -q origin
$ git checkout -q feat/picture-audio
$ git pull -q --ff-only
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

PROMPT: PS3 on composite playing something with continuous sound; laptop on HDMI. Unplug the Elgato and plug it in again; wait 5 s.
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -v width=720,height=480,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -c brightness=0,contrast=0,saturation=0,hue=0,analog_audio_gain_db=0
$ pic
brightness: 0 contrast: 0 saturation: 0 hue: 0 analog_audio_gain_db: 0 
[PASS] controls present, all 0
--- dmesg step 1
[  575.223584] usbcore: deregistering interface driver elgato_gchd
[  585.320177] usbcore: registered new interface driver elgato_gchd
[  591.248744] usb 1-2: new high-speed USB device number 5 using xhci_hcd
[  591.641577] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[  591.641603] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[  591.641610] usb 1-2: Product: GameCapture HD
[  591.641616] usb 1-2: Manufacturer: Elgato
[  591.641620] usb 1-2: SerialNumber: 04107F1267
[  591.651913] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[  591.657863] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[  594.194380] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[  594.194411] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== step 2: picture settings while capturing (composite)
PROMPT: A window 'Elgato composite' shows the PS3. Watch it during the next changes (each is applied 3 s later).
$ v4l2-ctl -d /dev/video0 -c brightness=-100
$ v4l2-ctl -d /dev/video0 -c brightness=100
$ v4l2-ctl -d /dev/video0 -c brightness=0
[FAIL] Brightness: the picture became dark, then bright, then normal, without stopping?
$ v4l2-ctl -d /dev/video0 -c contrast=-100
$ v4l2-ctl -d /dev/video0 -c contrast=100
$ v4l2-ctl -d /dev/video0 -c contrast=0
[FAIL] Contrast: flat grey, then very hard, then normal?
$ v4l2-ctl -d /dev/video0 -c saturation=-100
$ v4l2-ctl -d /dev/video0 -c saturation=100
$ v4l2-ctl -d /dev/video0 -c saturation=0
[FAIL] Saturation: black and white, then very colourful, then normal?
$ v4l2-ctl -d /dev/video0 -c hue=-180
$ v4l2-ctl -d /dev/video0 -c hue=180
$ v4l2-ctl -d /dev/video0 -c hue=0
[FAIL] Hue: the colours shifted one way, then the other way, then normal?
$ v4l2-ctl -d /dev/video0 -c analog_audio_gain_db=-12
$ v4l2-ctl -d /dev/video0 -c analog_audio_gain_db=12
$ v4l2-ctl -d /dev/video0 -c analog_audio_gain_db=0
[PASS] Audio: quieter, then louder (maybe distorted), then normal, while the picture kept running?
$ count_new encoder restart
0
[PASS] no encoder restart for picture or audio changes
--- dmesg step 2
[  594.241042] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[  594.358082] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  594.604307] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  594.667216] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  595.061975] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  595.064649] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[  595.064651] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[  595.064653] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[  595.064654] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[  595.823803] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  595.939132] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  595.953442] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  595.953681] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  595.974712] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  595.975312] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  595.975825] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  596.092886] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  600.972235] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=22
[  605.996729] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[  611.019284] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[  616.044100] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[  621.066628] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  626.090051] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=679 bytes=44498944 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[  631.115072] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=794 bytes=52035584 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[  636.135825] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=909 bytes=59572224 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[  641.159204] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1024 bytes=67108864 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  646.181349] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1139 bytes=74645504 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[  651.204681] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1254 bytes=82182144 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[  656.227612] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1369 bytes=89718784 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[  661.250457] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1484 bytes=97255424 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[  666.273589] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1599 bytes=104792064 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[  671.295563] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1714 bytes=112328704 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  676.316873] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1829 bytes=119865344 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[  681.077005] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 3: audio gain measured (composite)
mean volume: 0 dB: -10.1, -12 dB: -23.1, +6 dB: -13.2
mean volume: 0 dB: -10.1 dB, -12 dB: -23.1 dB, +6 dB: -13.2 dB
$ sh -c awk -v a='-10.1' -v b='-23.1' 'BEGIN { d = a - b; print (d >= 8 && d <= 16 ? "ok" : "no"), d }'
ok 13
[PASS] -12 dB: about 12 dB quieter (8-16)
$ sh -c awk -v a='-10.1' -v b='-13.2' 'BEGIN { d = b - a; print (d >= 3 && d <= 9 ? "ok" : "no"), d }'
no -3,1
[FAIL] +6 dB: about 6 dB louder (3-9) (expected /^ok/, got: no -3,1)
--- dmesg step 3
[  684.142852] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[  684.148763] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  684.307875] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  684.368572] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  684.777459] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[  684.779558] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[  684.860615] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  684.863420] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[  684.950783] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  684.954098] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[  685.046930] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[  685.049657] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[  685.049661] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[  685.049663] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[  685.049666] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[  685.806525] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  685.920703] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  685.938643] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  685.938992] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  685.963608] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  685.964227] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  685.964637] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  686.061352] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  690.985758] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  696.008767] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  701.031313] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  706.053652] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  707.191038] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[  707.260741] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[  707.265388] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  707.265392] elgato_gchd 1-2:1.0: waiting 1427 ms after the previous shutdown
[  709.054743] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  709.118264] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  709.440012] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x36 masked_mode=0x6
[  709.442119] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[  709.510515] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  709.512622] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[  709.616611] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  709.619640] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[  709.751433] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  709.757542] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[  709.757555] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[  709.757567] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[  709.757572] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[  710.538730] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  710.654857] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  710.671869] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  710.672121] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  710.695186] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  710.695774] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  710.696211] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  710.811081] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  715.732544] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  720.756292] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  725.779862] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  730.802586] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  731.922191] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[  731.991488] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[  731.996221] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  731.996224] elgato_gchd 1-2:1.0: waiting 1426 ms after the previous shutdown
[  733.786928] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  733.847337] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  734.144210] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x26 masked_mode=0x6
[  734.146867] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[  734.236747] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  734.239722] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[  734.331795] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[  734.426417] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[  734.543097] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  734.545364] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[  734.545367] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[  734.545369] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[  734.545371] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[  735.302561] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  735.414470] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  735.429354] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  735.429551] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  735.450998] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  735.451675] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  735.452073] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  735.551954] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  740.475306] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  745.499942] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  750.522267] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  755.545733] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  756.882258] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 4: remembered after a replug
$ v4l2-ctl -d /dev/video0 -c brightness=20,contrast=-10,saturation=30,hue=45,analog_audio_gain_db=-3
PROMPT: Unplug the Elgato and plug it in again; wait 5 s.
$ pic
brightness: 20 contrast: -10 saturation: 30 hue: 45 analog_audio_gain_db: -3 
[PASS] kept: 20 -10 30 45 -3
$ v4l2-ctl -d /dev/video0 -c brightness=0,contrast=0,saturation=0,hue=0,analog_audio_gain_db=0
--- dmesg step 4
[  780.433714] usb 1-2: USB disconnect, device number 5
[  787.789104] workqueue: hub_event hogged CPU for >10000us 4 times, consider switching to WQ_UNBOUND
[  787.903225] usb 1-2: new high-speed USB device number 6 using xhci_hcd
[  788.280496] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[  788.280515] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[  788.280521] usb 1-2: Product: GameCapture HD
[  788.280526] usb 1-2: Manufacturer: Elgato
[  788.280530] usb 1-2: SerialNumber: 04107F1267
[  788.287730] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[  788.292323] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON

=== step 5: HDMI (laptop, RGB): picture settings with the RGB conversion
PROMPT: A window 'Elgato HDMI' shows the laptop.
$ v4l2-ctl -d /dev/video0 -c saturation=-100
[PASS] HDMI: black and white?
$ v4l2-ctl -d /dev/video0 -c saturation=0,contrast=50
[PASS] HDMI: harder contrast, colours right (no tint)?
$ v4l2-ctl -d /dev/video0 -c hdmi_color_range=1
[PASS] Expanded with contrast +50: still a picture with the colours right (even harder)?
$ v4l2-ctl -d /dev/video0 -c hdmi_color_range=0,contrast=0
--- dmesg step 5
[  795.734737] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=0 streaming=0
[  795.734804] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[  795.852698] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  796.189691] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  796.255734] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  796.273788] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb6d7 rgb=1
[  796.273792] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1920x1080 fps=60/1 interlaced=0 signal_present=1
[  796.273795] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1920x1080 fps=60/1 interlaced=0
[  796.273797] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1920x1080 fps=60/1 interlaced=0
[  797.025386] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  798.160158] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  798.174888] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  798.175077] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  798.176558] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  798.177409] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1920x1080 fps=60/1 interlaced=0
[  798.177828] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x000a
[  798.247435] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  803.206713] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=20
[  808.238421] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=47
[  813.271131] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=47
[  818.303417] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[  823.334285] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=47
[  828.362453] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=679 bytes=44498944 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=49
[  833.396546] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=794 bytes=52035584 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=47
[  838.429585] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=909 bytes=59572224 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=48
[  843.464237] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1024 bytes=67108864 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=47
[  848.496174] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1139 bytes=74645504 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=47
[  853.528684] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1254 bytes=82182144 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=47
[  858.560744] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1369 bytes=89718784 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=48
[  863.594778] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1484 bytes=97255424 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=48
[  868.622738] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1599 bytes=104792064 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=53
[  869.469771] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 6: settings window
PROMPT: Open 'Elgato Game Capture HD Settings' (Input Composite).
[PASS] Rows Brightness, Contrast, Saturation, Hue (degrees), Audio gain (dB) with sliders at 0, and a 'Reset picture' button?
PROMPT: Move Brightness to about 40; wait 2 s.
$ pic
brightness: 40 contrast: 0 saturation: 0 hue: 0 analog_audio_gain_db: 0 
[PASS] window: brightness about 40
PROMPT: Click 'Reset picture'; wait 2 s. Choose Input HDMI; wait 3 s.
$ pic
brightness: 0 contrast: 0 saturation: 0 hue: 0 analog_audio_gain_db: 0 
[PASS] window: reset
[PASS] HDMI: the Audio gain row is hidden, the picture rows are shown?
PROMPT: Close the window.
--- dmesg step 6
[  889.673888] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[  889.673919] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[  957.319094] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=0 streaming=0
[  957.319174] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open

=== step 7: OBS plugin
PROMPT: Start OBS (source 'Elgato Game Capture HD', input Composite); open the source's properties.
[PASS] OBS: sliders Brightness, Contrast, Saturation, Hue (degrees), Audio gain (dB)?
PROMPT: Set Hue to -60 -> OK; wait 3 s.
$ pic
brightness: 0 contrast: 0 saturation: 0 hue: -60 analog_audio_gain_db: 0 
[PASS] OBS: hue -60
$ v4l2-ctl -d /dev/video0 -c hue=0
PROMPT: Close OBS.
--- dmesg step 7
[  987.855870] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  988.042258] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  988.113132] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  988.133078] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xba97 samples6463=1 avg6463=0xb6d7 rgb=1
[  988.133088] elgato_gchd 1-2:1.0: gchd-diag detected input=0 mode=1920x1080 fps=60/1 interlaced=0 signal_present=1
[  988.133098] elgato_gchd 1-2:1.0: signal detected: input=0 mode=1920x1080 fps=60/1 interlaced=0
[  988.133101] elgato_gchd 1-2:1.0: capture setup: input=0 mode=1920x1080 fps=60/1 interlaced=0
[  989.019213] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  990.355300] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  990.382071] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  990.382632] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  990.383616] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  990.384054] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=0 mode=1920x1080 fps=60/1 interlaced=0
[  990.384832] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x000a (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x000a
[  990.455321] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  995.413409] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1000.446748] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1001.885274] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=1 streaming=1
[ 1002.960694] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[ 1003.181439] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x26 masked_mode=0x6
[ 1003.183988] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 1003.251314] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x26 masked_mode=0x6
[ 1003.254037] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 1003.323764] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 1003.326056] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[ 1003.392887] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[ 1003.462655] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[ 1003.536024] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[ 1003.605219] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[ 1003.607584] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[ 1003.607586] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 1003.607589] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 1003.607595] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 1003.748206] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 1003.774639] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1003.775045] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1003.803087] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1003.804251] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 1003.804828] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 1003.914027] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1008.836780] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1013.860500] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1018.883596] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1023.906632] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1028.930436] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1033.954028] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=680 bytes=44564480 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1038.977705] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=795 bytes=52101120 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1044.001994] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=910 bytes=59637760 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1049.026087] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1025 bytes=67174400 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1054.050128] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1140 bytes=74711040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1059.073586] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1255 bytes=82247680 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1064.097557] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1370 bytes=89784320 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1069.120745] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1485 bytes=97320960 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1072.643589] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 8: text menu
PROMPT: Run gchd-settings in another terminal: choose Picture, Contrast, enter 25; then Quit.
$ pic
brightness: 0 contrast: 25 saturation: 0 hue: 0 analog_audio_gain_db: 0 
[PASS] text menu: contrast 25
$ v4l2-ctl -d /dev/video0 -c contrast=0
--- dmesg step 8


=== step 9: v4l2-compliance
$ sh -c grep -E 'fail|FAIL' /home/ff5/gchd-test-logs/T-080-compliance.txt | head -10; grep -E '^Total' /home/ff5/gchd-test-logs/T-080-compliance.txt
Total for elgato-gchd device /dev/video0: 115, Succeeded: 115, Failed: 0, Warnings: 4
$ grep -E ^Total /home/ff5/gchd-test-logs/T-080-compliance.txt
Total for elgato-gchd device /dev/video0: 115, Succeeded: 115, Failed: 0, Warnings: 4
[PASS] compliance: 0 failures
$ v4l2-ctl -d /dev/video0 -c brightness=0,contrast=0,saturation=0,hue=0,analog_audio_gain_db=0
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
--- dmesg step 9
[ 1139.207089] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=0 streaming=0
[ 1139.207125] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 1139.207134] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 1139.207147] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 1139.207150] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 1139.207158] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 1139.207161] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 1139.207170] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 1139.207173] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[ 1139.207182] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 1139.207184] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=0 streaming=0
[ 1139.207192] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 1139.207195] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 1139.207202] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 1139.207204] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 1139.207212] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 1139.207214] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 1139.207223] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 1139.207242] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 1139.207249] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 1139.207253] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 1139.207259] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 1139.207262] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 1139.207269] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 1139.207274] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 1139.207281] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 1139.207285] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 1139.207292] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 1139.207309] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 1139.207316] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 1139.207334] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 1139.207341] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 1139.207343] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 1139.207350] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 1139.207353] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 1139.207361] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 1139.207363] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 1139.207370] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 1139.207372] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 1139.207378] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 1139.207380] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 1139.207387] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 1139.207389] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 1139.207395] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 1139.207403] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 1139.207410] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 1139.207412] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 1139.207419] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 1139.207421] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 1139.207428] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 1139.207431] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 1139.207437] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 1139.212400] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 1139.212413] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 1139.214346] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 1139.214354] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 1139.216690] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 1139.216702] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 1139.219728] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[ 1139.219746] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 1139.219809] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 1139.219818] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 1139.232269] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0

=== summary (T-080, 6d94540 TASK: T-080 done so far, hardware test)
PASS | step 1 install | controls present, all 0
FAIL | step 2 picture settings while capturing (composite) | Brightness: the picture became dark, then bright, then normal, without stopping?
FAIL | step 2 picture settings while capturing (composite) | Contrast: flat grey, then very hard, then normal?
FAIL | step 2 picture settings while capturing (composite) | Saturation: black and white, then very colourful, then normal?
FAIL | step 2 picture settings while capturing (composite) | Hue: the colours shifted one way, then the other way, then normal?
PASS | step 2 picture settings while capturing (composite) | Audio: quieter, then louder (maybe distorted), then normal, while the picture kept running?
PASS | step 2 picture settings while capturing (composite) | no encoder restart for picture or audio changes
PASS | step 3 audio gain measured (composite) | -12 dB: about 12 dB quieter (8-16)
FAIL | step 3 audio gain measured (composite) | +6 dB: about 6 dB louder (3-9) (expected /^ok/, got: no -3,1)
PASS | step 4 remembered after a replug | kept: 20 -10 30 45 -3
PASS | step 5 HDMI (laptop, RGB): picture settings with the RGB conversion | HDMI: black and white?
PASS | step 5 HDMI (laptop, RGB): picture settings with the RGB conversion | HDMI: harder contrast, colours right (no tint)?
PASS | step 5 HDMI (laptop, RGB): picture settings with the RGB conversion | Expanded with contrast +50: still a picture with the colours right (even harder)?
PASS | step 6 settings window | Rows Brightness, Contrast, Saturation, Hue (degrees), Audio gain (dB) with sliders at 0, and a 'Reset picture' button?
PASS | step 6 settings window | window: brightness about 40
PASS | step 6 settings window | window: reset
PASS | step 6 settings window | HDMI: the Audio gain row is hidden, the picture rows are shown?
PASS | step 7 OBS plugin | OBS: sliders Brightness, Contrast, Saturation, Hue (degrees), Audio gain (dB)?
PASS | step 7 OBS plugin | OBS: hue -60
PASS | step 8 text menu | text menu: contrast 25
PASS | step 9 v4l2-compliance | compliance: 0 failures
