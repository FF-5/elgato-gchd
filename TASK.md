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

- Third T-060 run (6a1895d), hung at the input change: steps 1, 4, 5
  PASS (audio: a stream from the bridge into the sink, samples delivered).
  The capture device started 1.2 s after the bridge started, with no test
  reader, and again 2 s after the shutdown in step 4: something opens the
  loopback device whenever the bridge starts or stops writing (most likely
  PipeWire/WirePlumber listing the device, which changes between output
  and capture with exclusive_caps), and the bridge took that for an
  application. Changes: the bridge ignores pipewire/wireplumber
  (`GCHD_IGNORE`) and logs the readers it acts on; ffmpeg is killed when
  it does not stop on TERM within 3 s (an ffmpeg on an input without
  signal); the test's readers have a time limit. Reader frame rate 33/s
  (500 frames in 15 s, capture already running): the VM may not keep up
  with decoding + deinterlacing at 50 fields/s; to be checked.
  With the driver's own node the start can follow a real STREAMON instead
  of an open.
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

These test are not very helpful, too many fails and problems. Why should we keep wasting time if we are not going to use the v4l2loopback device?

gchd-test T-060 20261118-180156 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-060-20261118-180156.log
== build
$ git fetch -q origin
$ git checkout -q feat/app-bridge
$ git pull -q --ff-only
commit: b1c13da service: ignore PipeWire/WirePlumber device probing, log readers, kill a stuck ffmpeg after 3 s
commit: b1c13da service: ignore PipeWire/WirePlumber device probing, log readers, kill a stuck ffmpeg after 3 s
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
[85883.274611] usbcore: deregistering interface driver elgato_gchd
[85900.094238] usb 1-2: new high-speed USB device number 85 using xhci_hcd
[85900.468499] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[85900.468505] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[85900.468507] usb 1-2: Product: GameCapture HD
[85900.468508] usb 1-2: Manufacturer: Elgato
[85900.468509] usb 1-2: SerialNumber: 04107F1267
[85901.164592] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[85901.167654] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[85901.167696] usbcore: registered new interface driver elgato_gchd

=== step 2: bridge idle: black picture, capture device off
PROMPT: PS1 on composite, game running.
$ v4l2-ctl -d /dev/video0 -i 2
VIDIOC_S_INPUT: failed: Device or resource busy
$ bash /home/ff5/elgato-gchd/service/gchd-bridge status
capture device:  /dev/video0
loopback device: /dev/video42
service:         running (pid 65513)
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
706	elgato_gchd.monitor	PipeWire	float32le 2ch 48000Hz	SUSPENDED
713	elgato_gchd_audio	PipeWire	float32le 2ch 48000Hz	SUSPENDED
1009	alsa_output.pci-0000_00_05.0.analog-stereo.monitor	PipeWire	s16le 2ch 48000Hz	SUSPENDED
1010	alsa_input.pci-0000_00_05.0.analog-stereo	PipeWire	s16le 2ch 48000Hz	SUSPENDED
[PASS] audio source present
$ sh -c sudo dmesg | grep -c 'starting hardware boot'
1
[FAIL] capture device off while idle (expected /^0$/, got: 1)
$ cat /home/ff5/gchd-test-logs/T-060-20261118-180156/bridge.log
gchd-bridge: readers: 65157:ffmpeg
gchd-bridge: cannot set the MPEG format on /dev/video0
gchd-bridge: capture: /dev/video0 -> /dev/video42 (1280x720)
Stream map '0:v:0' matches no streams.
To ignore this, add a trailing '?' to the map.
Failed to set value '0:v:0' for option 'map': Invalid argument
Error parsing options for output file /dev/video42.
Error opening output files: Invalid argument
gchd-bridge: capture pipeline ended
gchd-bridge: cannot set the MPEG format on /dev/video0
gchd-bridge: capture: /dev/video0 -> /dev/video42 (1280x720)
Stream map '0:v:0' matches no streams.
To ignore this, add a trailing '?' to the map.
Failed to set value '0:v:0' for option 'map': Invalid argument
Error parsing options for output file /dev/video42.
Error opening output files: Invalid argument
--- dmesg step 2
[85901.455891] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[85901.633075] elgato_gchd 1-2:1.0: transcoder defaults loaded
[85901.694421] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[85901.712577] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85901.988945] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85902.263463] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85902.537423] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85902.818159] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85903.096242] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85903.370244] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85903.645813] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85903.917059] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85904.187464] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85904.461223] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85904.733856] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85905.005986] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85905.285309] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85905.556093] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85905.833828] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85906.113801] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85906.399565] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85906.670744] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85906.943868] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85907.214176] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85907.486044] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85907.767524] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85908.038060] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85908.311465] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85908.579618] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85908.855101] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85909.124434] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85909.394483] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85909.667010] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85909.938427] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85910.213930] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85910.486023] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85910.756846] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85911.028354] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85911.301651] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85911.572038] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85911.842199] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85912.119223] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85912.388374] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85912.673028] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85912.939218] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85913.212313] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85913.484476] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85913.759238] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85914.028430] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85914.300004] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85914.571447] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85914.844687] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85915.117336] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85915.390675] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85915.662437] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85915.934444] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85916.203713] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85916.478836] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85916.750138] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85917.021660] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85917.294896] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85917.566371] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85917.833477] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85918.108771] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85918.383013] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85918.653784] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85918.927928] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85919.200788] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85919.471111] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85919.742901] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85920.016075] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85920.285197] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85920.557813] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85920.827739] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85921.105372] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85921.379643] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85921.653696] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85921.924730] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85922.198713] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0

