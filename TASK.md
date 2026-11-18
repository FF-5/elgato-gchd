# TASK T-060 — guvcview, OBS and similar applications

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** application device registered at module load (sizes chosen by the application) + bridge; hardware test
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
7. (Maintainer, after two runs with re-registration) Instead of
   re-registering: the application device is registered while the module
   is loaded (independent of the USB device), and behaves like a usual
   capture device: the application chooses the size from a list, the
   bridge scales to it; source changes do not change the format.

## Implementation

Driver (`src/app.c`, hooks in `core.c`; no protocol change):

- "Elgato Game Capture HD (H.264)": the compressed stream, per connected
  device (the former device, renamed; one name for all models).
- "Elgato Game Capture HD" (YUYV capture) and "Elgato Game Capture HD
  (frame input)" (output) are registered at module load and stay while
  the module is loaded; the first connected Game Capture HD is attached
  (detached on unplug, the next one attached at plug-in).
- Sizes 1920×1080, 1280×720, 720×576, 720×480, 640×480; rates 60,
  59.94, 50, 29.97, 25. While no application has buffers the format
  follows the detected source (720×480 at 59.94 after loading); while
  streaming it stays, a source change queues V4L2_EVENT_SOURCE_CHANGE.
- `VIDIOC_S_INPUT` on the application device switches the attached
  device's input (also while capturing).
- sysfs `/sys/class/video4linux/videoN/gchd_state` of the application
  device: `STATE W H NUM DEN FEED-NODE STREAM-NODE` (`-` without device).
- The frame input treats a buffer with 0 bytes as a whole frame
  (`write()`).
- Driver data lives as long as its `v4l2_device`.

Bridge (`service/gchd-bridge`): while an application streams, ffmpeg
decodes the attached device's stream, deinterlaces one frame per field,
scales to the chosen size, `fps` to the chosen rate, writes YUYV into the
frame input and the audio to the audio source; a black picture while no
Game Capture HD is attached; stops when the application stops (capture
device off); restarts on a format or device change.

Test runner: finds the H.264 device by name (the application device now
gets the first number).

Next steps after the test: systemd user service (start at login),
`install-driver.sh` integration, remembered application format.

## Hardware test (maintainer)

