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
10. (Maintainer, 2026-11-18) One video device for programs: "Elgato
    Game Capture HD". The frame input becomes a private non-video device
    of the bridge; the bridge gets the compressed stream through a private
    device too; the H.264 V4L2 device only with a module parameter (for
    recording the original stream without a second encode); the encoder
    controls (bitrate, mode, profile, level, stretch) move to the main
    device. Quality: the program gets the decoded device stream
    (decoding exact, 4:2:0 → 4:2:2 without loss, no scaling);
    deinterlacing is the only change to the picture (filter selectable,
    line doubling to be compared with bwdif for 240p/288p).

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

- Run of the output-size design (a7fbe2f): mostly PASS. Working: three
  composite sizes; first read after loading paused (audio only) when PAL
  was detected while 720×480 was selected, then 720×576 while idle; PAL
  and later NTSC remembered (`std_composite`); 782 frames in 15 s at
  720×576; 640×480 from the encoder (530 frames in 10 s); input change and
  unplug/replug without losing the device; "Please connect"; idle CPU 0.
  FAILs: (1) step 7 no audio stream at the check: the bridge chose the
  mode before setting the encoder back from 640×480 to 720×576, started
  audio-only and restarted 1 s later; now it decides after setting the
  size. (2) step 9: test pattern without the two new state fields. (3)
  step 11: a sink left by an earlier, interrupted bridge was not removed;
  the bridge now removes leftovers before creating its own. (4) guvcview
  after the swap "more border at the bottom": guvcview had opened at
  640×480 (its remembered resolution), which the encoder delivers for PAL
  and NTSC alike, so the picture did not stop; the border is the encoder's
  640×480 picture of the NTSC source. guvcview offered 720×480 only after
  reopening (it reads the sizes when it opens the device). WARN
  "analog mode change failed: -67": the console swap (decoder not locked
  during the family change), detected again 1 s later (240p).
- Maintainer question (2026-11-18): "Does the driver really need to
  expose that many video devices? Don't we only need one?" Decision:
  Decisions 10.
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

This had 1 error. Can you maybe recommend another method by which I can use this in OBS while still making output and other settings available like a v4l2 device?