=== step 3: application reads: capture starts, 720x576 at 50 frames/s
$ cat /home/ff5/gchd-test-logs/T-060-20261118-180156/read.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 165888 kb/s, 30 fps, 30 tbr, 1000k tbn
frame=    1 fps=0.7 q=-0.0 size=N/A time=00:00:00.00 bitrate=N/A speed=   0x    
$ cat /home/ff5/gchd-test-logs/T-060-20261118-180156/read.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 165888 kb/s, 30 fps, 30 tbr, 1000k tbn
frame=    1 fps=0.7 q=-0.0 size=N/A time=00:00:00.00 bitrate=N/A speed=   0x    
[PASS] loopback stream YUYV 720x480 or 720x576
$ cat /home/ff5/gchd-test-logs/T-060-20261118-180156/read.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 165888 kb/s, 30 fps, 30 tbr, 1000k tbn
frame=    1 fps=0.7 q=-0.0 size=N/A time=00:00:00.00 bitrate=N/A speed=   0x    
[FAIL] frame count >= 400 in 15 s (capture running) (expected /frame= *([4-9][0-9]{2}|[0-9]{4})/, got:   Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 165888 kb/s, 30 fps, 30 tbr, 1000k tbn frame=    1 fps=0.7 q=-0.0 size=N/A time=00:00:00.0)
$ sudo dmesg
[85922.470315] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85922.470322] gchd_detection_worker: 9 callbacks suppressed
[85922.470323] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85922.740826] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85922.740835] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85923.017497] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85923.017504] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85923.291997] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85923.292006] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85923.564027] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85923.564034] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85923.573950] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=65824 comm=v4l2-ctl
[85923.576223] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=65826 comm=v4l2-ctl
[85923.627398] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=65829 comm=ffmpeg
[85923.836059] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85923.836067] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85924.109605] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85924.109612] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85924.317386] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=65849 comm=v4l2-ctl
[85924.358797] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=65852 comm=ffmpeg
[85924.376685] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85924.376690] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85924.645191] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85924.645199] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85924.941205] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85924.941217] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85925.235674] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85925.537710] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85925.685899] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=65899 comm=v4l2-ctl
[85925.687953] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=65901 comm=v4l2-ctl
[85925.724588] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=65904 comm=ffmpeg
[85925.852827] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85926.131677] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85926.413830] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85926.429706] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=65926 comm=v4l2-ctl
[85926.431765] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=65928 comm=v4l2-ctl
[85926.471358] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=65931 comm=ffmpeg
[85926.708507] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85926.997109] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85927.314832] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85927.602478] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85927.602484] gchd_detection_worker: 8 callbacks suppressed
[85927.602485] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85927.801848] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=65977 comm=v4l2-ctl
[85927.803802] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=65979 comm=v4l2-ctl
[85927.841488] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=65982 comm=ffmpeg
[85927.872736] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85927.872742] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85928.181928] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85928.181945] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85928.466184] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85928.466192] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85928.536085] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66004 comm=v4l2-ctl
[85928.537867] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66006 comm=v4l2-ctl
[85928.577160] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66009 comm=ffmpeg
[85928.792383] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85928.792398] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85929.123026] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85929.123035] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85929.429702] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85929.429713] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85929.718644] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85929.718654] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85929.896178] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66055 comm=v4l2-ctl
[85929.898853] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66057 comm=v4l2-ctl
[85929.937332] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66060 comm=ffmpeg
[85930.007569] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85930.007575] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85930.291325] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85930.291335] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85930.598018] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85930.622852] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66082 comm=v4l2-ctl
[85930.625314] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66084 comm=v4l2-ctl
[85930.663593] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66087 comm=ffmpeg
[85930.871031] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85931.165218] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85931.465015] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85931.736082] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85931.999331] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66133 comm=v4l2-ctl
[85932.002429] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66135 comm=v4l2-ctl
[85932.016807] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85932.043016] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66138 comm=ffmpeg
[85932.313707] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85932.640653] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85932.640668] gchd_detection_worker: 7 callbacks suppressed
[85932.640671] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85932.724018] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66160 comm=v4l2-ctl
[85932.726378] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66162 comm=v4l2-ctl
[85932.765386] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66165 comm=ffmpeg
[85932.946010] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85932.946029] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85933.249842] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85933.249854] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85933.561188] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85933.561202] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85933.871630] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85933.871639] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85934.136543] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66211 comm=v4l2-ctl
[85934.138713] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66213 comm=v4l2-ctl
[85934.147042] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85934.147049] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85934.182990] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66216 comm=ffmpeg
[85934.458959] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85934.458967] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85934.766613] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85934.766626] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85934.816779] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66238 comm=v4l2-ctl
[85934.818950] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66240 comm=v4l2-ctl
[85934.857045] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66243 comm=ffmpeg
[85935.072259] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85935.072271] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85935.376474] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85935.376483] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85935.714438] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85935.989837] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85936.263940] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85936.274033] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66289 comm=v4l2-ctl
[85936.276225] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66291 comm=v4l2-ctl
[85936.318396] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66294 comm=ffmpeg
[85936.543547] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85936.823850] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85936.913488] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66316 comm=v4l2-ctl
[85936.915285] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66318 comm=v4l2-ctl
[85936.951172] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66321 comm=ffmpeg
[85937.140835] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85937.436042] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85937.751394] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85937.751408] gchd_detection_worker: 7 callbacks suppressed
[85937.751411] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85938.020440] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85938.020456] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85938.329923] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85938.329929] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85938.378507] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66367 comm=v4l2-ctl
[85938.380995] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66369 comm=v4l2-ctl
[85938.425843] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66372 comm=ffmpeg
[85938.625907] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85938.625917] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85938.928498] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85938.928513] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85939.030712] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66394 comm=v4l2-ctl
[85939.032876] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66396 comm=v4l2-ctl
[85939.071937] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66399 comm=ffmpeg
[85939.219428] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85939.219441] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85939.505667] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85939.505676] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85939.851861] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85939.851880] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85940.157604] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85940.157612] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85940.472514] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85940.472521] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85940.495457] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66445 comm=v4l2-ctl
[85940.497585] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66447 comm=v4l2-ctl
[85940.534658] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66450 comm=ffmpeg
[85940.805846] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85941.090852] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85941.113064] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66472 comm=v4l2-ctl
[85941.115125] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66474 comm=v4l2-ctl
[85941.150909] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66477 comm=ffmpeg
[85941.414058] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85941.698729] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85942.000890] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85942.290474] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85942.585105] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85942.604909] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66523 comm=v4l2-ctl
[85942.606682] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66525 comm=v4l2-ctl
[85942.646220] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66528 comm=ffmpeg
[85942.869456] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85942.869466] gchd_detection_worker: 7 callbacks suppressed
[85942.869468] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85943.176904] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85943.176911] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85943.194634] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66549 comm=v4l2-ctl
[85943.197334] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66552 comm=v4l2-ctl
[85943.236191] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66555 comm=ffmpeg
[85943.475722] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85943.475730] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85943.783699] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85943.783709] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85944.064162] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85944.064168] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85944.344235] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85944.344247] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85944.632617] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85944.632629] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85944.752117] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66601 comm=v4l2-ctl
[85944.754026] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66603 comm=v4l2-ctl
[85944.791282] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66606 comm=ffmpeg
[85944.918592] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85944.918599] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85945.260926] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85945.260938] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85945.281240] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66628 comm=v4l2-ctl
[85945.283091] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66630 comm=v4l2-ctl
[85945.319550] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66633 comm=ffmpeg
[85945.552911] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85945.552922] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85945.819170] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85946.108105] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85946.427972] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85946.753112] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85946.865192] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66679 comm=v4l2-ctl
[85946.867566] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66681 comm=v4l2-ctl
[85946.905380] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66684 comm=ffmpeg
[85947.079581] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85947.359170] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85947.371499] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66705 comm=v4l2-ctl
[85947.374099] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66707 comm=v4l2-ctl
[85947.414811] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66711 comm=ffmpeg
[85947.631299] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85947.907743] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85947.907749] gchd_detection_worker: 7 callbacks suppressed
[85947.907750] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85948.229285] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85948.229296] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85948.504381] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85948.504388] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85948.809674] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85948.809692] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85948.994519] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66757 comm=v4l2-ctl
[85948.996474] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66759 comm=v4l2-ctl
[85949.034667] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66762 comm=ffmpeg
[85949.135722] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85949.135739] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85949.452150] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85949.452156] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85949.471397] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66784 comm=v4l2-ctl
[85949.474115] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66786 comm=v4l2-ctl
[85949.514365] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66789 comm=ffmpeg
[85949.746639] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85949.746651] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85950.034570] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85950.034576] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85950.346666] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85950.346682] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85950.661100] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85950.661123] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85950.972493] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85951.110356] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66837 comm=v4l2-ctl
[85951.112092] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66839 comm=v4l2-ctl
[85951.150078] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66842 comm=ffmpeg
[85951.269696] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85951.561622] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85951.577739] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66864 comm=v4l2-ctl
[85951.579669] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66866 comm=v4l2-ctl
[85951.617192] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66869 comm=ffmpeg
[85951.853094] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85952.147884] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85952.446270] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85952.763114] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85953.197769] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85953.197776] gchd_detection_worker: 7 callbacks suppressed
[85953.197777] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85953.215953] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66915 comm=v4l2-ctl
[85953.218023] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66917 comm=v4l2-ctl
[85953.254072] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66920 comm=ffmpeg
[85953.522577] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85953.522602] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85953.674527] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66942 comm=v4l2-ctl
[85953.676450] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66944 comm=v4l2-ctl
[85953.716607] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66947 comm=ffmpeg
[85953.823330] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85953.823344] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85954.129241] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85954.129255] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85954.418426] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85954.418433] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85954.707742] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85954.707748] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85955.009481] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85955.009495] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85955.308087] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85955.308094] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85955.328095] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66993 comm=v4l2-ctl
[85955.330514] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66995 comm=v4l2-ctl
[85955.367557] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=66998 comm=ffmpeg
[85955.603961] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85955.603973] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85955.769121] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67020 comm=v4l2-ctl
[85955.770993] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67022 comm=v4l2-ctl
[85955.809659] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67025 comm=ffmpeg
[85955.917018] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85955.917037] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85956.201775] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85956.506858] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85956.810050] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85957.237125] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85957.447413] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67071 comm=v4l2-ctl
[85957.449248] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67073 comm=v4l2-ctl
[85957.487434] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67076 comm=ffmpeg
[85957.513874] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85957.822911] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85957.857806] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67098 comm=v4l2-ctl
[85957.859968] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67100 comm=v4l2-ctl
[85957.897082] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67103 comm=ffmpeg
[85958.109636] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85958.406825] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85958.406838] gchd_detection_worker: 7 callbacks suppressed
[85958.406840] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85958.715057] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85958.715076] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85959.018503] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85959.018524] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85959.325129] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85959.325148] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85959.579943] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67149 comm=v4l2-ctl
[85959.582280] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67151 comm=v4l2-ctl
[85959.593212] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85959.593221] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85959.630638] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67154 comm=ffmpeg
[85959.911712] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85959.911720] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85959.939297] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67175 comm=v4l2-ctl
[85959.941800] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67178 comm=v4l2-ctl
[85959.981768] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67181 comm=ffmpeg
[85960.252361] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85960.252371] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85960.563707] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85960.563721] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85960.840990] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85960.840998] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85961.162172] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85961.162196] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85961.474852] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85961.713095] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67227 comm=v4l2-ctl
[85961.715117] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67229 comm=v4l2-ctl
[85961.746193] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85961.755808] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67232 comm=ffmpeg
[85962.027459] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85962.043868] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67254 comm=v4l2-ctl
[85962.046587] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67256 comm=v4l2-ctl
[85962.085656] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67259 comm=ffmpeg
[85962.328324] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[FAIL] capture device started (expected /starting hardware boot/, got: [85922.470315] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0 [85922.470322] gchd_detection_worker:)
--- dmesg step 3
[85922.470315] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85922.740826] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85923.017497] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85923.291997] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85923.564027] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85923.836059] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85924.109605] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85924.376685] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85924.645191] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85924.941205] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85925.235674] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85925.537710] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85925.852827] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85926.131677] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85926.413830] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85926.708507] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85926.997109] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85927.314832] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85927.602478] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85927.872736] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85928.181928] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85928.466184] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85928.792383] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85929.123026] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85929.429702] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85929.718644] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85930.007569] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85930.291325] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85930.598018] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85930.871031] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85931.165218] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85931.465015] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85931.736082] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85932.016807] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85932.313707] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85932.640653] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85932.946010] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85933.249842] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85933.561188] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85933.871630] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85934.147042] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85934.458959] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85934.766613] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85935.072259] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85935.376474] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85935.714438] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85935.989837] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85936.263940] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85936.543547] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85936.823850] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85937.140835] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85937.436042] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85937.751394] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85938.020440] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85938.329923] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85938.625907] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85938.928498] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85939.219428] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85939.505667] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85939.851861] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85940.157604] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85940.472514] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85940.805846] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85941.090852] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85941.414058] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85941.698729] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85942.000890] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85942.290474] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85942.585105] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85942.869456] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85943.176904] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85943.475722] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85943.783699] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85944.064162] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85944.344235] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85944.632617] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85944.918592] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85945.260926] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85945.552911] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85945.819170] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85946.108105] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85946.427972] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85946.753112] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85947.079581] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85947.359170] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85947.631299] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85947.907743] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85948.229285] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85948.504381] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85948.809674] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85949.135722] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85949.452150] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85949.746639] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85950.034570] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85950.346666] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85950.661100] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85950.972493] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85951.269696] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85951.561622] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85951.853094] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85952.147884] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85952.446270] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85952.763114] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85953.197769] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85953.522577] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85953.823330] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85954.129241] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85954.418426] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85954.707742] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85955.009481] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85955.308087] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85955.603961] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85955.917018] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85956.201775] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85956.506858] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85956.810050] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85957.237125] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85957.513874] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85957.822911] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85958.109636] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85958.406825] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85958.715057] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85959.018503] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85959.325129] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85959.593212] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85959.911712] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85960.252361] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85960.563707] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85960.840990] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85961.162172] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85961.474852] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85961.746193] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85962.027459] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85962.328324] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0

