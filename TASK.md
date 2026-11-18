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

- Persistent application device, second run (0ca508b): 27/27 PASS (the
  format follows the source again after guvcview closes).
- Maintainer (2026-11-18): (1) only the device should be claimed while
  idle; work only when a program opens it. The driver already does
  nothing while idle (no timer, thread or USB traffic); the bridge polled
  a sysfs file every 0.5 s. Now the driver sends a "change" uevent with
  each state change and the bridge sleeps on `udevadm monitor` (at most
  30 s idle, 1 s while ffmpeg runs to notice its end). (2) a picture
  "Please connect the Elgato Game Capture HD" while no device is
  attached: drawn by the bridge (ffmpeg drawtext; plain black without
  it). Both in the test (idle CPU of the bridge over 20 s; guvcview with
  the Elgato unplugged).
- First run of the persistent application device (f7c1e21): 26/27 PASS.
  Device registered at load (video0, frame input video1, H.264 video2),
  capture device off while idle, frames from the first read (772 in
  12 s at 59.94 before detection), idle format followed the PAL source
  (720×576, 50), 778 frames in 15 s at 720×576, 1280×720 scaled (543 in
  10 s), shutdown after the reader, audio stream, input change and
  unplug/replug while reading without losing the device (black picture
  without the Game Capture HD, attached again after the replug),
  guvcview: PS1 picture smooth, N64 after the swap without reopening
  (maintainer: "runs at max 50fps": guvcview had opened the device at
  640×480, 50 frames/s during the PAL source, and the rate stays while
  streaming). FAIL: after closing guvcview the format stayed 640×480/50:
  guvcview stops streaming before it frees the buffers, and the source
  was looked up only at the stop; now also after REQBUFS(0) and the last
  close.
- Maintainer questions (2026-11-18): (1) resources of the persistent
  device: only memory for two device structures while idle (no buffers,
  no USB traffic, no thread); buffers are allocated when an application
  requests them; the bridge reads one sysfs file every 0.5 s. The module
  is loaded when the Game Capture HD is plugged in (or by the installer)
  and stays loaded. (2) device number: the driver asks for the first free
  number (not a fixed video0); it got video0 because it was free when the
  module was loaded. A module parameter for a start number is possible
  (suggested to the maintainer).
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

One more thing I forgot to mention. I don't want any output scaling, I want a 1:1 output resolution. Tell me otherwise why scaling is better.

Here are the logs:
gchd-test T-060 20261118-193802 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-060-20261118-193802.log
== build
$ git fetch -q origin
$ git checkout -q feat/app-bridge
$ git pull -q --ff-only
commit: 3b53941 dkms: change uevent on application-device state changes; bridge sleeps on uevents, 'Please connect' picture (T-060)
commit: 3b53941 dkms: change uevent on application-device state changes; bridge sleeps on uevents, 'Please connect' picture (T-060)
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
[91645.715427] usbcore: deregistering interface driver elgato_gchd
[91647.721059] usb 1-2: USB disconnect, device number 92
[91653.535031] elgato_gchd: application device video0, frame input video1
[91653.535055] usbcore: registered new interface driver elgato_gchd
[91653.651014] usb 1-2: new high-speed USB device number 93 using xhci_hcd
[91654.036728] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[91654.036749] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[91654.036755] usb 1-2: Product: GameCapture HD
[91654.036760] usb 1-2: Manufacturer: Elgato
[91654.036764] usb 1-2: SerialNumber: 04107F1267
[91654.042896] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[91654.047454] elgato_gchd 1-2:1.0: attached to the application device video0
[91654.047475] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON

