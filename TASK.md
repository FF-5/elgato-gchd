# TASK T-060 — guvcview, OBS and similar applications

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** application device registered at module load (encoder output sizes, unscaled) + bridge; hardware test
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
8. (Maintainer, 2026-11-18) No scaling: 1:1 output. The resolutions an
   application gets are the device's own output sizes for the input type
   (composite/S-video: source size, 640×480, 480×360; HDMI/component:
   1920×1080, 1280×720, 640×480, 480×360, observed with a 1080p60
   source), selected on the device's encoder. After a source change while
   streaming (NTSC ↔ PAL, HDMI mode) the application gets
   V4L2_EVENT_SOURCE_CHANGE and the picture stands still (audio continues)
   until it selects the new format or reopens; no cropping, borders or
   scaling, and the stream is never ended with an error ("making the host
   program panic and close is a no-no").
9. (Maintainer, 2026-11-18) No periodic work while no application uses
   the device; a "Please connect" picture without a Game Capture HD.

## Implementation

Driver (`src/app.c`, hooks in `core.c`; no protocol change):

- "Elgato Game Capture HD (H.264)": the compressed stream, per connected
  device (the former device, renamed; one name for all models).
- "Elgato Game Capture HD" (YUYV capture) and "Elgato Game Capture HD
  (frame input)" (output) are registered at module load and stay while
  the module is loaded; the first connected Game Capture HD is attached
  (detached on unplug, the next one attached at plug-in).
- Sizes = the encoder's output sizes of the current input (composite and
  S-video: source size at 50/59.94 one frame per field, 640×480 and
  480×360 at 25/29.97 progressive; HDMI/component: the source size until
  stage 3 adds its output sizes). While no application has buffers the
  list and format follow the source; while streaming the format stays, and
  when the encoder delivers another size, V4L2_EVENT_SOURCE_CHANGE is
  queued (the bridge plays audio only: picture stands still). A "change"
  uevent on each real state change. The standard per input is remembered
  across loads (`std_composite`, `std_svideo`, udev rule).
- `VIDIOC_S_INPUT` on the application device switches the attached
  device's input (also while capturing).
- sysfs `/sys/class/video4linux/videoN/gchd_state` of the application
  device: `STATE W H NUM DEN FEED-NODE STREAM-NODE` (`-` without device).
- The frame input treats a buffer with 0 bytes as a whole frame
  (`write()`).
- Driver data lives as long as its `v4l2_device`.

Bridge (`service/gchd-bridge`): while an application streams, ffmpeg
decodes the attached device's stream, deinterlaces one frame per field,
sets the encoder to the selected size first (no scaling, no frame-rate
conversion), writes YUYV into the
frame input and the audio to the audio source; a black picture while no
Game Capture HD is attached ("Please connect" picture); sleeps on the
driver's uevents (`udevadm monitor`, bounded reads); stops when the application stops (capture
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

- Maintainer (2026-11-18) clarified the resolutions (Decisions 8): the
  application gets the device's own output sizes for the input, not a
  free list; the earlier list (1920×1080 … 640×480 for every input) and
  the "1280×720 request" test were a misunderstanding. Mid-stream source
  change: event + still picture, never an error that ends the stream.
- Run with uevents (3b53941): 20 PASS, 6 FAIL, 1 WARN. Cause: a uevent
  loop. Every uevent makes udev open and close the application device;
  the close re-checked the source and notified even without a change, so
  uevents followed one another without end; the bridge's loop that reads
  queued uevents never ended (stuck from step 3 to step 8: no frames in
  steps 4–5, capture device not shut down, guvcview slow; guvcview also
  rescans on every uevent). Fixed: notify only real changes; the bridge
  reads at most 50 queued uevents per round. Bridge CPU while idle: 0
  ticks in 20 s. "Please connect" shown in guvcview while unplugged and
  the N64 picture after the replug (PASS). WARN "bulk-IN resubmit failed
  -19": the unplug in step 9. guvcview "can't find parent usb device": the
  application device is not a USB device (as with v4l2loopback).
- Maintainer (2026-11-18): no scaling, 1:1 output (Decisions 8):
  implemented; offline the placement gives continuous 720×576 frames for
  a PAL → NTSC stream (NTSC centred, 48 black lines top and bottom).
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

Does the driver really need to expose that many video devices? Don't we only need one?

Here's the log:
gchd-test T-060 20261118-202230 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-060-20261118-202230.log
== build
$ git fetch -q origin
$ git checkout -q feat/app-bridge
$ git pull -q --ff-only
commit: a7fbe2f dkms: application device offers the encoder's output sizes of the input; remembered standard per input (T-060)
commit: a7fbe2f dkms: application device offers the encoder's output sizes of the input; remembered standard per input (T-060)
build rc=0 warnings=0

=== step 1: driver: application device registered at module load
$ sh -c pkill -f 'gchd-bridge run'; sudo modprobe -r v4l2loopback; true
$ sudo install -m 0644 /home/ff5/elgato-gchd/udev-rules/56-elgato-gchd-settings.rules /etc/udev/rules.d/
$ sudo udevadm control --reload
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
idle 1280 720 60 1 video1 video2 0 0
[PASS] application device idle, Game Capture HD attached
--- dmesg step 1
[94317.738906] usbcore: deregistering interface driver elgato_gchd
[94336.139128] usb 1-2: USB disconnect, device number 96
[94342.465575] usb 1-2: new high-speed USB device number 97 using xhci_hcd
[94342.855617] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[94342.855639] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[94342.855646] usb 1-2: Product: GameCapture HD
[94342.855651] usb 1-2: Manufacturer: Elgato
[94342.855654] usb 1-2: SerialNumber: 04107F1267
[94343.662356] elgato_gchd: application device video0, frame input video1
[94343.662405] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[94343.664866] elgato_gchd 1-2:1.0: attached to the application device video0
[94343.664877] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[94343.664909] usbcore: registered new interface driver elgato_gchd