=== step 4: application closed: capture device off
$ sudo dmesg
[85962.609331] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85962.936533] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85963.239410] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85963.539253] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85963.539265] gchd_detection_worker: 7 callbacks suppressed
[85963.539268] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85963.820562] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85963.820569] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85963.836990] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67343 comm=v4l2-ctl
[85964.094380] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85964.094386] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85964.143894] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67376 comm=v4l2-ctl
[85964.369042] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85964.369050] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85964.640026] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85964.640035] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85964.908023] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85964.908031] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85965.179401] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85965.179411] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85965.185073] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67399 comm=v4l2-ctl
[85965.187982] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67401 comm=v4l2-ctl
[85965.240353] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67404 comm=ffmpeg
[85965.453380] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85965.453387] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85965.726808] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85965.726815] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85965.998232] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85965.998242] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[85966.272135] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85966.545889] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85966.826531] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85967.107039] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85967.273501] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67446 comm=v4l2-ctl
[85967.275929] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67448 comm=v4l2-ctl
[85967.326864] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=67451 comm=ffmpeg
[85967.381246] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[FAIL] hardware shut down after the last reader (expected /hardware shutdown done/, got: [85962.609331] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0 [85962.936533] elgato_gchd 1-2:1.0: g)
--- dmesg step 4
[85962.609331] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85962.936533] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85963.239410] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85963.539253] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85963.820562] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85964.094380] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85964.369042] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85964.640026] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85964.908023] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85965.179401] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85965.453380] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85965.726808] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85965.998232] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85966.272135] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85966.545889] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85966.826531] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85967.107039] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85967.381246] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0

