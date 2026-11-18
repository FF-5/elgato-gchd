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

- Second T-060 run (d285582), stopped by the test at the audio step:
  the bridge started, found both devices, made the audio source; the
  capture device stayed off while idle, started when a program read
  (577 frames in 15 s: ~3.5 s black picture at 5 frames/s, then ~49
  frames/s from PAL 576i) and shut down after the program closed. The
  idle size was 720×480: before the first detection the driver reports
  the NTSC size, so the PAL picture was scaled to 480 lines (the
  driver's own application node will take the detected size). The hang
  was the test's `wait`, which waited for the bridge (also a background
  job). Audio −85 dB: the PS1 source had no sound there (maintainer);
  the test now checks that the bridge plays into the sink and that the
  source delivers samples.
- First T-060 run (edb0656, v4l2loopback 0.12.7): 4 PASS, 10 FAIL, all
  from one cause: the bridge exited at once ("no v4l2loopback device")
  because it looked for the device's driver binding in sysfs, and
  v4l2loopback devices have none (bus `platform:v4l2loopback-000`).
  Without a producer the loopback device offers no capture format
  (G_FMT EINVAL, guvcview: "no video device"), no audio source was made
  (the -91 dB recording came from a fallback input). The N64 picture in
  guvcview came from the driver's own device (`/dev/video0`, H.264,
  640×480 output; 480i ↔ 240p changes handled). Fixed: devices found by
  sysfs name and binding without opening them; the test checks that the
  bridge runs.
- T-066 accepted: gchd-test.sh T-066 6/6 PASS on 978d3fc
  (`v4l2-compliance` 0 failures; DV timings on HDMI without a signal; DV
  timings list; power-present 0; PAL composite 720×576 capture; input
  change from another process refused). Merged as 0.5.8.

## Maintainer comments

Now it hangs while changing inputs.

gchd-test T-060 20261118-175106 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-060-20261118-175106.log
== build
$ git fetch -q origin
$ git checkout -q feat/app-bridge
$ git pull -q --ff-only
commit: 6a1895d TASK: T-060 second run findings
commit: 6a1895d TASK: T-060 second run findings
build rc=0 warnings=0

=== step 1: v4l2loopback and the driver
$ sh -c modinfo v4l2loopback | grep -E '^(version|filename)'
filename:       /lib/modules/6.8.0-146-generic/updates/dkms/v4l2loopback.ko.zst
PROMPT: Unplug the Elgato from the PC (or detach it in VirtualBox), wait 3 s, plug it in again.
$ sudo insmod /home/ff5/elgato-gchd/src/elgato_gchd.ko
module: 0.5.8 
$ sudo modprobe -r v4l2loopback
modprobe: FATAL: Module v4l2loopback is in use.
$ sudo modprobe v4l2loopback devices=1 video_nr=42 exclusive_caps=1 card_label=Elgato Game Capture HD
$ v4l2-ctl --list-devices
Elgato Game Capture HD (platform:v4l2loopback-000):
	/dev/video42

Elgato Game Capture HD (usb-0000:00:0c.0-2):
	/dev/video0
[PASS] loopback device present
--- dmesg step 1
[85230.020591] usbcore: deregistering interface driver elgato_gchd
[85233.052015] usb 1-2: USB disconnect, device number 82
[85240.206789] usbcore: registered new interface driver elgato_gchd
[85240.310909] usb 1-2: new high-speed USB device number 83 using xhci_hcd
[85240.681150] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[85240.681162] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[85240.681163] usb 1-2: Product: GameCapture HD
[85240.681168] usb 1-2: Manufacturer: Elgato
[85240.681169] usb 1-2: SerialNumber: 04107F1267
[85240.682465] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[85240.685082] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON

=== step 2: bridge idle: black picture, capture device off
PROMPT: PS1 on composite, game running.
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ bash /home/ff5/elgato-gchd/service/gchd-bridge status
capture device:  /dev/video0
loopback device: /dev/video42
service:         running (pid 49826)
[PASS] bridge running and both devices found
$ grep -q service: *running /dev/fd/63
$ v4l2-ctl -d /dev/video42 --get-fmt-video
Format Video Capture:
	Width/Height      : 720/480
	Pixel Format      : 'YUYV' (YUYV 4:2:2)
	Field             : None
	Bytes per Line    : 1440
	Size Image        : 691200
	Colorspace        : sRGB
	Transfer Function : Default (maps to sRGB)
	YCbCr/HSV Encoding: Default (maps to ITU-R 601)
	Quantization      : Default (maps to Limited Range)
	Flags             : 
[PASS] loopback offers YUYV of the driver's current size while idle
$ pactl list short sources
47	alsa_output.pci-0000_00_05.0.analog-stereo.monitor	PipeWire	s16le 2ch 48000Hz	SUSPENDED
48	alsa_input.pci-0000_00_05.0.analog-stereo	PipeWire	s16le 2ch 48000Hz	SUSPENDED
706	elgato_gchd.monitor	PipeWire	float32le 2ch 48000Hz	SUSPENDED
713	elgato_gchd_audio	PipeWire	float32le 2ch 48000Hz	SUSPENDED
[PASS] audio source present
$ sh -c sudo dmesg | grep -c 'starting hardware boot'
1
[FAIL] capture device off while idle (expected /^0$/, got: 1)
--- dmesg step 2
[85273.581782] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[85273.581822] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[85274.747306] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[85274.903248] elgato_gchd 1-2:1.0: transcoder defaults loaded
[85274.972023] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[85275.371943] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[85275.374512] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[85275.374517] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[85275.374520] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[85275.374523] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[85276.311240] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[85276.561185] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[85276.584637] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[85276.584988] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[85276.592436] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[85276.592955] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[85276.594178] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[85276.710651] elgato_gchd 1-2:1.0: gchd-diag START transition completed