=== step 2: bridge idle: capture device off
PROMPT: PS1 on composite, game running.
$ v4l2-ctl -d /dev/video2 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ bash /home/ff5/elgato-gchd/service/gchd-bridge status
application:    /dev/video0
driver state:   idle 720 480 60000 1001 video1 video2
service:        running (pid 110395)
$ bash /home/ff5/elgato-gchd/service/gchd-bridge status
application:    /dev/video0
driver state:   idle 720 480 60000 1001 video1 video2
service:        running (pid 110395)
[PASS] bridge running
$ sh -c sudo dmesg | grep -c 'starting hardware boot'
0
[PASS] capture device off while no application streams
bridge CPU while idle: 0 ticks in 20 s
$ echo 0
0
[PASS] bridge sleeps while idle (< 5 ticks = 0.05 s CPU in 20 s)
--- dmesg step 2
[91656.715556] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[91656.715633] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== step 3: first read (format before detection), then the format follows the source
$ cat /home/ff5/gchd-test-logs/T-060-20261118-193802/read1.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 331444 kb/s, 59.94 fps, 59.94 tbr, 1000k tbn
frame=  760 fps= 63 q=-0.0 Lsize=N/A time=00:00:11.96 bitrate=N/A speed=0.996x    
$ cat /home/ff5/gchd-test-logs/T-060-20261118-193802/read1.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 331444 kb/s, 59.94 fps, 59.94 tbr, 1000k tbn
frame=  760 fps= 63 q=-0.0 Lsize=N/A time=00:00:11.96 bitrate=N/A speed=0.996x    
[PASS] frames delivered (>= 200 in 12 s)
$ grep -c No such device /home/ff5/gchd-test-logs/T-060-20261118-193802/read1.txt
0
[PASS] reader not cut off (no 'No such device')
$ app_state
idle 720 576 50 1 video1 video2
$ app_state
idle 720 576 50 1 video1 video2
[PASS] idle format now the PAL source: 720x576, 50 frames/s
$ tail -6 /home/ff5/gchd-test-logs/T-060-20261118-193802/bridge.log
gchd-bridge: capture: /dev/video2 -> /dev/video1 (720x480, 60000/1001 frames/s)
--- dmesg step 3
[91679.942283] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[91680.044713] elgato_gchd 1-2:1.0: transcoder defaults loaded
[91680.106391] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[91680.471944] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[91680.474263] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[91680.474266] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[91680.474268] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[91680.474270] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[91681.240808] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[91681.461834] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[91681.481185] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[91681.481418] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[91681.487255] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[91681.488039] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[91681.489240] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[91681.608269] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[91686.514543] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91691.540682] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91696.568176] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 4: read the source format: 720x576 YUYV, about 50 frames/s
$ cat /home/ff5/gchd-test-logs/T-060-20261118-193802/read2.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=    0 fps=0.0 q=0.0 Lsize=       0kB time=N/A bitrate=N/A speed=N/A    
$ cat /home/ff5/gchd-test-logs/T-060-20261118-193802/read2.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=    0 fps=0.0 q=0.0 Lsize=       0kB time=N/A bitrate=N/A speed=N/A    
[PASS] stream YUYV 720x576
$ cat /home/ff5/gchd-test-logs/T-060-20261118-193802/read2.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=    0 fps=0.0 q=0.0 Lsize=       0kB time=N/A bitrate=N/A speed=N/A    
[FAIL] frame count >= 450 in 15 s (expected /frame= *(4[5-9][0-9]|[5-9][0-9]{2}|[0-9]{4})/, got:   Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn frame=    0 fps=0.0 q=0.0 Lsize=       0kB time=N/A)
$ sudo dmesg
[91701.593688] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91706.620676] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91711.645506] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=679 bytes=44498944 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91716.671193] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=794 bytes=52035584 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91721.697242] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=909 bytes=59572224 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91726.724327] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1024 bytes=67108864 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91731.749952] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1139 bytes=74645504 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[FAIL] capture device started (expected /starting hardware boot/, got: [91701.593688] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pendin)
--- dmesg step 4
[91701.593688] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91706.620676] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91711.645506] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=679 bytes=44498944 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91716.671193] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=794 bytes=52035584 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91721.697242] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=909 bytes=59572224 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91726.724327] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1024 bytes=67108864 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91731.749952] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1139 bytes=74645504 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 5: application chooses 1280x720: scaled
$ cat /home/ff5/gchd-test-logs/T-060-20261118-193802/read3.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 1280x720, 737280 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=    0 fps=0.0 q=0.0 Lsize=       0kB time=N/A bitrate=N/A speed=N/A    
$ cat /home/ff5/gchd-test-logs/T-060-20261118-193802/read3.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 1280x720, 737280 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=    0 fps=0.0 q=0.0 Lsize=       0kB time=N/A bitrate=N/A speed=N/A    
[PASS] stream YUYV 1280x720
$ cat /home/ff5/gchd-test-logs/T-060-20261118-193802/read3.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 1280x720, 737280 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=    0 fps=0.0 q=0.0 Lsize=       0kB time=N/A bitrate=N/A speed=N/A    
[FAIL] frames delivered (>= 250 in 10 s) (expected /frame= *([2-9][5-9][0-9]|[3-9][0-9]{2}|[0-9]{4})/, got:   Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 1280x720, 737280 kb/s, 50 fps, 50 tbr, 1000k tbn frame=    0 fps=0.0 q=0.0 Lsize=       0kB time=N/)
--- dmesg step 5
[91736.775056] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1254 bytes=82182144 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91741.801814] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1369 bytes=89718784 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91746.827002] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1484 bytes=97255424 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91751.854140] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1599 bytes=104792064 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91756.879637] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1714 bytes=112328704 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91761.905867] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1829 bytes=119865344 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 6: application closed: capture device off
$ sudo dmesg
[91766.932291] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1944 bytes=127401984 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[FAIL] hardware shut down after the application stopped (expected /hardware shutdown done/, got: [91766.932291] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1944 bytes=127401984 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pend)
$ app_state
idle 720 576 50 1 video1 video2
[PASS] state idle
--- dmesg step 6
[91766.932291] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1944 bytes=127401984 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 7: audio while capturing
$ sink_streams
stream into elgato_gchd: 1770	1729	1769	PipeWire	s16le 2ch 48000Hz
[PASS] the bridge plays into the audio sink
$ ffmpeg -hide_banner -nostdin -f pulse -i elgato_gchd_audio -t 4 -af volumedetect -f null -
[aist#0:0/pcm_s16le @ 0x633f860a7640] Guessed Channel Layout: stereo
Input #0, pulse, from 'elgato_gchd_audio':
  Duration: N/A, start: 1795027222.582607, bitrate: 1536 kb/s
  Stream #0:0: Audio: pcm_s16le, 48000 Hz, 2 channels, s16, 1536 kb/s
[Parsed_volumedetect_0 @ 0x633f860a9000] n_samples: 0
Stream mapping:
  Stream #0:0 -> #0:0 (pcm_s16le (native) -> pcm_s16le (native))
Output #0, null, to 'pipe:':
  Metadata:
    encoder         : Lavf60.16.100
  Stream #0:0: Audio: pcm_s16le, 48000 Hz, stereo, s16, 1536 kb/s
    Metadata:
      encoder         : Lavc60.31.102 pcm_s16le
size=       0kB time=00:00:00.00 bitrate=N/A speed=N/A    
size=N/A time=00:00:00.57 bitrate=N/A speed=1.04x    
size=N/A time=00:00:01.08 bitrate=N/A speed=1.02x    
size=N/A time=00:00:01.56 bitrate=N/A speed=0.993x    
size=N/A time=00:00:02.06 bitrate=N/A speed=0.986x    
size=N/A time=00:00:02.56 bitrate=N/A speed=0.985x    
size=N/A time=00:00:03.12 bitrate=N/A speed=   1x    
size=N/A time=00:00:03.62 bitrate=N/A speed=   1x    
[out#0/null @ 0x633f860a4b00] video:0kB audio:763kB subtitle:0kB other streams:0kB global headers:0kB muxing overhead: unknown
size=N/A time=00:00:03.98 bitrate=N/A speed=0.972x    
[Parsed_volumedetect_0 @ 0x633f860aba80] n_samples: 398400
[Parsed_volumedetect_0 @ 0x633f860aba80] mean_volume: -85.5 dB
[Parsed_volumedetect_0 @ 0x633f860aba80] max_volume: -76.3 dB
[Parsed_volumedetect_0 @ 0x633f860aba80] histogram_76db: 70
[Parsed_volumedetect_0 @ 0x633f860aba80] histogram_77db: 0
[Parsed_volumedetect_0 @ 0x633f860aba80] histogram_78db: 3246
[PASS] audio source delivers samples
--- dmesg step 7
[91771.958624] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2059 bytes=134938624 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91776.984154] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2174 bytes=142475264 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91782.010621] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2289 bytes=150011904 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91787.035835] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2404 bytes=157548544 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91792.061256] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2519 bytes=165085184 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91797.088116] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2634 bytes=172621824 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91802.115257] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2749 bytes=180158464 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 8: input change on the application device while reading
$ v4l2-ctl -d /dev/video0 -i 3
Video input set to 3 (S-Video: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ cat /home/ff5/gchd-test-logs/T-060-20261118-193802/read4.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame= 1533 fps= 51 q=-0.0 Lsize=N/A time=00:00:29.94 bitrate=N/A speed=0.997x    
$ grep -c No such device /home/ff5/gchd-test-logs/T-060-20261118-193802/read4.txt
0
[PASS] reader kept the device (no 'No such device')
$ cat /home/ff5/gchd-test-logs/T-060-20261118-193802/read4.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame= 1533 fps= 51 q=-0.0 Lsize=N/A time=00:00:29.94 bitrate=N/A speed=0.997x    
[PASS] reader got frames (>= 600 in 30 s)
$ v4l2-ctl -d /dev/video2 --get-input
Video input : 2 (Composite: ok)
[PASS] input composite on the H.264 device too
$ tail -10 /home/ff5/gchd-test-logs/T-060-20261118-193802/bridge.log
gchd-bridge: capture: /dev/video2 -> /dev/video1 (720x480, 60000/1001 frames/s)
gchd-bridge: now 720x576, 50/1, stream device video2
gchd-bridge: capture stopped
gchd-bridge: capture: /dev/video2 -> /dev/video1 (720x576, 50/1 frames/s)
--- dmesg step 8
[91807.141530] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2864 bytes=187695104 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91812.167281] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2979 bytes=195231744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91814.721853] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=1 streaming=1
[91815.584740] elgato_gchd 1-2:1.0: input selected: input=3; detection scheduled
[91815.616087] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[91815.830789] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[91815.830790] elgato_gchd 1-2:1.0: waiting 1288 ms after the previous shutdown
[91817.281107] elgato_gchd 1-2:1.0: transcoder defaults loaded
[91817.347379] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[91817.456220] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[91817.564935] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[91820.600206] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=1 streaming=1
[91820.601034] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[91820.850032] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[91820.985140] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[91820.987883] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[91820.987887] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[91820.987891] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[91820.987925] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[91821.776801] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[91821.981867] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[91822.005257] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[91822.005675] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[91822.014423] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[91822.015417] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[91822.016247] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[91822.131687] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[91827.037936] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91832.064118] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91837.090764] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91842.117323] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91847.142890] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91852.169362] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=679 bytes=44498944 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 9: unplug and replug while reading
PROMPT: Unplug the Elgato (or detach it in VirtualBox) now, wait 5 s, then plug it in again.
$ app_state
streaming 720 576 50 1 video1 video2
$ cat /home/ff5/gchd-test-logs/T-060-20261118-193802/read5.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame= 1951 fps= 43 q=-0.0 Lsize=N/A time=00:00:44.96 bitrate=N/A speed=0.999x    
$ grep -c No such device /home/ff5/gchd-test-logs/T-060-20261118-193802/read5.txt
0
[PASS] reader kept the device through unplug/replug
$ cat /home/ff5/gchd-test-logs/T-060-20261118-193802/read5.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame= 1951 fps= 43 q=-0.0 Lsize=N/A time=00:00:44.96 bitrate=N/A speed=0.999x    
[PASS] frames throughout (>= 900 in 45 s)
$ app_state
idle 720 576 50 1 video1 video2
[PASS] Game Capture HD attached again
$ tail -10 /home/ff5/gchd-test-logs/T-060-20261118-193802/bridge.log
gchd-bridge: now 720x576, 50/1, stream device video2
gchd-bridge: capture stopped
gchd-bridge: capture: /dev/video2 -> /dev/video1 (720x576, 50/1 frames/s)
gchd-bridge: now 720x576, 50/1, stream device -
gchd-bridge: ffmpeg 182104 did not stop; killing it
gchd-bridge: capture stopped
gchd-bridge: no Game Capture HD: picture 720x576 -> /dev/video1
gchd-bridge: now 720x576, 50/1, stream device video2
gchd-bridge: capture stopped
gchd-bridge: capture: /dev/video2 -> /dev/video1 (720x576, 50/1 frames/s)
--- dmesg step 9
[91857.195515] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=794 bytes=52035584 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91862.220939] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=909 bytes=59572224 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91867.247806] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1024 bytes=67108864 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91868.985068] usb 1-2: USB disconnect, device number 93
[91876.243961] usb 1-2: new high-speed USB device number 94 using xhci_hcd
[91876.623027] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[91876.623038] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[91876.623042] usb 1-2: Product: GameCapture HD
[91876.623045] usb 1-2: Manufacturer: Elgato
[91876.623047] usb 1-2: SerialNumber: 04107F1267
[91876.628048] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[91876.630582] elgato_gchd 1-2:1.0: attached to the application device video0
[91876.630601] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[91877.246708] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[91877.387025] elgato_gchd 1-2:1.0: transcoder defaults loaded
[91877.454623] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[91877.856060] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[91877.858171] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[91877.858173] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[91877.858175] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[91877.858177] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[91878.648388] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[91878.908309] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[91878.923317] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[91878.923601] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[91878.929547] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[91878.930211] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[91878.930721] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[91879.069508] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[91883.932264] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91888.957583] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91893.984268] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91899.009351] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=448 bytes=29360128 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 10: guvcview
PROMPT: Start in a second terminal: guvcview -d /dev/video0   (leave it open)
[FAIL] guvcview shows the PS1 picture, smooth, without interlacing lines? -- it says can't find parent usb device
PROMPT: Swap the PS1 for the N64 (NTSC) while guvcview stays open; wait about 15 s.
$ app_state
streaming 640 480 50 1 video1 video2
[PASS] guvcview shows the N64 picture (in the PAL size, scaled) without reopening? -- it took a while for the image to appear
PROMPT: Unplug the Elgato while guvcview stays open; wait about 5 s.
[PASS] guvcview shows 'Please connect the Elgato Game Capture HD'?
PROMPT: Plug the Elgato in again; wait about 15 s.
[PASS] guvcview shows the N64 picture again?
PROMPT: Close guvcview.
$ app_state
idle 720 480 60000 1001 video1 video2
[PASS] idle format follows the NTSC source after closing
$ tail -20 /home/ff5/gchd-test-logs/T-060-20261118-193802/bridge.log
[aist#0:1/mp2 @ 0x60f2319cfd00] Error submitting packet to decoder: Invalid data found when processing input
gchd-bridge: now 640x480, 50/1, stream device -
gchd-bridge: ffmpeg 285386 did not stop; killing it
gchd-bridge: capture stopped
gchd-bridge: no Game Capture HD: picture 640x480 -> /dev/video1
gchd-bridge: now 640x480, 50/1, stream device video2
gchd-bridge: capture stopped
gchd-bridge: capture: /dev/video2 -> /dev/video1 (640x480, 50/1 frames/s)
[h264 @ 0x624d02bf26c0] co located POCs unavailable
gchd-bridge: now 640x480, 50/1, stream device -
gchd-bridge: ffmpeg 318837 did not stop; killing it
gchd-bridge: capture stopped
gchd-bridge: no Game Capture HD: picture 640x480 -> /dev/video1
gchd-bridge: now 640x480, 50/1, stream device video2
gchd-bridge: capture stopped
gchd-bridge: capture: /dev/video2 -> /dev/video1 (640x480, 50/1 frames/s)
[h264 @ 0x5a8af91e37c0] co located POCs unavailable
[h264 @ 0x5a8af91a4840] error while decoding MB 17 24, bytestream -13
[mp2 @ 0x5a8af92a7b40] Header missing
[aist#0:1/mp2 @ 0x5a8af92a7880] Error submitting packet to decoder: Invalid data found when processing input
--- dmesg step 10
[91904.036246] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=563 bytes=36896768 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91909.063143] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=678 bytes=44433408 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91914.089247] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=793 bytes=51970048 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91919.114253] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=908 bytes=59506688 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91924.140700] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1023 bytes=67043328 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91929.165232] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1138 bytes=74579968 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91934.191271] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1253 bytes=82116608 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91939.217378] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1368 bytes=89653248 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91944.242059] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1483 bytes=97189888 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91949.266813] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1598 bytes=104726528 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91954.294847] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1713 bytes=112263168 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91959.320982] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1828 bytes=119799808 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91964.346396] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1943 bytes=127336448 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91969.372812] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2058 bytes=134873088 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91974.397768] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2173 bytes=142409728 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91979.425041] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2288 bytes=149946368 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91984.449832] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2403 bytes=157483008 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91989.477749] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2518 bytes=165019648 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91994.503513] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2633 bytes=172556288 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[91999.530289] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2748 bytes=180092928 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[92004.560033] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2863 bytes=187629568 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[92009.598718] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2978 bytes=195166208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[92009.832923] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=6 CA=ee E7=e9); configuring again
[92011.608205] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[92012.705860] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[92012.706498] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[92012.714746] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[92012.715358] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[92012.715919] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[92012.812751] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[92012.812754] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[92014.947597] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[92015.007842] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[92015.007844] elgato_gchd 1-2:1.0: waiting 1441 ms after the previous shutdown
[92016.599178] elgato_gchd 1-2:1.0: transcoder defaults loaded
[92016.675072] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[92016.828053] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[92016.982805] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[92017.009276] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[92017.011309] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[92017.078334] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[92017.110346] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x26 masked_mode=0x6
[92017.112419] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[92017.112422] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[92017.112423] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[92017.112425] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[92017.888108] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[92018.015871] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[92018.041213] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[92018.041556] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[92018.068455] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[92018.069592] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[92018.070308] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[92018.180933] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[92018.433910] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[92020.104934] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[92020.105314] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[92020.111755] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[92020.113024] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[92020.113770] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[92020.227852] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[92020.227856] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[92020.600935] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[92025.110890] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[92030.143822] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[92033.530482] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[92034.983371] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[92034.983706] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[92034.990747] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[92034.991315] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[92034.991871] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[92035.107243] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[92035.107247] elgato_gchd 1-2:1.0: analog mode now 240p, output 1
[92035.478253] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[92039.994301] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[92045.027448] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[92050.059738] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=21889024 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[92055.091098] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=29425664 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[92060.123232] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=36962304 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[92064.674003] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[92065.154946] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=679 bytes=44498944 ep84_bytes=44498944 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[92066.279041] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[92066.279413] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[92066.288098] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[92066.289108] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[92066.290965] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[92066.384998] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[92066.385001] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[92066.753485] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[92071.313678] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[92076.346313] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=14417920 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[92079.678075] usb 1-2: USB disconnect, device number 94
[92079.678168] elgato_gchd 1-2:1.0: bulk-IN resubmit failed: ep=0x81 error=-19
[92087.854046] usb 1-2: new high-speed USB device number 95 using xhci_hcd
[92088.232231] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[92088.232238] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[92088.232239] usb 1-2: Product: GameCapture HD
[92088.232240] usb 1-2: Manufacturer: Elgato
[92088.232245] usb 1-2: SerialNumber: 04107F1267
[92088.240944] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[92088.241988] elgato_gchd 1-2:1.0: attached to the application device video0
[92088.241998] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[92088.862277] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[92088.996971] elgato_gchd 1-2:1.0: transcoder defaults loaded
[92089.063167] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[92089.458053] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[92089.460293] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[92089.460299] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[92089.460303] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[92089.460306] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[92090.279813] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[92090.428957] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[92090.446095] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[92090.446330] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[92090.471095] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[92090.471620] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[92090.472262] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[92090.585388] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[92092.380465] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[92092.406826] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[92092.408948] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[92092.408952] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[92092.408954] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[92092.408956] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[92092.649844] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[92092.666770] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[92092.667088] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[92092.674462] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[92092.674960] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[92092.675435] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[92092.789538] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[92092.789544] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[92094.593105] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[92094.593343] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[92094.598834] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[92094.599433] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[92094.599918] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[92094.715726] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[92094.715729] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[92095.085535] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[92097.660429] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[92098.941744] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[92098.942261] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[92098.950209] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[92098.951306] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[92098.951852] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[92099.064959] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[92099.064963] elgato_gchd 1-2:1.0: analog mode now 240p, output 1
[92099.436393] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[92103.951892] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[92108.985507] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[92114.017967] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=21889024 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[92116.902843] usb 1-2: USB disconnect, device number 95
[92126.174941] usb 1-2: new high-speed USB device number 96 using xhci_hcd
[92126.551615] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[92126.551624] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[92126.551626] usb 1-2: Product: GameCapture HD
[92126.551627] usb 1-2: Manufacturer: Elgato
[92126.551628] usb 1-2: SerialNumber: 04107F1267
[92126.556442] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[92126.557450] elgato_gchd 1-2:1.0: attached to the application device video0
[92126.557461] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[92127.175175] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[92127.938665] elgato_gchd 1-2:1.0: state 0x11 reached without completion flag (completion=0x0008); accepted
[92127.975880] elgato_gchd 1-2:1.0: transcoder defaults loaded
[92128.045511] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[92128.416980] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[92128.484501] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[92128.486951] elgato_gchd 1-2:1.0: analog format status 0x76 not locked yet
[92128.499868] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[92128.501873] elgato_gchd 1-2:1.0: analog format status 0x76 not locked yet
[92128.569899] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[92128.600019] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[92128.602141] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[92128.602143] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[92128.602145] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[92128.602147] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[92129.390304] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[92129.537023] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[92129.556080] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[92129.556632] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[92129.581115] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[92129.581688] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[92129.582284] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[92129.728885] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[92129.985016] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[92131.669595] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[92131.669998] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[92131.676157] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[92131.676957] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[92131.677685] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[92131.792406] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[92131.792410] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[92132.160787] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[92136.675999] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[92141.707953] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[92146.737033] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=21889024 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[92151.761378] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=29425664 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[92156.793501] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=36962304 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[92161.743603] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[92161.821740] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=679 bytes=44498944 ep84_bytes=44498944 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[92163.289096] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[92163.289354] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[92163.294696] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[92163.295531] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[92163.296129] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[92163.395340] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[92163.395344] elgato_gchd 1-2:1.0: analog mode now 240p, output 1
[92163.767166] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[92168.325133] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[92173.359084] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=14417920 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[WARN] dmesg: elgato_gchd 1-2:1.0: bulk-IN resubmit failed: ep=0x81 error=-19

