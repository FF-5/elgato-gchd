# TASK T-060 — guvcview, OBS and similar applications

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** application device in the driver + bridge; first hardware test
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

## Implementation

Driver (`src/app.c`, hooks in `core.c`; no protocol change):

- Three devices per Game Capture HD: "Elgato Game Capture HD (H.264)"
  (the existing device, renamed), "Elgato Game Capture HD" (application
  device: YUYV, capture) and "Elgato Game Capture HD (frame input)"
  (output device the bridge writes decoded frames into; the driver copies
  each frame into the next buffer of the application device).
- Application format = source: detected mode, else the nominal size of
  the input; 50/1 or 60000/1001 frames/s for analog (one frame per
  field), the mode rate for HDMI/component. 480i/240p and 576i/288p keep
  the size. A capture start or a lost signal keeps the format.
- Format change while an application has buffers: the device is
  unregistered (handles get ENODEV, like an unplugged camera) and
  registered again once it is closed, at the latest after 2 s, with the
  same number when free. Without buffers the format changes in place.
- `VIDIOC_S_INPUT` on the application device (also while capturing).
- sysfs `/sys/bus/usb/drivers/elgato_gchd/<interface>/app`:
  `STATE APP-NODE WIDTH HEIGHT NUM DEN FEED-NODE STREAM-NODE`, notified
  on changes.
- Lifetime: the driver data lives as long as the `v4l2_device`, which
  every registered node (also re-registered application devices still
  open) holds.

Bridge (`service/gchd-bridge`): reads the sysfs state every 0.5 s; while
an application streams, ffmpeg reads the H.264 device's transport
stream, deinterlaces (`bwdif`, one frame per field), scales to the
application format, `fps` to its rate, writes YUYV into the frame input
device and the audio to the audio source; stops when the application
stops (capture device off); restarts on a format change. v4l2loopback,
`fuser` and the black picture are gone.

Offline: build W=1 clean; shellcheck clean; the filter chain gives 150
frames for 3 s of 576i25 at 50/1. Not run on hardware yet.

Next steps after the test: systemd user service (start at login),
`install-driver.sh` integration, input selection from the desktop,
remembered standard per input (a PAL source then starts at 720×576
without a first re-registration).

## Hardware test (maintainer)