=== step 2: bridge idle: capture device off
PROMPT: PS1 on composite, game running.
$ v4l2-ctl -d /dev/video2 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ v4l2-ctl -d /dev/video0 --list-formats-ext
ioctl: VIDIOC_ENUM_FMT
	Type: Video Capture

	[0]: 'YUYV' (YUYV 4:2:2)
		Size: Discrete 720x480
			Interval: Discrete 0.017s (59.940 fps)
		Size: Discrete 640x480
			Interval: Discrete 0.033s (29.970 fps)
		Size: Discrete 480x360
			Interval: Discrete 0.033s (29.970 fps)
$ sh -c v4l2-ctl -d /dev/video0 --list-formats-ext | tr '\n' ' '
ioctl: VIDIOC_ENUM_FMT 	Type: Video Capture  	[0]: 'YUYV' (YUYV 4:2:2) 		Size: Discrete 720x480 			Interval: Discrete 0.017s (59.940 fps) 		Size: Discrete 640x480 			Interval: Discrete 0.033s (29.970 fps) 		Size: Discrete 480x360 			Interval: Discrete 0.033s (29.970 fps) 
[PASS] composite: three sizes (source size, 640x480, 480x360)
$ bash /home/ff5/elgato-gchd/service/gchd-bridge status
application:    /dev/video0
driver state:   idle 720 480 60000 1001 video1 video2 0 0
service:        running (pid 1709289)
$ bash /home/ff5/elgato-gchd/service/gchd-bridge status
application:    /dev/video0
driver state:   idle 720 480 60000 1001 video1 video2 0 0
service:        running (pid 1709289)
[PASS] bridge running
$ sh -c sudo dmesg | grep -c 'starting hardware boot'
0
[PASS] capture device off while no application streams
bridge CPU while idle: 0 ticks in 20 s
$ echo 0
0
[PASS] bridge sleeps while idle (< 5 ticks = 0.05 s CPU in 20 s)
--- dmesg step 2
[94366.406316] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[94366.407428] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== step 3: first read after loading: PAL detected while streaming 720x480, then the format follows
$ cat /etc/modprobe.d/elgato-gchd-settings.conf
options elgato_gchd input=2 output=0 format=0 bitrate=0 bitrate_mode=1 stretch=0 h264_profile=4 h264_level=0 std_composite=0 std_svideo=0
$ cat /home/ff5/gchd-test-logs/T-060-20261118-202230/read1.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 331444 kb/s, 59.94 fps, 59.94 tbr, 1000k tbn
frame=    0 fps=0.0 q=0.0 Lsize=       0kB time=N/A bitrate=N/A speed=N/A    
$ grep -c No such device /home/ff5/gchd-test-logs/T-060-20261118-202230/read1.txt
0
[PASS] reader not cut off (no 'No such device')
$ tail -6 /home/ff5/gchd-test-logs/T-060-20261118-202230/bridge.log
Error initializing filters!
Error marking filters as finished
[out#0/video4linux2,v4l2 @ 0x5e4aad919780] Nothing was written into output file, because at least one of its streams received no packets.
[out#1/pulse @ 0x5e4aaddfdf00] Nothing was written into output file, because at least one of its streams received no packets.
gchd-bridge: capture stopped
gchd-bridge: capture (audio): /dev/video2 -> /dev/video1 (720x480)
$ app_state
idle 720 576 50 1 video1 video2 720 576
$ app_state
idle 720 576 50 1 video1 video2 720 576
[PASS] idle format now the PAL source: 720x576, 50 frames/s
$ cat /etc/modprobe.d/elgato-gchd-settings.conf
options elgato_gchd input=2 output=0 format=1 bitrate=0 bitrate_mode=1 stretch=0 h264_profile=4 h264_level=0 std_composite=2 std_svideo=0
[PASS] PAL remembered for composite
--- dmesg step 3
[94390.760775] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[94390.908379] elgato_gchd 1-2:1.0: transcoder defaults loaded
[94390.968446] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[94391.342221] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[94391.344613] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[94391.344616] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[94391.344619] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[94391.344622] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[94392.129024] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[94392.343089] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[94392.359223] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94392.359425] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94392.365229] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94392.365776] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[94392.366438] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[94392.505101] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94394.411456] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[94394.818382] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[94394.818383] elgato_gchd 1-2:1.0: waiting 1093 ms after the previous shutdown
[94396.067866] elgato_gchd 1-2:1.0: transcoder defaults loaded
[94396.125971] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[94396.229870] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[94396.290777] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[94396.293423] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[94396.293428] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[94396.293431] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[94396.293439] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[94397.076029] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[94397.302725] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[94397.318358] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94397.318593] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94397.324237] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94397.325067] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[94397.325525] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[94397.464662] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94402.328256] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94407.353681] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94412.380448] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94417.406147] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=448 bytes=29360128 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94422.433116] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=563 bytes=36896768 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94423.522189] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 4: read the source format: 720x576 YUYV, about 50 frames/s
$ cat /home/ff5/gchd-test-logs/T-060-20261118-202230/read2.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=  782 fps= 52 q=-0.0 Lsize=N/A time=00:00:14.90 bitrate=N/A speed=0.994x    
$ cat /home/ff5/gchd-test-logs/T-060-20261118-202230/read2.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=  782 fps= 52 q=-0.0 Lsize=N/A time=00:00:14.90 bitrate=N/A speed=0.994x    
[PASS] stream YUYV 720x576
$ cat /home/ff5/gchd-test-logs/T-060-20261118-202230/read2.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=  782 fps= 52 q=-0.0 Lsize=N/A time=00:00:14.90 bitrate=N/A speed=0.994x    
[PASS] frame count >= 450 in 15 s
$ sudo dmesg
[94426.803170] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1709617 comm=v4l2-ctl
[94426.847408] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1709621 comm=ffmpeg
[94426.847464] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[94426.861975] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[94426.862210] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
[94426.925541] elgato_gchd 1-2:1.0: transcoder defaults loaded
[94426.925670] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[94426.925928] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x04
[94426.927424] elgato_gchd 1-2:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.928522] elgato_gchd 1-2:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.929667] elgato_gchd 1-2:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.930795] elgato_gchd 1-2:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.931897] elgato_gchd 1-2:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.933005] elgato_gchd 1-2:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.934159] elgato_gchd 1-2:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.935298] elgato_gchd 1-2:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.936435] elgato_gchd 1-2:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.937526] elgato_gchd 1-2:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.938655] elgato_gchd 1-2:1.0: firmware chunk offset=327680 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.939749] elgato_gchd 1-2:1.0: firmware chunk offset=360448 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.940936] elgato_gchd 1-2:1.0: firmware chunk offset=393216 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.942054] elgato_gchd 1-2:1.0: firmware chunk offset=425984 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.943153] elgato_gchd 1-2:1.0: firmware chunk offset=458752 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.944294] elgato_gchd 1-2:1.0: firmware chunk offset=491520 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.945377] elgato_gchd 1-2:1.0: firmware chunk offset=524288 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.946499] elgato_gchd 1-2:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.947657] elgato_gchd 1-2:1.0: firmware chunk offset=589824 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.948791] elgato_gchd 1-2:1.0: firmware chunk offset=622592 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.949891] elgato_gchd 1-2:1.0: firmware chunk offset=655360 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.951005] elgato_gchd 1-2:1.0: firmware chunk offset=688128 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.952288] elgato_gchd 1-2:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.953463] elgato_gchd 1-2:1.0: firmware chunk offset=753664 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.954678] elgato_gchd 1-2:1.0: firmware chunk offset=786432 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.955806] elgato_gchd 1-2:1.0: firmware chunk offset=819200 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.957076] elgato_gchd 1-2:1.0: firmware chunk offset=851968 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.958304] elgato_gchd 1-2:1.0: firmware chunk offset=884736 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.959482] elgato_gchd 1-2:1.0: firmware chunk offset=917504 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.960633] elgato_gchd 1-2:1.0: firmware chunk offset=950272 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.961781] elgato_gchd 1-2:1.0: firmware chunk offset=983040 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.962921] elgato_gchd 1-2:1.0: firmware chunk offset=1015808 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.964034] elgato_gchd 1-2:1.0: firmware chunk offset=1048576 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.965153] elgato_gchd 1-2:1.0: firmware chunk offset=1081344 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.966290] elgato_gchd 1-2:1.0: firmware chunk offset=1114112 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.967393] elgato_gchd 1-2:1.0: firmware chunk offset=1146880 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.968489] elgato_gchd 1-2:1.0: firmware chunk offset=1179648 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.969659] elgato_gchd 1-2:1.0: firmware chunk offset=1212416 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.970779] elgato_gchd 1-2:1.0: firmware chunk offset=1245184 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.971925] elgato_gchd 1-2:1.0: firmware chunk offset=1277952 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.973150] elgato_gchd 1-2:1.0: firmware chunk offset=1310720 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.974470] elgato_gchd 1-2:1.0: firmware chunk offset=1343488 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.975836] elgato_gchd 1-2:1.0: firmware chunk offset=1376256 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.977277] elgato_gchd 1-2:1.0: firmware chunk offset=1409024 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.978694] elgato_gchd 1-2:1.0: firmware chunk offset=1441792 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.980186] elgato_gchd 1-2:1.0: firmware chunk offset=1474560 size=26224 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[94426.983908] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[94427.083131] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[94427.132084] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[94427.140598] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[94427.142747] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[94427.142749] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[94427.142751] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[94427.142753] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[94427.924021] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[94427.924439] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[94428.156265] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[94428.178370] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94428.178505] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[94428.178692] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94428.197304] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94428.197798] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[94428.199217] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[94428.199695] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[94428.200194] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[94428.201821] elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0150 ack=1
[94428.203750] elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[94428.204465] elgato_gchd 1-2:1.0: START poll=1 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[94428.205424] elgato_gchd 1-2:1.0: START poll=2 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[94428.206596] elgato_gchd 1-2:1.0: START poll=3 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[94428.207421] elgato_gchd 1-2:1.0: START poll=4 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[94428.208159] elgato_gchd 1-2:1.0: START poll=5 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[94428.208669] elgato_gchd 1-2:1.0: START poll=6 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[94428.209175] elgato_gchd 1-2:1.0: START poll=7 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[94428.209640] elgato_gchd 1-2:1.0: START poll=8 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[94428.210061] elgato_gchd 1-2:1.0: START poll=9 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[94428.211226] elgato_gchd 1-2:1.0: START poll=10 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[94428.211638] elgato_gchd 1-2:1.0: START poll=11 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[94428.212036] elgato_gchd 1-2:1.0: START poll=12 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[94428.213099] elgato_gchd 1-2:1.0: START poll=13 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[94428.213522] elgato_gchd 1-2:1.0: START poll=14 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[94428.213979] elgato_gchd 1-2:1.0: START poll=15 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[94428.246859] elgato_gchd 1-2:1.0: START poll=100 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[94428.278619] elgato_gchd 1-2:1.0: START poll=200 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[94428.306617] elgato_gchd 1-2:1.0: START poll=300 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[94428.335441] elgato_gchd 1-2:1.0: START poll=400 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[94428.343867] elgato_gchd 1-2:1.0: START poll=430 state=0x0002 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0150 ack=1
[94428.344309] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94428.741810] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[94428.787703] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[94428.832829] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[94428.877449] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[94428.922818] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[94433.208578] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94438.233398] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94443.260952] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[PASS] capture device started
--- dmesg step 4
[94426.847464] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[94426.925541] elgato_gchd 1-2:1.0: transcoder defaults loaded
[94426.983908] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[94427.083131] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[94427.140598] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[94427.142747] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[94427.142749] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[94427.142751] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[94427.142753] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[94427.924021] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[94428.156265] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[94428.178370] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94428.178692] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94428.197304] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94428.197798] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[94428.199217] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[94428.344309] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94433.208578] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94438.233398] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94443.260952] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 5: application selects 640x480: the encoder delivers it (no scaling)
$ cat /home/ff5/gchd-test-logs/T-060-20261118-202230/read3.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 640x480, 122880 kb/s, 25 fps, 25 tbr, 1000k tbn
frame=  530 fps= 53 q=-0.0 Lsize=N/A time=00:00:09.96 bitrate=N/A speed=0.994x    
$ cat /home/ff5/gchd-test-logs/T-060-20261118-202230/read3.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 640x480, 122880 kb/s, 25 fps, 25 tbr, 1000k tbn
frame=  530 fps= 53 q=-0.0 Lsize=N/A time=00:00:09.96 bitrate=N/A speed=0.994x    
[PASS] stream YUYV 640x480, 25 frames/s
$ cat /home/ff5/gchd-test-logs/T-060-20261118-202230/read3.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 640x480, 122880 kb/s, 25 fps, 25 tbr, 1000k tbn
frame=  530 fps= 53 q=-0.0 Lsize=N/A time=00:00:09.96 bitrate=N/A speed=0.994x    
[PASS] frames delivered (>= 150 in 10 s)
$ tail -4 /home/ff5/gchd-test-logs/T-060-20261118-202230/bridge.log
[out#0/pulse @ 0x59d8edc71680] Nothing was written into output file, because at least one of its streams received no packets.
gchd-bridge: ffmpeg 1709809 did not stop; killing it
gchd-bridge: capture stopped
gchd-bridge: capture (video): /dev/video2 -> /dev/video1 (640x480)
$ v4l2-ctl -d /dev/video0 -v width=720,height=576
$ app_state
idle 720 576 50 1 video1 video2 640 480
[PASS] 720x576 selected again
--- dmesg step 5
[94446.209162] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[94446.829669] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[94446.829670] elgato_gchd 1-2:1.0: waiting 880 ms after the previous shutdown
[94447.791900] elgato_gchd 1-2:1.0: transcoder defaults loaded
[94447.855873] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[94447.947887] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[94448.132626] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[94448.134540] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[94448.203291] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[94448.228925] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[94448.231024] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[94448.231026] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[94448.231028] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[94448.231030] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[94448.986270] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[94449.183365] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[94449.200115] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94449.200336] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94449.206422] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94449.206858] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[94449.207305] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[94449.346000] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94449.605158] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[94451.231151] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94451.231462] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94451.237069] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94451.237609] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[94451.238035] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[94451.376840] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94451.376844] elgato_gchd 1-2:1.0: analog mode now 576i, output 1
[94451.770043] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[94453.498516] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[94453.543483] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[94453.543485] elgato_gchd 1-2:1.0: waiting 1456 ms after the previous shutdown
[94455.084846] elgato_gchd 1-2:1.0: transcoder defaults loaded
[94455.151592] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[94455.263752] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[94455.332184] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[94455.334659] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[94455.334663] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[94455.334666] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[94455.334669] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[94456.129815] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[94456.359441] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[94456.374521] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94456.374723] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94456.380142] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94456.381097] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[94456.381580] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[94456.500504] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94456.782599] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[94458.610440] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94458.610951] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94458.620211] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94458.621106] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[94458.622756] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[94458.736395] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94458.736401] elgato_gchd 1-2:1.0: analog mode now 576i, output 1
[94459.129153] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[94463.645350] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94468.680595] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 6: application closed: capture device off
$ sudo dmesg
[94470.393440] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[94470.394142] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[94471.142528] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[94471.143370] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[94471.311144] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[94471.312000] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[94471.312460] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
[94471.332077] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[PASS] hardware shut down after the application stopped
$ app_state
idle 720 576 50 1 video1 video2 640 480
[PASS] state idle
--- dmesg step 6
[94471.332077] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 7: audio while capturing
$ sink_streams