=== step 3: application reads: capture starts, 720x576 at 50 frames/s
$ cat /home/ff5/gchd-test-logs/T-060-20261118-175106/read.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 165888 kb/s, 30 fps, 30 tbr, 1000k tbn
frame=  500 fps= 33 q=-0.0 Lsize=N/A time=00:00:14.93 bitrate=N/A speed=0.995x    
$ cat /home/ff5/gchd-test-logs/T-060-20261118-175106/read.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 165888 kb/s, 30 fps, 30 tbr, 1000k tbn
frame=  500 fps= 33 q=-0.0 Lsize=N/A time=00:00:14.93 bitrate=N/A speed=0.995x    
[PASS] loopback stream YUYV 720x480 or 720x576
$ cat /home/ff5/gchd-test-logs/T-060-20261118-175106/read.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 165888 kb/s, 30 fps, 30 tbr, 1000k tbn
frame=  500 fps= 33 q=-0.0 Lsize=N/A time=00:00:14.93 bitrate=N/A speed=0.995x    
[PASS] frame count >= 400 in 15 s (capture running)
$ sudo dmesg
[85279.239793] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50110 comm=v4l2-ctl
[85279.279146] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50113 comm=ffmpeg
[85281.338473] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50155 comm=v4l2-ctl
[85281.340414] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50157 comm=v4l2-ctl
[85281.380004] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50160 comm=ffmpeg
[85281.617123] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[85283.418297] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50202 comm=v4l2-ctl
[85283.420517] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50204 comm=v4l2-ctl
[85283.462917] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50207 comm=ffmpeg
[85285.508258] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50249 comm=v4l2-ctl
[85285.510176] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50251 comm=v4l2-ctl
[85285.546997] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50254 comm=ffmpeg
[85286.644234] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[85287.590319] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50296 comm=v4l2-ctl
[85287.592308] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50298 comm=v4l2-ctl
[85287.633689] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50301 comm=ffmpeg
[85289.662613] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50343 comm=v4l2-ctl
[85289.664757] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50345 comm=v4l2-ctl
[85289.703320] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50348 comm=ffmpeg
[85291.669975] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[85291.748419] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50390 comm=v4l2-ctl
[85291.751988] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50392 comm=v4l2-ctl
[85291.795703] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50395 comm=ffmpeg
[FAIL] capture device started (expected /starting hardware boot/, got: [85279.239793] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50110 comm=v4l2-ctl [85279.279146] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open)
--- dmesg step 3
[85281.617123] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[85286.644234] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[85291.669975] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 4: application closed: capture device off
$ sudo dmesg
[85293.551261] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[85293.551576] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[85293.853368] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50476 comm=v4l2-ctl
[85294.416659] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[85294.417519] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[85294.475924] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[85294.477126] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[85294.477381] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
[85294.496813] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[85295.537960] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50519 comm=v4l2-ctl
[85295.544243] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50523 comm=v4l2-ctl
[85296.597720] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50546 comm=v4l2-ctl
[85296.600273] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50548 comm=v4l2-ctl
[85296.645151] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=50551 comm=ffmpeg
[85296.645267] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[85296.660379] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[85296.660756] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
[85296.748805] elgato_gchd 1-2:1.0: transcoder defaults loaded
[85296.749082] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[85296.749340] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x04
[85296.750796] elgato_gchd 1-2:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.751962] elgato_gchd 1-2:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.753227] elgato_gchd 1-2:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.754316] elgato_gchd 1-2:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.755435] elgato_gchd 1-2:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.756544] elgato_gchd 1-2:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.757726] elgato_gchd 1-2:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.758839] elgato_gchd 1-2:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.759961] elgato_gchd 1-2:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.761078] elgato_gchd 1-2:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.762274] elgato_gchd 1-2:1.0: firmware chunk offset=327680 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.763380] elgato_gchd 1-2:1.0: firmware chunk offset=360448 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.764546] elgato_gchd 1-2:1.0: firmware chunk offset=393216 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.765691] elgato_gchd 1-2:1.0: firmware chunk offset=425984 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.766955] elgato_gchd 1-2:1.0: firmware chunk offset=458752 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.768067] elgato_gchd 1-2:1.0: firmware chunk offset=491520 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.769186] elgato_gchd 1-2:1.0: firmware chunk offset=524288 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.770536] elgato_gchd 1-2:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.771813] elgato_gchd 1-2:1.0: firmware chunk offset=589824 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.773058] elgato_gchd 1-2:1.0: firmware chunk offset=622592 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.774307] elgato_gchd 1-2:1.0: firmware chunk offset=655360 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.775557] elgato_gchd 1-2:1.0: firmware chunk offset=688128 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.776691] elgato_gchd 1-2:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.777948] elgato_gchd 1-2:1.0: firmware chunk offset=753664 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.779350] elgato_gchd 1-2:1.0: firmware chunk offset=786432 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.780569] elgato_gchd 1-2:1.0: firmware chunk offset=819200 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.781886] elgato_gchd 1-2:1.0: firmware chunk offset=851968 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.783318] elgato_gchd 1-2:1.0: firmware chunk offset=884736 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.784603] elgato_gchd 1-2:1.0: firmware chunk offset=917504 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.785824] elgato_gchd 1-2:1.0: firmware chunk offset=950272 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.787085] elgato_gchd 1-2:1.0: firmware chunk offset=983040 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.788320] elgato_gchd 1-2:1.0: firmware chunk offset=1015808 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.789713] elgato_gchd 1-2:1.0: firmware chunk offset=1048576 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.790871] elgato_gchd 1-2:1.0: firmware chunk offset=1081344 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.792203] elgato_gchd 1-2:1.0: firmware chunk offset=1114112 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.793567] elgato_gchd 1-2:1.0: firmware chunk offset=1146880 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.794733] elgato_gchd 1-2:1.0: firmware chunk offset=1179648 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.795981] elgato_gchd 1-2:1.0: firmware chunk offset=1212416 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.797191] elgato_gchd 1-2:1.0: firmware chunk offset=1245184 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.798590] elgato_gchd 1-2:1.0: firmware chunk offset=1277952 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.799916] elgato_gchd 1-2:1.0: firmware chunk offset=1310720 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.801173] elgato_gchd 1-2:1.0: firmware chunk offset=1343488 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.802361] elgato_gchd 1-2:1.0: firmware chunk offset=1376256 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.803646] elgato_gchd 1-2:1.0: firmware chunk offset=1409024 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.804845] elgato_gchd 1-2:1.0: firmware chunk offset=1441792 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.806041] elgato_gchd 1-2:1.0: firmware chunk offset=1474560 size=26224 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[85296.810412] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[85296.906909] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[85296.957075] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x0f mode=0xf
[85296.959429] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0x7e 0x98:F5=0x62
[85296.959432] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[85297.014845] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[85297.026511] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[85297.028747] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[85297.028749] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[85297.028752] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[85297.028754] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[PASS] hardware shut down after the last reader
--- dmesg step 4
[85294.496813] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[85296.645267] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[85296.748805] elgato_gchd 1-2:1.0: transcoder defaults loaded
[85296.810412] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[85296.906909] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[85297.026511] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[85297.028747] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[85297.028749] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[85297.028752] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[85297.028754] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[85297.871407] elgato_gchd 1-2:1.0: capture setup: encoder start completed