=== step 5: audio while capturing
$ sink_streams

[FAIL] the bridge plays into the audio sink (expected /stream into elgato_gchd/, got: )
$ ffmpeg -hide_banner -nostdin -f pulse -i elgato_gchd_audio -t 4 -af volumedetect -f null -
[aist#0:0/pcm_s16le @ 0x579db2023640] Guessed Channel Layout: stereo
Input #0, pulse, from 'elgato_gchd_audio':
  Duration: N/A, start: 1795021418.505277, bitrate: 1536 kb/s
  Stream #0:0: Audio: pcm_s16le, 48000 Hz, 2 channels, s16, 1536 kb/s
[Parsed_volumedetect_0 @ 0x579db2025080] n_samples: 0
Stream mapping:
  Stream #0:0 -> #0:0 (pcm_s16le (native) -> pcm_s16le (native))
Output #0, null, to 'pipe:':
  Metadata:
    encoder         : Lavf60.16.100
  Stream #0:0: Audio: pcm_s16le, 48000 Hz, stereo, s16, 1536 kb/s
    Metadata:
      encoder         : Lavc60.31.102 pcm_s16le
size=       0kB time=00:00:00.00 bitrate=N/A speed=N/A    
size=N/A time=00:00:00.47 bitrate=N/A speed=0.941x    
size=N/A time=00:00:00.91 bitrate=N/A speed=0.902x    
size=N/A time=00:00:01.55 bitrate=N/A speed=1.02x    
size=N/A time=00:00:02.13 bitrate=N/A speed=1.04x    
size=N/A time=00:00:02.67 bitrate=N/A speed=1.05x    
size=N/A time=00:00:03.19 bitrate=N/A speed=1.04x    
size=N/A time=00:00:03.65 bitrate=N/A speed=1.02x    
size=N/A time=00:00:03.97 bitrate=N/A speed=0.972x    
[out#0/null @ 0x579db2020b00] video:0kB audio:698kB subtitle:0kB other streams:0kB global headers:0kB muxing overhead: unknown
size=N/A time=00:00:03.97 bitrate=N/A speed=0.972x    
[Parsed_volumedetect_0 @ 0x579db2027b00] n_samples: 364800
[Parsed_volumedetect_0 @ 0x579db2027b00] mean_volume: -91.0 dB
[Parsed_volumedetect_0 @ 0x579db2027b00] max_volume: -91.0 dB
[Parsed_volumedetect_0 @ 0x579db2027b00] histogram_91db: 364800
[PASS] audio source delivers samples
--- dmesg step 5
[85967.653049] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85967.932793] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85968.208089] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85968.478195] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85968.748923] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85969.025092] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85969.337521] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85969.611114] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85969.921943] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85970.245556] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85970.565270] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85970.858941] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85971.157413] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85971.465856] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85971.739719] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85972.018603] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85972.316559] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85972.589015] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85972.888357] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85973.174561] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85973.468779] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85973.763477] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85974.054561] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85974.390616] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85974.661669] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85974.952954] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85975.260090] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85975.568395] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85975.847502] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85976.142318] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85976.452237] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85976.731128] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85977.004615] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85977.295996] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85977.591473] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85977.859530] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85978.132644] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85978.467896] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85978.824469] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85979.099398] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85979.392888] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85979.677543] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85979.947710] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85980.217046] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85980.504749] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85980.800305] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85981.272100] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85981.624816] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85981.912827] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85982.220122] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85982.510135] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85982.829309] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85983.116448] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85983.405125] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85983.687900] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85983.981994] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85984.251086] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85984.537601] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85984.837154] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85985.134620] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85985.440910] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85985.750755] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85986.050920] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85986.324189] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85986.597294] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85986.893652] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85987.207524] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85987.496466] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85987.784711] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85988.117134] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85988.394411] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85988.677763] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85988.979019] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85989.276650] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85989.587718] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85989.896185] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85990.188767] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85990.491917] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85990.791979] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85991.075584] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85991.385300] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85991.699079] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85991.998921] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85992.288398] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85992.558724] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85992.854797] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85993.125565] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85993.416677] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85993.682223] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85993.975072] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85994.268974] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85994.576947] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85994.849584] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85995.124591] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85995.415001] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85995.706026] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85996.010663] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85996.283541] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85996.584262] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85996.866418] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85997.182528] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85997.465959] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85997.768492] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85998.058501] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85998.349564] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85998.674522] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85998.960610] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85999.266230] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85999.571429] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[85999.875978] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86000.193145] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86000.469610] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86000.765411] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86001.065012] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86001.423832] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86001.782272] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86002.256655] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86002.522079] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86002.841412] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86003.149933] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86003.438367] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86003.742514] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86004.031494] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86004.319525] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86004.605059] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86004.918285] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86005.189320] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86005.461084] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86005.731334] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86006.016425] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86006.305473] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86006.599215] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0