[FAIL] the bridge plays into the audio sink (expected /stream into elgato_gchd/, got: )
$ ffmpeg -hide_banner -nostdin -f pulse -i elgato_gchd_audio -t 4 -af volumedetect -f null -
[aist#0:0/pcm_s16le @ 0x5839120b8640] Guessed Channel Layout: stereo
Input #0, pulse, from 'elgato_gchd_audio':
  Duration: N/A, start: 1795029928.313650, bitrate: 1536 kb/s
  Stream #0:0: Audio: pcm_s16le, 48000 Hz, 2 channels, s16, 1536 kb/s
[Parsed_volumedetect_0 @ 0x5839120ba000] n_samples: 0
Stream mapping:
  Stream #0:0 -> #0:0 (pcm_s16le (native) -> pcm_s16le (native))
Output #0, null, to 'pipe:':
  Metadata:
    encoder         : Lavf60.16.100
  Stream #0:0: Audio: pcm_s16le, 48000 Hz, stereo, s16, 1536 kb/s
    Metadata:
      encoder         : Lavc60.31.102 pcm_s16le
size=       0kB time=00:00:00.00 bitrate=N/A speed=N/A    
size=N/A time=00:00:00.58 bitrate=N/A speed=1.05x    
size=N/A time=00:00:01.10 bitrate=N/A speed=1.03x    
size=N/A time=00:00:01.58 bitrate=N/A speed=   1x    
size=N/A time=00:00:02.08 bitrate=N/A speed=0.996x    
size=N/A time=00:00:02.58 bitrate=N/A speed=0.979x    
size=N/A time=00:00:03.19 bitrate=N/A speed=0.998x    
size=N/A time=00:00:03.74 bitrate=N/A speed=0.998x    
[out#0/null @ 0x5839120b5b00] video:0kB audio:759kB subtitle:0kB other streams:0kB global headers:0kB muxing overhead: unknown
size=N/A time=00:00:03.95 bitrate=N/A speed=0.973x    
[Parsed_volumedetect_0 @ 0x5839120bca80] n_samples: 393600
[Parsed_volumedetect_0 @ 0x5839120bca80] mean_volume: -85.5 dB
[Parsed_volumedetect_0 @ 0x5839120bca80] max_volume: -74.7 dB
[Parsed_volumedetect_0 @ 0x5839120bca80] histogram_74db: 1
[Parsed_volumedetect_0 @ 0x5839120bca80] histogram_75db: 0
[Parsed_volumedetect_0 @ 0x5839120bca80] histogram_76db: 93
[Parsed_volumedetect_0 @ 0x5839120bca80] histogram_77db: 0
[Parsed_volumedetect_0 @ 0x5839120bca80] histogram_78db: 3604
[PASS] audio source delivers samples
--- dmesg step 7
[94476.537192] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[94476.625540] elgato_gchd 1-2:1.0: transcoder defaults loaded
[94476.687422] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[94476.798150] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[94476.856415] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[94476.858834] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[94476.858837] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[94476.858840] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[94476.858843] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[94477.638814] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[94477.861628] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[94477.876520] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94477.876714] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94477.881811] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94477.882452] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[94477.882857] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[94478.022446] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94480.026529] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[94480.589700] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[94480.589701] elgato_gchd 1-2:1.0: waiting 947 ms after the previous shutdown
[94481.686394] elgato_gchd 1-2:1.0: transcoder defaults loaded
[94481.752327] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[94481.883683] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[94481.933571] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[94482.033582] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[94482.060247] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[94482.062499] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[94482.223854] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[94482.249463] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[94482.251731] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[94482.251734] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[94482.251736] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[94482.251738] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[94483.030880] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[94483.319589] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[94483.337960] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94483.338352] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94483.343980] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94483.344598] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[94483.345308] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[94483.464304] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94488.370875] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94493.398337] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94498.424116] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 8: input change on the application device while reading
$ v4l2-ctl -d /dev/video0 -i 3
Video input set to 3 (S-Video: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ cat /home/ff5/gchd-test-logs/T-060-20261118-202230/read4.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame= 1077 fps= 36 q=-0.0 Lsize=N/A time=00:00:29.98 bitrate=N/A speed=0.998x    
$ grep -c No such device /home/ff5/gchd-test-logs/T-060-20261118-202230/read4.txt
0
[PASS] reader kept the device (no 'No such device')
$ cat /home/ff5/gchd-test-logs/T-060-20261118-202230/read4.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame= 1077 fps= 36 q=-0.0 Lsize=N/A time=00:00:29.98 bitrate=N/A speed=0.998x    
[PASS] reader got frames (>= 600 in 30 s)
$ v4l2-ctl -d /dev/video2 --get-input
Video input : 2 (Composite: ok)
[PASS] input composite on the H.264 device too
$ tail -10 /home/ff5/gchd-test-logs/T-060-20261118-202230/bridge.log
Error initializing filters!
Error marking filters as finished
[out#0/pulse @ 0x62b7c37b8680] Nothing was written into output file, because at least one of its streams received no packets.
gchd-bridge: capture stopped
gchd-bridge: capture (video): /dev/video2 -> /dev/video1 (720x576)
gchd-bridge: capture stopped
gchd-bridge: capture (video): /dev/video2 -> /dev/video1 (720x576)
[h264 @ 0x5f36c8504ac0] error while decoding MB 4 28, bytestream -12
[mp2 @ 0x5f36c8494dc0] Header missing
[aist#0:1/mp2 @ 0x5f36c83c6a40] Error submitting packet to decoder: Invalid data found when processing input
--- dmesg step 8
[94500.345732] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[94500.957509] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[94500.957510] elgato_gchd 1-2:1.0: waiting 889 ms after the previous shutdown
[94501.937439] elgato_gchd 1-2:1.0: transcoder defaults loaded
[94502.000803] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[94502.097801] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[94502.317341] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[94502.364046] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[94502.366596] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[94502.366600] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[94502.366603] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[94502.366606] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[94503.143956] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[94503.371127] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[94503.389775] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94503.389978] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94503.395923] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94503.396723] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[94503.397222] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[94503.536034] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94508.399789] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94509.332275] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=1 streaming=1
[94510.244215] elgato_gchd 1-2:1.0: input selected: input=3; detection scheduled
[94510.366068] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[94510.419423] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[94515.270171] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=1 streaming=1
[94515.273315] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[94515.564576] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[94515.708150] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[94515.710371] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[94515.710373] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[94515.710376] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[94515.710378] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[94515.951607] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[94515.969519] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94515.969849] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94515.977251] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94515.977756] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[94515.978435] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[94516.115367] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94520.988623] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94526.024271] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 9: unplug and replug while reading
PROMPT: Unplug the Elgato (or detach it in VirtualBox) now, wait 5 s, then plug it in again.
$ app_state
idle 720 576 50 1 video1 video2 0 0
$ cat /home/ff5/gchd-test-logs/T-060-20261118-202230/read5.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame= 2283 fps= 51 q=-0.0 Lsize=N/A time=00:00:44.94 bitrate=N/A speed=0.998x    
$ grep -c No such device /home/ff5/gchd-test-logs/T-060-20261118-202230/read5.txt
0
[PASS] reader kept the device through unplug/replug
$ cat /home/ff5/gchd-test-logs/T-060-20261118-202230/read5.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame= 2283 fps= 51 q=-0.0 Lsize=N/A time=00:00:44.94 bitrate=N/A speed=0.998x    
[PASS] frames throughout (>= 900 in 45 s)
$ app_state
idle 720 576 50 1 video1 video2 0 0
[FAIL] Game Capture HD attached again (expected /^idle .* video2$/, got: idle 720 576 50 1 video1 video2 0 0)
$ tail -10 /home/ff5/gchd-test-logs/T-060-20261118-202230/bridge.log
gchd-bridge: capture stopped
gchd-bridge: capture (video): /dev/video2 -> /dev/video1 (720x576)
gchd-bridge: capture stopped
gchd-bridge: capture (video): /dev/video2 -> /dev/video1 (720x576)
[h264 @ 0x5f36c8504ac0] error while decoding MB 4 28, bytestream -12
[mp2 @ 0x5f36c8494dc0] Header missing
[aist#0:1/mp2 @ 0x5f36c83c6a40] Error submitting packet to decoder: Invalid data found when processing input
gchd-bridge: capture stopped
gchd-bridge: capture (video): /dev/video2 -> /dev/video1 (720x576)
gchd-bridge: capture stopped
--- dmesg step 9
[94530.261269] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[94530.581950] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[94530.581951] elgato_gchd 1-2:1.0: waiting 1180 ms after the previous shutdown
[94531.915260] elgato_gchd 1-2:1.0: transcoder defaults loaded
[94531.971377] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[94532.066441] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[94532.121089] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[94532.123331] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[94532.123333] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[94532.123335] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[94532.123337] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[94532.881118] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[94533.105784] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[94533.124739] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94533.125525] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94533.133439] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94533.134285] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[94533.134827] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[94533.270532] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94538.135157] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94543.161294] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94548.189392] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94553.212526] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=448 bytes=29360128 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94558.240374] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=563 bytes=36896768 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94563.267167] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=678 bytes=44433408 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94568.291996] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=793 bytes=51970048 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94573.317351] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=908 bytes=59506688 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94578.343413] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1023 bytes=67043328 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94581.075168] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[94713.005632] usb 1-2: USB disconnect, device number 97
[94718.536287] usb 1-2: new high-speed USB device number 98 using xhci_hcd
[94718.920494] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[94718.920516] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[94718.920522] usb 1-2: Product: GameCapture HD
[94718.920527] usb 1-2: Manufacturer: Elgato
[94718.920547] usb 1-2: SerialNumber: 04107F1267
[94718.930101] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[94718.932505] elgato_gchd 1-2:1.0: attached to the application device video0
[94718.932518] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON

=== step 10: guvcview
PROMPT: Start in a second terminal: guvcview -d /dev/video0   (leave it open; resolution 720x576 if it asks)
[PASS] guvcview shows the PS1 picture, smooth, without interlacing lines?
PROMPT: Swap the PS1 for the N64 (NTSC) while guvcview stays open; wait about 15 s.
$ app_state
streaming 640 480 25 1 video1 video2 640 480
[FAIL] the picture stands still (never squeezed, cropped or with borders), and guvcview did not close or show an error? -- i didn't pay attention, but there seems to be more border at the bottom
PROMPT: In guvcview select 720x480 (Video tab, resolution), or close and reopen guvcview; wait about 10 s.
[PASS] guvcview shows the N64 picture (720x480)? -- i had to close guvcview first, because otherwise i could only see PAL settings and no 720x480
$ cat /etc/modprobe.d/elgato-gchd-settings.conf
options elgato_gchd input=2 output=0 format=1 bitrate=0 bitrate_mode=1 stretch=0 h264_profile=4 h264_level=0 std_composite=1 std_svideo=0
[PASS] NTSC remembered for composite
PROMPT: Unplug the Elgato while guvcview stays open; wait about 5 s.
[PASS] guvcview shows 'Please connect the Elgato Game Capture HD'?
PROMPT: Plug the Elgato in again; wait about 15 s.
[PASS] guvcview shows the N64 picture again?
PROMPT: Close guvcview.
$ app_state
idle 720 480 60000 1001 video1 video2 720 480
[PASS] idle format: the NTSC source 720x480, 59.94 frames/s
$ tail -20 /home/ff5/gchd-test-logs/T-060-20261118-202230/bridge.log
[h264 @ 0x5c27cf0a9b40] mmco: unref short failure
[h264 @ 0x5c27cf0d3140] mmco: unref short failure
[h264 @ 0x5c27cf0d3140] error while decoding MB 33 9, bytestream -12
[mp2 @ 0x5c27cefc1840] Header missing
[aist#0:1/mp2 @ 0x5c27cf055080] Error submitting packet to decoder: Invalid data found when processing input
[mp2 @ 0x5c27cefc1840] Header missing
[aist#0:1/mp2 @ 0x5c27cf055080] Error submitting packet to decoder: Invalid data found when processing input
[h264 @ 0x5c27cf0dad40] error while decoding MB 4 24, bytestream -26
[mp2 @ 0x5c27cefc1840] Header missing
[aist#0:1/mp2 @ 0x5c27cf055080] Error submitting packet to decoder: Invalid data found when processing input
gchd-bridge: now 720x480, 60000/1001, stream device -, encoder 0x0: video
gchd-bridge: ffmpeg 1712221 did not stop; killing it
gchd-bridge: capture stopped
gchd-bridge: no Game Capture HD: picture 720x480 -> /dev/video1
gchd-bridge: now 720x480, 60000/1001, stream device video2, encoder 0x0: video
gchd-bridge: capture stopped
gchd-bridge: capture (video): /dev/video2 -> /dev/video1 (720x480)
[mp2 @ 0x5aa72b7e4e00] Header missing
[aist#0:1/mp2 @ 0x5aa72b722200] Error submitting packet to decoder: Invalid data found when processing input
gchd-bridge: capture stopped
--- dmesg step 10
[94743.256906] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[94743.398661] elgato_gchd 1-2:1.0: transcoder defaults loaded
[94743.461639] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[94743.872618] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[94743.874857] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[94743.874858] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[94743.874861] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[94743.874863] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[94744.671268] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[94744.918570] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[94744.934165] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94744.934399] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94744.939665] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94744.940546] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[94744.941585] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[94745.080042] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94745.334615] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[94747.023569] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94747.023917] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94747.029554] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94747.030129] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[94747.030553] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[94747.149453] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94747.149456] elgato_gchd 1-2:1.0: analog mode now 576i, output 1
[94747.542880] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[94752.059794] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94757.093139] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94762.127421] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=21889024 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94767.167704] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=29425664 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94768.333362] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=f CA=6e E7=f9); configuring again
[94773.485738] elgato_gchd 1-2:1.0: family change: decoder not locked (FA=fd CA=6e)
[94773.485756] elgato_gchd 1-2:1.0: analog mode change failed: -67
[94774.573753] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[94774.604918] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[94774.607056] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[94774.738480] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[94774.740812] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[94774.740814] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[94774.740816] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[94774.740818] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[94775.481403] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[94775.497828] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94775.498157] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94775.505960] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94775.506461] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[94775.506903] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[94775.620543] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94775.887359] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[94777.521775] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94777.522267] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94777.529067] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94777.529579] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[94777.530007] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[94777.627762] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94777.627765] elgato_gchd 1-2:1.0: analog mode now 240p, output 1
[94778.000455] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[94779.011471] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[94780.850707] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94780.851256] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94780.857714] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94780.858421] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[94780.859076] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[94780.973415] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94780.973419] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[94781.343109] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[94785.858990] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94790.892083] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94795.924076] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=21889024 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94800.954857] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=29425664 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94805.986232] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=36962304 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94811.017518] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=679 bytes=44498944 ep84_bytes=44498944 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94813.006458] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[94814.484856] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94814.485072] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94814.490272] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94814.491064] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[94814.491959] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[94814.607441] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94814.607444] elgato_gchd 1-2:1.0: analog mode now 240p, output 1
[94814.980303] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[94819.495207] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94824.529076] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94829.560983] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=21889024 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94834.593612] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=29425664 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94839.625220] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=36962304 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94843.799387] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[94844.656042] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=679 bytes=44498944 ep84_bytes=44498944 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94845.406207] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94845.406470] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94845.412859] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94845.413357] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[94845.413816] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[94845.512126] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94845.512129] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[94845.882559] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[94850.440444] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94855.473965] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=14417920 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94860.504888] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=21954560 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94865.537433] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=29491200 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94870.570374] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=37027840 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94875.600275] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=680 bytes=44564480 ep84_bytes=44564480 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94877.361239] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[94878.829303] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94878.829534] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94878.835866] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94878.836840] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[94878.837567] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[94878.968605] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94878.968609] elgato_gchd 1-2:1.0: analog mode now 240p, output 1
[94879.341793] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[94881.776453] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[94881.980695] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[94881.980696] elgato_gchd 1-2:1.0: waiting 1297 ms after the previous shutdown
[94883.429979] elgato_gchd 1-2:1.0: transcoder defaults loaded
[94883.495543] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[94883.601133] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[94883.735269] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[94883.737548] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[94883.752677] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[94883.754774] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[94883.830842] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[94883.864058] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[94883.866164] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[94883.866166] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[94883.866167] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[94883.866169] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[94884.647023] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[94885.511566] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[94885.528311] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94885.528513] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94885.534937] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94885.535578] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[94885.536100] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[94885.650853] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94890.576099] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94895.596575] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94900.619836] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94905.642401] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94906.927266] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[94907.230228] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[94907.230229] elgato_gchd 1-2:1.0: waiting 1211 ms after the previous shutdown
[94908.584236] elgato_gchd 1-2:1.0: transcoder defaults loaded
[94908.649494] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[94908.809222] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[94908.984807] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[94909.024881] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[94909.027167] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[94909.099726] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[94909.101963] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[94909.203182] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[94909.296952] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[94909.394271] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[94909.398459] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[94909.398465] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[94909.398469] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[94909.398472] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[94910.244912] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[94910.375194] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[94910.396070] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94910.396343] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94910.421202] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94910.421817] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[94910.422789] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[94910.602163] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94910.854123] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[94912.454882] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94912.455232] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94912.460631] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94912.461252] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[94912.461727] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[94912.560888] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94912.560892] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[94912.930634] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[94914.296893] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[94914.343710] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[94914.343711] elgato_gchd 1-2:1.0: waiting 1456 ms after the previous shutdown
[94915.882315] elgato_gchd 1-2:1.0: transcoder defaults loaded
[94915.942762] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[94916.038294] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[94916.162366] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[94916.246532] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[94916.278108] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[94916.280473] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[94916.344440] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[94916.377883] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[94916.380134] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[94916.459256] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[94916.461969] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[94916.461972] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[94916.461974] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[94916.461976] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[94917.257069] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[94917.379689] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[94917.397368] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94917.397651] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94917.418950] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94917.419602] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[94917.420051] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[94917.536675] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94917.790400] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[94919.363988] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94919.364283] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94919.369886] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94919.370535] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[94919.370990] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[94919.470111] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94919.470115] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[94919.838917] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[94924.398594] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94929.429922] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=14417920 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94934.069819] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[94934.411529] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[94934.411530] elgato_gchd 1-2:1.0: waiting 1168 ms after the previous shutdown
[94935.715999] elgato_gchd 1-2:1.0: transcoder defaults loaded
[94935.774536] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[94935.872101] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[94936.123016] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[94936.162545] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[94936.164915] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[94936.246350] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[94936.249158] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[94936.339498] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[94936.450424] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[94936.453132] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[94936.453137] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[94936.453140] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[94936.453144] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[94937.248470] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[94937.371283] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[94937.386870] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94937.387094] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94937.411009] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94937.411823] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[94937.412560] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[94937.593000] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94941.851903] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[94942.428734] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94943.261203] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94943.261445] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94943.266688] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94943.267584] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[94943.268149] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[94943.384417] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94943.384421] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[94948.307896] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94953.331838] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94958.352789] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94963.376269] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94968.399364] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94972.574696] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[94973.421047] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=680 bytes=44564480 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94973.953413] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94973.953643] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94973.958861] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94973.959543] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[94973.960145] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[94974.076138] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94974.076143] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[94978.998507] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94982.436617] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[94985.779069] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[94985.835361] elgato_gchd 1-2:1.0: transcoder defaults loaded
[94985.897170] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[94986.017531] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[94986.154061] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[94986.238683] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[94986.265128] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x36 masked_mode=0x6
[94986.267156] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[94986.348595] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[94986.376889] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[94986.378904] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[94986.480985] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[94986.484572] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[94986.551893] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[94986.553771] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[94986.553773] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[94986.553776] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[94986.553777] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[94987.323468] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[94987.434759] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[94987.449624] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[94987.449818] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[94987.470868] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[94987.471806] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[94987.472663] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[94987.655721] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[94992.491411] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[94997.515172] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95002.538572] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95006.877612] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[95007.561448] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=448 bytes=29360128 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95008.312576] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[95008.313245] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[95008.318669] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[95008.319210] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[95008.319967] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[95008.435072] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[95008.435076] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[95013.360466] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95018.382559] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95023.407397] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95028.428107] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95033.452941] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95037.896178] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[95038.474884] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=680 bytes=44564480 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95039.392942] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[95039.393125] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[95039.398188] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[95039.398741] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[95039.399183] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[95039.498888] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[95039.498891] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[95044.420123] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95049.442891] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95054.467177] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95059.490415] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95064.513939] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95069.536934] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=680 bytes=44564480 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95071.661721] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[95073.060334] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[95073.060549] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[95073.065783] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[95073.066421] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[95073.066937] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[95073.166323] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[95073.166326] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[95077.338973] usb 1-2: USB disconnect, device number 98
[95086.243420] usb 1-2: new high-speed USB device number 99 using xhci_hcd
[95086.619300] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[95086.619311] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[95086.619314] usb 1-2: Product: GameCapture HD
[95086.619317] usb 1-2: Manufacturer: Elgato
[95086.619318] usb 1-2: SerialNumber: 04107F1267
[95086.625198] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[95086.627791] elgato_gchd 1-2:1.0: attached to the application device video0
[95086.627803] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[95087.253592] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[95087.450925] elgato_gchd 1-2:1.0: transcoder defaults loaded
[95087.522837] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[95087.881710] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[95087.950485] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[95087.952607] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[95087.952609] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[95087.952610] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[95087.952612] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[95088.740585] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[95089.503929] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[95089.519750] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[95089.520162] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[95089.526634] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[95089.527145] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[95089.527640] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[95089.625665] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[95094.549019] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95099.572686] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95102.630415] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[95104.005182] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[95104.005433] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[95104.011251] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[95104.011767] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[95104.012332] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[95104.111154] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[95104.111157] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[95109.034109] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95114.057245] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95119.080133] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95120.911511] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[WARN] dmesg: elgato_gchd 1-2:1.0: analog mode change failed: -67