gchd-test T-060 20261118-204714 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-060-20261118-204714.log
== build
$ git fetch -q origin
$ git checkout -q feat/app-bridge
$ git pull -q --ff-only
commit: 929d000 TASK: T-060 decision: one video device, H.264 device optional, private bridge devices
commit: 929d000 TASK: T-060 decision: one video device, H.264 device optional, private bridge devices
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
[95797.659789] usbcore: deregistering interface driver elgato_gchd
[95800.821581] usb 1-2: USB disconnect, device number 99
[95804.744964] elgato_gchd: application device video0, frame input video1
[95804.744984] usbcore: registered new interface driver elgato_gchd
[95804.839486] usb 1-2: new high-speed USB device number 100 using xhci_hcd
[95805.251002] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[95805.251024] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[95805.251030] usb 1-2: Product: GameCapture HD
[95805.251035] usb 1-2: Manufacturer: Elgato
[95805.251039] usb 1-2: SerialNumber: 04107F1267
[95805.258769] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[95805.264006] elgato_gchd 1-2:1.0: attached to the application device video0
[95805.264029] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON

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
service:        running (pid 1713545)
$ bash /home/ff5/elgato-gchd/service/gchd-bridge status
application:    /dev/video0
driver state:   idle 720 480 60000 1001 video1 video2 0 0
service:        running (pid 1713545)
[PASS] bridge running
$ sh -c sudo dmesg | grep -c 'starting hardware boot'
0
[PASS] capture device off while no application streams
bridge CPU while idle: 0 ticks in 20 s
$ echo 0
0
[PASS] bridge sleeps while idle (< 5 ticks = 0.05 s CPU in 20 s)
--- dmesg step 2
[95833.751628] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[95833.751669] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== step 3: first read after loading: PAL detected while streaming 720x480, then the format follows
$ cat /etc/modprobe.d/elgato-gchd-settings.conf
options elgato_gchd input=2 output=0 format=0 bitrate=0 bitrate_mode=1 stretch=0 h264_profile=4 h264_level=0 std_composite=0 std_svideo=0
$ cat /home/ff5/gchd-test-logs/T-060-20261118-204714/read1.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 331444 kb/s, 59.94 fps, 59.94 tbr, 1000k tbn
frame=    0 fps=0.0 q=0.0 Lsize=       0kB time=N/A bitrate=N/A speed=N/A    
$ grep -c No such device /home/ff5/gchd-test-logs/T-060-20261118-204714/read1.txt
0
[PASS] reader not cut off (no 'No such device')
$ tail -6 /home/ff5/gchd-test-logs/T-060-20261118-204714/bridge.log
Error initializing filters!
Error marking filters as finished
[out#0/video4linux2,v4l2 @ 0x5a2496479780] Nothing was written into output file, because at least one of its streams received no packets.
[out#1/pulse @ 0x5a249695df00] Nothing was written into output file, because at least one of its streams received no packets.
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
[95858.071321] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[95858.213665] elgato_gchd 1-2:1.0: transcoder defaults loaded
[95858.276622] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[95858.642819] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[95858.644930] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[95858.644931] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[95858.644933] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[95858.644936] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[95859.434700] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[95859.719254] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[95859.741046] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[95859.741307] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[95859.749831] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[95859.750569] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[95859.751189] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[95859.866829] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[95861.780628] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[95862.126830] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[95862.126831] elgato_gchd 1-2:1.0: waiting 1159 ms after the previous shutdown
[95863.435380] elgato_gchd 1-2:1.0: transcoder defaults loaded
[95863.495516] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[95863.573289] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[95863.630778] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[95863.632913] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[95863.632914] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[95863.632916] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[95863.632918] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[95864.415964] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[95864.649742] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[95864.670031] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[95864.670383] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[95864.676599] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[95864.677185] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[95864.678061] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[95864.795923] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[95869.702901] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95874.728665] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95879.754541] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95884.780797] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95889.806238] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95890.842416] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 4: read the source format: 720x576 YUYV, about 50 frames/s
$ cat /home/ff5/gchd-test-logs/T-060-20261118-204714/read2.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=  779 fps= 52 q=-0.0 Lsize=N/A time=00:00:14.92 bitrate=N/A speed=0.995x    
$ cat /home/ff5/gchd-test-logs/T-060-20261118-204714/read2.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=  779 fps= 52 q=-0.0 Lsize=N/A time=00:00:14.92 bitrate=N/A speed=0.995x    
[PASS] stream YUYV 720x576
$ cat /home/ff5/gchd-test-logs/T-060-20261118-204714/read2.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=  779 fps= 52 q=-0.0 Lsize=N/A time=00:00:14.92 bitrate=N/A speed=0.995x    
[PASS] frame count >= 450 in 15 s
$ sudo dmesg
[95894.127308] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1713880 comm=v4l2-ctl
[95894.167451] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1713886 comm=ffmpeg
[95894.167505] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[95894.183064] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[95894.183313] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
[95894.245684] elgato_gchd 1-2:1.0: transcoder defaults loaded
[95894.245845] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[95894.246095] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x04
[95894.247692] elgato_gchd 1-2:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.248978] elgato_gchd 1-2:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.250092] elgato_gchd 1-2:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.251270] elgato_gchd 1-2:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.252464] elgato_gchd 1-2:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.253693] elgato_gchd 1-2:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.254853] elgato_gchd 1-2:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.256023] elgato_gchd 1-2:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.257118] elgato_gchd 1-2:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.258252] elgato_gchd 1-2:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.259441] elgato_gchd 1-2:1.0: firmware chunk offset=327680 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.260690] elgato_gchd 1-2:1.0: firmware chunk offset=360448 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.262043] elgato_gchd 1-2:1.0: firmware chunk offset=393216 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.263288] elgato_gchd 1-2:1.0: firmware chunk offset=425984 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.264521] elgato_gchd 1-2:1.0: firmware chunk offset=458752 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.265724] elgato_gchd 1-2:1.0: firmware chunk offset=491520 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.266930] elgato_gchd 1-2:1.0: firmware chunk offset=524288 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.268304] elgato_gchd 1-2:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.269688] elgato_gchd 1-2:1.0: firmware chunk offset=589824 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.271016] elgato_gchd 1-2:1.0: firmware chunk offset=622592 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.272244] elgato_gchd 1-2:1.0: firmware chunk offset=655360 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.273396] elgato_gchd 1-2:1.0: firmware chunk offset=688128 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.274616] elgato_gchd 1-2:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.275898] elgato_gchd 1-2:1.0: firmware chunk offset=753664 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.277050] elgato_gchd 1-2:1.0: firmware chunk offset=786432 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.278306] elgato_gchd 1-2:1.0: firmware chunk offset=819200 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.279533] elgato_gchd 1-2:1.0: firmware chunk offset=851968 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.280766] elgato_gchd 1-2:1.0: firmware chunk offset=884736 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.281980] elgato_gchd 1-2:1.0: firmware chunk offset=917504 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.283154] elgato_gchd 1-2:1.0: firmware chunk offset=950272 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.284405] elgato_gchd 1-2:1.0: firmware chunk offset=983040 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.285621] elgato_gchd 1-2:1.0: firmware chunk offset=1015808 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.286859] elgato_gchd 1-2:1.0: firmware chunk offset=1048576 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.288068] elgato_gchd 1-2:1.0: firmware chunk offset=1081344 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.289310] elgato_gchd 1-2:1.0: firmware chunk offset=1114112 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.290606] elgato_gchd 1-2:1.0: firmware chunk offset=1146880 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.291827] elgato_gchd 1-2:1.0: firmware chunk offset=1179648 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.293031] elgato_gchd 1-2:1.0: firmware chunk offset=1212416 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.294156] elgato_gchd 1-2:1.0: firmware chunk offset=1245184 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.295482] elgato_gchd 1-2:1.0: firmware chunk offset=1277952 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.296687] elgato_gchd 1-2:1.0: firmware chunk offset=1310720 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.297997] elgato_gchd 1-2:1.0: firmware chunk offset=1343488 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.299174] elgato_gchd 1-2:1.0: firmware chunk offset=1376256 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.300444] elgato_gchd 1-2:1.0: firmware chunk offset=1409024 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.301679] elgato_gchd 1-2:1.0: firmware chunk offset=1441792 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.302887] elgato_gchd 1-2:1.0: firmware chunk offset=1474560 size=26224 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[95894.307113] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[95894.393747] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[95894.441615] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x0f mode=0xf
[95894.443723] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0x6e 0x98:F5=0x62
[95894.443725] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[95894.505604] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x07 mode=0x7
[95894.514190] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[95894.516372] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[95894.516374] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[95894.516376] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[95894.516378] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[95895.316818] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[95895.316981] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[95895.551723] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[95895.567072] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[95895.567191] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[95895.567291] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[95895.572732] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[95895.573352] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[95895.573748] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[95895.573866] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[95895.574070] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[95895.574527] elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0150 ack=1
[95895.575309] elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[95895.575599] elgato_gchd 1-2:1.0: START poll=1 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[95895.575866] elgato_gchd 1-2:1.0: START poll=2 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[95895.576166] elgato_gchd 1-2:1.0: START poll=3 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[95895.576443] elgato_gchd 1-2:1.0: START poll=4 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[95895.576713] elgato_gchd 1-2:1.0: START poll=5 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[95895.577003] elgato_gchd 1-2:1.0: START poll=6 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[95895.577281] elgato_gchd 1-2:1.0: START poll=7 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[95895.577573] elgato_gchd 1-2:1.0: START poll=8 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[95895.577844] elgato_gchd 1-2:1.0: START poll=9 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[95895.578195] elgato_gchd 1-2:1.0: START poll=10 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[95895.578472] elgato_gchd 1-2:1.0: START poll=11 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[95895.578745] elgato_gchd 1-2:1.0: START poll=12 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[95895.579051] elgato_gchd 1-2:1.0: START poll=13 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[95895.579321] elgato_gchd 1-2:1.0: START poll=14 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[95895.579602] elgato_gchd 1-2:1.0: START poll=15 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[95895.604804] elgato_gchd 1-2:1.0: START poll=100 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[95895.633703] elgato_gchd 1-2:1.0: START poll=200 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[95895.663852] elgato_gchd 1-2:1.0: START poll=300 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[95895.700637] elgato_gchd 1-2:1.0: START poll=400 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[95895.712462] elgato_gchd 1-2:1.0: START poll=438 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0150 ack=1
[95895.712911] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[95896.110330] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[95896.156091] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[95896.200864] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[95896.245796] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[95896.291112] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[95900.576553] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95905.600471] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95910.628619] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[PASS] capture device started
--- dmesg step 4
[95894.167505] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[95894.245684] elgato_gchd 1-2:1.0: transcoder defaults loaded
[95894.307113] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[95894.393747] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[95894.514190] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[95894.516372] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[95894.516374] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[95894.516376] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[95894.516378] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[95895.316818] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[95895.551723] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[95895.567072] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[95895.567291] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[95895.572732] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[95895.573352] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[95895.573748] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[95895.712911] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[95900.576553] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95905.600471] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95910.628619] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 5: application selects 640x480: the encoder delivers it (no scaling)
$ cat /home/ff5/gchd-test-logs/T-060-20261118-204714/read3.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 640x480, 122880 kb/s, 25 fps, 25 tbr, 1000k tbn
frame=  528 fps= 53 q=-0.0 Lsize=N/A time=00:00:09.92 bitrate=N/A speed=0.994x    
$ cat /home/ff5/gchd-test-logs/T-060-20261118-204714/read3.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 640x480, 122880 kb/s, 25 fps, 25 tbr, 1000k tbn
frame=  528 fps= 53 q=-0.0 Lsize=N/A time=00:00:09.92 bitrate=N/A speed=0.994x    
[PASS] stream YUYV 640x480, 25 frames/s
$ cat /home/ff5/gchd-test-logs/T-060-20261118-204714/read3.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 640x480, 122880 kb/s, 25 fps, 25 tbr, 1000k tbn
frame=  528 fps= 53 q=-0.0 Lsize=N/A time=00:00:09.92 bitrate=N/A speed=0.994x    
[PASS] frames delivered (>= 150 in 10 s)
$ tail -4 /home/ff5/gchd-test-logs/T-060-20261118-204714/bridge.log
[h264 @ 0x5b6eb4873640] mmco: unref short failure
[h264 @ 0x5b6eb476a600] mmco: unref short failure
gchd-bridge: capture stopped
gchd-bridge: capture (video): /dev/video2 -> /dev/video1 (640x480)
$ v4l2-ctl -d /dev/video0 -v width=720,height=576
$ app_state
idle 720 576 50 1 video1 video2 640 480
[PASS] 720x576 selected again
--- dmesg step 5
[95913.472513] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[95913.653389] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[95913.653390] elgato_gchd 1-2:1.0: waiting 1327 ms after the previous shutdown
[95915.055658] elgato_gchd 1-2:1.0: transcoder defaults loaded
[95915.112421] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[95915.203644] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[95915.261665] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[95915.263770] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[95915.263771] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[95915.263773] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[95915.263775] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[95916.057841] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[95916.288911] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[95916.304393] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[95916.304633] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[95916.310114] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[95916.310618] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[95916.311092] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[95916.430292] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[95916.687776] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[95918.317050] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[95918.317309] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[95918.323195] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[95918.323724] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[95918.324154] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[95918.463087] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[95918.463091] elgato_gchd 1-2:1.0: analog mode now 576i, output 1
[95918.855676] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[95923.331159] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95928.363898] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 6: application closed: capture device off
$ sudo dmesg
[95930.041056] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[95930.041347] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[95930.882000] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[95930.883613] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[95931.058135] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[95931.058897] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[95931.059327] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
[95931.078810] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[PASS] hardware shut down after the application stopped
$ app_state
idle 720 576 50 1 video1 video2 640 480
[PASS] state idle
--- dmesg step 6
[95931.078810] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 7: audio while capturing
$ sink_streams
stream into elgato_gchd: 2046	1999	2045	PipeWire	s16le 2ch 48000Hz
[PASS] the bridge plays into the audio sink
$ ffmpeg -hide_banner -nostdin -f pulse -i elgato_gchd_audio -t 4 -af volumedetect -f null -
[aist#0:0/pcm_s16le @ 0x5ce797908640] Guessed Channel Layout: stereo
Input #0, pulse, from 'elgato_gchd_audio':
  Duration: N/A, start: 1795031387.907907, bitrate: 1536 kb/s
  Stream #0:0: Audio: pcm_s16le, 48000 Hz, 2 channels, s16, 1536 kb/s
[Parsed_volumedetect_0 @ 0x5ce79790a000] n_samples: 0
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
size=N/A time=00:00:01.11 bitrate=N/A speed=1.04x    
size=N/A time=00:00:01.60 bitrate=N/A speed=1.02x    
size=N/A time=00:00:02.10 bitrate=N/A speed=1.01x    
size=N/A time=00:00:02.60 bitrate=N/A speed=   1x    
size=N/A time=00:00:03.16 bitrate=N/A speed=1.01x    
size=N/A time=00:00:03.66 bitrate=N/A speed=1.01x    
[out#0/null @ 0x5ce797905b00] video:0kB audio:756kB subtitle:0kB other streams:0kB global headers:0kB muxing overhead: unknown
size=N/A time=00:00:03.96 bitrate=N/A speed=0.977x    
[Parsed_volumedetect_0 @ 0x5ce79790ca80] n_samples: 393600
[Parsed_volumedetect_0 @ 0x5ce79790ca80] mean_volume: -85.5 dB
[Parsed_volumedetect_0 @ 0x5ce79790ca80] max_volume: -74.7 dB
[Parsed_volumedetect_0 @ 0x5ce79790ca80] histogram_74db: 1
[Parsed_volumedetect_0 @ 0x5ce79790ca80] histogram_75db: 0
[Parsed_volumedetect_0 @ 0x5ce79790ca80] histogram_76db: 86
[Parsed_volumedetect_0 @ 0x5ce79790ca80] histogram_77db: 0
[Parsed_volumedetect_0 @ 0x5ce79790ca80] histogram_78db: 3452
[PASS] audio source delivers samples
--- dmesg step 7
[95936.185985] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[95936.313947] elgato_gchd 1-2:1.0: transcoder defaults loaded
[95936.377929] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[95936.483557] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[95936.550707] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[95936.553117] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[95936.553122] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[95936.553125] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[95936.553128] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[95937.346365] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[95937.590465] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[95937.606363] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[95937.606576] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[95937.612424] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[95937.613021] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[95937.613752] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[95937.752355] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[95942.615300] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95947.641017] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95952.667574] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 8: input change on the application device while reading
$ v4l2-ctl -d /dev/video0 -i 3
Video input set to 3 (S-Video: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ cat /home/ff5/gchd-test-logs/T-060-20261118-204714/read4.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=  925 fps= 31 q=-0.0 Lsize=N/A time=00:00:29.96 bitrate=N/A speed=0.997x    
$ grep -c No such device /home/ff5/gchd-test-logs/T-060-20261118-204714/read4.txt
0
[PASS] reader kept the device (no 'No such device')
$ cat /home/ff5/gchd-test-logs/T-060-20261118-204714/read4.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=  925 fps= 31 q=-0.0 Lsize=N/A time=00:00:29.96 bitrate=N/A speed=0.997x    
[PASS] reader got frames (>= 600 in 30 s)
$ v4l2-ctl -d /dev/video2 --get-input
Video input : 2 (Composite: ok)
[PASS] input composite on the H.264 device too
$ tail -10 /home/ff5/gchd-test-logs/T-060-20261118-204714/bridge.log
gchd-bridge: capture (video): /dev/video2 -> /dev/video1 (640x480)
gchd-bridge: capture stopped
gchd-bridge: capture (video): /dev/video2 -> /dev/video1 (720x576)
[h264 @ 0x5e59f83ac540] mmco: unref short failure
[h264 @ 0x5e59f83cce00] mmco: unref short failure
gchd-bridge: capture stopped
gchd-bridge: capture (video): /dev/video2 -> /dev/video1 (720x576)
[h264 @ 0x566a667153c0] mmco: unref short failure
[h264 @ 0x566a66735d00] mmco: unref short failure
[h264 @ 0x566a66747d40] error while decoding MB 3 28, bytestream -12
--- dmesg step 8
[95954.758299] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[95955.286294] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[95955.286296] elgato_gchd 1-2:1.0: waiting 983 ms after the previous shutdown
[95956.337807] elgato_gchd 1-2:1.0: transcoder defaults loaded
[95956.397659] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[95956.494884] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[95956.553683] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[95956.555905] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[95956.555907] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[95956.555909] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[95956.555911] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[95957.330992] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[95957.575765] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[95957.591973] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[95957.592201] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[95957.598300] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[95957.598958] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[95957.599456] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[95957.717861] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[95962.625181] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95963.603224] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=1 streaming=1
[95964.507605] elgato_gchd 1-2:1.0: input selected: input=3; detection scheduled
[95964.613849] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[95964.728038] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[95969.539234] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=1 streaming=1
[95969.539992] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[95969.827965] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[95969.997376] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[95970.000137] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[95970.000142] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[95970.000145] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[95970.000148] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[95970.234607] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[95970.249870] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[95970.250140] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[95970.256593] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[95970.257145] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[95970.257871] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[95970.395685] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[95975.268540] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95980.305095] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 9: unplug and replug while reading
PROMPT: Unplug the Elgato (or detach it in VirtualBox) now, wait 5 s, then plug it in again.
$ app_state
streaming 720 576 50 1 video1 video2 720 576
$ cat /home/ff5/gchd-test-logs/T-060-20261118-204714/read5.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame= 1696 fps= 38 q=-0.0 Lsize=N/A time=00:00:44.98 bitrate=N/A speed=0.999x    
$ grep -c No such device /home/ff5/gchd-test-logs/T-060-20261118-204714/read5.txt
0
[PASS] reader kept the device through unplug/replug
$ cat /home/ff5/gchd-test-logs/T-060-20261118-204714/read5.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame= 1696 fps= 38 q=-0.0 Lsize=N/A time=00:00:44.98 bitrate=N/A speed=0.999x    
[PASS] frames throughout (>= 900 in 45 s)
$ app_state
idle 720 576 50 1 video1 video2 720 576
[PASS] Game Capture HD attached again
$ tail -10 /home/ff5/gchd-test-logs/T-060-20261118-204714/bridge.log
[h264 @ 0x566a66747d40] error while decoding MB 3 28, bytestream -12
gchd-bridge: capture stopped
gchd-bridge: capture (video): /dev/video2 -> /dev/video1 (720x576)
gchd-bridge: now 720x576, 50/1, stream device -, encoder 0x0: video
gchd-bridge: ffmpeg 1714771 did not stop; killing it
gchd-bridge: capture stopped
gchd-bridge: no Game Capture HD: picture 720x576 -> /dev/video1
gchd-bridge: now 720x576, 50/1, stream device video2, encoder 0x0: video
gchd-bridge: capture stopped
gchd-bridge: capture (video): /dev/video2 -> /dev/video1 (720x576)
--- dmesg step 9
[95984.816870] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[95985.427724] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[95985.427725] elgato_gchd 1-2:1.0: waiting 895 ms after the previous shutdown
[95986.473112] elgato_gchd 1-2:1.0: transcoder defaults loaded
[95986.528756] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[95986.627737] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[95986.746078] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[95986.748279] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[95986.748281] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[95986.748283] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[95986.748286] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[95987.489174] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[95987.715998] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[95987.730348] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[95987.730537] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[95987.735637] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[95987.736388] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[95987.736740] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[95987.876116] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[95992.741688] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[95996.246732] usb 1-2: USB disconnect, device number 100
[96002.416047] usb 1-2: new high-speed USB device number 101 using xhci_hcd
[96002.800699] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[96002.800719] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[96002.800725] usb 1-2: Product: GameCapture HD
[96002.800730] usb 1-2: Manufacturer: Elgato
[96002.800734] usb 1-2: SerialNumber: 04107F1267
[96002.808257] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[96002.811240] elgato_gchd 1-2:1.0: attached to the application device video0
[96002.811258] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[96003.437046] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[96003.612300] elgato_gchd 1-2:1.0: transcoder defaults loaded
[96003.669502] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[96004.094247] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[96004.096276] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[96004.096278] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[96004.096279] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[96004.096281] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[96004.851811] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[96005.074847] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[96005.092479] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[96005.092719] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[96005.098575] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[96005.099400] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[96005.099915] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[96005.238276] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[96010.102767] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96015.126873] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96020.153498] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96025.179039] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=448 bytes=29360128 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 10: guvcview
PROMPT: Start in a second terminal: guvcview -d /dev/video0   (leave it open; resolution 720x576 if it asks)
[PASS] guvcview shows the PS1 picture, smooth, without interlacing lines?
PROMPT: Swap the PS1 for the N64 (NTSC) while guvcview stays open; wait about 15 s.
$ app_state
streaming 720 576 50 1 video1 video2 720 480
[FAIL] the picture stands still (never squeezed, cropped or with borders), and guvcview did not close or show an error? -- it froze
PROMPT: In guvcview select 720x480 (Video tab, resolution), or close and reopen guvcview; wait about 10 s.
[PASS] guvcview shows the N64 picture (720x480)?
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
$ tail -20 /home/ff5/gchd-test-logs/T-060-20261118-204714/bridge.log
[aist#0:1/mp2 @ 0x5ea5275df5c0] Error submitting packet to decoder: Invalid data found when processing input
gchd-bridge: capture stopped
gchd-bridge: capture (video): /dev/video2 -> /dev/video1 (720x480)
[h264 @ 0x59d57f0f49c0] decode_slice_header error
[h264 @ 0x59d57f0f49c0] mmco: unref short failure
[h264 @ 0x59d57f0196c0] mmco: unref short failure
gchd-bridge: now 720x480, 60000/1001, stream device -, encoder 0x0: video
gchd-bridge: ffmpeg 1715841 did not stop; killing it
gchd-bridge: capture stopped
gchd-bridge: no Game Capture HD: picture 720x480 -> /dev/video1
gchd-bridge: now 720x480, 60000/1001, stream device video2, encoder 0x0: video
gchd-bridge: capture stopped
gchd-bridge: capture (video): /dev/video2 -> /dev/video1 (720x480)
[h264 @ 0x649736a12500] error while decoding MB 39 25, bytestream -16
[mp2 @ 0x6497368ec840] Header missing
[aist#0:1/mp2 @ 0x6497369af280] Error submitting packet to decoder: Invalid data found when processing input
[h264 @ 0x649736a04ec0] error while decoding MB 13 21, bytestream -26
[mp2 @ 0x6497368ec840] Header missing
[aist#0:1/mp2 @ 0x6497369af280] Error submitting packet to decoder: Invalid data found when processing input
gchd-bridge: capture stopped
--- dmesg step 10
[96029.975949] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[96041.280872] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[96041.388551] elgato_gchd 1-2:1.0: transcoder defaults loaded
[96041.447490] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[96041.555609] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[96041.703732] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[96041.706430] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[96041.785943] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[96041.819785] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[96041.822895] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[96041.822900] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[96041.822903] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[96041.822924] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[96042.610344] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[96042.860096] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[96042.876270] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[96042.876474] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[96042.881990] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[96042.882543] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[96042.883246] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[96043.002085] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[96047.910753] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96052.935815] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96057.962106] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96062.986372] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96068.012734] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96073.049988] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=679 bytes=44498944 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96076.161264] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=e CA=ee E7=d9); configuring again
[96077.624720] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[96077.624953] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[96077.630379] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[96077.631089] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[96077.631545] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[96077.747541] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[96077.747547] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[96079.591131] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[96080.115772] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[96080.115773] elgato_gchd 1-2:1.0: waiting 976 ms after the previous shutdown
[96081.241855] elgato_gchd 1-2:1.0: transcoder defaults loaded
[96081.300764] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[96081.422592] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[96081.482654] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[96081.584532] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[96081.635231] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[96081.637347] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=89); waiting up to 5000 ms
[96081.729649] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[96081.839616] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[96081.991249] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[96081.993846] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[96081.993850] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[96081.993853] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[96081.993855] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[96082.775174] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[96082.897119] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[96082.918761] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[96082.919123] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[96082.942536] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[96082.943124] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[96082.943534] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[96083.074921] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[96087.954138] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96092.979456] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96098.000943] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96101.816764] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[96103.025346] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=443 bytes=29032448 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96103.340129] elgato_gchd 1-2:1.0: back-end not settled: status 0xf2
[96103.372460] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[96103.372837] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[96103.380959] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[96103.381728] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[96103.382397] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[96103.478622] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[96103.478625] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[96108.401130] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96113.424906] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96118.447101] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96123.472233] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96128.495198] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96133.021744] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[96133.517401] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=680 bytes=44564480 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96134.664923] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[96134.665268] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[96134.671726] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[96134.672538] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[96134.673363] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[96134.787570] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[96134.787573] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[96139.710212] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96144.731867] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96149.756104] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96154.779231] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96159.802206] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96164.825224] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=680 bytes=44564480 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96166.637761] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[96168.308705] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[96168.308949] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[96168.314755] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[96168.315503] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[96168.316122] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[96168.431599] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[96168.431604] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[96170.267948] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[96171.163809] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[96171.163810] elgato_gchd 1-2:1.0: waiting 607 ms after the previous shutdown
[96171.999309] elgato_gchd 1-2:1.0: transcoder defaults loaded
[96172.066822] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[96172.175217] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[96172.457146] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[96172.495649] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[96172.497788] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[96172.596351] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[96172.599184] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[96172.690262] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[96172.782251] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[96172.785098] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[96172.785102] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[96172.785106] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[96172.785109] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[96173.573644] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[96174.546186] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[96174.562053] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[96174.562277] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[96174.567435] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[96174.568074] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[96174.568504] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[96174.684670] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[96179.608045] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96184.630521] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96189.655897] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96194.677826] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96195.079814] usb 1-2: USB disconnect, device number 101
[96213.843098] usb 1-2: new high-speed USB device number 102 using xhci_hcd
[96214.216862] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[96214.216870] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[96214.216872] usb 1-2: Product: GameCapture HD
[96214.216874] usb 1-2: Manufacturer: Elgato
[96214.216876] usb 1-2: SerialNumber: 04107F1267
[96214.219880] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[96214.221987] elgato_gchd 1-2:1.0: attached to the application device video0
[96214.222043] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[96214.834510] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[96214.969710] elgato_gchd 1-2:1.0: transcoder defaults loaded
[96215.034068] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[96215.412670] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[96215.415321] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[96215.415323] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[96215.415325] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[96215.415327] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[96216.249144] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[96216.367835] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[96216.386113] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[96216.386463] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[96216.408222] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[96216.408824] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[96216.410861] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[96216.525548] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[96218.462750] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[96218.487507] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[96218.489413] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[96218.489415] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[96218.489417] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[96218.489419] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[96218.712635] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[96218.734125] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[96218.734425] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[96218.740560] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[96218.741298] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[96218.741883] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[96218.857158] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[96223.778758] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96228.801694] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[96231.042004] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[96232.482635] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[96232.482941] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[96232.488285] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[96232.488797] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[96232.489374] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[96232.605476] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[96232.605494] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[96237.382030] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 11: bridge stopped
$ kill 1713545
$ sh -c pactl list short sources | grep -c elgato_gchd
0
[PASS] audio source removed
--- dmesg step 11