=== step 6: input change through the bridge while an application reads
$ bash /home/ff5/elgato-gchd/service/gchd-bridge input svideo
$ bash /home/ff5/elgato-gchd/service/gchd-bridge input composite
$ cat /home/ff5/gchd-test-logs/T-060-20261118-180156/input-change.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 165888 kb/s, 30 fps, 30 tbr, 1000k tbn
frame=    0 fps=0.0 q=-0.0 size=       0kB time=00:00:00.00 bitrate=N/A speed=N/A    
$ sh -c tail -12 '/home/ff5/gchd-test-logs/T-060-20261118-180156/bridge.log'
Failed to set value '0:v:0' for option 'map': Invalid argument
Error parsing options for output file /dev/video42.
Error opening output files: Invalid argument
gchd-bridge: capture pipeline ended
gchd-bridge: cannot set the MPEG format on /dev/video0
gchd-bridge: capture: /dev/video0 -> /dev/video42 (1280x720)
Stream map '0:v:0' matches no streams.
To ignore this, add a trailing '?' to the map.
Failed to set value '0:v:0' for option 'map': Invalid argument
Error parsing options for output file /dev/video42.
Error opening output files: Invalid argument
gchd-bridge: capture pipeline ended
$ cat /home/ff5/gchd-test-logs/T-060-20261118-180156/input-change.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 165888 kb/s, 30 fps, 30 tbr, 1000k tbn
frame=    0 fps=0.0 q=-0.0 size=       0kB time=00:00:00.00 bitrate=N/A speed=N/A    
[FAIL] reader kept running (frame count >= 500) (expected /frame= *([5-9][0-9]{2}|[0-9]{4})/, got:   Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 165888 kb/s, 30 fps, 30 tbr, 1000k tbn frame=    0 fps=0.0 q=-0.0 size=       0kB time=00:)
$ v4l2-ctl -d /dev/video0 --get-input
Video input : 0 (HDMI: no signal)
[FAIL] input back on composite (expected /Video input : 2/, got: Video input : 0 (HDMI: no signal))
--- dmesg step 6
[86006.921804] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86007.233395] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86007.523132] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86007.795900] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86008.091335] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86008.384971] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86008.678130] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86008.954257] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86009.282286] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86009.563243] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86009.862049] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86010.173677] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86010.478980] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86010.770733] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86011.072453] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86011.375944] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86011.670015] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86011.980423] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86012.278354] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86012.568760] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86012.836846] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86013.110906] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86013.422057] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86013.711354] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86013.988223] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86014.310118] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86014.619572] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86014.922926] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86015.233537] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86015.508971] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86015.786754] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86016.059953] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86016.335226] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86016.630287] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86016.928966] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86017.223823] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86017.512024] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86017.803981] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86018.085981] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86018.388138] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86018.754677] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86019.081334] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86019.510539] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86019.804877] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86020.150944] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86020.450768] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86020.757496] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86021.084464] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86021.376530] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86021.672322] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86021.946300] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86022.308803] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86022.639252] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86022.906784] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86023.207759] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86023.495568] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86023.786880] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86024.072154] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86024.359046] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86024.672504] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86024.959125] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86025.264348] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86025.532707] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86025.826649] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86026.105219] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86026.395515] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86026.700441] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86027.001728] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86027.319910] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86027.593170] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86027.886637] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86028.179307] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86028.458551] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86028.750716] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86029.057475] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86029.354707] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86029.640322] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86029.962351] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86030.263656] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86030.559695] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86030.853385] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86031.158177] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86031.504516] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86031.814520] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86032.151318] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86032.435416] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86032.724891] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86033.001149] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86033.333018] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86033.617832] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86033.928681] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86034.240479] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86034.537526] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86034.850226] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86035.140944] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86035.458931] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86035.744299] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86036.063787] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86036.356827] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86036.677316] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86037.040501] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86037.306787] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86037.600031] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86037.879506] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86038.188839] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86038.466831] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86038.761403] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86039.064321] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86039.359685] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86039.672571] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86039.955821] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86040.271155] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86040.569277] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86040.892637] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86041.185051] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86041.500752] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86041.826989] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86042.100383] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86042.417619] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86042.738466] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86043.029489] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86043.335616] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86043.611106] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86043.913342] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86044.190826] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86044.484632] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86044.790838] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86045.099031] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86045.404345] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86045.701168] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86045.993753] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86046.297122] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86046.581996] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86046.870690] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86047.153934] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86047.425826] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86047.722049] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86048.010454] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86048.310537] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86048.621198] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86048.893412] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86049.191781] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86049.477713] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86049.780172] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86050.084799] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86050.385452] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86050.686098] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86050.983658] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86051.275875] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86051.593716] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86051.928694] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86052.227345] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86052.538672] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86052.848638] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86053.124992] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86053.442913] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86053.729386] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86054.011905] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86054.327582] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86054.630612] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86054.915707] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86055.209030] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86055.511447] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0