PS1 (PAL) on composite, the N64 (NTSC) for the swap with guvcview open;
one unplug/replug of the Elgato while a read runs. Needs guvcview and
pactl. The test stops a bridge left from earlier runs and unloads
v4l2loopback (no longer used).

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-060.sh > tests/T-060.sh
bash gchd-test.sh T-060 --branch feat/app-bridge
```

Paste the summary; on FAILs the log too (the bridge's own messages are
at the end of it).

## Findings

- Design change (maintainer, 2026-11-18): the re-registration on format
  changes is replaced by an application device registered while the
  module is loaded, with sizes chosen by the application (see Decisions
  7).
- Second application-device run (15634d8): 13 PASS. Working: names,
  sysfs state, capture device off while idle, start on the first read,
  re-registration at the PAL detection (480 → 576; the first reader gets
  ENODEV as planned) and on the input change, 576 again on composite,
  audio stream into the source, capture device off after the reader. No
  frames: every write of the bridge's ffmpeg into the frame input device
  failed with EINVAL. Cause: the kernel's write() emulation queues the
  buffers with a byte count of 0 (its own count is overwritten by the
  V4L2 layer), and the driver accepted only the exact frame size; fixed
  (0 = whole buffer, as for QBUF). guvcview therefore got no image. The
  N64 swap (PAL → NTSC, 240p) re-registered the device as video3, since
  guvcview still had video2 open after 2 s (as a re-plugged camera gets a
  new number while the old one is open); whether guvcview finds it again
  is still to be seen with frames.
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

A few notes and questions:
- is having the device constantly be on consume much resources? Or does it only start consuming once the GCHD is plugged in?
- Can the driver not assign the next freely available device instead of video0? Because if it's fixed it could conflict with another registered device.

The log is:
gchd-test T-060 20261118-190024 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-060-20261118-190024.log
== build
$ git fetch -q origin
$ git checkout -q feat/app-bridge
$ git pull -q --ff-only
commit: f7c1e21 dkms: application device registered at module load, sizes chosen by the application (T-060)
commit: f7c1e21 dkms: application device registered at module load, sizes chosen by the application (T-060)
build rc=0 warnings=0

=== step 1: driver: application device registered at module load
$ sh -c pkill -f 'gchd-bridge run'; sudo modprobe -r v4l2loopback; true
PROMPT: Unplug the Elgato from the PC (or detach it in VirtualBox), wait 3 s, plug it in again.
$ sudo insmod /home/ff5/elgato-gchd/src/elgato_gchd.ko
H.264 device: /dev/video2
module: 0.5.8 
$ v4l2-ctl --list-devices
Elgato Game Capture HD (platform:elgato-gchd):
	/dev/video0
	/dev/video1

Elgato Game Capture HD (platform:v4l2loopback-000):
	/dev/video42

Elgato Game Capture HD (H.264) (usb-0000:00:0c.0-2):
	/dev/video2

$ v4l2-ctl -d /dev/video2 -D
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
[PASS] H.264 device found by name
$ app_state
idle 720 480 60000 1001 video1 video2
[PASS] application device idle, Game Capture HD attached
$ sh -c v4l2-ctl -d /dev/video0 --list-formats-ext | tr '\n' ' '
ioctl: VIDIOC_ENUM_FMT 	Type: Video Capture  	[0]: 'YUYV' (YUYV 4:2:2) 		Size: Discrete 1920x1080 			Interval: Discrete 0.017s (60.000 fps) 			Interval: Discrete 0.017s (59.940 fps) 			Interval: Discrete 0.020s (50.000 fps) 			Interval: Discrete 0.033s (29.970 fps) 			Interval: Discrete 0.040s (25.000 fps) 		Size: Discrete 1280x720 			Interval: Discrete 0.017s (60.000 fps) 			Interval: Discrete 0.017s (59.940 fps) 			Interval: Discrete 0.020s (50.000 fps) 			Interval: Discrete 0.033s (29.970 fps) 			Interval: Discrete 0.040s (25.000 fps) 		Size: Discrete 720x576 			Interval: Discrete 0.017s (60.000 fps) 			Interval: Discrete 0.017s (59.940 fps) 			Interval: Discrete 0.020s (50.000 fps) 			Interval: Discrete 0.033s (29.970 fps) 			Interval: Discrete 0.040s (25.000 fps) 		Size: Discrete 720x480 			Interval: Discrete 0.017s (60.000 fps) 			Interval: Discrete 0.017s (59.940 fps) 			Interval: Discrete 0.020s (50.000 fps) 			Interval: Discrete 0.033s (29.970 fps) 			Interval: Discrete 0.040s (25.000 fps) 		Size: Discrete 640x480 			Interval: Discrete 0.017s (60.000 fps) 			Interval: Discrete 0.017s (59.940 fps) 			Interval: Discrete 0.020s (50.000 fps) 			Interval: Discrete 0.033s (29.970 fps) 			Interval: Discrete 0.040s (25.000 fps) 
[PASS] sizes 1920x1080 ... 640x480 offered
--- dmesg step 1
[89390.726587] usbcore: deregistering interface driver elgato_gchd
[89398.027594] usb 1-2: USB disconnect, device number 87
[89410.164888] usb 1-2: new high-speed USB device number 88 using xhci_hcd
[89410.430741] elgato_gchd: application device video0, frame input video1
[89410.430759] usbcore: registered new interface driver elgato_gchd
[89410.547513] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[89410.547579] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[89410.547584] usb 1-2: Product: GameCapture HD
[89410.547588] usb 1-2: Manufacturer: Elgato
[89410.547591] usb 1-2: SerialNumber: 04107F1267
[89410.551806] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[89410.556361] elgato_gchd 1-2:1.0: attached to the application device video0
[89410.556515] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON

=== step 2: bridge idle: capture device off
PROMPT: PS1 on composite, game running.
$ v4l2-ctl -d /dev/video2 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ bash /home/ff5/elgato-gchd/service/gchd-bridge status
application:    /dev/video0
driver state:   idle 720 480 60000 1001 video1 video2
service:        running (pid 100741)
$ bash /home/ff5/elgato-gchd/service/gchd-bridge status
application:    /dev/video0
driver state:   idle 720 480 60000 1001 video1 video2
service:        running (pid 100741)
[PASS] bridge running
$ sh -c sudo dmesg | grep -c 'starting hardware boot'
0
[PASS] capture device off while no application streams
--- dmesg step 2
[89424.818423] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[89424.818492] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== step 3: first read (format before detection), then the format follows the source
$ cat /home/ff5/gchd-test-logs/T-060-20261118-190024/read1.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 331444 kb/s, 59.94 fps, 59.94 tbr, 1000k tbn
frame=  772 fps= 64 q=-0.0 Lsize=N/A time=00:00:11.97 bitrate=N/A speed=0.996x    
$ cat /home/ff5/gchd-test-logs/T-060-20261118-190024/read1.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 331444 kb/s, 59.94 fps, 59.94 tbr, 1000k tbn
frame=  772 fps= 64 q=-0.0 Lsize=N/A time=00:00:11.97 bitrate=N/A speed=0.996x    
[PASS] frames delivered (>= 200 in 12 s)
$ grep -c No such device /home/ff5/gchd-test-logs/T-060-20261118-190024/read1.txt
0
[PASS] reader not cut off (no 'No such device')
$ app_state
idle 720 576 50 1 video1 video2
$ app_state
idle 720 576 50 1 video1 video2
[PASS] idle format now the PAL source: 720x576, 50 frames/s
$ tail -6 /home/ff5/gchd-test-logs/T-060-20261118-190024/bridge.log
gchd-bridge: capture: /dev/video2 -> /dev/video1 (720x480, 60000/1001 frames/s)
gchd-bridge: capture stopped
--- dmesg step 3
[89428.026441] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[89428.207771] elgato_gchd 1-2:1.0: transcoder defaults loaded
[89428.277347] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[89428.660171] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[89428.663043] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[89428.663044] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[89428.663046] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[89428.663048] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[89429.478615] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[89429.729413] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[89429.749714] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[89429.750013] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[89429.755636] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[89429.756445] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[89429.757252] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[89429.875542] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[89434.783778] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89439.809110] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89444.766973] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 4: read the source format: 720x576 YUYV, about 50 frames/s
$ cat /home/ff5/gchd-test-logs/T-060-20261118-190024/read2.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=  778 fps= 52 q=-0.0 Lsize=N/A time=00:00:14.90 bitrate=N/A speed=0.993x    
$ cat /home/ff5/gchd-test-logs/T-060-20261118-190024/read2.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=  778 fps= 52 q=-0.0 Lsize=N/A time=00:00:14.90 bitrate=N/A speed=0.993x    
[PASS] stream YUYV 720x576
$ cat /home/ff5/gchd-test-logs/T-060-20261118-190024/read2.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=  778 fps= 52 q=-0.0 Lsize=N/A time=00:00:14.90 bitrate=N/A speed=0.993x    
[PASS] frame count >= 450 in 15 s
$ sudo dmesg
[89448.111484] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=101138 comm=v4l2-ctl
[89448.157403] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=101141 comm=ffmpeg
[89448.157455] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[89448.174062] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[89448.174301] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
[89448.241071] elgato_gchd 1-2:1.0: transcoder defaults loaded
[89448.241189] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[89448.241417] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x04
[89448.242860] elgato_gchd 1-2:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.244016] elgato_gchd 1-2:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.245187] elgato_gchd 1-2:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.246365] elgato_gchd 1-2:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.247527] elgato_gchd 1-2:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.248680] elgato_gchd 1-2:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.249793] elgato_gchd 1-2:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.250919] elgato_gchd 1-2:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.252247] elgato_gchd 1-2:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.253653] elgato_gchd 1-2:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.254780] elgato_gchd 1-2:1.0: firmware chunk offset=327680 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.255878] elgato_gchd 1-2:1.0: firmware chunk offset=360448 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.256972] elgato_gchd 1-2:1.0: firmware chunk offset=393216 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.258228] elgato_gchd 1-2:1.0: firmware chunk offset=425984 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.259437] elgato_gchd 1-2:1.0: firmware chunk offset=458752 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.260635] elgato_gchd 1-2:1.0: firmware chunk offset=491520 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.261722] elgato_gchd 1-2:1.0: firmware chunk offset=524288 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.262908] elgato_gchd 1-2:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.264033] elgato_gchd 1-2:1.0: firmware chunk offset=589824 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.265212] elgato_gchd 1-2:1.0: firmware chunk offset=622592 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.266407] elgato_gchd 1-2:1.0: firmware chunk offset=655360 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.267554] elgato_gchd 1-2:1.0: firmware chunk offset=688128 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.268710] elgato_gchd 1-2:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.269908] elgato_gchd 1-2:1.0: firmware chunk offset=753664 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.271230] elgato_gchd 1-2:1.0: firmware chunk offset=786432 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.272587] elgato_gchd 1-2:1.0: firmware chunk offset=819200 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.273873] elgato_gchd 1-2:1.0: firmware chunk offset=851968 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.275112] elgato_gchd 1-2:1.0: firmware chunk offset=884736 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.276413] elgato_gchd 1-2:1.0: firmware chunk offset=917504 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.277699] elgato_gchd 1-2:1.0: firmware chunk offset=950272 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.278861] elgato_gchd 1-2:1.0: firmware chunk offset=983040 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.280246] elgato_gchd 1-2:1.0: firmware chunk offset=1015808 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.281619] elgato_gchd 1-2:1.0: firmware chunk offset=1048576 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.282957] elgato_gchd 1-2:1.0: firmware chunk offset=1081344 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.284324] elgato_gchd 1-2:1.0: firmware chunk offset=1114112 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.285762] elgato_gchd 1-2:1.0: firmware chunk offset=1146880 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.286936] elgato_gchd 1-2:1.0: firmware chunk offset=1179648 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.288334] elgato_gchd 1-2:1.0: firmware chunk offset=1212416 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.289751] elgato_gchd 1-2:1.0: firmware chunk offset=1245184 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.291117] elgato_gchd 1-2:1.0: firmware chunk offset=1277952 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.292453] elgato_gchd 1-2:1.0: firmware chunk offset=1310720 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.293736] elgato_gchd 1-2:1.0: firmware chunk offset=1343488 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.295040] elgato_gchd 1-2:1.0: firmware chunk offset=1376256 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.296531] elgato_gchd 1-2:1.0: firmware chunk offset=1409024 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.297862] elgato_gchd 1-2:1.0: firmware chunk offset=1441792 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.299166] elgato_gchd 1-2:1.0: firmware chunk offset=1474560 size=26224 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[89448.305541] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[89448.411431] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[89448.475790] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x0f mode=0xf
[89448.478600] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0x6e 0x98:F5=0x62
[89448.478628] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[89448.549190] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[89448.569217] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x1f mode=0xf
[89448.571741] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xfe 0x98:F5=0xe2
[89448.571743] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[89448.635162] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x07 mode=0x7
[89448.645220] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[89448.647635] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[89448.647637] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[89448.719698] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[89448.740471] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x07 mode=0x7
[89448.749254] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[89448.751249] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[89448.751269] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[89448.751271] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[89448.751273] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[89449.555351] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[89449.555496] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[89449.816671] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[89449.838381] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[89449.838624] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[89449.838759] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[89449.844467] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[89449.845099] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[89449.845672] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[89449.845816] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[89449.846089] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[89449.846602] elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0150 ack=1
[89449.847454] elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[89449.847835] elgato_gchd 1-2:1.0: START poll=1 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[89449.848170] elgato_gchd 1-2:1.0: START poll=2 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[89449.848609] elgato_gchd 1-2:1.0: START poll=3 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[89449.849003] elgato_gchd 1-2:1.0: START poll=4 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[89449.849325] elgato_gchd 1-2:1.0: START poll=5 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[89449.849638] elgato_gchd 1-2:1.0: START poll=6 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[89449.849967] elgato_gchd 1-2:1.0: START poll=7 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[89449.850307] elgato_gchd 1-2:1.0: START poll=8 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[89449.850643] elgato_gchd 1-2:1.0: START poll=9 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[89449.850982] elgato_gchd 1-2:1.0: START poll=10 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[89449.851325] elgato_gchd 1-2:1.0: START poll=11 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[89449.851655] elgato_gchd 1-2:1.0: START poll=12 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[89449.851978] elgato_gchd 1-2:1.0: START poll=13 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[89449.852324] elgato_gchd 1-2:1.0: START poll=14 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[89449.852652] elgato_gchd 1-2:1.0: START poll=15 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[89449.887070] elgato_gchd 1-2:1.0: START poll=100 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[89449.924744] elgato_gchd 1-2:1.0: START poll=200 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[89449.959973] elgato_gchd 1-2:1.0: START poll=300 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[89449.964426] elgato_gchd 1-2:1.0: START poll=312 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0150 ack=1
[89449.964916] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[89450.361780] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[89450.407567] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[89450.452091] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[89450.497632] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[89450.542801] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[89454.872295] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89459.897431] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89464.924504] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[PASS] capture device started
--- dmesg step 4
[89448.157455] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[89448.241071] elgato_gchd 1-2:1.0: transcoder defaults loaded
[89448.305541] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[89448.411431] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[89448.549190] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[89448.645220] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[89448.647635] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[89448.719698] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[89448.749254] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[89448.751249] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[89448.751269] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[89448.751271] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[89448.751273] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[89449.555351] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[89449.816671] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[89449.838381] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[89449.838759] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[89449.844467] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[89449.845099] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[89449.845672] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[89449.964916] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[89454.872295] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89459.897431] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89464.924504] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 5: application chooses 1280x720: scaled
$ cat /home/ff5/gchd-test-logs/T-060-20261118-190024/read3.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 1280x720, 737280 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=  543 fps= 54 q=-0.0 Lsize=N/A time=00:00:09.96 bitrate=N/A speed=0.995x    
$ cat /home/ff5/gchd-test-logs/T-060-20261118-190024/read3.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 1280x720, 737280 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=  543 fps= 54 q=-0.0 Lsize=N/A time=00:00:09.96 bitrate=N/A speed=0.995x    
[PASS] stream YUYV 1280x720
$ cat /home/ff5/gchd-test-logs/T-060-20261118-190024/read3.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 1280x720, 737280 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=  543 fps= 54 q=-0.0 Lsize=N/A time=00:00:09.96 bitrate=N/A speed=0.995x    
[PASS] frames delivered (>= 250 in 10 s)
--- dmesg step 5
[89467.952519] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[89468.203086] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[89468.203088] elgato_gchd 1-2:1.0: waiting 1264 ms after the previous shutdown
[89469.609319] elgato_gchd 1-2:1.0: transcoder defaults loaded
[89469.672951] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[89469.779149] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[89469.910851] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[89469.912987] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[89469.983300] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[89470.015411] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[89470.017692] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[89470.091081] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[89470.093424] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[89470.093427] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[89470.093430] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[89470.093433] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[89470.855132] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[89471.078410] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[89471.095936] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[89471.096179] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[89471.103259] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[89471.103779] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[89471.104235] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[89471.241781] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[89476.107810] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89481.132230] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 6: application closed: capture device off
$ sudo dmesg
[89483.613792] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[89483.614306] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[89484.302874] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[89484.303776] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[89484.449277] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[89484.450209] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[89484.450600] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
[89484.469910] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[PASS] hardware shut down after the application stopped
$ app_state
idle 720 576 50 1 video1 video2
[PASS] state idle
--- dmesg step 6
[89484.469910] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 7: audio while capturing
$ sink_streams
stream into elgato_gchd: 1576	1531	1575	PipeWire	s16le 2ch 48000Hz
[PASS] the bridge plays into the audio sink
$ ffmpeg -hide_banner -nostdin -f pulse -i elgato_gchd_audio -t 4 -af volumedetect -f null -
[aist#0:0/pcm_s16le @ 0x59dbf72d9640] Guessed Channel Layout: stereo
Input #0, pulse, from 'elgato_gchd_audio':
  Duration: N/A, start: 1795024941.241067, bitrate: 1536 kb/s
  Stream #0:0: Audio: pcm_s16le, 48000 Hz, 2 channels, s16, 1536 kb/s
[Parsed_volumedetect_0 @ 0x59dbf72db000] n_samples: 0
Stream mapping:
  Stream #0:0 -> #0:0 (pcm_s16le (native) -> pcm_s16le (native))
Output #0, null, to 'pipe:':
  Metadata:
    encoder         : Lavf60.16.100
  Stream #0:0: Audio: pcm_s16le, 48000 Hz, stereo, s16, 1536 kb/s
    Metadata:
      encoder         : Lavc60.31.102 pcm_s16le
size=       0kB time=00:00:00.00 bitrate=N/A speed=N/A    
size=N/A time=00:00:00.61 bitrate=N/A speed=1.11x    
size=N/A time=00:00:01.17 bitrate=N/A speed= 1.1x    
size=N/A time=00:00:01.67 bitrate=N/A speed=1.06x    
size=N/A time=00:00:02.16 bitrate=N/A speed=1.04x    
size=N/A time=00:00:02.66 bitrate=N/A speed=1.02x    
size=N/A time=00:00:03.20 bitrate=N/A speed=1.03x    
size=N/A time=00:00:03.70 bitrate=N/A speed=1.02x    
[out#0/null @ 0x59dbf72d6b00] video:0kB audio:748kB subtitle:0kB other streams:0kB global headers:0kB muxing overhead: unknown
size=N/A time=00:00:03.95 bitrate=N/A speed=0.998x    
[Parsed_volumedetect_0 @ 0x59dbf72dda80] n_samples: 388800
[Parsed_volumedetect_0 @ 0x59dbf72dda80] mean_volume: -85.5 dB
[Parsed_volumedetect_0 @ 0x59dbf72dda80] max_volume: -74.7 dB
[Parsed_volumedetect_0 @ 0x59dbf72dda80] histogram_74db: 1
[Parsed_volumedetect_0 @ 0x59dbf72dda80] histogram_75db: 0
[Parsed_volumedetect_0 @ 0x59dbf72dda80] histogram_76db: 80
[Parsed_volumedetect_0 @ 0x59dbf72dda80] histogram_77db: 0
[Parsed_volumedetect_0 @ 0x59dbf72dda80] histogram_78db: 3031
[PASS] audio source delivers samples
--- dmesg step 7
[89489.686337] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[89489.778591] elgato_gchd 1-2:1.0: transcoder defaults loaded
[89489.844082] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[89489.945858] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[89490.008089] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[89490.010503] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[89490.010504] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[89490.010506] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[89490.010508] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[89490.813160] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[89491.054342] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[89491.070389] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[89491.070608] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[89491.076986] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[89491.077474] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[89491.078033] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[89491.196250] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[89496.103146] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89501.129903] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89506.156316] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 8: input change on the application device while reading
$ v4l2-ctl -d /dev/video0 -i 3
Video input set to 3 (S-Video: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ cat /home/ff5/gchd-test-logs/T-060-20261118-190024/read4.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame= 1033 fps= 34 q=-0.0 Lsize=N/A time=00:00:29.96 bitrate=N/A speed=0.999x    
$ grep -c No such device /home/ff5/gchd-test-logs/T-060-20261118-190024/read4.txt
0
[PASS] reader kept the device (no 'No such device')
$ cat /home/ff5/gchd-test-logs/T-060-20261118-190024/read4.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame= 1033 fps= 34 q=-0.0 Lsize=N/A time=00:00:29.96 bitrate=N/A speed=0.999x    
[PASS] reader got frames (>= 600 in 30 s)
$ v4l2-ctl -d /dev/video2 --get-input
Video input : 2 (Composite: ok)
[PASS] input composite on the H.264 device too
$ tail -10 /home/ff5/gchd-test-logs/T-060-20261118-190024/bridge.log
gchd-bridge: capture: /dev/video2 -> /dev/video1 (720x576, 50/1 frames/s)
[h264 @ 0x5845f36c0dc0] mmco: unref short failure
[h264 @ 0x5845f36e18c0] mmco: unref short failure
gchd-bridge: capture stopped
gchd-bridge: capture: /dev/video2 -> /dev/video1 (720x576, 50/1 frames/s)
[h264 @ 0x651b6280b780] mmco: unref short failure
[h264 @ 0x651b6282c380] mmco: unref short failure
[h264 @ 0x651b627073c0] error while decoding MB 19 1, bytestream -6
[mp2 @ 0x651b628403c0] Header missing
[aist#0:1/mp2 @ 0x651b62703140] Error submitting packet to decoder: Invalid data found when processing input
--- dmesg step 8
[89508.112593] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[89509.172395] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[89509.172396] elgato_gchd 1-2:1.0: waiting 441 ms after the previous shutdown
[89509.704928] elgato_gchd 1-2:1.0: transcoder defaults loaded
[89509.767070] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[89509.867962] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[89509.928889] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[89509.931421] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[89509.931424] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[89509.931426] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[89509.931428] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[89510.694551] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[89510.921275] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[89510.936877] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[89510.937077] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[89510.942428] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[89510.943039] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[89510.943477] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[89511.082661] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[89515.947841] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89517.056044] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=1 streaming=1
[89517.893561] elgato_gchd 1-2:1.0: input selected: input=3; detection scheduled
[89518.009318] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[89518.063795] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[89522.940593] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=1 streaming=1
[89522.943688] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[89523.226644] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[89523.414798] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[89523.416959] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[89523.416960] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[89523.416962] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[89523.416964] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[89523.647488] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[89523.662802] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[89523.663014] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[89523.668794] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[89523.669444] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[89523.670050] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[89523.808671] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[89528.681668] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89533.718606] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 9: unplug and replug while reading
PROMPT: Unplug the Elgato (or detach it in VirtualBox) now, wait 5 s, then plug it in again.
$ app_state
idle 720 576 50 1 video1 video2
$ cat /home/ff5/gchd-test-logs/T-060-20261118-190024/read5.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame= 2150 fps= 46 q=-0.0 Lsize=N/A time=00:00:42.94 bitrate=N/A speed=0.925x    
$ grep -c No such device /home/ff5/gchd-test-logs/T-060-20261118-190024/read5.txt
0
[PASS] reader kept the device through unplug/replug
$ cat /home/ff5/gchd-test-logs/T-060-20261118-190024/read5.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame= 2150 fps= 46 q=-0.0 Lsize=N/A time=00:00:42.94 bitrate=N/A speed=0.925x    
[PASS] frames throughout (>= 900 in 45 s)
$ app_state
idle 720 576 50 1 video1 video2
[PASS] Game Capture HD attached again
$ tail -10 /home/ff5/gchd-test-logs/T-060-20261118-190024/bridge.log
[h264 @ 0x651b6280b780] mmco: unref short failure
[h264 @ 0x651b6282c380] mmco: unref short failure
[h264 @ 0x651b627073c0] error while decoding MB 19 1, bytestream -6
[mp2 @ 0x651b628403c0] Header missing
[aist#0:1/mp2 @ 0x651b62703140] Error submitting packet to decoder: Invalid data found when processing input
gchd-bridge: now 720x576, 50/1, stream device -
gchd-bridge: ffmpeg 102041 did not stop; killing it
gchd-bridge: capture stopped
gchd-bridge: no Game Capture HD: black 720x576 -> /dev/video1
gchd-bridge: capture stopped
--- dmesg step 9
[89538.754879] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89543.790759] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=448 bytes=29360128 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89548.828217] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=563 bytes=36896768 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89553.864949] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=678 bytes=44433408 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89558.901195] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=793 bytes=51970048 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89563.937464] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=908 bytes=59506688 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89568.974000] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1023 bytes=67043328 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89574.011224] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1138 bytes=74579968 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89579.047188] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1253 bytes=82116608 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89580.223520] usb 1-2: USB disconnect, device number 88
[89589.129556] usb 1-2: new high-speed USB device number 89 using xhci_hcd
[89589.511269] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[89589.511291] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[89589.511297] usb 1-2: Product: GameCapture HD
[89589.511302] usb 1-2: Manufacturer: Elgato
[89589.511306] usb 1-2: SerialNumber: 04107F1267
[89589.520295] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[89589.525442] elgato_gchd 1-2:1.0: attached to the application device video0
[89589.525466] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON

=== step 10: guvcview
PROMPT: Start in a second terminal: guvcview -d /dev/video0   (leave it open)
[PASS] guvcview shows the PS1 picture, smooth, without interlacing lines?
PROMPT: Swap the PS1 for the N64 (NTSC) while guvcview stays open; wait about 15 s.
$ app_state
streaming 640 480 50 1 video1 video2
[PASS] guvcview shows the N64 picture (in the PAL size, scaled) without reopening? -- it runs at max 50fps
PROMPT: Close guvcview.
$ app_state
idle 640 480 50 1 video1 video2
[FAIL] idle format follows the NTSC source after closing (expected /^idle 720 480 60000 1001 /, got: idle 640 480 50 1 video1 video2)
$ tail -20 /home/ff5/gchd-test-logs/T-060-20261118-190024/bridge.log
[h264 @ 0x651b6280b780] mmco: unref short failure
[h264 @ 0x651b6282c380] mmco: unref short failure
[h264 @ 0x651b627073c0] error while decoding MB 19 1, bytestream -6
[mp2 @ 0x651b628403c0] Header missing
[aist#0:1/mp2 @ 0x651b62703140] Error submitting packet to decoder: Invalid data found when processing input
gchd-bridge: now 720x576, 50/1, stream device -
gchd-bridge: ffmpeg 102041 did not stop; killing it
gchd-bridge: capture stopped
gchd-bridge: no Game Capture HD: black 720x576 -> /dev/video1
gchd-bridge: capture stopped
gchd-bridge: capture: /dev/video2 -> /dev/video1 (640x480, 50/1 frames/s)
[h264 @ 0x57f83e6d8c80] error while decoding MB 3 12, bytestream -15
[mp2 @ 0x57f83e843300] Header missing
[aist#0:1/mp2 @ 0x57f83e693140] Error submitting packet to decoder: Invalid data found when processing input
[h264 @ 0x57f83e730dc0] error while decoding MB 3 20, bytestream -7
[mp2 @ 0x57f83e843300] Header missing
[aist#0:1/mp2 @ 0x57f83e693140] Error submitting packet to decoder: Invalid data found when processing input
[mp2 @ 0x57f83e843300] Header missing
[aist#0:1/mp2 @ 0x57f83e693140] Error submitting packet to decoder: Invalid data found when processing input
gchd-bridge: capture stopped
--- dmesg step 10
[89609.840361] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[89609.963016] elgato_gchd 1-2:1.0: transcoder defaults loaded
[89610.025684] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[89610.445387] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[89610.447585] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[89610.447587] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[89610.447589] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[89610.447591] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[89611.205678] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[89611.404890] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[89611.419657] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[89611.419849] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[89611.425384] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[89611.426093] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[89611.426503] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[89611.565581] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[89611.822525] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[89613.452834] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[89613.453096] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[89613.458600] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[89613.459020] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[89613.459949] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[89613.598957] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[89613.598962] elgato_gchd 1-2:1.0: analog mode now 576i, output 1
[89613.991957] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[89618.464814] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89623.498886] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89628.533321] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 ep84_bytes=21823488 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89633.568296] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=448 bytes=29360128 ep84_bytes=29360128 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89638.602826] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=563 bytes=36896768 ep84_bytes=36896768 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89643.636609] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=678 bytes=44433408 ep84_bytes=44433408 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89648.671610] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=793 bytes=51970048 ep84_bytes=51970048 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89653.708524] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=908 bytes=59506688 ep84_bytes=59506688 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89656.841034] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=e CA=ee E7=d9); configuring again
[89658.598640] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[89658.598872] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[89658.604508] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[89658.605401] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[89658.606079] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[89658.721279] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[89658.721284] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[89659.090076] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[89663.605575] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89668.638588] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89673.665802] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=21889024 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89678.690388] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=29425664 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89683.722805] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=36962304 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89685.396583] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[89686.706296] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[89686.706524] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[89686.712206] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[89686.712759] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[89686.713437] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[89686.828933] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[89686.828936] elgato_gchd 1-2:1.0: analog mode now 240p, output 1
[89687.202020] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[89691.717427] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89696.751633] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89701.782442] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=21889024 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89706.814523] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=29425664 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89711.846346] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=36962304 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89716.393721] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[89716.877325] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=679 bytes=44498944 ep84_bytes=44498944 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89717.690760] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[89717.690968] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[89717.696061] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[89717.696821] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[89717.697306] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[89717.813303] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[89717.813307] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[89718.183977] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[89722.695870] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89727.728954] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89732.761802] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=21889024 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89737.791347] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=29425664 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89742.823561] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=36962304 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[89747.022518] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 11: bridge stopped
$ kill 100741
$ sh -c pactl list short sources | grep -c elgato_gchd
0
[PASS] audio source removed
--- dmesg step 11


=== summary (T-060, f7c1e21 dkms: application device registered at module load, sizes chosen by the application (T-060))
PASS | step 1 driver: application device registered at module load | H.264 device found by name
PASS | step 1 driver: application device registered at module load | application device idle, Game Capture HD attached
PASS | step 1 driver: application device registered at module load | sizes 1920x1080 ... 640x480 offered
PASS | step 2 bridge idle: capture device off | bridge running
PASS | step 2 bridge idle: capture device off | capture device off while no application streams
PASS | step 3 first read (format before detection), then the format follows the source | frames delivered (>= 200 in 12 s)
PASS | step 3 first read (format before detection), then the format follows the source | reader not cut off (no 'No such device')
PASS | step 3 first read (format before detection), then the format follows the source | idle format now the PAL source: 720x576, 50 frames/s
PASS | step 4 read the source format: 720x576 YUYV, about 50 frames/s | stream YUYV 720x576
PASS | step 4 read the source format: 720x576 YUYV, about 50 frames/s | frame count >= 450 in 15 s
PASS | step 4 read the source format: 720x576 YUYV, about 50 frames/s | capture device started
PASS | step 5 application chooses 1280x720: scaled | stream YUYV 1280x720
PASS | step 5 application chooses 1280x720: scaled | frames delivered (>= 250 in 10 s)
PASS | step 6 application closed: capture device off | hardware shut down after the application stopped
PASS | step 6 application closed: capture device off | state idle
PASS | step 7 audio while capturing | the bridge plays into the audio sink
PASS | step 7 audio while capturing | audio source delivers samples
PASS | step 8 input change on the application device while reading | reader kept the device (no 'No such device')
PASS | step 8 input change on the application device while reading | reader got frames (>= 600 in 30 s)
PASS | step 8 input change on the application device while reading | input composite on the H.264 device too
PASS | step 9 unplug and replug while reading | reader kept the device through unplug/replug
PASS | step 9 unplug and replug while reading | frames throughout (>= 900 in 45 s)
PASS | step 9 unplug and replug while reading | Game Capture HD attached again
PASS | step 10 guvcview | guvcview shows the PS1 picture, smooth, without interlacing lines?
PASS | step 10 guvcview | guvcview shows the N64 picture (in the PAL size, scaled) without reopening? -- it runs at max 50fps
FAIL | step 10 guvcview | idle format follows the NTSC source after closing (expected /^idle 720 480 60000 1001 /, got: idle 640 480 50 1 video1 video2)
PASS | step 11 bridge stopped | audio source removed
