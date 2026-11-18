# TASK T-060 — guvcview, OBS and similar applications

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** prototype (`service/gchd-bridge`); first hardware test
- **Type:** new userspace component (service) next to the driver
- **Branch:** `feat/app-bridge` (from `dev` 0.5.8)
- **TODO items:** T-060 (stage 1b)

## Why

The driver delivers H.264 or an MPEG transport stream (H.264 + MP2 audio).
guvcview, OBS's V4L2 source, browsers and video-call programs expect a
V4L2 device with uncompressed frames (YUYV/NV12) and a separate audio
source. Decoding is not allowed in the kernel (RULES E2), so a small
userspace service turns the driver's stream into such a device.

## Goal (acceptance criteria, draft)

1. A second video device, named "Elgato Game Capture HD", offers
   uncompressed frames (YUYV and/or NV12) at a fixed size for the session,
   deinterlaced (480i/576i) or line-doubled (240p/288p).
2. An audio source of the same name in PipeWire/PulseAudio, in sync with
   the picture.
3. The service starts when an application opens the device and releases
   the capture device when no application uses it (or after a delay).
4. A source change (NTSC ↔ PAL, 480i ↔ 240p, console swap) does not change
   the format the applications see; the picture is scaled and the aspect
   ratio kept.
5. The input can be selected without `v4l2-ctl` (the driver's menu control
   was removed in T-066).
6. Works in guvcview and OBS (V4L2 source and audio capture) with PS1 (PAL)
   and N64 (NTSC) on composite; installed by `install-driver.sh`, removed by
   `--uninstall`.

## Decisions (maintainer, 2026-11-18)

1. Language: the fastest in the end; shell + ffmpeg for now, kept modular
   so parts can be replaced (ffmpeg does the decoding in C either way).
2. The capture device runs only while an application uses it (the device
   gets hot when it runs without need).
3. Output size: the size of the source. Large changes (NTSC ↔ PAL, a
   different HDMI/component resolution, an input change) do not happen
   within one piece of content; when they happen, the device the
   applications use is removed and added again, as when a UVC camera is
   unplugged and plugged in again (applications such as guvcview and OBS
   handle that while running).
4. Folder: `service/`.
5. (Maintainer, after the analysis of 3) The application device becomes a
   second video node of `elgato_gchd`: the bridge writes decoded frames
   into it, applications read them (frames are passed through only, no
   decoding in the kernel). The driver unregisters and registers it again
   on a large change, which v4l2loopback cannot do: it refuses to remove a
   device while an application has it open (EBUSY, v4l2loopback 0.13.2
   `V4L2LOOPBACK_CTL_REMOVE`; 0.12.7 cannot add or remove at runtime). The
   driver also tells the bridge when an application opens the node (no
   polling), and v4l2loopback is no longer needed.
6. Tests without OBS for now (the VM cannot run it); guvcview only.

## Prototype (`service/gchd-bridge`)

- `gchd-bridge run`: finds the driver's device and the v4l2loopback
  device (by driver, label "Elgato Game Capture HD"), creates a null sink
  and an audio source "Elgato Game Capture HD" (`pactl`).
- Idle: a black YUYV picture of the current source size at 5 frames/s,
  so applications can list and open the device; the capture device stays
  off. The idle size follows the source (checked every 5 s).
- An application opens the loopback device (`fuser`, 2 checks 1 s apart):
  the black picture stops; ffmpeg reads the transport stream of the
  driver, deinterlaces (`bwdif`, one frame per field: 50/60 frames/s),
  scales to the opened size, writes YUYV to the loopback device and the
  audio to the null sink. `keep_format=1` on the loopback device keeps
  the format during the switch.
- Last application closed: ffmpeg stops, the driver shuts the device down,
  the black picture returns.
- `gchd-bridge input composite|svideo|component|hdmi`: stops the capture,
  selects the input, the capture restarts with the same size.
- Offline check: an interlaced 576i → 480i transport stream through the
  same filter chain gives continuous 720×576 YUYV frames.

Next steps after the test: the driver's own application node (decision
5) replacing v4l2loopback, size and input changes as remove/add;
systemd user service (start at login),
`install-driver.sh` integration (v4l2loopback options in `modprobe.d`,
service install), documentation (`docs/`), input selection from the
desktop.

## Hardware test (maintainer)

