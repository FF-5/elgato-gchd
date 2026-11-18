# TASK T-061 — Bitrate and stretch for analog sources

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** implemented (4d1a342); hardware test with the test script
- **Type:** driver feature
- **Branch:** `feat/bitrate` (from `dev` 0.5.3)
- **TODO items:** T-061 (stage 1)

## Why

The capture application sets the video bitrate (a slider per output size)
and offers "stretch" for standard-definition input; the driver always
encodes the default bitrate and never stretches. Recorded
(`docs/composite.md §10`, `traces/composite/`):
`gchd_288p_standard_bitrate_change_small`,
`gchd_288p_mobile_bitrate_change_small`, `gchd_288p_stretch_input_small`.

| Setting | Encoder fields |
|---|---|
| Standard, maximum (default) | CBR (`v_rate_mode 0`), `v_max_cpb_delay 3`, 11250 / 10125 / 5625 / 2812 |
| Standard, lower | VBR (`v_rate_mode 1`), `v_max_cpb_delay 10`, e.g. 6000 / 5400 / 3000 / 600 |
| Mobile | VBR, 4000 … 15000 |
| Stretch | `v_in_source_type 2` instead of 3 |

## Implementation

Recorded values (`docs/composite.md §10`): maximum 0.9·B, average B/2,
minimum 0 / B/10 / B/4; `system_rate` ⌊1.07285·B + 461.7⌋ (+8 at 60 Hz);
CBR only at the default 11250, VBR otherwise; stretch = `v_in_source_type`
2. Controls `video_bitrate`, `video_bitrate_mode`, `stretch_sd_input`;
on composite/S-video a change while capturing restarts the encoder.

## Hardware test (maintainer)