=== step 11: bridge stopped
$ kill 110395
$ sh -c pactl list short sources | grep -c elgato_gchd
2
[FAIL] audio source removed (expected /^0$/, got: 2)
--- dmesg step 11
[92178.374921] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=328 bytes=21495808 ep84_bytes=21495808 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=11
[92178.431569] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== summary (T-060, 3b53941 dkms: change uevent on application-device state changes; bridge sleeps on uevents, 'Please connect' picture (T-060))
PASS | step 1 driver: application device registered at module load | H.264 device found by name
PASS | step 1 driver: application device registered at module load | application device idle, Game Capture HD attached
PASS | step 1 driver: application device registered at module load | sizes 1920x1080 ... 640x480 offered
PASS | step 2 bridge idle: capture device off | bridge running
PASS | step 2 bridge idle: capture device off | capture device off while no application streams
PASS | step 2 bridge idle: capture device off | bridge sleeps while idle (< 5 ticks = 0.05 s CPU in 20 s)
PASS | step 3 first read (format before detection), then the format follows the source | frames delivered (>= 200 in 12 s)
PASS | step 3 first read (format before detection), then the format follows the source | reader not cut off (no 'No such device')
PASS | step 3 first read (format before detection), then the format follows the source | idle format now the PAL source: 720x576, 50 frames/s
PASS | step 4 read the source format: 720x576 YUYV, about 50 frames/s | stream YUYV 720x576
FAIL | step 4 read the source format: 720x576 YUYV, about 50 frames/s | frame count >= 450 in 15 s (expected /frame= *(4[5-9][0-9]|[5-9][0-9]{2}|[0-9]{4})/, got:   Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn frame=    0 fps=0.0 q=0.0 Lsize=       0kB time=N/A)
FAIL | step 4 read the source format: 720x576 YUYV, about 50 frames/s | capture device started (expected /starting hardware boot/, got: [91701.593688] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pendin)
PASS | step 5 application chooses 1280x720: scaled | stream YUYV 1280x720
FAIL | step 5 application chooses 1280x720: scaled | frames delivered (>= 250 in 10 s) (expected /frame= *([2-9][5-9][0-9]|[3-9][0-9]{2}|[0-9]{4})/, got:   Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 1280x720, 737280 kb/s, 50 fps, 50 tbr, 1000k tbn frame=    0 fps=0.0 q=0.0 Lsize=       0kB time=N/)
FAIL | step 6 application closed: capture device off | hardware shut down after the application stopped (expected /hardware shutdown done/, got: [91766.932291] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1944 bytes=127401984 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pend)
PASS | step 6 application closed: capture device off | state idle
PASS | step 7 audio while capturing | the bridge plays into the audio sink
PASS | step 7 audio while capturing | audio source delivers samples
PASS | step 8 input change on the application device while reading | reader kept the device (no 'No such device')
PASS | step 8 input change on the application device while reading | reader got frames (>= 600 in 30 s)
PASS | step 8 input change on the application device while reading | input composite on the H.264 device too
PASS | step 9 unplug and replug while reading | reader kept the device through unplug/replug
PASS | step 9 unplug and replug while reading | frames throughout (>= 900 in 45 s)
PASS | step 9 unplug and replug while reading | Game Capture HD attached again
FAIL | step 10 guvcview | guvcview shows the PS1 picture, smooth, without interlacing lines? -- it says can't find parent usb device
PASS | step 10 guvcview | guvcview shows the N64 picture (in the PAL size, scaled) without reopening? -- it took a while for the image to appear
PASS | step 10 guvcview | guvcview shows 'Please connect the Elgato Game Capture HD'?
PASS | step 10 guvcview | guvcview shows the N64 picture again?
PASS | step 10 guvcview | idle format follows the NTSC source after closing
WARN | step 10 guvcview | dmesg: elgato_gchd 1-2:1.0: bulk-IN resubmit failed: ep=0x81 error=-19
FAIL | step 11 bridge stopped | audio source removed (expected /^0$/, got: 2)