First test of the prototype with v4l2loopback: decoding, deinterlacing,
sound, capture device only on while read, input selection, guvcview.
Needs `v4l2loopback-dkms` and guvcview. PS1 (PAL) on composite, the N64
(NTSC) for the source change (with v4l2loopback the picture is still
scaled). The test loads v4l2loopback as
`/dev/video42`; if it is loaded already with other options it is
reloaded.

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-060.sh > tests/T-060.sh
bash gchd-test.sh T-060 --branch feat/app-bridge
```

Paste the summary; on FAILs the log too (the bridge's own messages are
at the end of it).

## Findings

- T-066 accepted: gchd-test.sh T-066 6/6 PASS on 978d3fc
  (`v4l2-compliance` 0 failures; DV timings on HDMI without a signal; DV
  timings list; power-present 0; PAL composite 720×576 capture; input
  change from another process refused). Merged as 0.5.8.

## Maintainer comments

gchd-test T-060 20261118-173356 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-060-20261118-173356.log
== build
$ git fetch -q origin
$ git checkout -q feat/app-bridge
$ git pull -q --ff-only
commit: edb0656 TASK/TODO: T-060 decision: own application node with unplug-like restart; tests without OBS
commit: edb0656 TASK/TODO: T-060 decision: own application node with unplug-like restart; tests without OBS
build rc=0 warnings=0

=== step 1: v4l2loopback and the driver
$ sh -c modinfo v4l2loopback | grep -E '^(version|filename)'
filename:       /lib/modules/6.8.0-146-generic/updates/dkms/v4l2loopback.ko.zst
PROMPT: Unplug the Elgato from the PC (or detach it in VirtualBox), wait 3 s, plug it in again.
$ sudo insmod /home/ff5/elgato-gchd/src/elgato_gchd.ko
module: 0.5.8 
$ sudo modprobe -r v4l2loopback
$ sudo modprobe v4l2loopback devices=1 video_nr=42 exclusive_caps=1 card_label=Elgato Game Capture HD
$ v4l2-ctl --list-devices
Elgato Game Capture HD (platform:v4l2loopback-000):
	/dev/video42

Elgato Game Capture HD (usb-0000:00:0c.0-2):
	/dev/video0
[PASS] loopback device present
--- dmesg step 1
[84202.727386] usbcore: deregistering interface driver elgato_gchd
[84206.700943] usb 1-2: USB disconnect, device number 80
[84212.111867] usb 1-2: new high-speed USB device number 81 using xhci_hcd
[84212.499211] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[84212.499233] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[84212.499239] usb 1-2: Product: GameCapture HD
[84212.499244] usb 1-2: Manufacturer: Elgato
[84212.499248] usb 1-2: SerialNumber: 04107F1267
[84214.028777] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[84214.029895] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[84214.029914] usbcore: registered new interface driver elgato_gchd
[84214.055083] v4l2loopback driver version 0.12.7 loaded

=== step 2: bridge idle: black picture, capture device off
PROMPT: PS1 on composite, game running.
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ bash /home/ff5/elgato-gchd/service/gchd-bridge status
capture device:  /dev/video0
loopback device: none
service:         not running
$ v4l2-ctl -d /dev/video42 --get-fmt-video
VIDIOC_G_FMT: failed: Invalid argument
[FAIL] loopback offers 720x576 YUYV while idle (expected /720/576/, got: VIDIOC_G_FMT: failed: Invalid argument)
$ pactl list short sources
47	alsa_output.pci-0000_00_05.0.analog-stereo.monitor	PipeWire	s16le 2ch 48000Hz	SUSPENDED
48	alsa_input.pci-0000_00_05.0.analog-stereo	PipeWire	s16le 2ch 48000Hz	SUSPENDED
[FAIL] audio source present (expected /elgato_gchd_audio/, got: 47	alsa_output.pci-0000_00_05.0.analog-stereo.monitor	PipeWire	s16le 2ch 48000Hz	SUSPENDED 48	alsa_input.pci-0000_00_05.0.analog-stereo	PipeWire	s16le 2ch 48000)
$ sh -c sudo dmesg | grep -c 'starting hardware boot'
0
[PASS] capture device off while idle
--- dmesg step 2
[84230.198219] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[84230.198247] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== step 3: application reads: capture starts, 720x576 at 50 frames/s
$ cat /home/ff5/gchd-test-logs/T-060-20261118-173356/read.txt
$ cat /home/ff5/gchd-test-logs/T-060-20261118-173356/read.txt

[FAIL] loopback stream 720x576 YUYV (expected /yuyv422.*720x576/, got: )
$ cat /home/ff5/gchd-test-logs/T-060-20261118-173356/read.txt

[FAIL] frame count >= 400 in 15 s (capture running) (expected /frame= *([4-9][0-9]{2}|[0-9]{4})/, got: )
$ sudo dmesg

[FAIL] capture device started (expected /starting hardware boot/, got: )
--- dmesg step 3


=== step 4: application closed: capture device off
$ sudo dmesg

[FAIL] hardware shut down after the last reader (expected /hardware shutdown done/, got: )
--- dmesg step 4


=== step 5: audio while capturing
$ ffmpeg -hide_banner -nostdin -f pulse -i elgato_gchd_audio -t 4 -af volumedetect -f null -
[aist#0:0/pcm_s16le @ 0x5db2be389640] Guessed Channel Layout: stereo
Input #0, pulse, from 'elgato_gchd_audio':
  Duration: N/A, start: 1795019690.100315, bitrate: 1536 kb/s
  Stream #0:0: Audio: pcm_s16le, 48000 Hz, 2 channels, s16, 1536 kb/s
[Parsed_volumedetect_0 @ 0x5db2be38b000] n_samples: 0
Stream mapping:
  Stream #0:0 -> #0:0 (pcm_s16le (native) -> pcm_s16le (native))
Output #0, null, to 'pipe:':
  Metadata:
    encoder         : Lavf60.16.100
  Stream #0:0: Audio: pcm_s16le, 48000 Hz, stereo, s16, 1536 kb/s
    Metadata:
      encoder         : Lavc60.31.102 pcm_s16le
size=       0kB time=00:00:00.00 bitrate=N/A speed=   0x    
size=N/A time=00:00:00.58 bitrate=N/A speed=1.03x    
size=N/A time=00:00:01.11 bitrate=N/A speed=1.02x    
size=N/A time=00:00:01.62 bitrate=N/A speed=0.995x    
size=N/A time=00:00:02.15 bitrate=N/A speed=0.989x    
size=N/A time=00:00:02.68 bitrate=N/A speed=0.985x    
size=N/A time=00:00:03.27 bitrate=N/A speed=   1x    
size=N/A time=00:00:03.80 bitrate=N/A speed=1.01x    
[out#0/null @ 0x5db2be386b00] video:0kB audio:729kB subtitle:0kB other streams:0kB global headers:0kB muxing overhead: unknown
size=N/A time=00:00:03.96 bitrate=N/A speed=0.983x    
[Parsed_volumedetect_0 @ 0x5db2be38da80] n_samples: 379200
[Parsed_volumedetect_0 @ 0x5db2be38da80] mean_volume: -91.0 dB
[Parsed_volumedetect_0 @ 0x5db2be38da80] max_volume: -91.0 dB
[Parsed_volumedetect_0 @ 0x5db2be38da80] histogram_91db: 379200
[FAIL] audio source carries sound (mean volume above -60 dB) (expected /mean_volume: -([0-9]|[1-5][0-9])\./, got: [aist#0:0/pcm_s16le @ 0x5db2be389640] Guessed Channel Layout: stereo Input #0, pulse, from 'elgato_gchd_audio':   Duration: N/A, start: 1795019690.100315, bitra)
--- dmesg step 5


=== step 6: input change through the bridge while an application reads
$ bash /home/ff5/elgato-gchd/service/gchd-bridge input svideo
Video input set to 3 (S-Video: Camera, no signal)
$ bash /home/ff5/elgato-gchd/service/gchd-bridge input composite
Video input set to 2 (Composite: Camera, no signal)
$ cat /home/ff5/gchd-test-logs/T-060-20261118-173356/input-change.txt
$ cat /home/ff5/gchd-test-logs/T-060-20261118-173356/input-change.txt

[FAIL] reader kept running (frame count >= 500) (expected /frame= *([5-9][0-9]{2}|[0-9]{4})/, got: )
$ v4l2-ctl -d /dev/video0 --get-input
Video input : 2 (Composite: no signal)
[PASS] input back on composite
--- dmesg step 6
[84258.722378] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[84258.722431] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[84263.769261] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[84263.769293] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== step 7: guvcview
PROMPT: Start: guvcview -d /dev/video42   (in a second terminal; leave it open)
[FAIL] guvcview shows the PS1 picture, smooth, no interlacing lines? -- it give an error no video device /dev/video42 found
PROMPT: Swap the PS1 for the N64 (NTSC) while guvcview stays open; wait about 10 s.
[PASS] guvcview shows the N64 picture again (scaled to the PAL size)? -- now it does, on PS1 i was stuck on an error and had to select a different device in guvcview to open
PROMPT: Close guvcview.
$ v4l2-ctl -d /dev/video42 --get-fmt-video
VIDIOC_G_FMT: failed: Invalid argument
[FAIL] idle size follows the NTSC source after closing (expected /720/480/, got: VIDIOC_G_FMT: failed: Invalid argument)
--- dmesg step 7
[84383.881612] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[84384.014147] elgato_gchd 1-2:1.0: transcoder defaults loaded
[84384.081635] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[84384.500301] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[84384.502432] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[84384.502434] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[84384.502436] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[84384.502438] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[84385.283809] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[84385.404386] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[84385.419651] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[84385.419866] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[84385.441129] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[84385.441695] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[84385.442233] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[84385.575674] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[84385.863605] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[84387.636434] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[84387.636667] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[84387.642835] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[84387.643396] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[84387.644024] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[84387.742695] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[84387.742698] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[84388.113994] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[84392.671180] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[84397.667656] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[84397.699072] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=14417920 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[84399.255447] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[84399.255806] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[84399.261882] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[84399.262352] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[84399.263066] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[84399.378261] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[84399.378263] elgato_gchd 1-2:1.0: analog mode now 240p, output 1
[84399.751308] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[84404.267015] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[84409.300539] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[84414.332547] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=21889024 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[84419.364446] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=29425664 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[84424.396337] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=36962304 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[84428.663751] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[84429.426638] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=679 bytes=44498944 ep84_bytes=44498944 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[84430.134662] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[84430.134951] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[84430.140811] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[84430.141601] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[84430.142299] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[84430.257284] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[84430.257287] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[84430.627285] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[84435.140114] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[84440.172898] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[84445.204586] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=21889024 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[84450.235330] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=29425664 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[84455.267019] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=36962304 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[84459.559349] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 8: bridge stopped
$ kill 43249
gchd-test.sh: line 88: kill: (43249) - No such process
$ sh -c pactl list short sources | grep -c elgato_gchd
0
[PASS] audio source removed
$ tail -30 /home/ff5/gchd-test-logs/T-060-20261118-173356/bridge.log
gchd-bridge: no v4l2loopback device "Elgato Game Capture HD" (modprobe v4l2loopback exclusive_caps=1 card_label="Elgato Game Capture HD")
$ sudo modprobe -r v4l2loopback
--- dmesg step 8


=== summary (T-060, edb0656 TASK/TODO: T-060 decision: own application node with unplug-like restart; tests without OBS)
PASS | step 1 v4l2loopback and the driver | loopback device present
FAIL | step 2 bridge idle: black picture, capture device off | loopback offers 720x576 YUYV while idle (expected /720/576/, got: VIDIOC_G_FMT: failed: Invalid argument)
FAIL | step 2 bridge idle: black picture, capture device off | audio source present (expected /elgato_gchd_audio/, got: 47	alsa_output.pci-0000_00_05.0.analog-stereo.monitor	PipeWire	s16le 2ch 48000Hz	SUSPENDED 48	alsa_input.pci-0000_00_05.0.analog-stereo	PipeWire	s16le 2ch 48000)
PASS | step 2 bridge idle: black picture, capture device off | capture device off while idle
FAIL | step 3 application reads: capture starts, 720x576 at 50 frames/s | loopback stream 720x576 YUYV (expected /yuyv422.*720x576/, got: )
FAIL | step 3 application reads: capture starts, 720x576 at 50 frames/s | frame count >= 400 in 15 s (capture running) (expected /frame= *([4-9][0-9]{2}|[0-9]{4})/, got: )
FAIL | step 3 application reads: capture starts, 720x576 at 50 frames/s | capture device started (expected /starting hardware boot/, got: )
FAIL | step 4 application closed: capture device off | hardware shut down after the last reader (expected /hardware shutdown done/, got: )
FAIL | step 5 audio while capturing | audio source carries sound (mean volume above -60 dB) (expected /mean_volume: -([0-9]|[1-5][0-9])\./, got: [aist#0:0/pcm_s16le @ 0x5db2be389640] Guessed Channel Layout: stereo Input #0, pulse, from 'elgato_gchd_audio':   Duration: N/A, start: 1795019690.100315, bitra)
FAIL | step 6 input change through the bridge while an application reads | reader kept running (frame count >= 500) (expected /frame= *([5-9][0-9]{2}|[0-9]{4})/, got: )
PASS | step 6 input change through the bridge while an application reads | input back on composite
FAIL | step 7 guvcview | guvcview shows the PS1 picture, smooth, no interlacing lines? -- it give an error no video device /dev/video42 found
PASS | step 7 guvcview | guvcview shows the N64 picture again (scaled to the PAL size)? -- now it does, on PS1 i was stuck on an error and had to select a different device in guvcview to open
FAIL | step 7 guvcview | idle size follows the NTSC source after closing (expected /720/480/, got: VIDIOC_G_FMT: failed: Invalid argument)
PASS | step 8 bridge stopped | audio source removed