=== summary (T-060, 929d000 TASK: T-060 decision: one video device, H.264 device optional, private bridge devices)
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
PASS | step 7 audio while capturing | the bridge plays into the audio sink
PASS | step 7 audio while capturing | audio source delivers samples
PASS | step 8 input change on the application device while reading | reader kept the device (no 'No such device')
PASS | step 8 input change on the application device while reading | reader got frames (>= 600 in 30 s)
PASS | step 8 input change on the application device while reading | input composite on the H.264 device too
PASS | step 9 unplug and replug while reading | reader kept the device through unplug/replug
PASS | step 9 unplug and replug while reading | frames throughout (>= 900 in 45 s)
PASS | step 9 unplug and replug while reading | Game Capture HD attached again
PASS | step 10 guvcview | guvcview shows the PS1 picture, smooth, without interlacing lines?
FAIL | step 10 guvcview | the picture stands still (never squeezed, cropped or with borders), and guvcview did not close or show an error? -- it froze
PASS | step 10 guvcview | guvcview shows the N64 picture (720x480)?
PASS | step 10 guvcview | NTSC remembered for composite
PASS | step 10 guvcview | guvcview shows 'Please connect the Elgato Game Capture HD'?
PASS | step 10 guvcview | guvcview shows the N64 picture again?
PASS | step 10 guvcview | idle format: the NTSC source 720x480, 59.94 frames/s
PASS | step 11 bridge stopped | audio source removed