=== step 11: bridge stopped
$ kill 1709289
$ sh -c pactl list short sources | grep -c elgato_gchd
2
[FAIL] audio source removed (expected /^0$/, got: 2)
--- dmesg step 11


=== summary (T-060, a7fbe2f dkms: application device offers the encoder's output sizes of the input; remembered standard per input (T-060))
PASS | step 1 driver: application device registered at module load | H.264 device found by name
PASS | step 1 driver: application device registered at module load | application device idle, Game Capture HD attached
PASS | step 2 bridge idle: capture device off | composite: three sizes (source size, 640x480, 480x360)
PASS | step 2 bridge idle: capture device off | bridge running
PASS | step 2 bridge idle: capture device off | capture device off while no application streams
PASS | step 2 bridge idle: capture device off | bridge sleeps while idle (< 5 ticks = 0.05 s CPU in 20 s)
PASS | step 3 first read after loading: PAL detected while streaming 720x480, then the format follows | reader not cut off (no 'No such device')
PASS | step 3 first read after loading: PAL detected while streaming 720x480, then the format follows | idle format now the PAL source: 720x576, 50 frames/s
PASS | step 3 first read after loading: PAL detected while streaming 720x480, then the format follows | PAL remembered for composite
PASS | step 4 read the source format: 720x576 YUYV, about 50 frames/s | stream YUYV 720x576
PASS | step 4 read the source format: 720x576 YUYV, about 50 frames/s | frame count >= 450 in 15 s
PASS | step 4 read the source format: 720x576 YUYV, about 50 frames/s | capture device started
PASS | step 5 application selects 640x480: the encoder delivers it (no scaling) | stream YUYV 640x480, 25 frames/s
PASS | step 5 application selects 640x480: the encoder delivers it (no scaling) | frames delivered (>= 150 in 10 s)
PASS | step 5 application selects 640x480: the encoder delivers it (no scaling) | 720x576 selected again
PASS | step 6 application closed: capture device off | hardware shut down after the application stopped
PASS | step 6 application closed: capture device off | state idle
FAIL | step 7 audio while capturing | the bridge plays into the audio sink (expected /stream into elgato_gchd/, got: )
PASS | step 7 audio while capturing | audio source delivers samples
PASS | step 8 input change on the application device while reading | reader kept the device (no 'No such device')
PASS | step 8 input change on the application device while reading | reader got frames (>= 600 in 30 s)
PASS | step 8 input change on the application device while reading | input composite on the H.264 device too
PASS | step 9 unplug and replug while reading | reader kept the device through unplug/replug
PASS | step 9 unplug and replug while reading | frames throughout (>= 900 in 45 s)
FAIL | step 9 unplug and replug while reading | Game Capture HD attached again (expected /^idle .* video2$/, got: idle 720 576 50 1 video1 video2 0 0)
PASS | step 10 guvcview | guvcview shows the PS1 picture, smooth, without interlacing lines?
FAIL | step 10 guvcview | the picture stands still (never squeezed, cropped or with borders), and guvcview did not close or show an error? -- i didn't pay attention, but there seems to be more border at the bottom
PASS | step 10 guvcview | guvcview shows the N64 picture (720x480)? -- i had to close guvcview first, because otherwise i could only see PAL settings and no 720x480
PASS | step 10 guvcview | NTSC remembered for composite
PASS | step 10 guvcview | guvcview shows 'Please connect the Elgato Game Capture HD'?
PASS | step 10 guvcview | guvcview shows the N64 picture again?
PASS | step 10 guvcview | idle format: the NTSC source 720x480, 59.94 frames/s
WARN | step 10 guvcview | dmesg: elgato_gchd 1-2:1.0: analog mode change failed: -67
FAIL | step 11 bridge stopped | audio source removed (expected /^0$/, got: 2)