=== step 5: audio while capturing
$ sink_streams
stream into elgato_gchd: 782	706	781	PipeWire	s16le 2ch 48000Hz
[PASS] the bridge plays into the audio sink
$ ffmpeg -hide_banner -nostdin -f pulse -i elgato_gchd_audio -t 4 -af volumedetect -f null -
[aist#0:0/pcm_s16le @ 0x590fe6b09640] Guessed Channel Layout: stereo
Input #0, pulse, from 'elgato_gchd_audio':
  Duration: N/A, start: 1795020748.757930, bitrate: 1536 kb/s
  Stream #0:0: Audio: pcm_s16le, 48000 Hz, 2 channels, s16, 1536 kb/s
[Parsed_volumedetect_0 @ 0x590fe6b0b000] n_samples: 0
Stream mapping:
  Stream #0:0 -> #0:0 (pcm_s16le (native) -> pcm_s16le (native))
Output #0, null, to 'pipe:':
  Metadata:
    encoder         : Lavf60.16.100
  Stream #0:0: Audio: pcm_s16le, 48000 Hz, stereo, s16, 1536 kb/s
    Metadata:
      encoder         : Lavc60.31.102 pcm_s16le
size=       0kB time=00:00:00.00 bitrate=N/A speed=N/A    
size=N/A time=00:00:00.60 bitrate=N/A speed=1.08x    
size=N/A time=00:00:01.15 bitrate=N/A speed=1.08x    
size=N/A time=00:00:01.65 bitrate=N/A speed=1.05x    
size=N/A time=00:00:02.15 bitrate=N/A speed=1.03x    
size=N/A time=00:00:02.65 bitrate=N/A speed=1.02x    
size=N/A time=00:00:03.20 bitrate=N/A speed=1.03x    
size=N/A time=00:00:03.70 bitrate=N/A speed=1.02x    
[out#0/null @ 0x590fe6b06b00] video:0kB audio:750kB subtitle:0kB other streams:0kB global headers:0kB muxing overhead: unknown
size=N/A time=00:00:03.95 bitrate=N/A speed=0.995x    
[Parsed_volumedetect_0 @ 0x590fe6b0da80] n_samples: 388800
[Parsed_volumedetect_0 @ 0x590fe6b0da80] mean_volume: -85.5 dB
[Parsed_volumedetect_0 @ 0x590fe6b0da80] max_volume: -74.7 dB
[Parsed_volumedetect_0 @ 0x590fe6b0da80] histogram_74db: 1
[Parsed_volumedetect_0 @ 0x590fe6b0da80] histogram_75db: 0
[Parsed_volumedetect_0 @ 0x590fe6b0da80] histogram_76db: 35
[Parsed_volumedetect_0 @ 0x590fe6b0da80] histogram_77db: 0
[Parsed_volumedetect_0 @ 0x590fe6b0da80] histogram_78db: 2855
[PASS] audio source delivers samples
--- dmesg step 5
[85298.116895] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[85298.135492] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[85298.135724] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[85298.141720] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[85298.142264] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[85298.142726] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[85298.281305] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[85303.144632] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[85308.170650] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 6: input change through the bridge while an application reads
$ bash /home/ff5/elgato-gchd/service/gchd-bridge input svideo
$ bash /home/ff5/elgato-gchd/service/gchd-bridge input composite