First run of the test script (`tools/gchd-test.sh`, `TODO.md` "Testing").
It builds the branch, loads the module, runs everything and asks you for
the console and for two visual checks. Needs `v4l-utils` and `ffmpeg`
(already installed). In your home directory:

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-061.sh > tests/T-061.sh
bash gchd-test.sh T-061 --branch feat/bitrate
```

Paste the summary block it prints at the end. If any line says FAIL or
WARN, paste the whole log file too (path printed at the end, in
`~/gchd-test-logs/`). If the script itself misbehaves (wrong prompt,
hangs), say where; Ctrl+C stops it.

## Findings
- Maintainer requests (2026-11-18, before T-061): all wanted Windows
  recordings listed in `TODO.md` "Evidence wanted" (R1-R10, marked
  needed / optional / not needed); hardware tests through a script
  (`TODO.md` "Testing", `tools/gchd-test.sh` with `tools/tests/smoke.sh`
  on the `tools` branch, CLAUDE.md "Hardware testing"); pre-release test
  T-090.

## Maintainer comments

gchd-test T-061 20261118-113815 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-061-20261118-113815.log
== build
$ git fetch -q origin
$ git checkout -q feat/bitrate
$ git pull -q --ff-only
commit: 3768423 TASK: T-061 hardware test via gchd-test.sh
commit: 3768423 TASK: T-061 hardware test via gchd-test.sh
build rc=0 warnings=1

=== step 1: load module (fresh plug-in), controls listed
PROMPT: Unplug the Elgato from the PC (or detach it in VirtualBox), wait 3 s, plug it in again.
$ sudo insmod /home/ff5/elgato-gchd/dkms/elgato_gchd.ko
module: 0.5.3 
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -v width=720,height=576,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -l

User Controls

                   input_source 0x009819f0 (menu)   : min=0 max=3 default=0 value=2 (Composite)
               stretch_sd_input 0x009819f1 (bool)   : default=0 value=0

Codec Controls

             video_bitrate_mode 0x009909ce (menu)   : min=0 max=1 default=1 value=1 (Constant Bitrate) flags=update
                  video_bitrate 0x009909cf (int)    : min=32000 max=40000000 step=1000 default=11250000 value=11250000
                     h264_level 0x00990a67 (menu)   : min=0 max=15 default=12 value=12 (4.1)
                   h264_profile 0x00990a6b (menu)   : min=0 max=4 default=2 value=2 (Main)
--- dmesg step 1
[62861.881213] usbcore: deregistering interface driver elgato_gchd
[62886.405072] usb 1-2: USB disconnect, device number 69
[62894.033992] usb 1-2: new high-speed USB device number 70 using xhci_hcd
[62894.437639] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[62894.437682] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[62894.437689] usb 1-2: Product: GameCapture HD
[62894.437694] usb 1-2: Manufacturer: Elgato
[62894.437698] usb 1-2: SerialNumber: 04107F1267
[62894.456263] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[62894.458419] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[62894.458445] usbcore: registered new interface driver elgato_gchd
[62894.471885] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[62894.471892] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[62894.471894] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0

=== step 2: default: 11250 kbit/s CBR
PROMPT: PS1 on composite, game running (PAL).
capturing 10s to step2-16541.ts
capture: 11579392 bytes, ~9263 kbit/s; codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 ; decode errors 1
  codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 ; ~9263 kbit/s; decode errors 1
audio: mp2
[FAIL] stream expected 720x576 25/1 tt, got: codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 
--- dmesg step 2
[62939.464826] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[62939.659560] elgato_gchd 1-2:1.0: transcoder defaults loaded
[62939.727366] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[62940.143571] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[62940.145980] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[62940.145986] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[62940.145988] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[62940.145991] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[62940.955387] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[62941.215311] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[62941.230496] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[62941.230806] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[62941.236191] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[62941.237179] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[62941.237605] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[62941.376484] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[62946.241057] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[62950.263039] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 3: 6000 kbit/s VBR (set while stopped)
$ v4l2-ctl -d /dev/video0 -c video_bitrate_mode=0,video_bitrate=6000000
capturing 10s to step3-30134.ts
capture: 2289664 bytes, ~1831 kbit/s; codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 ; decode errors 3
  codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 ; ~1831 kbit/s; decode errors 3
audio: mp2
[FAIL] stream expected 720x576 25/1 tt, got: codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 
--- dmesg step 3
[62950.882803] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[62950.882806] elgato_gchd 1-2:1.0: waiting 880 ms after the previous shutdown
[62951.857101] elgato_gchd 1-2:1.0: transcoder defaults loaded
[62951.915376] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[62951.997881] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[62952.111534] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[62952.198005] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[62952.225391] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[62952.227263] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[62952.293539] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[62952.319926] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[62952.321913] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[62952.321915] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[62952.321917] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[62952.321919] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[62953.069837] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[62953.265361] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[62953.280120] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[62953.280340] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[62953.285350] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[62953.285850] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[62953.286267] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[62953.425974] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[62953.935943] elgato_gchd: input 2 signal lost; scheduling redetection
[62953.961840] elgato_gchd 1-2:1.0: signal present: input=2; first video TS packet on PID 0x1011
[62956.088280] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[62956.090637] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[62956.090640] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[62956.090643] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[62956.090645] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[62956.336641] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[62956.354449] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[62956.354657] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[62956.359917] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[62956.360636] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[62956.361342] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[62956.500160] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[62961.447247] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=31 bytes=2031616 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=4
[62962.522894] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 4: 14040 kbit/s VBR, changed while capturing
capturing 14s to step4-4338.ts
during capture: v4l2-ctl -d /dev/video0 -c video_bitrate=14040000
capture: 5558272 bytes, ~3176 kbit/s; codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 ; decode errors 6
  codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 ; ~3176 kbit/s; decode errors 6
audio: mp2
[FAIL] stream expected 720x576 25/1 tt, got: codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 
--- dmesg step 4
[62962.833066] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[62962.833069] elgato_gchd 1-2:1.0: waiting 1190 ms after the previous shutdown
[62964.206690] elgato_gchd 1-2:1.0: transcoder defaults loaded
[62964.268683] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[62964.399857] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[62964.458061] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[62964.483669] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[62964.485695] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[62964.485697] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[62964.485698] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[62964.485700] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[62965.251347] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[62965.517288] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[62965.535480] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[62965.535836] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[62965.543541] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[62965.545100] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[62965.545891] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[62965.661320] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[62967.844638] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[62969.438241] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[62969.438498] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[62969.444054] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[62969.444627] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[62969.444995] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[62969.583981] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[62969.583984] elgato_gchd 1-2:1.0: analog mode now 576i, output 0
[62974.446595] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=46 bytes=3014656 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[62978.571688] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 5: back to 11250 CBR while capturing
capturing 14s to step5-31635.ts
during capture: v4l2-ctl -d /dev/video0 -c video_bitrate_mode=1,video_bitrate=11250000
capture: 12034048 bytes, ~6876 kbit/s; codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 ; decode errors 3
  codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 ; ~6876 kbit/s; decode errors 3
audio: mp2
[FAIL] stream expected 720x576 25/1 tt, got: codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 
--- dmesg step 5
[62979.038167] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[62979.038169] elgato_gchd 1-2:1.0: waiting 1035 ms after the previous shutdown
[62980.181298] elgato_gchd 1-2:1.0: transcoder defaults loaded
[62980.248274] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[62980.357909] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[62980.421086] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[62980.423308] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[62980.423310] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[62980.423312] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[62980.423314] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[62981.206504] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[62981.448157] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[62981.463236] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[62981.463545] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[62981.469314] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[62981.470029] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[62981.470712] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[62981.589117] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[62984.049070] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[62985.633621] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[62985.633927] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[62985.638774] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[62985.639290] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[62985.639674] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[62985.779356] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[62985.779359] elgato_gchd 1-2:1.0: analog mode now 576i, output 0
[62990.645838] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[62994.035249] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 6: stretch
$ v4l2-ctl -d /dev/video0 -c stretch_sd_input=1
ffplay for 15s
[PASS] Is the picture different from normal (stretched)? Describe in the note how. -- horizontally stretched
$ v4l2-ctl -d /dev/video0 -c stretch_sd_input=0
--- dmesg step 6
[62994.806828] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[62994.806830] elgato_gchd 1-2:1.0: waiting 728 ms after the previous shutdown
[62995.648131] elgato_gchd 1-2:1.0: transcoder defaults loaded
[62995.711318] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[62995.793020] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[62995.852507] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[62995.855258] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[62995.855259] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[62995.855261] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[62995.855263] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[62996.633195] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[62996.870845] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[62996.887691] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[62996.887919] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[62996.894617] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[62996.895197] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[62996.896327] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[62997.033655] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[63001.896342] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=21
[63006.922553] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[63010.494910] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 7: mobile 480x360 at 4000 kbit/s
$ v4l2-ctl -d /dev/video0 -v width=480,height=360,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -c video_bitrate=4000000
capturing 10s to step7-10855.ts
capture: 389120 bytes, ~311 kbit/s; codec_name=h264 width=480 height=360 field_order=progressive r_frame_rate=25/1 ; decode errors 1
  codec_name=h264 width=480 height=360 field_order=progressive r_frame_rate=25/1 ; ~311 kbit/s; decode errors 1
audio: mp2
[PASS] stream 480x360 25/1 progressive
--- dmesg step 7
[63031.266768] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[63031.393753] elgato_gchd 1-2:1.0: transcoder defaults loaded
[63031.464486] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[63031.578041] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[63031.739315] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[63031.779704] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[63031.782474] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[63031.782477] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[63031.782480] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[63031.782483] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[63032.560808] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[63032.897210] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[63032.919732] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[63032.920077] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[63032.926340] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[63032.927168] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[63032.927678] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[63033.065701] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[63033.323833] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 2
[63035.085697] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[63035.087125] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[63035.100735] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[63035.101375] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[63035.101870] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[63035.231555] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[63035.231558] elgato_gchd 1-2:1.0: analog mode now 576i, output 2
[63035.777141] elgato_gchd: input 2 signal lost; scheduling redetection
[63035.848544] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[63035.848555] elgato_gchd 1-2:1.0: signal present: input=2; first video TS packet on PID 0x1011
[63037.964996] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[63037.967029] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[63037.967032] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[63037.967034] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[63037.967040] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[63038.220061] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[63038.240338] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[63038.240650] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[63038.246953] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[63038.247661] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[63038.248255] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[63038.386377] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[63038.386402] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 2
[63040.331768] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[63040.331995] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[63040.338344] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[63040.338859] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[63040.339430] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[63040.477601] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[63040.477604] elgato_gchd 1-2:1.0: analog mode now 576i, output 2
[63041.021610] elgato_gchd: input 2 signal lost; scheduling redetection
[63041.093039] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[63041.093064] elgato_gchd 1-2:1.0: signal present: input=2; first video TS packet on PID 0x1011
[63043.120794] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[63043.123391] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[63043.123396] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[63043.123400] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[63043.123402] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[63043.351376] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[63043.366221] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[63043.366451] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[63043.372172] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[63043.373007] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[63043.373464] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[63043.512170] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[63043.512269] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 2
[63045.459329] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[63045.459552] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[63045.467158] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[63045.468070] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[63045.469063] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[63045.585258] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[63045.585262] elgato_gchd 1-2:1.0: analog mode now 576i, output 2
[63046.201100] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[63048.329454] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 8: mobile 480x360 at 15000 kbit/s
$ v4l2-ctl -d /dev/video0 -c video_bitrate=15000000
capturing 10s to step8-5909.ts
capture: 3399680 bytes, ~2719 kbit/s; codec_name=h264 width=480 height=360 field_order=progressive r_frame_rate=25/1 ; decode errors 1
  codec_name=h264 width=480 height=360 field_order=progressive r_frame_rate=25/1 ; ~2719 kbit/s; decode errors 1
audio: mp2
[PASS] stream 480x360 25/1 progressive
--- dmesg step 8
[63048.542374] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[63048.542377] elgato_gchd 1-2:1.0: waiting 1287 ms after the previous shutdown
[63049.935016] elgato_gchd 1-2:1.0: transcoder defaults loaded
[63049.997257] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[63050.110971] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[63050.248570] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[63050.370758] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[63050.403980] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[63050.406229] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[63050.502509] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[63050.504942] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[63050.504945] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[63050.504948] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[63050.504951] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[63051.276440] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[63051.547793] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[63051.564372] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[63051.564588] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[63051.571308] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[63051.572418] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[63051.572926] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[63051.710294] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[63051.968982] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 2
[63053.650789] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[63053.651134] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[63053.657467] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[63053.658333] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[63053.658944] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[63053.796554] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[63053.796557] elgato_gchd 1-2:1.0: analog mode now 576i, output 2
[63054.189993] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[63058.695870] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=52 bytes=3407872 ep84_bytes=3407872 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[63060.025459] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 9: picture check at the end
$ v4l2-ctl -d /dev/video0 -v width=720,height=576,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -c video_bitrate=11250000
ffplay for 10s
[PASS] Normal PS1 picture with sound?
--- dmesg step 9
[63060.540384] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[63060.540386] elgato_gchd 1-2:1.0: waiting 985 ms after the previous shutdown
[63061.633179] elgato_gchd 1-2:1.0: transcoder defaults loaded
[63061.694433] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[63061.780250] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[63061.833923] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[63061.835821] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[63061.835823] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[63061.835825] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[63061.835826] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[63062.580624] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[63062.791656] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[63062.807625] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[63062.807950] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[63062.813389] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[63062.814179] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[63062.814576] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[63062.953246] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[63067.818216] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=21
[63071.224054] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== summary (T-061, 3768423 TASK: T-061 hardware test via gchd-test.sh)
FAIL | step 2 default: 11250 kbit/s CBR | stream expected 720x576 25/1 tt, got: codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 
FAIL | step 3 6000 kbit/s VBR (set while stopped) | stream expected 720x576 25/1 tt, got: codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 
FAIL | step 4 14040 kbit/s VBR, changed while capturing | stream expected 720x576 25/1 tt, got: codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 
FAIL | step 5 back to 11250 CBR while capturing | stream expected 720x576 25/1 tt, got: codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 
PASS | step 6 stretch | Is the picture different from normal (stretched)? Describe in the note how. -- horizontally stretched
PASS | step 7 mobile 480x360 at 4000 kbit/s | stream 480x360 25/1 progressive
PASS | step 8 mobile 480x360 at 15000 kbit/s | stream 480x360 25/1 progressive
PASS | step 9 picture check at the end | Normal PS1 picture with sound?