=== step 7: guvcview
PROMPT: Start: guvcview -d /dev/video42   (in a second terminal; leave it open). If guvcview opens another device, select "Elgato Game Capture HD" at /dev/video42, not the one at /dev/video0.
[FAIL] guvcview shows the PS1 picture, smooth, no interlacing lines? -- error saying that it couldn't start a stream in the device
PROMPT: Swap the PS1 for the N64 (NTSC) while guvcview stays open; wait about 10 s.
[FAIL] guvcview shows the N64 picture again (scaled to the PAL size)? -- same error as with the PS1
PROMPT: Close guvcview.
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
[PASS] idle size follows the NTSC source after closing
--- dmesg step 7
[86055.833199] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86056.100572] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86056.376988] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86056.646612] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86056.916642] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86057.190963] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86057.462101] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86057.733636] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86058.007486] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86058.275779] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86058.549511] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86058.822779] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86059.100602] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86059.380007] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86059.654408] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86059.925537] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86060.200857] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86060.472252] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86060.741591] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86061.011276] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86061.283060] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86061.554046] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86061.829113] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86062.100473] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86062.371126] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86062.647725] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86062.918802] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86063.186163] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86063.461753] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86063.734029] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86064.005544] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86064.277202] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86064.550520] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86064.821297] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86065.098726] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86065.375454] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86065.646057] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86065.919191] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86066.188610] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86066.463525] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86066.735784] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86067.009336] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86067.288513] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86067.565460] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86067.839597] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86068.107915] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86068.382423] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86068.653919] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86068.925183] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86069.199041] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86069.470902] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86069.745380] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86070.022312] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86070.295182] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86070.567824] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86070.837761] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86071.108469] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86071.381339] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86071.654122] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86071.926454] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86072.200063] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86072.471437] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86072.743419] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86073.012892] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86073.289728] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86073.574508] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86073.850777] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86074.130599] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86074.408312] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86074.678084] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86074.979240] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86075.326546] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86075.634021] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86075.974126] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86076.273274] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86076.576814] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86076.873004] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86077.189739] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86077.481123] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86077.748370] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86078.057956] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86078.372413] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86078.658673] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86078.955826] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86079.252422] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86079.565943] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86079.864917] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86080.163142] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86080.467096] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86080.780646] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86081.094295] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86081.386514] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86081.686217] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86081.957045] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86081.974878] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86082.245740] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86082.514103] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86082.808660] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86083.114917] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86083.397307] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86083.694595] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86083.985046] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86084.271017] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86084.559896] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86084.865701] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86085.156921] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86085.576416] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86085.914151] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86086.225276] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86086.523616] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86086.814500] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86087.111533] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86087.410441] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86087.699969] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86087.981648] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86088.281231] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86088.556942] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86088.851724] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86089.138731] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86089.436085] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86089.719159] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86089.999165] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86090.309029] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86090.589628] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86090.900119] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86091.221577] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86091.496527] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86091.824508] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86092.094607] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86092.111813] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86092.402181] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86092.759026] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86093.028981] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86093.373114] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86093.672355] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86093.962738] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86094.261515] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86094.555512] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86094.847987] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86095.125681] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86095.415310] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86095.695357] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86095.989411] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86096.262473] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86096.566419] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86096.850308] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86097.136534] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86097.428039] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86097.712433] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86098.022226] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86098.291015] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86098.617534] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86098.909178] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86099.224499] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86099.587504] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86099.910821] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86100.256721] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86100.524190] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86100.821508] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86101.131478] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86101.438073] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86101.732052] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86102.033361] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86102.366626] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86102.636570] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86102.944137] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86103.246354] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86103.616695] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86103.900199] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86104.193480] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86104.488708] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86104.754660] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86105.067394] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86105.392585] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86105.676400] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86106.000066] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86106.305530] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86106.600199] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86106.869422] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86107.187186] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86107.492147] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86107.806198] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86108.148028] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86108.429387] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86108.723127] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86108.998009] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86109.299587] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86109.603905] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86109.896277] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86110.199996] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86110.519488] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86110.841324] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86111.148555] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86111.441693] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86111.745272] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86112.037698] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86112.335681] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86112.640930] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86112.957282] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86113.254622] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86113.572518] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86113.874338] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86114.151500] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86114.450715] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86114.769370] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86115.072769] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86115.350888] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86115.629898] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86115.930867] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86116.215828] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86116.530953] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86116.848352] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86117.136375] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86117.404105] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86117.701426] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86117.990457] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86118.327006] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86118.629878] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86118.948569] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86119.233305] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86119.507273] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86119.832544] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86120.129776] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86120.461650] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86120.765996] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86121.070541] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86121.375544] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86121.674227] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86121.965594] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86122.236893] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86122.547460] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86122.850523] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86123.145466] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86123.445303] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86123.715198] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86124.068721] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86124.366474] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86124.672590] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86124.981872] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86125.279184] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86125.583820] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86125.849329] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86126.160833] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86126.491843] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86126.794692] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86127.090441] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86127.368987] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86127.636541] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86127.917191] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86128.230504] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86128.535237] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86128.813207] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86129.094869] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86129.411271] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86129.709768] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86129.994359] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86130.305380] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86130.586265] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86130.882666] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86131.173196] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86131.457831] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86131.743834] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86132.044179] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86132.334927] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86132.640635] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86132.940759] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86133.272220] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86133.556950] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86133.854111] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86134.146663] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86134.436594] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86134.711291] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86134.982385] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86135.252698] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86135.530561] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86135.822055] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86136.172498] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86136.476216] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86136.775402] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86137.083304] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86137.357989] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86137.657439] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86137.970530] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86138.272205] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86138.578839] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86138.886138] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86139.197174] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86139.467082] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86139.791266] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86140.108465] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86140.401512] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86140.714918] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86141.030619] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86141.329159] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86141.608046] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86141.924459] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86142.227909] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86142.568624] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86142.885962] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86143.183505] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86143.482348] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86143.756522] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86144.072857] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86144.379184] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86144.740002] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86145.074336] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86145.383004] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86145.684307] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86146.003803] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86146.311680] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86146.654051] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86146.929310] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86147.272867] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86147.607248] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86147.914128] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86148.221916] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86148.536086] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86148.850703] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86149.127507] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86149.420935] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86149.712805] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86149.995294] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86150.322630] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86150.609631] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86150.932093] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86151.203879] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86151.509195] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86151.821918] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86152.137277] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86152.458980] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86152.774598] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86153.063065] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86153.360519] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86153.657105] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86153.964640] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86154.245144] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86154.548862] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86154.847593] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86155.139878] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86155.410537] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86155.713730] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86156.028344] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86156.346400] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86156.671504] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86156.981202] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86157.288042] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86157.590946] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86157.879149] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86158.177976] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86158.456081] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86158.764181] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86159.080017] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86159.398340] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86159.687382] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86160.002931] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86160.331134] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86160.604464] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86160.923761] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86161.257445] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86161.613101] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86161.918034] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86162.220979] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86162.529650] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86162.825712] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86163.183032] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86163.473808] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86163.746545] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86164.060532] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86164.353814] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86164.694907] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86164.961771] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86165.275537] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86165.594901] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86165.904173] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86166.208617] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86166.503785] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86166.832083] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86167.111132] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86167.432426] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86167.755758] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86168.052161] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86168.344749] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86168.659190] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86169.058016] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86169.396977] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86169.723711] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86170.073008] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86170.376648] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86170.671802] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86170.956290] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86171.236793] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86171.574966] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86171.889132] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86172.235954] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86172.533712] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86172.834796] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86173.155998] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86173.447344] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86173.738504] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86174.028554] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86174.339186] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86174.617597] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86174.908884] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86175.209794] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86175.485006] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86175.768895] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86176.073876] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86176.352816] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86176.619999] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86176.934445] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86177.227787] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86177.514116] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86177.816349] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86178.132482] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86178.430914] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86178.699663] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86179.003876] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86179.299704] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86179.588475] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86179.867561] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86180.164588] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86180.488654] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86180.764695] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86181.071911] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86181.394222] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86181.687621] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86181.980369] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86182.250533] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86182.568490] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86182.869907] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86183.144220] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86183.448735] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86183.740622] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86184.016017] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86184.312201] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86184.620999] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86184.922605] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86185.226893] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86185.517986] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86185.826029] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86186.146620] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86186.469495] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86186.785516] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86187.090453] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86187.406994] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86187.702609] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86187.992735] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86188.289571] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86188.604546] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86188.906572] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86189.200468] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86189.544036] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86189.845066] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86190.213266] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86190.482031] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86190.780027] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86191.071280] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86191.337459] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86191.646936] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86191.937506] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86192.225451] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86192.503436] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86192.791108] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86193.099173] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86193.425704] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86193.699273] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86194.002342] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86194.308176] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86194.626969] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86194.908109] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86195.212445] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86195.512645] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86195.777600] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86196.146535] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86196.529701] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86196.806134] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86197.111453] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86197.407825] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86197.677287] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86197.944281] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86198.261822] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86198.564113] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86198.915853] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86199.231975] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86199.525393] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86199.801850] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86200.073830] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86200.364519] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86200.637741] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86200.911242] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86201.201141] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86201.525012] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86201.813594] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86202.114447] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86202.405098] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86202.699439] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86202.990206] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86203.269865] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86203.550395] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86203.847825] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86204.131434] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86204.432298] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86204.706408] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86204.994283] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86205.272177] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86205.588816] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86205.901780] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86206.174632] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86206.471305] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86206.756710] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86207.055905] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86207.325911] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86207.610987] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86207.917367] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86208.211605] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86208.483933] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86208.771846] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86209.068644] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86209.357645] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86209.661649] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86209.937879] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86210.238186] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86210.507822] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86210.817850] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86211.120550] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86211.466551] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86211.754933] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86212.052273] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86212.321231] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86212.585132] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86212.871748] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86213.169820] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86213.466496] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86213.742880] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86214.021822] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86214.311644] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86214.597759] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86214.878772] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86215.183270] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86215.455586] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86215.721671] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86215.996805] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86216.316629] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86216.614263] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86216.908030] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86217.182071] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86217.466968] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86217.766976] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86218.053674] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86218.370320] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86218.679363] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86218.991442] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86219.282756] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86219.588579] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86219.888887] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86220.207446] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86220.539884] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86220.862102] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86221.179141] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86221.489391] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86221.778932] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86222.097965] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86222.385783] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86222.656949] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86222.943068] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86223.274296] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86223.549815] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86223.849756] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86224.136392] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86224.458643] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86224.763934] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86225.052257] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86225.345079] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86225.611105] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86225.911602] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86226.195199] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86226.518681] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86226.793539] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86227.097653] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86227.387111] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86227.679072] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86228.012942] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86228.331332] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86228.619374] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86228.919874] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86229.233596] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86229.524774] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86229.826272] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86230.154656] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86230.470867] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86230.767525] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86231.063219] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86231.433493] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86231.707482] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86231.988474] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86232.272477] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86232.563968] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86232.867027] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86233.150198] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86233.443490] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86233.745117] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86234.081574] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86234.373157] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86234.718091] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86235.008657] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86235.339614] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86235.641323] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86235.916954] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86236.197608] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86236.508486] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86236.804548] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86237.093745] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86237.417212] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86237.718882] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86237.998895] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86238.302194] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86238.601512] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86238.890338] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86239.216970] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86239.491820] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86239.777590] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86240.075579] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86240.350387] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86240.733406] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86241.012630] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86241.290921] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86241.564929] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86241.848012] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86242.188120] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86242.478676] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86242.774080] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86243.085536] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86243.434253] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86243.707669] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86244.057212] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86244.326450] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86244.614701] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86244.891854] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86245.168095] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86245.438237] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86245.715094] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86245.995739] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86246.270205] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86246.544096] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86246.818408] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86247.100527] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86247.377609] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86247.656395] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86247.929655] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86248.211518] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86248.486510] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86248.755244] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86249.030418] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86249.308402] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86249.582900] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86249.853378] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86250.125665] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86250.404190] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86250.688399] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86250.963631] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86251.240206] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86251.515984] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86251.791654] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86252.067203] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86252.343490] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86252.618455] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86252.899978] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86253.182476] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86253.455150] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86253.727179] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86253.999228] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86254.271483] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86254.550263] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86254.823510] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86255.097500] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86255.374820] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86255.645720] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86255.918906] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86256.189818] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86256.463142] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86256.731786] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86257.004869] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86257.277583] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86257.548834] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86257.824658] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86258.091431] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86258.363875] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86258.637077] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86258.907567] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86259.180276] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86259.452488] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86259.727284] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86259.997741] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86260.272126] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86260.538532] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86260.813674] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86261.086439] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86261.354693] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86261.632778] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86261.900372] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86262.174926] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86262.455661] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86262.725105] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86263.001468] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86263.278528] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86263.554225] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86263.829193] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86264.100990] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86264.375914] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86264.645366] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86264.918748] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86265.189969] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86265.461424] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86265.733656] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86266.005165] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86266.284308] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86266.558502] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86266.831500] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86267.102190] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86267.373859] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86267.645433] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86267.917747] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86268.188820] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86268.459887] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0

