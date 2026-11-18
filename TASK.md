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

The test is hanging on audio capture. Note: the PS1 source doesn't have any audio where it is.

gchd-test T-060 20261118-174304 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-060-20261118-174304.log
== build
$ git fetch -q origin
$ git checkout -q feat/app-bridge
$ git pull -q --ff-only
commit: d285582 service: find the loopback device by sysfs name (v4l2loopback devices have no bound driver)
commit: d285582 service: find the loopback device by sysfs name (v4l2loopback devices have no bound driver)
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
[84746.375628] usbcore: deregistering interface driver elgato_gchd
[84752.335314] usb 1-2: USB disconnect, device number 81
[84758.889837] usb 1-2: new high-speed USB device number 82 using xhci_hcd
[84759.158372] usbcore: registered new interface driver elgato_gchd
[84759.274386] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[84759.274395] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[84759.274397] usb 1-2: Product: GameCapture HD
[84759.274399] usb 1-2: Manufacturer: Elgato
[84759.274401] usb 1-2: SerialNumber: 04107F1267
[84759.276643] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[84759.278681] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[84759.690328] v4l2loopback driver version 0.12.7 loaded

=== step 2: bridge idle: black picture, capture device off
PROMPT: PS1 on composite, game running.
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ bash /home/ff5/elgato-gchd/service/gchd-bridge status
capture device:  /dev/video0
loopback device: /dev/video42
service:         running (pid 44155)
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
[FAIL] loopback offers 720x576 YUYV while idle (expected /720/576/, got: Format Video Capture: 	Width/Height      : 720/480 	Pixel Format      : 'YUYV' (YUYV 4:2:2) 	Field             : None 	Bytes per Line    : 1440 	Size Image     )
$ pactl list short sources
47	alsa_output.pci-0000_00_05.0.analog-stereo.monitor	PipeWire	s16le 2ch 48000Hz	SUSPENDED
48	alsa_input.pci-0000_00_05.0.analog-stereo	PipeWire	s16le 2ch 48000Hz	SUSPENDED
706	elgato_gchd.monitor	PipeWire	float32le 2ch 48000Hz	SUSPENDED
713	elgato_gchd_audio	PipeWire	float32le 2ch 48000Hz	SUSPENDED
[PASS] audio source present
$ sh -c sudo dmesg | grep -c 'starting hardware boot'
0
[PASS] capture device off while idle
--- dmesg step 2
[84780.555892] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[84780.555951] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== step 3: application reads: capture starts, 720x576 at 50 frames/s
$ cat /home/ff5/gchd-test-logs/T-060-20261118-174304/read.txt
frame=  572 fps= 38 q=-0.0 size=N/A time=00:00:14.86 bitrate=N/A speed=   1x    
frame=  577 fps= 38 q=-0.0 Lsize=N/A time=00:00:14.96 bitrate=N/A speed=0.994x    
$ cat /home/ff5/gchd-test-logs/T-060-20261118-174304/read.txt
frame=  572 fps= 38 q=-0.0 size=N/A time=00:00:14.86 bitrate=N/A speed=   1x    
frame=  577 fps= 38 q=-0.0 Lsize=N/A time=00:00:14.96 bitrate=N/A speed=0.994x    
[FAIL] loopback stream 720x576 YUYV (expected /yuyv422.*720x576/, got: frame=  572 fps= 38 q=-0.0 size=N/A time=00:00:14.86 bitrate=N/A speed=   1x     frame=  577 fps= 38 q=-0.0 Lsize=N/A time=00:00:14.96 bitrate=N/A speed=0.994x )
$ cat /home/ff5/gchd-test-logs/T-060-20261118-174304/read.txt
frame=  572 fps= 38 q=-0.0 size=N/A time=00:00:14.86 bitrate=N/A speed=   1x    
frame=  577 fps= 38 q=-0.0 Lsize=N/A time=00:00:14.96 bitrate=N/A speed=0.994x    
[PASS] frame count >= 400 in 15 s (capture running)
$ sudo dmesg
[84785.814029] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=44355 comm=v4l2-ctl
[84785.850968] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=44358 comm=ffmpeg
[84785.851017] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[84785.856588] elgato_gchd 1-2:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84785.857949] elgato_gchd 1-2:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84785.859454] elgato_gchd 1-2:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84785.860929] elgato_gchd 1-2:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84785.862326] elgato_gchd 1-2:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84785.863637] elgato_gchd 1-2:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84785.864894] elgato_gchd 1-2:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84785.866247] elgato_gchd 1-2:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84785.867503] elgato_gchd 1-2:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84785.868793] elgato_gchd 1-2:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84785.870215] elgato_gchd 1-2:1.0: firmware chunk offset=327680 size=4016 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84785.922590] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[84785.923695] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
[84786.040730] elgato_gchd 1-2:1.0: transcoder defaults loaded
[84786.040852] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[84786.041120] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x04
[84786.042569] elgato_gchd 1-2:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.043705] elgato_gchd 1-2:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.044887] elgato_gchd 1-2:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.046028] elgato_gchd 1-2:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.047340] elgato_gchd 1-2:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.048607] elgato_gchd 1-2:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.049920] elgato_gchd 1-2:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.051221] elgato_gchd 1-2:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.052518] elgato_gchd 1-2:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.053784] elgato_gchd 1-2:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.055046] elgato_gchd 1-2:1.0: firmware chunk offset=327680 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.056308] elgato_gchd 1-2:1.0: firmware chunk offset=360448 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.057605] elgato_gchd 1-2:1.0: firmware chunk offset=393216 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.058858] elgato_gchd 1-2:1.0: firmware chunk offset=425984 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.060047] elgato_gchd 1-2:1.0: firmware chunk offset=458752 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.061324] elgato_gchd 1-2:1.0: firmware chunk offset=491520 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.062534] elgato_gchd 1-2:1.0: firmware chunk offset=524288 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.063728] elgato_gchd 1-2:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.064875] elgato_gchd 1-2:1.0: firmware chunk offset=589824 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.066048] elgato_gchd 1-2:1.0: firmware chunk offset=622592 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.067302] elgato_gchd 1-2:1.0: firmware chunk offset=655360 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.068542] elgato_gchd 1-2:1.0: firmware chunk offset=688128 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.069690] elgato_gchd 1-2:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.070843] elgato_gchd 1-2:1.0: firmware chunk offset=753664 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.072029] elgato_gchd 1-2:1.0: firmware chunk offset=786432 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.073324] elgato_gchd 1-2:1.0: firmware chunk offset=819200 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.074498] elgato_gchd 1-2:1.0: firmware chunk offset=851968 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.075771] elgato_gchd 1-2:1.0: firmware chunk offset=884736 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.076990] elgato_gchd 1-2:1.0: firmware chunk offset=917504 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.078234] elgato_gchd 1-2:1.0: firmware chunk offset=950272 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.079597] elgato_gchd 1-2:1.0: firmware chunk offset=983040 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.080816] elgato_gchd 1-2:1.0: firmware chunk offset=1015808 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.082130] elgato_gchd 1-2:1.0: firmware chunk offset=1048576 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.083442] elgato_gchd 1-2:1.0: firmware chunk offset=1081344 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.084706] elgato_gchd 1-2:1.0: firmware chunk offset=1114112 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.085895] elgato_gchd 1-2:1.0: firmware chunk offset=1146880 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.087059] elgato_gchd 1-2:1.0: firmware chunk offset=1179648 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.088250] elgato_gchd 1-2:1.0: firmware chunk offset=1212416 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.089570] elgato_gchd 1-2:1.0: firmware chunk offset=1245184 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.090731] elgato_gchd 1-2:1.0: firmware chunk offset=1277952 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.091959] elgato_gchd 1-2:1.0: firmware chunk offset=1310720 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.093160] elgato_gchd 1-2:1.0: firmware chunk offset=1343488 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.094461] elgato_gchd 1-2:1.0: firmware chunk offset=1376256 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.095685] elgato_gchd 1-2:1.0: firmware chunk offset=1409024 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.096919] elgato_gchd 1-2:1.0: firmware chunk offset=1441792 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.098148] elgato_gchd 1-2:1.0: firmware chunk offset=1474560 size=26224 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[84786.102244] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[84786.518465] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[84786.527791] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[84786.529933] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[84786.529934] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[84786.529936] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[84786.529938] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[84787.307699] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[84787.307811] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[84787.571416] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[84787.587173] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[84787.587294] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[84787.587402] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[84787.593178] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[84787.593891] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[84787.594395] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[84787.594526] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[84787.594765] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[84787.595169] elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0000 ack=1
[84787.595919] elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[84787.596221] elgato_gchd 1-2:1.0: START poll=1 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[84787.596548] elgato_gchd 1-2:1.0: START poll=2 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[84787.596849] elgato_gchd 1-2:1.0: START poll=3 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[84787.597164] elgato_gchd 1-2:1.0: START poll=4 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[84787.597482] elgato_gchd 1-2:1.0: START poll=5 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[84787.597802] elgato_gchd 1-2:1.0: START poll=6 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[84787.598131] elgato_gchd 1-2:1.0: START poll=7 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[84787.598464] elgato_gchd 1-2:1.0: START poll=8 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[84787.598801] elgato_gchd 1-2:1.0: START poll=9 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[84787.599149] elgato_gchd 1-2:1.0: START poll=10 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[84787.599478] elgato_gchd 1-2:1.0: START poll=11 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[84787.599799] elgato_gchd 1-2:1.0: START poll=12 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[84787.600152] elgato_gchd 1-2:1.0: START poll=13 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[84787.600502] elgato_gchd 1-2:1.0: START poll=14 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[84787.600814] elgato_gchd 1-2:1.0: START poll=15 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[84787.626303] elgato_gchd 1-2:1.0: START poll=100 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[84787.657644] elgato_gchd 1-2:1.0: START poll=200 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[84787.693161] elgato_gchd 1-2:1.0: START poll=300 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[84787.727156] elgato_gchd 1-2:1.0: START poll=400 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[84787.732622] elgato_gchd 1-2:1.0: START poll=419 state=0x0002 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0010 ack=1
[84787.733025] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[84788.133142] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[84788.178058] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[84788.222407] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[84788.269217] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[84788.312362] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[84792.597127] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[84797.624321] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[PASS] capture device started
--- dmesg step 3
[84785.851017] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[84786.040730] elgato_gchd 1-2:1.0: transcoder defaults loaded
[84786.102244] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[84786.527791] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[84786.529933] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[84786.529934] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[84786.529936] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[84786.529938] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[84787.307699] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[84787.571416] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[84787.587173] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[84787.587402] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[84787.593178] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[84787.593891] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[84787.594395] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[84787.733025] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[84792.597127] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[84797.624321] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 4: application closed: capture device off
$ sudo dmesg
[84800.481841] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[84800.482086] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[84801.202166] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[84801.203095] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[84801.266573] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[84801.267699] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[84801.268065] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
[84801.287866] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[84802.338743] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=44590 comm=v4l2-ctl
[84804.413782] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=44633 comm=v4l2-ctl
[PASS] hardware shut down after the last reader
--- dmesg step 4
[84801.287866] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 5: audio while capturing
$ ffmpeg -hide_banner -nostdin -f pulse -i elgato_gchd_audio -t 4 -af volumedetect -f null -
[aist#0:0/pcm_s16le @ 0x5a1e1be47640] Guessed Channel Layout: stereo
Input #0, pulse, from 'elgato_gchd_audio':
  Duration: N/A, start: 1795020255.852201, bitrate: 1536 kb/s
  Stream #0:0: Audio: pcm_s16le, 48000 Hz, 2 channels, s16, 1536 kb/s
[Parsed_volumedetect_0 @ 0x5a1e1be49000] n_samples: 0
Stream mapping:
  Stream #0:0 -> #0:0 (pcm_s16le (native) -> pcm_s16le (native))
Output #0, null, to 'pipe:':
  Metadata:
    encoder         : Lavf60.16.100
  Stream #0:0: Audio: pcm_s16le, 48000 Hz, stereo, s16, 1536 kb/s
    Metadata:
      encoder         : Lavc60.31.102 pcm_s16le
size=       0kB time=00:00:00.00 bitrate=N/A speed=N/A    
size=N/A time=00:00:00.58 bitrate=N/A speed=1.02x    
size=N/A time=00:00:01.14 bitrate=N/A speed=0.988x    
size=N/A time=00:00:01.67 bitrate=N/A speed=0.983x    
size=N/A time=00:00:02.21 bitrate=N/A speed=0.995x    
size=N/A time=00:00:02.72 bitrate=N/A speed=0.997x    
size=N/A time=00:00:03.22 bitrate=N/A speed=0.997x    
size=N/A time=00:00:03.73 bitrate=N/A speed=0.995x    
[out#0/null @ 0x5a1e1be44b00] video:0kB audio:762kB subtitle:0kB other streams:0kB global headers:0kB muxing overhead: unknown
size=N/A time=00:00:03.98 bitrate=N/A speed=0.972x    
[Parsed_volumedetect_0 @ 0x5a1e1be4ba80] n_samples: 398400
[Parsed_volumedetect_0 @ 0x5a1e1be4ba80] mean_volume: -85.5 dB
[Parsed_volumedetect_0 @ 0x5a1e1be4ba80] max_volume: -76.3 dB
[Parsed_volumedetect_0 @ 0x5a1e1be4ba80] histogram_76db: 98
[Parsed_volumedetect_0 @ 0x5a1e1be4ba80] histogram_77db: 0
[Parsed_volumedetect_0 @ 0x5a1e1be4ba80] histogram_78db: 3716
[FAIL] audio source carries sound (mean volume above -60 dB) (expected /mean_volume: -([0-9]|[1-5][0-9])\./, got: [aist#0:0/pcm_s16le @ 0x5a1e1be47640] Guessed Channel Layout: stereo Input #0, pulse, from 'elgato_gchd_audio':   Duration: N/A, start: 1795020255.852201, bitra)