PS1 (PAL) on composite, the N64 (NTSC) for the swap with guvcview open.
Needs guvcview and pactl. The test stops a bridge left from earlier runs
and unloads v4l2loopback (no longer used). The first read is expected to
end early: after loading, the device starts with the NTSC size and is
registered again when the PAL source is detected.

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-060.sh > tests/T-060.sh
bash gchd-test.sh T-060 --branch feat/app-bridge
```

Paste the summary; on FAILs the log too (the bridge's own messages are
at the end of it).

## Findings

- First run of the application device (1c46631): the bridge never
  started ffmpeg. The 0fd9:005d is the HDNew family, whose H.264 device
  was named "Elgato Game Capture HD (HDNew, H.264)": longer than the
  31 characters of a V4L2 name, cut to "Elgato Game Capture HD (HDNew, ",
  so the bridge's search for "(H.264)" failed ("stream device: none").
  Hence no frames (guvcview "could not grab image", 0 frames), no audio
  stream, capture device never started. Working: three devices, YUYV at
  the input's size (1280×720 at load: remembered input HDMI; 720×480 on
  composite), capture device off while idle, re-registration on an input
  change while reading (reader ended, device back as video2), input
  change on the application device. Fixed: one name "Elgato Game Capture
  HD (H.264)" for all models; the sysfs state names the H.264 device and
  the bridge takes it from there.
- Fourth T-060 run (b1c13da, v4l2loopback): leftovers of the hung run
  (module in use, an old reader, input busy) made it unusable. The
  maintainer stopped the v4l2loopback tests; the application device is
  now in the driver.
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

gchd-test T-060 20261118-183549 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-060-20261118-183549.log
== build
$ git fetch -q origin
$ git checkout -q feat/app-bridge
$ git pull -q --ff-only
commit: 15634d8 dkms: H.264 device name fits V4L2 (31 characters); sysfs state names the H.264 device; bridge uses it (T-060)
commit: 15634d8 dkms: H.264 device name fits V4L2 (31 characters); sysfs state names the H.264 device; bridge uses it (T-060)
build rc=0 warnings=0

=== step 1: driver with three devices
$ sh -c pkill -f 'gchd-bridge run'; sudo modprobe -r v4l2loopback; true
PROMPT: Unplug the Elgato from the PC (or detach it in VirtualBox), wait 3 s, plug it in again.
$ sudo insmod /home/ff5/elgato-gchd/src/elgato_gchd.ko
module: 0.5.8 
$ v4l2-ctl --list-devices
Elgato Game Capture HD (platform:v4l2loopback-000):
	/dev/video42

Elgato Game Capture HD (H.264) (usb-0000:00:0c.0-2):
	/dev/video0
	/dev/video1
	/dev/video2

$ v4l2-ctl -d /dev/video0 -D
Driver Info:
	Driver name      : elgato-gchd
	Card type        : Elgato Game Capture HD (H.264)
	Bus info         : usb-0000:00:0c.0-2
	Driver version   : 6.8.12
	Capabilities     : 0x85200001
		Video Capture
		Read/Write
		Streaming
		Extended Pix Format
		Device Capabilities
	Device Caps      : 0x05200001
		Video Capture
		Read/Write
		Streaming
		Extended Pix Format
[PASS] H.264 device name
$ app_attr
idle video2 1280 720 60 1 video1 video0
[PASS] application device registered, idle, H.264 device named
$ v4l2-ctl -d /dev/video2 --list-formats-ext
ioctl: VIDIOC_ENUM_FMT
	Type: Video Capture

	[0]: 'YUYV' (YUYV 4:2:2)
		Size: Discrete 1280x720
			Interval: Discrete 0.017s (60.000 fps)
[PASS] application device offers YUYV
--- dmesg step 1
[87912.310176] usbcore: deregistering interface driver elgato_gchd
[87915.015074] usb 1-2: USB disconnect, device number 86
[87919.871747] usb 1-2: new high-speed USB device number 87 using xhci_hcd
[87920.118619] usbcore: registered new interface driver elgato_gchd
[87920.271419] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[87920.271440] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[87920.271446] usb 1-2: Product: GameCapture HD
[87920.271451] usb 1-2: Manufacturer: Elgato
[87920.271455] usb 1-2: SerialNumber: 04107F1267
[87920.277732] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[87920.282582] elgato_gchd 1-2:1.0: application device video2: 1280x720, 60/1 frames/s
[87920.282591] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON

=== step 2: bridge idle: capture device off
PROMPT: PS1 on composite, game running.
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ bash /home/ff5/elgato-gchd/service/gchd-bridge status
driver state:   idle video2 720 480 60000 1001 video1 video0
service:        running (pid 97022)
$ bash /home/ff5/elgato-gchd/service/gchd-bridge status
driver state:   idle video2 720 480 60000 1001 video1 video0
service:        running (pid 97022)
[PASS] bridge running
$ app_attr
idle video2 720 480 60000 1001 video1 video0
[PASS] application device on composite: 720x480 or 720x576
$ sh -c sudo dmesg | grep -c 'starting hardware boot'
0
[PASS] capture device off while no application streams
--- dmesg step 2
[87949.786394] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[87949.786542] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[87949.786558] elgato_gchd 1-2:1.0: application device: 720x480, 60000/1001 frames/s

=== step 3: first read: source detected (PAL), device registered again with 720x576
$ cat /home/ff5/gchd-test-logs/T-060-20261118-183549/read1.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 331444 kb/s, 59.94 fps, 59.94 tbr, 1000k tbn
[in#0/video4linux2,v4l2 @ 0x5f44b9579dc0] Error during demuxing: No such device
[in#0/video4linux2,v4l2 @ 0x5f44b9579dc0] Error retrieving a packet from demuxer: No such device
frame=    0 fps=0.0 q=0.0 Lsize=N/A time=N/A bitrate=N/A speed=N/A    
$ app_attr
idle video2 720 576 50 1 video1 video0
$ app_attr
idle video2 720 576 50 1 video1 video0
[PASS] application device now 720x576, 50 frames/s
$ tail -8 /home/ff5/gchd-test-logs/T-060-20261118-183549/bridge.log
    Last message repeated 1 times
[graph_1_in_0_1 @ 0x6438d9091bc0] Error setting option time_base to value 1/0.
[graph_1_in_0_1 @ 0x6438d9091bc0] Error applying generic filter options.
Error initializing filters!
Error marking filters as finished
[out#0/video4linux2,v4l2 @ 0x6438d906c780] Nothing was written into output file, because at least one of its streams received no packets.
[out#1/pulse @ 0x6438d9551ec0] Nothing was written into output file, because at least one of its streams received no packets.
gchd-bridge: capture stopped
--- dmesg step 3
[87953.002822] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[87953.150952] elgato_gchd 1-2:1.0: transcoder defaults loaded
[87953.217012] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[87953.591727] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[87953.594093] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[87953.594097] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[87953.594099] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[87953.594102] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[87954.378260] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[87954.595829] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[87954.611908] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[87954.612235] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[87954.612248] elgato_gchd 1-2:1.0: application device: source now 720x576, 50/1 frames/s; re-registering
[87954.617606] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[87954.618110] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[87954.619332] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[87954.719476] elgato_gchd 1-2:1.0: application device video2: 720x576, 50/1 frames/s
[87954.758149] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[87956.797349] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 4: read: 720x576 YUYV, about 50 frames/s
$ cat /home/ff5/gchd-test-logs/T-060-20261118-183549/read2.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=    0 fps=0.0 q=0.0 Lsize=       0kB time=N/A bitrate=N/A speed=N/A    
$ cat /home/ff5/gchd-test-logs/T-060-20261118-183549/read2.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=    0 fps=0.0 q=0.0 Lsize=       0kB time=N/A bitrate=N/A speed=N/A    
[PASS] stream YUYV 720x576
$ cat /home/ff5/gchd-test-logs/T-060-20261118-183549/read2.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=    0 fps=0.0 q=0.0 Lsize=       0kB time=N/A bitrate=N/A speed=N/A    
[FAIL] frame count >= 450 in 15 s (expected /frame= *([4-9][5-9][0-9]|[5-9][0-9]{2}|[0-9]{4})/, got:   Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn frame=    0 fps=0.0 q=0.0 Lsize=       0kB time=N/A)
$ sudo dmesg
[87959.137955] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=97231 comm=v4l2-ctl
[87959.183862] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=97235 comm=ffmpeg
[87959.183931] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[87959.200399] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[87959.200645] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
[87959.252270] elgato_gchd 1-2:1.0: transcoder defaults loaded
[87959.252400] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[87959.252651] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x04
[87959.254086] elgato_gchd 1-2:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.255223] elgato_gchd 1-2:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.256443] elgato_gchd 1-2:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.257624] elgato_gchd 1-2:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.258788] elgato_gchd 1-2:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.259974] elgato_gchd 1-2:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.261141] elgato_gchd 1-2:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.262328] elgato_gchd 1-2:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.263523] elgato_gchd 1-2:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.264675] elgato_gchd 1-2:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.265849] elgato_gchd 1-2:1.0: firmware chunk offset=327680 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.266984] elgato_gchd 1-2:1.0: firmware chunk offset=360448 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.268150] elgato_gchd 1-2:1.0: firmware chunk offset=393216 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.269432] elgato_gchd 1-2:1.0: firmware chunk offset=425984 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.270743] elgato_gchd 1-2:1.0: firmware chunk offset=458752 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.272093] elgato_gchd 1-2:1.0: firmware chunk offset=491520 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.273349] elgato_gchd 1-2:1.0: firmware chunk offset=524288 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.274582] elgato_gchd 1-2:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.275805] elgato_gchd 1-2:1.0: firmware chunk offset=589824 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.277074] elgato_gchd 1-2:1.0: firmware chunk offset=622592 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.278433] elgato_gchd 1-2:1.0: firmware chunk offset=655360 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.279657] elgato_gchd 1-2:1.0: firmware chunk offset=688128 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.280935] elgato_gchd 1-2:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.282164] elgato_gchd 1-2:1.0: firmware chunk offset=753664 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.283431] elgato_gchd 1-2:1.0: firmware chunk offset=786432 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.284790] elgato_gchd 1-2:1.0: firmware chunk offset=819200 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.286103] elgato_gchd 1-2:1.0: firmware chunk offset=851968 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.287561] elgato_gchd 1-2:1.0: firmware chunk offset=884736 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.288840] elgato_gchd 1-2:1.0: firmware chunk offset=917504 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.290176] elgato_gchd 1-2:1.0: firmware chunk offset=950272 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.291509] elgato_gchd 1-2:1.0: firmware chunk offset=983040 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.292907] elgato_gchd 1-2:1.0: firmware chunk offset=1015808 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.294139] elgato_gchd 1-2:1.0: firmware chunk offset=1048576 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.295423] elgato_gchd 1-2:1.0: firmware chunk offset=1081344 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.296699] elgato_gchd 1-2:1.0: firmware chunk offset=1114112 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.298089] elgato_gchd 1-2:1.0: firmware chunk offset=1146880 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.299246] elgato_gchd 1-2:1.0: firmware chunk offset=1179648 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.300478] elgato_gchd 1-2:1.0: firmware chunk offset=1212416 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.301814] elgato_gchd 1-2:1.0: firmware chunk offset=1245184 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.303157] elgato_gchd 1-2:1.0: firmware chunk offset=1277952 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.304454] elgato_gchd 1-2:1.0: firmware chunk offset=1310720 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.305758] elgato_gchd 1-2:1.0: firmware chunk offset=1343488 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.307102] elgato_gchd 1-2:1.0: firmware chunk offset=1376256 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.308374] elgato_gchd 1-2:1.0: firmware chunk offset=1409024 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.309580] elgato_gchd 1-2:1.0: firmware chunk offset=1441792 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.310863] elgato_gchd 1-2:1.0: firmware chunk offset=1474560 size=26224 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[87959.316399] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[87959.406771] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[87959.456723] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[87959.466284] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[87959.468866] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[87959.468868] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[87959.468870] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[87959.468872] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[87960.271220] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[87960.271390] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[87960.532122] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[87960.548397] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[87960.548519] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[87960.548620] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[87960.554511] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[87960.555294] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[87960.556001] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[87960.556134] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[87960.556428] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[87960.556846] elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0150 ack=1
[87960.557601] elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[87960.557907] elgato_gchd 1-2:1.0: START poll=1 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[87960.558250] elgato_gchd 1-2:1.0: START poll=2 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[87960.558580] elgato_gchd 1-2:1.0: START poll=3 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[87960.558898] elgato_gchd 1-2:1.0: START poll=4 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[87960.559221] elgato_gchd 1-2:1.0: START poll=5 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[87960.559529] elgato_gchd 1-2:1.0: START poll=6 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[87960.559902] elgato_gchd 1-2:1.0: START poll=7 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[87960.560274] elgato_gchd 1-2:1.0: START poll=8 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[87960.560579] elgato_gchd 1-2:1.0: START poll=9 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[87960.560901] elgato_gchd 1-2:1.0: START poll=10 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[87960.561272] elgato_gchd 1-2:1.0: START poll=11 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[87960.561578] elgato_gchd 1-2:1.0: START poll=12 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[87960.561901] elgato_gchd 1-2:1.0: START poll=13 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[87960.562213] elgato_gchd 1-2:1.0: START poll=14 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[87960.562523] elgato_gchd 1-2:1.0: START poll=15 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[87960.589688] elgato_gchd 1-2:1.0: START poll=100 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[87960.623854] elgato_gchd 1-2:1.0: START poll=200 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[87960.657778] elgato_gchd 1-2:1.0: START poll=300 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[87960.673960] elgato_gchd 1-2:1.0: START poll=347 state=0x0002 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0150 ack=1
[87960.674533] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[87961.071955] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[87961.117889] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[87961.162230] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[87961.207210] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[87961.252840] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[87965.581030] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[87970.605703] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[87975.632835] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[87980.658543] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[87985.683658] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[87990.709918] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=679 bytes=44498944 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[PASS] capture device started
--- dmesg step 4
[87959.183931] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[87959.252270] elgato_gchd 1-2:1.0: transcoder defaults loaded
[87959.316399] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[87959.406771] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[87959.466284] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[87959.468866] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[87959.468868] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[87959.468870] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[87959.468872] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[87960.271220] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[87960.532122] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[87960.548397] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[87960.548620] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[87960.554511] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[87960.555294] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[87960.556001] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[87960.674533] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[87965.581030] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[87970.605703] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[87975.632835] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[87980.658543] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[87985.683658] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[87990.709918] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=679 bytes=44498944 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 5: application closed: capture device off
$ sudo dmesg
[87993.925419] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[87993.925814] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[87994.682420] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[87994.683063] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[87994.845775] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[87994.846927] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[87994.847246] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
[87994.867477] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[PASS] hardware shut down after the application stopped
$ app_attr
idle video2 720 576 50 1 video1 video0
[PASS] state idle
--- dmesg step 5
[87994.867477] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 6: audio while capturing
$ sink_streams
stream into elgato_gchd: 1482	1450	1481	PipeWire	s16le 2ch 48000Hz
[PASS] the bridge plays into the audio sink
$ ffmpeg -hide_banner -nostdin -f pulse -i elgato_gchd_audio -t 4 -af volumedetect -f null -
[aist#0:0/pcm_s16le @ 0x5c37a9527640] Guessed Channel Layout: stereo
Input #0, pulse, from 'elgato_gchd_audio':
  Duration: N/A, start: 1795023451.634495, bitrate: 1536 kb/s
  Stream #0:0: Audio: pcm_s16le, 48000 Hz, 2 channels, s16, 1536 kb/s
[Parsed_volumedetect_0 @ 0x5c37a9529000] n_samples: 0
Stream mapping:
  Stream #0:0 -> #0:0 (pcm_s16le (native) -> pcm_s16le (native))
Output #0, null, to 'pipe:':
  Metadata:
    encoder         : Lavf60.16.100
  Stream #0:0: Audio: pcm_s16le, 48000 Hz, stereo, s16, 1536 kb/s
    Metadata:
      encoder         : Lavc60.31.102 pcm_s16le
size=       0kB time=00:00:00.00 bitrate=N/A speed=N/A    
size=N/A time=00:00:00.59 bitrate=N/A speed= 1.1x    
size=N/A time=00:00:01.14 bitrate=N/A speed=1.08x    
size=N/A time=00:00:01.63 bitrate=N/A speed=1.04x    
size=N/A time=00:00:02.13 bitrate=N/A speed=1.03x    
size=N/A time=00:00:02.63 bitrate=N/A speed=1.02x    
size=N/A time=00:00:03.18 bitrate=N/A speed=1.03x    
size=N/A time=00:00:03.68 bitrate=N/A speed=1.02x    
[out#0/null @ 0x5c37a9524b00] video:0kB audio:752kB subtitle:0kB other streams:0kB global headers:0kB muxing overhead: unknown
size=N/A time=00:00:03.98 bitrate=N/A speed=0.985x    
[Parsed_volumedetect_0 @ 0x5c37a952ba80] n_samples: 393600
[Parsed_volumedetect_0 @ 0x5c37a952ba80] mean_volume: -85.5 dB
[Parsed_volumedetect_0 @ 0x5c37a952ba80] max_volume: -76.3 dB
[Parsed_volumedetect_0 @ 0x5c37a952ba80] histogram_76db: 73
[Parsed_volumedetect_0 @ 0x5c37a952ba80] histogram_77db: 0
[Parsed_volumedetect_0 @ 0x5c37a952ba80] histogram_78db: 2999
[PASS] audio source delivers samples
--- dmesg step 6
[88000.041053] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[88000.164564] elgato_gchd 1-2:1.0: transcoder defaults loaded
[88000.227723] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[88000.316992] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[88000.449029] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[88000.451293] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[88000.451297] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[88000.451300] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[88000.451303] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[88001.263693] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[88001.555840] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[88001.578560] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[88001.578970] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[88001.586204] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[88001.587160] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[88001.588033] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[88001.704540] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[88006.611052] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[88011.637283] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[88016.664120] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[88021.689787] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[88026.715593] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[88031.741736] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=679 bytes=44498944 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 7: input change on the application device while reading
$ v4l2-ctl -d /dev/video2 -i 3
Video input set to 3 (S-Video: Camera, no signal)
$ app_attr
idle video2 720 480 60000 1001 video1 video0
$ cat /home/ff5/gchd-test-logs/T-060-20261118-183549/read3.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
[in#0/video4linux2,v4l2 @ 0x593a97873dc0] Error during demuxing: No such device
[in#0/video4linux2,v4l2 @ 0x593a97873dc0] Error retrieving a packet from demuxer: No such device
frame=    0 fps=0.0 q=0.0 Lsize=N/A time=N/A bitrate=N/A speed=N/A    
$ app_attr
idle video2 720 480 60000 1001 video1 video0
[PASS] S-video (no source, NTSC size): device re-registered with 720x480
$ v4l2-ctl -d /dev/video2 -i 2
Video input set to 2 (Composite: Camera, ok)
$ app_attr
idle video2 720 576 50 1 video1 video0
[PASS] composite again: 720x576
$ v4l2-ctl -d /dev/video0 --get-input
Video input : 2 (Composite: no signal)
[PASS] input composite on the H.264 device too
--- dmesg step 7
[88036.768280] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=794 bytes=52035584 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[88041.796062] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=909 bytes=59572224 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[88043.831306] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=1 streaming=1
[88044.584845] elgato_gchd 1-2:1.0: input selected: input=3; detection scheduled
[88044.587906] elgato_gchd 1-2:1.0: application device: source now 720x480, 60000/1001 frames/s; re-registering
[88044.695507] elgato_gchd 1-2:1.0: application device video2: 720x480, 60000/1001 frames/s
[88044.696307] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[88044.812816] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[88047.869851] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[88048.618640] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[88048.618675] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[88048.618682] elgato_gchd 1-2:1.0: application device: 720x576, 50/1 frames/s

=== step 8: guvcview
PROMPT: Start in a second terminal: guvcview -d /dev/video2   (leave it open)
[FAIL] guvcview shows the PS1 picture, smooth, without interlacing lines? -- no, v4l2_core still says it couldn't grab the image
PROMPT: Swap the PS1 for the N64 (NTSC) while guvcview stays open; wait about 15 s.
$ app_attr
idle video3 720 480 60000 1001 video1 video0
[FAIL] guvcview reconnected by itself and shows the N64 picture (720x480)? -- i guess same problem as with PS1
PROMPT: Close guvcview.
$ tail -20 /home/ff5/gchd-test-logs/T-060-20261118-183549/bridge.log
Error initializing filters!
Error marking filters as finished
[out#0/video4linux2,v4l2 @ 0x6438d906c780] Nothing was written into output file, because at least one of its streams received no packets.
[out#1/pulse @ 0x6438d9551ec0] Nothing was written into output file, because at least one of its streams received no packets.
gchd-bridge: capture stopped
gchd-bridge: capture: /dev/video0 -> /dev/video1 (720x576, 50/1 frames/s)
[vost#0:0/rawvideo @ 0x626a4845c700] Error submitting a packet to the muxer: Invalid argument
[out#0/video4linux2,v4l2 @ 0x626a482fb780] Error muxing a packet
gchd-bridge: capture stopped
gchd-bridge: capture: /dev/video0 -> /dev/video1 (720x576, 50/1 frames/s)
[vost#0:0/rawvideo @ 0x5cdb31c660c0] Error submitting a packet to the muxer: Invalid argument
[out#0/video4linux2,v4l2 @ 0x5cdb31bee780] Error muxing a packet
gchd-bridge: ffmpeg 97750 did not stop; killing it
gchd-bridge: capture stopped
gchd-bridge: capture: /dev/video0 -> /dev/video1 (720x576, 50/1 frames/s)
[h264 @ 0x5a87af0d1880] mmco: unref short failure
[h264 @ 0x5a87af0f2380] mmco: unref short failure
[vost#0:0/rawvideo @ 0x5a87af03bb40] Error submitting a packet to the muxer: Invalid argument
[out#0/video4linux2,v4l2 @ 0x5a87aefc3780] Error muxing a packet
gchd-bridge: capture stopped
--- dmesg step 8
[88077.431833] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[88077.562559] elgato_gchd 1-2:1.0: transcoder defaults loaded
[88077.631178] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[88078.182875] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[88078.192377] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[88078.192385] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[88078.192393] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[88078.192400] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[88079.116132] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[88079.429480] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[88079.445411] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[88079.445621] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[88079.450906] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[88079.451615] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[88079.452110] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[88079.591616] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[88084.455507] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[88089.481892] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[88094.508189] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[88099.534756] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=448 bytes=29360128 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[88104.560425] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=563 bytes=36896768 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[88109.585691] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=678 bytes=44433408 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[88114.611213] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=793 bytes=51970048 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[88119.637477] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=908 bytes=59506688 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[88124.662699] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1023 bytes=67043328 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[88129.690357] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1138 bytes=74579968 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[88134.728203] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1253 bytes=82116608 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[88139.590123] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=e CA=ee E7=e9); configuring again
[88139.764941] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1368 bytes=89653248 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[88141.344059] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[88141.344278] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[88141.350022] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[88141.350632] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[88141.351248] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[88141.466831] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[88141.466835] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[88141.466838] elgato_gchd 1-2:1.0: application device: source now 720x480, 60000/1001 frames/s; re-registering
[88143.401497] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[88143.559411] elgato_gchd 1-2:1.0: application device video3: 720x480, 60000/1001 frames/s

=== step 9: bridge stopped
$ kill 97022
$ sh -c pactl list short sources | grep -c elgato_gchd
0
[PASS] audio source removed
--- dmesg step 9


=== summary (T-060, 15634d8 dkms: H.264 device name fits V4L2 (31 characters); sysfs state names the H.264 device; bridge uses it (T-060))
PASS | step 1 driver with three devices | H.264 device name
PASS | step 1 driver with three devices | application device registered, idle, H.264 device named
PASS | step 1 driver with three devices | application device offers YUYV
PASS | step 2 bridge idle: capture device off | bridge running
PASS | step 2 bridge idle: capture device off | application device on composite: 720x480 or 720x576
PASS | step 2 bridge idle: capture device off | capture device off while no application streams
PASS | step 3 first read: source detected (PAL), device registered again with 720x576 | application device now 720x576, 50 frames/s
PASS | step 4 read: 720x576 YUYV, about 50 frames/s | stream YUYV 720x576
FAIL | step 4 read: 720x576 YUYV, about 50 frames/s | frame count >= 450 in 15 s (expected /frame= *([4-9][5-9][0-9]|[5-9][0-9]{2}|[0-9]{4})/, got:   Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn frame=    0 fps=0.0 q=0.0 Lsize=       0kB time=N/A)
PASS | step 4 read: 720x576 YUYV, about 50 frames/s | capture device started
PASS | step 5 application closed: capture device off | hardware shut down after the application stopped
PASS | step 5 application closed: capture device off | state idle
PASS | step 6 audio while capturing | the bridge plays into the audio sink
PASS | step 6 audio while capturing | audio source delivers samples
PASS | step 7 input change on the application device while reading | S-video (no source, NTSC size): device re-registered with 720x480
PASS | step 7 input change on the application device while reading | composite again: 720x576
PASS | step 7 input change on the application device while reading | input composite on the H.264 device too
FAIL | step 8 guvcview | guvcview shows the PS1 picture, smooth, without interlacing lines? -- no, v4l2_core still says it couldn't grab the image
FAIL | step 8 guvcview | guvcview reconnected by itself and shows the N64 picture (720x480)? -- i guess same problem as with PS1
PASS | step 9 bridge stopped | audio source removed