=== step 8: bridge stopped
$ kill 65513
$ sh -c pactl list short sources | grep -c elgato_gchd
2
[FAIL] audio source removed (expected /^0$/, got: 2)
$ tail -30 /home/ff5/gchd-test-logs/T-060-20261118-180156/bridge.log
Stream map '0:v:0' matches no streams.
To ignore this, add a trailing '?' to the map.
Failed to set value '0:v:0' for option 'map': Invalid argument
Error parsing options for output file /dev/video42.
Error opening output files: Invalid argument
gchd-bridge: capture pipeline ended
gchd-bridge: cannot set the MPEG format on /dev/video0
gchd-bridge: capture: /dev/video0 -> /dev/video42 (1280x720)
Stream map '0:v:0' matches no streams.
To ignore this, add a trailing '?' to the map.
Failed to set value '0:v:0' for option 'map': Invalid argument
Error parsing options for output file /dev/video42.
Error opening output files: Invalid argument
gchd-bridge: capture pipeline ended
gchd-bridge: cannot set the MPEG format on /dev/video0
gchd-bridge: capture: /dev/video0 -> /dev/video42 (1280x720)
Stream map '0:v:0' matches no streams.
To ignore this, add a trailing '?' to the map.
Failed to set value '0:v:0' for option 'map': Invalid argument
Error parsing options for output file /dev/video42.
Error opening output files: Invalid argument
gchd-bridge: capture pipeline ended
gchd-bridge: cannot set the MPEG format on /dev/video0
gchd-bridge: capture: /dev/video0 -> /dev/video42 (1280x720)
Stream map '0:v:0' matches no streams.
To ignore this, add a trailing '?' to the map.
Failed to set value '0:v:0' for option 'map': Invalid argument
Error parsing options for output file /dev/video42.
Error opening output files: Invalid argument
gchd-bridge: capture pipeline ended
$ sudo modprobe -r v4l2loopback
modprobe: FATAL: Module v4l2loopback is in use.
--- dmesg step 8
[86269.005310] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86269.274710] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86269.547026] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86269.820168] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86270.092131] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86270.366117] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[86270.639631] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0

=== summary (T-060, b1c13da service: ignore PipeWire/WirePlumber device probing, log readers, kill a stuck ffmpeg after 3 s)
PASS | step 1 v4l2loopback and the driver | loopback device present
PASS | step 2 bridge idle: black picture, capture device off | bridge running and both devices found
PASS | step 2 bridge idle: black picture, capture device off | loopback offers YUYV of the driver's current size while idle
PASS | step 2 bridge idle: black picture, capture device off | audio source present
FAIL | step 2 bridge idle: black picture, capture device off | capture device off while idle (expected /^0$/, got: 1)
PASS | step 3 application reads: capture starts, 720x576 at 50 frames/s | loopback stream YUYV 720x480 or 720x576
FAIL | step 3 application reads: capture starts, 720x576 at 50 frames/s | frame count >= 400 in 15 s (capture running) (expected /frame= *([4-9][0-9]{2}|[0-9]{4})/, got:   Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 165888 kb/s, 30 fps, 30 tbr, 1000k tbn frame=    1 fps=0.7 q=-0.0 size=N/A time=00:00:00.0)
FAIL | step 3 application reads: capture starts, 720x576 at 50 frames/s | capture device started (expected /starting hardware boot/, got: [85922.470315] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0 [85922.470322] gchd_detection_worker:)
FAIL | step 4 application closed: capture device off | hardware shut down after the last reader (expected /hardware shutdown done/, got: [85962.609331] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0 [85962.936533] elgato_gchd 1-2:1.0: g)
FAIL | step 5 audio while capturing | the bridge plays into the audio sink (expected /stream into elgato_gchd/, got: )
PASS | step 5 audio while capturing | audio source delivers samples
FAIL | step 6 input change through the bridge while an application reads | reader kept running (frame count >= 500) (expected /frame= *([5-9][0-9]{2}|[0-9]{4})/, got:   Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 165888 kb/s, 30 fps, 30 tbr, 1000k tbn frame=    0 fps=0.0 q=-0.0 size=       0kB time=00:)
FAIL | step 6 input change through the bridge while an application reads | input back on composite (expected /Video input : 2/, got: Video input : 0 (HDMI: no signal))
FAIL | step 7 guvcview | guvcview shows the PS1 picture, smooth, no interlacing lines? -- error saying that it couldn't start a stream in the device
FAIL | step 7 guvcview | guvcview shows the N64 picture again (scaled to the PAL size)? -- same error as with the PS1
PASS | step 7 guvcview | idle size follows the NTSC source after closing
FAIL | step 8 bridge stopped | audio source removed (expected /^0$/, got: 2)
