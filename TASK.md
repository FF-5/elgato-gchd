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
11. (Maintainer, 2026-11-18) A stand-alone OBS plugin is preferred for
    OBS (the Media Source needs the user to type the device); keep the
    driver as it is (application device + bridge for other programs).
    Decisions 10 on hold: the plugin uses the H.264 device, which stays
    visible. OBS runs in the VM (tests with OBS possible again).

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

- OBS plugin (`obs-plugin/`, source "Elgato Game Capture HD"): finds
  the H.264 device by name, applies input, output size, bitrate, mode,
  profile and stretch with V4L2 ioctls, reads the transport stream (poll,
  non-blocking, stoppable), decodes with FFmpeg, outputs I420 video and
  the audio with common timestamps; OBS deinterlacing Yadif 2x top field
  first set once; captures while shown. Built against libobs 30.0.2 (as
  in Ubuntu 24.04), no warnings. Offline with a test harness: a PAL
  576i25 H.264 + MP2 stream gives 73 of 75 frames (2 held by the decoder
  at the end of the file) and audio; PAL followed by NTSC gives 163
  frames, the size changes from 720×576 to 720×480 without a restart.
  A first version used AVFMT_FLAG_NOBUFFER, which drops the packets read
  while probing (the first keyframe): removed.
- Run 1786e0d: 32/33 PASS (the bridge fixes work: audio stream at once,
  leftover sink removed, replug state). FAIL: step 10 after the NTSC swap
  "it froze": to be clarified whether the picture stood still (intended:
  V4L2_EVENT_SOURCE_CHANGE, no frames until the new format) or guvcview
  itself hung.
- Maintainer question (2026-11-18): another method for OBS with the
  output and other settings available. Answered: (1) OBS Media Source on
  the H.264 device (input format mpegts; OBS decodes video and audio,
  follows size changes, deinterlaces itself; settings via v4l2-ctl or a
  small settings tool); (2) a native OBS source plugin with its own
  properties (input, output size, bitrate, deinterlacing), reading the
  H.264 device, OBS only. Recommended: try (1) first, then decide on (2)
  and on the application device (Decisions 10 on hold).
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

Ok, although I prefer simpler interfaces, let's move the kernel module to be a media source. We'll have to create a configuration GUI or TUI for the settings.

How much does the OBS plugin need from the kernel module? If it can be a stand-alone thing should we then maybe move the different sources to 2 folders under src?

Here is the last log:
gchd-test T-060 20261118-211812 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-060-20261118-211812.log
== build
$ git fetch -q origin
$ git checkout -q feat/app-bridge
$ git pull -q --ff-only
commit: 08ab082 obs-plugin: OBS source "Elgato Game Capture HD" (T-060)
commit: 08ab082 obs-plugin: OBS source "Elgato Game Capture HD" (T-060)
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
[97659.396737] usbcore: deregistering interface driver elgato_gchd
[97663.512199] usb 1-2: USB disconnect, device number 102
[97668.395585] usb 1-2: new high-speed USB device number 103 using xhci_hcd
[97668.773459] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[97668.773482] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[97668.773488] usb 1-2: Product: GameCapture HD
[97668.773493] usb 1-2: Manufacturer: Elgato
[97668.773497] usb 1-2: SerialNumber: 04107F1267
[97669.731097] elgato_gchd: application device video0, frame input video1
[97669.731122] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[97669.733933] elgato_gchd 1-2:1.0: attached to the application device video0
[97669.733943] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[97669.733964] usbcore: registered new interface driver elgato_gchd

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
service:        running (pid 1717847)
$ bash /home/ff5/elgato-gchd/service/gchd-bridge status
application:    /dev/video0
driver state:   idle 720 480 60000 1001 video1 video2 0 0
service:        running (pid 1717847)
[PASS] bridge running
$ sh -c sudo dmesg | grep -c 'starting hardware boot'
0
[PASS] capture device off while no application streams
bridge CPU while idle: 0 ticks in 20 s
$ echo 0
0
[PASS] bridge sleeps while idle (< 5 ticks = 0.05 s CPU in 20 s)
--- dmesg step 2
[97689.781824] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[97689.781864] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== step 3: first read after loading: PAL detected while streaming 720x480, then the format follows
$ cat /etc/modprobe.d/elgato-gchd-settings.conf
options elgato_gchd input=2 output=0 format=0 bitrate=0 bitrate_mode=1 stretch=0 h264_profile=4 h264_level=0 std_composite=0 std_svideo=0
$ cat /home/ff5/gchd-test-logs/T-060-20261118-211812/read1.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 331444 kb/s, 59.94 fps, 59.94 tbr, 1000k tbn
frame=    0 fps=0.0 q=0.0 Lsize=       0kB time=N/A bitrate=N/A speed=N/A    
$ grep -c No such device /home/ff5/gchd-test-logs/T-060-20261118-211812/read1.txt
0
[PASS] reader not cut off (no 'No such device')
$ tail -6 /home/ff5/gchd-test-logs/T-060-20261118-211812/bridge.log
Error initializing filters!
Error marking filters as finished
[out#0/video4linux2,v4l2 @ 0x60c3319fa780] Nothing was written into output file, because at least one of its streams received no packets.
[out#1/pulse @ 0x60c331edef00] Nothing was written into output file, because at least one of its streams received no packets.
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
[97714.072111] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[97714.203484] elgato_gchd 1-2:1.0: transcoder defaults loaded
[97714.265350] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[97714.658480] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[97714.660916] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[97714.660920] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[97714.660923] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[97714.660926] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[97715.456500] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[97715.693680] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[97715.709618] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[97715.709974] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[97715.716144] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[97715.716711] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[97715.717213] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[97715.855466] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[97717.767164] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[97718.307799] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[97718.307800] elgato_gchd 1-2:1.0: waiting 960 ms after the previous shutdown
[97719.351060] elgato_gchd 1-2:1.0: transcoder defaults loaded
[97719.409271] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[97719.497599] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[97719.621723] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[97719.624377] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[97719.624380] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[97719.624382] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[97719.624384] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[97720.412817] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[97720.665502] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[97720.687480] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[97720.687861] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[97720.694213] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[97720.694762] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[97720.695211] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[97720.813835] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[97725.720026] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97730.745907] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97735.772817] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97740.798586] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97745.824152] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97746.849393] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 4: read the source format: 720x576 YUYV, about 50 frames/s
$ cat /home/ff5/gchd-test-logs/T-060-20261118-211812/read2.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=  787 fps= 52 q=-0.0 Lsize=N/A time=00:00:14.98 bitrate=N/A speed=0.999x    
$ cat /home/ff5/gchd-test-logs/T-060-20261118-211812/read2.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=  787 fps= 52 q=-0.0 Lsize=N/A time=00:00:14.98 bitrate=N/A speed=0.999x    
[PASS] stream YUYV 720x576
$ cat /home/ff5/gchd-test-logs/T-060-20261118-211812/read2.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame=  787 fps= 52 q=-0.0 Lsize=N/A time=00:00:14.98 bitrate=N/A speed=0.999x    
[PASS] frame count >= 450 in 15 s
$ sudo dmesg
[97750.126094] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1718183 comm=v4l2-ctl
[97750.167189] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1718189 comm=ffmpeg
[97750.167240] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[97750.183031] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[97750.183235] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
[97750.251995] elgato_gchd 1-2:1.0: transcoder defaults loaded
[97750.252165] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[97750.252365] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x04
[97750.254139] elgato_gchd 1-2:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.255337] elgato_gchd 1-2:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.256525] elgato_gchd 1-2:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.257657] elgato_gchd 1-2:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.258849] elgato_gchd 1-2:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.260107] elgato_gchd 1-2:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.261341] elgato_gchd 1-2:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.262559] elgato_gchd 1-2:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.263686] elgato_gchd 1-2:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.264915] elgato_gchd 1-2:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.266041] elgato_gchd 1-2:1.0: firmware chunk offset=327680 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.267215] elgato_gchd 1-2:1.0: firmware chunk offset=360448 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.268384] elgato_gchd 1-2:1.0: firmware chunk offset=393216 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.269546] elgato_gchd 1-2:1.0: firmware chunk offset=425984 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.270706] elgato_gchd 1-2:1.0: firmware chunk offset=458752 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.271893] elgato_gchd 1-2:1.0: firmware chunk offset=491520 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.272985] elgato_gchd 1-2:1.0: firmware chunk offset=524288 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.274196] elgato_gchd 1-2:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.275456] elgato_gchd 1-2:1.0: firmware chunk offset=589824 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.276813] elgato_gchd 1-2:1.0: firmware chunk offset=622592 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.278083] elgato_gchd 1-2:1.0: firmware chunk offset=655360 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.279361] elgato_gchd 1-2:1.0: firmware chunk offset=688128 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.280608] elgato_gchd 1-2:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.282060] elgato_gchd 1-2:1.0: firmware chunk offset=753664 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.283393] elgato_gchd 1-2:1.0: firmware chunk offset=786432 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.284564] elgato_gchd 1-2:1.0: firmware chunk offset=819200 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.286010] elgato_gchd 1-2:1.0: firmware chunk offset=851968 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.287471] elgato_gchd 1-2:1.0: firmware chunk offset=884736 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.288756] elgato_gchd 1-2:1.0: firmware chunk offset=917504 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.290121] elgato_gchd 1-2:1.0: firmware chunk offset=950272 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.291387] elgato_gchd 1-2:1.0: firmware chunk offset=983040 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.292483] elgato_gchd 1-2:1.0: firmware chunk offset=1015808 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.293572] elgato_gchd 1-2:1.0: firmware chunk offset=1048576 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.294817] elgato_gchd 1-2:1.0: firmware chunk offset=1081344 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.296257] elgato_gchd 1-2:1.0: firmware chunk offset=1114112 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.297504] elgato_gchd 1-2:1.0: firmware chunk offset=1146880 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.298801] elgato_gchd 1-2:1.0: firmware chunk offset=1179648 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.300047] elgato_gchd 1-2:1.0: firmware chunk offset=1212416 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.301306] elgato_gchd 1-2:1.0: firmware chunk offset=1245184 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.302702] elgato_gchd 1-2:1.0: firmware chunk offset=1277952 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.303960] elgato_gchd 1-2:1.0: firmware chunk offset=1310720 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.305348] elgato_gchd 1-2:1.0: firmware chunk offset=1343488 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.306652] elgato_gchd 1-2:1.0: firmware chunk offset=1376256 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.308009] elgato_gchd 1-2:1.0: firmware chunk offset=1409024 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.309211] elgato_gchd 1-2:1.0: firmware chunk offset=1441792 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.310477] elgato_gchd 1-2:1.0: firmware chunk offset=1474560 size=26224 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[97750.313999] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[97750.408155] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[97750.453038] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x07 mode=0x7
[97750.461556] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[97750.463867] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[97750.463873] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[97750.463875] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[97750.463877] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[97751.230394] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[97751.230660] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[97751.452227] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[97751.468486] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[97751.468639] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[97751.469178] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[97751.474443] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[97751.474968] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[97751.475391] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[97751.475512] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[97751.475730] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[97751.476477] elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0150 ack=1
[97751.477286] elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[97751.477573] elgato_gchd 1-2:1.0: START poll=1 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[97751.478014] elgato_gchd 1-2:1.0: START poll=2 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[97751.478318] elgato_gchd 1-2:1.0: START poll=3 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[97751.478606] elgato_gchd 1-2:1.0: START poll=4 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[97751.479132] elgato_gchd 1-2:1.0: START poll=5 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[97751.479398] elgato_gchd 1-2:1.0: START poll=6 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[97751.479802] elgato_gchd 1-2:1.0: START poll=7 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[97751.480059] elgato_gchd 1-2:1.0: START poll=8 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[97751.480308] elgato_gchd 1-2:1.0: START poll=9 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[97751.480574] elgato_gchd 1-2:1.0: START poll=10 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[97751.481167] elgato_gchd 1-2:1.0: START poll=11 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[97751.481431] elgato_gchd 1-2:1.0: START poll=12 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[97751.481851] elgato_gchd 1-2:1.0: START poll=13 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[97751.482120] elgato_gchd 1-2:1.0: START poll=14 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[97751.482431] elgato_gchd 1-2:1.0: START poll=15 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[97751.505062] elgato_gchd 1-2:1.0: START poll=100 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[97751.532120] elgato_gchd 1-2:1.0: START poll=200 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[97751.563396] elgato_gchd 1-2:1.0: START poll=300 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[97751.590567] elgato_gchd 1-2:1.0: START poll=400 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[97751.613841] elgato_gchd 1-2:1.0: START poll=479 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0150 ack=1
[97751.614258] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[97752.011614] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[97752.057608] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[97752.102268] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[97752.147216] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[97752.192886] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[97756.476422] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97761.503715] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97766.530558] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[PASS] capture device started
--- dmesg step 4
[97750.167240] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[97750.251995] elgato_gchd 1-2:1.0: transcoder defaults loaded
[97750.313999] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[97750.408155] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[97750.461556] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[97750.463867] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[97750.463873] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[97750.463875] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[97750.463877] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[97751.230394] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[97751.452227] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[97751.468486] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[97751.469178] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[97751.474443] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[97751.474968] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[97751.475391] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[97751.614258] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[97756.476422] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97761.503715] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97766.530558] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 5: application selects 640x480: the encoder delivers it (no scaling)
$ cat /home/ff5/gchd-test-logs/T-060-20261118-211812/read3.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 640x480, 122880 kb/s, 25 fps, 25 tbr, 1000k tbn
frame=  532 fps= 53 q=-0.0 Lsize=N/A time=00:00:09.96 bitrate=N/A speed=0.994x    
$ cat /home/ff5/gchd-test-logs/T-060-20261118-211812/read3.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 640x480, 122880 kb/s, 25 fps, 25 tbr, 1000k tbn
frame=  532 fps= 53 q=-0.0 Lsize=N/A time=00:00:09.96 bitrate=N/A speed=0.994x    
[PASS] stream YUYV 640x480, 25 frames/s
$ cat /home/ff5/gchd-test-logs/T-060-20261118-211812/read3.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 640x480, 122880 kb/s, 25 fps, 25 tbr, 1000k tbn
frame=  532 fps= 53 q=-0.0 Lsize=N/A time=00:00:09.96 bitrate=N/A speed=0.994x    
[PASS] frames delivered (>= 150 in 10 s)
$ tail -4 /home/ff5/gchd-test-logs/T-060-20261118-211812/bridge.log
gchd-bridge: capture stopped
gchd-bridge: capture (video): /dev/video2 -> /dev/video1 (720x576)
gchd-bridge: capture stopped
gchd-bridge: capture (video): /dev/video2 -> /dev/video1 (640x480)
$ v4l2-ctl -d /dev/video0 -v width=720,height=576
$ app_state
idle 720 576 50 1 video1 video2 640 480
[PASS] 720x576 selected again
--- dmesg step 5
[97769.508393] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[97770.083399] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[97770.083401] elgato_gchd 1-2:1.0: waiting 926 ms after the previous shutdown
[97771.177150] elgato_gchd 1-2:1.0: transcoder defaults loaded
[97771.249312] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[97771.424371] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[97771.475633] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[97771.501840] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[97771.504969] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[97771.504974] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[97771.504977] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[97771.504980] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[97772.334614] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[97772.600439] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[97772.622914] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[97772.623273] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[97772.630290] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[97772.631545] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[97772.632268] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[97772.768742] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[97773.022907] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[97774.764902] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[97774.765188] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[97774.771028] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[97774.771592] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[97774.772151] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[97774.891003] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[97774.891007] elgato_gchd 1-2:1.0: analog mode now 576i, output 1
[97775.283646] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[97779.801478] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97784.836151] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 6: application closed: capture device off
$ sudo dmesg
[97786.465959] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[97786.466221] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[97787.295635] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[97787.296529] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[97787.457556] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[97787.458460] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[97787.458693] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
[97787.478426] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[PASS] hardware shut down after the application stopped
$ app_state
idle 720 576 50 1 video1 video2 640 480
[PASS] state idle
--- dmesg step 6
[97787.478426] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 7: audio while capturing
$ sink_streams
stream into elgato_gchd: 2219	2173	2218	PipeWire	s16le 2ch 48000Hz
[PASS] the bridge plays into the audio sink
$ ffmpeg -hide_banner -nostdin -f pulse -i elgato_gchd_audio -t 4 -af volumedetect -f null -
[aist#0:0/pcm_s16le @ 0x55fc6ef2c640] Guessed Channel Layout: stereo
Input #0, pulse, from 'elgato_gchd_audio':
  Duration: N/A, start: 1795033244.331992, bitrate: 1536 kb/s
  Stream #0:0: Audio: pcm_s16le, 48000 Hz, 2 channels, s16, 1536 kb/s
[Parsed_volumedetect_0 @ 0x55fc6ef2e000] n_samples: 0
Stream mapping:
  Stream #0:0 -> #0:0 (pcm_s16le (native) -> pcm_s16le (native))
Output #0, null, to 'pipe:':
  Metadata:
    encoder         : Lavf60.16.100
  Stream #0:0: Audio: pcm_s16le, 48000 Hz, stereo, s16, 1536 kb/s
    Metadata:
      encoder         : Lavc60.31.102 pcm_s16le
size=       0kB time=00:00:00.00 bitrate=N/A speed=N/A    
size=N/A time=00:00:00.61 bitrate=N/A speed= 1.1x    
size=N/A time=00:00:01.17 bitrate=N/A speed= 1.1x    
size=N/A time=00:00:01.66 bitrate=N/A speed=1.06x    
size=N/A time=00:00:02.16 bitrate=N/A speed=1.03x    
size=N/A time=00:00:02.66 bitrate=N/A speed=1.02x    
size=N/A time=00:00:03.20 bitrate=N/A speed=1.03x    
size=N/A time=00:00:03.70 bitrate=N/A speed=1.02x    
[out#0/null @ 0x55fc6ef29b00] video:0kB audio:749kB subtitle:0kB other streams:0kB global headers:0kB muxing overhead: unknown
size=N/A time=00:00:03.95 bitrate=N/A speed=0.997x    
[Parsed_volumedetect_0 @ 0x55fc6ef30a80] n_samples: 388800
[Parsed_volumedetect_0 @ 0x55fc6ef30a80] mean_volume: -85.5 dB
[Parsed_volumedetect_0 @ 0x55fc6ef30a80] max_volume: -76.3 dB
[Parsed_volumedetect_0 @ 0x55fc6ef30a80] histogram_76db: 79
[Parsed_volumedetect_0 @ 0x55fc6ef30a80] histogram_77db: 0
[Parsed_volumedetect_0 @ 0x55fc6ef30a80] histogram_78db: 3449
[PASS] audio source delivers samples
--- dmesg step 7
[97792.614921] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[97792.705335] elgato_gchd 1-2:1.0: transcoder defaults loaded
[97792.770104] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[97792.852964] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[97792.978225] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[97792.980224] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[97792.980226] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[97792.980228] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[97792.980230] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[97793.766233] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[97794.241739] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[97794.261727] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[97794.261959] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[97794.267865] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[97794.268372] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[97794.269118] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[97794.407812] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[97799.271289] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97804.296761] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97809.323276] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 8: input change on the application device while reading
$ v4l2-ctl -d /dev/video0 -i 3
Video input set to 3 (S-Video: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ cat /home/ff5/gchd-test-logs/T-060-20261118-211812/read4.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame= 1057 fps= 35 q=-0.0 Lsize=N/A time=00:00:29.98 bitrate=N/A speed=0.998x    
$ grep -c No such device /home/ff5/gchd-test-logs/T-060-20261118-211812/read4.txt
0
[PASS] reader kept the device (no 'No such device')
$ cat /home/ff5/gchd-test-logs/T-060-20261118-211812/read4.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame= 1057 fps= 35 q=-0.0 Lsize=N/A time=00:00:29.98 bitrate=N/A speed=0.998x    
[PASS] reader got frames (>= 600 in 30 s)
$ v4l2-ctl -d /dev/video2 --get-input
Video input : 2 (Composite: ok)
[PASS] input composite on the H.264 device too
$ tail -10 /home/ff5/gchd-test-logs/T-060-20261118-211812/bridge.log
gchd-bridge: capture (video): /dev/video2 -> /dev/video1 (720x576)
[h264 @ 0x60cfa1942f00] mmco: unref short failure
[h264 @ 0x60cfa192f040] mmco: unref short failure
gchd-bridge: capture stopped
gchd-bridge: capture (video): /dev/video2 -> /dev/video1 (720x576)
[h264 @ 0x63ac4d2697c0] mmco: unref short failure
[h264 @ 0x63ac4d28a080] mmco: unref short failure
[h264 @ 0x63ac4d28a080] error while decoding MB 22 20, bytestream -12
[mp2 @ 0x63ac4d1d3940] Header missing
[aist#0:1/mp2 @ 0x63ac4d194a00] Error submitting packet to decoder: Invalid data found when processing input
--- dmesg step 8
[97811.229186] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[97811.450224] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[97811.450225] elgato_gchd 1-2:1.0: waiting 1280 ms after the previous shutdown
[97812.885825] elgato_gchd 1-2:1.0: transcoder defaults loaded
[97812.944367] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[97813.055883] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[97813.127607] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[97813.240250] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[97813.243291] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[97813.318221] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[97813.357760] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[97813.359867] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[97813.359868] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[97813.359870] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[97813.359873] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[97814.143546] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[97814.400604] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[97814.416031] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[97814.416368] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[97814.421863] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[97814.422446] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[97814.423075] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[97814.542031] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[97819.448952] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97820.251086] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=1 streaming=1
[97821.017095] elgato_gchd 1-2:1.0: input selected: input=3; detection scheduled
[97821.048330] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[97821.353470] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[97826.043709] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=1 streaming=1
[97826.046466] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[97826.348148] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[97826.519211] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[97826.521342] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[97826.521345] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[97826.521347] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[97826.521349] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[97826.758778] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[97826.774859] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[97826.775088] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[97826.780791] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[97826.781462] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[97826.781944] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[97826.900806] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[97831.818204] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97836.853701] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 9: unplug and replug while reading
PROMPT: Unplug the Elgato (or detach it in VirtualBox) now, wait 5 s, then plug it in again.
$ app_state
streaming 720 576 50 1 video1 video2 720 576
$ cat /home/ff5/gchd-test-logs/T-060-20261118-211812/read5.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame= 1980 fps= 44 q=-0.0 Lsize=N/A time=00:00:44.94 bitrate=N/A speed=0.997x    
$ grep -c No such device /home/ff5/gchd-test-logs/T-060-20261118-211812/read5.txt
0
[PASS] reader kept the device through unplug/replug
$ cat /home/ff5/gchd-test-logs/T-060-20261118-211812/read5.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x576, 331776 kb/s, 50 fps, 50 tbr, 1000k tbn
frame= 1980 fps= 44 q=-0.0 Lsize=N/A time=00:00:44.94 bitrate=N/A speed=0.997x    
[PASS] frames throughout (>= 900 in 45 s)
$ app_state
idle 720 576 50 1 video1 video2 720 576
[PASS] Game Capture HD attached again
$ tail -10 /home/ff5/gchd-test-logs/T-060-20261118-211812/bridge.log
[aist#0:1/mp2 @ 0x63ac4d194a00] Error submitting packet to decoder: Invalid data found when processing input
gchd-bridge: capture stopped
gchd-bridge: capture (video): /dev/video2 -> /dev/video1 (720x576)
gchd-bridge: now 720x576, 50/1, stream device -, encoder 0x0: video
gchd-bridge: ffmpeg 1719066 did not stop; killing it
gchd-bridge: capture stopped
gchd-bridge: no Game Capture HD: picture 720x576 -> /dev/video1
gchd-bridge: now 720x576, 50/1, stream device video2, encoder 0x0: video
gchd-bridge: capture stopped
gchd-bridge: capture (video): /dev/video2 -> /dev/video1 (720x576)
--- dmesg step 9
[97841.286518] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[97841.550909] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[97841.550911] elgato_gchd 1-2:1.0: waiting 1245 ms after the previous shutdown
[97842.878899] elgato_gchd 1-2:1.0: transcoder defaults loaded
[97842.949730] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[97843.047121] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[97843.171001] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[97843.173105] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[97843.173107] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[97843.173109] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[97843.173110] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[97843.947874] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[97844.175705] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[97844.191296] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[97844.191534] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[97844.197227] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[97844.197877] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[97844.198613] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[97844.337508] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[97849.201744] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97852.967491] usb 1-2: USB disconnect, device number 103
[97859.024738] usb 1-2: new high-speed USB device number 104 using xhci_hcd
[97859.404780] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[97859.404802] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[97859.404808] usb 1-2: Product: GameCapture HD
[97859.404813] usb 1-2: Manufacturer: Elgato
[97859.404818] usb 1-2: SerialNumber: 04107F1267
[97859.414644] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[97859.416955] elgato_gchd 1-2:1.0: attached to the application device video0
[97859.417425] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[97860.051662] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[97860.172541] elgato_gchd 1-2:1.0: transcoder defaults loaded
[97860.232559] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[97860.630397] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[97860.632478] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[97860.632480] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[97860.632482] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[97860.632484] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[97861.420217] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[97861.673926] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[97861.695366] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[97861.695677] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[97861.701281] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[97861.701929] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[97861.702371] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[97861.821155] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[97866.728982] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97871.755386] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97876.782442] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97881.808301] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97886.832914] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0

=== step 10: guvcview
PROMPT: Start in a second terminal: guvcview -d /dev/video0   (leave it open; resolution 720x576 if it asks)
[PASS] guvcview shows the PS1 picture, smooth, without interlacing lines?
PROMPT: Swap the PS1 for the N64 (NTSC) while guvcview stays open; wait about 15 s.
$ app_state
streaming 720 576 50 1 video1 video2 720 480
[FAIL] the picture stands still (never squeezed, cropped or with borders), and guvcview did not close or show an error? -- error v4l2_core can't grab image
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
$ tail -20 /home/ff5/gchd-test-logs/T-060-20261118-211812/bridge.log
[mp2 @ 0x57c7835ee600] Header missing
[aist#0:1/mp2 @ 0x57c783566580] Error submitting packet to decoder: Invalid data found when processing input
[mp2 @ 0x57c7835ee600] Header missing
[aist#0:1/mp2 @ 0x57c783566580] Error submitting packet to decoder: Invalid data found when processing input
[mp2 @ 0x57c7835ee600] Header missing
[aist#0:1/mp2 @ 0x57c783566580] Error submitting packet to decoder: Invalid data found when processing input
gchd-bridge: capture stopped
gchd-bridge: capture (video): /dev/video2 -> /dev/video1 (720x480)
[h264 @ 0x611f5856eb40] mmco: unref short failure
[h264 @ 0x611f585cf500] mmco: unref short failure
gchd-bridge: now 720x480, 60000/1001, stream device -, encoder 0x0: video
gchd-bridge: ffmpeg 1720258 did not stop; killing it
gchd-bridge: capture stopped
gchd-bridge: no Game Capture HD: picture 720x480 -> /dev/video1
gchd-bridge: now 720x480, 60000/1001, stream device video2, encoder 0x0: video
gchd-bridge: capture stopped
gchd-bridge: capture (video): /dev/video2 -> /dev/video1 (720x480)
[h264 @ 0x5e381ee694c0] mmco: unref short failure
[h264 @ 0x5e381eeb9500] mmco: unref short failure
gchd-bridge: capture stopped
--- dmesg step 10
[97891.835659] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=676 bytes=44302336 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=10
[97892.105147] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[97953.437328] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[97953.549052] elgato_gchd 1-2:1.0: transcoder defaults loaded
[97953.614576] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[97953.730790] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[97953.801858] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[97953.804264] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[97953.804265] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[97953.804267] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[97953.804269] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[97954.605418] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[97954.823195] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[97954.843321] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[97954.843539] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[97954.849094] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[97954.849534] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[97954.850148] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[97954.969096] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[97959.876022] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97964.902549] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97969.928661] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97974.955861] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97979.980954] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97985.007441] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=679 bytes=44498944 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97990.042080] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=794 bytes=52035584 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97995.078913] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=909 bytes=59572224 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[97995.578148] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=f CA=ce E7=f9); configuring again
[97997.075791] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[97997.076042] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[97997.081882] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[97997.082561] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[97997.083083] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[97997.181840] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[97997.181844] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[97999.042302] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[97999.324405] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[97999.324407] elgato_gchd 1-2:1.0: waiting 1220 ms after the previous shutdown
[98000.698150] elgato_gchd 1-2:1.0: transcoder defaults loaded
[98000.757719] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[98000.850488] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[98001.057317] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[98001.163047] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[98001.199154] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[98001.201267] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[98001.287983] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[98001.290173] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[98001.384910] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[98001.387203] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[98001.479773] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[98001.482673] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[98001.482678] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[98001.482681] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[98001.482684] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[98002.264833] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[98003.074189] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[98003.090003] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[98003.090316] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[98003.095946] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[98003.096655] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[98003.097084] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[98003.212741] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[98004.924608] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[98006.438243] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[98006.438469] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[98006.443434] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[98006.444002] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[98006.444461] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[98006.560854] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[98006.560857] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[98011.483932] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[98016.506871] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[98021.529025] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[98026.551297] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[98031.575387] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[98036.597489] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=680 bytes=44564480 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[98039.217780] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[98040.769251] elgato_gchd 1-2:1.0: back-end not settled: status 0xf2
[98040.793603] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[98040.793828] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[98040.800350] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[98040.800835] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[98040.801383] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[98040.916125] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[98040.916128] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[98045.840325] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[98050.863891] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[98055.885050] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[98060.910018] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[98065.930234] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[98070.037274] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[98071.016549] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=679 bytes=44498944 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[98071.601885] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[98071.602110] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[98071.608535] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[98071.609233] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[98071.609824] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[98071.741510] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[98071.741530] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[98076.621154] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[98081.644580] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[98086.665429] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[98091.690949] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[98096.714053] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[98101.797595] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=678 bytes=44433408 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=13
[98102.195713] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[98104.803149] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[98104.871769] elgato_gchd 1-2:1.0: transcoder defaults loaded
[98104.934712] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[98105.068003] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[98105.299327] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[98105.304579] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[98105.380247] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[98105.413629] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[98105.415850] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[98105.509750] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[98105.596136] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[98105.598793] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[98105.598797] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[98105.598805] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[98105.598808] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[98106.389406] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[98107.105766] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[98107.121484] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[98107.121721] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[98107.127171] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[98107.128000] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[98107.128404] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[98107.244010] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[98112.167925] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[98117.191098] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[98122.213424] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[98126.891015] usb 1-2: USB disconnect, device number 104
[98145.266474] usb 1-2: new high-speed USB device number 105 using xhci_hcd
[98145.638086] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[98145.638093] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[98145.638095] usb 1-2: Product: GameCapture HD
[98145.638096] usb 1-2: Manufacturer: Elgato
[98145.638097] usb 1-2: SerialNumber: 04107F1267
[98145.639522] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[98145.641285] elgato_gchd 1-2:1.0: attached to the application device video0
[98145.641303] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[98146.254603] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[98146.386662] elgato_gchd 1-2:1.0: transcoder defaults loaded
[98146.450772] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[98146.812888] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[98146.814904] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[98146.814906] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[98146.814907] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[98146.814910] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[98147.603232] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[98147.728218] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[98147.745071] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[98147.745289] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[98147.766926] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[98147.767697] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[98147.768123] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[98147.884249] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[98152.807822] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[98157.830376] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[98162.852665] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[98167.086663] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 11: bridge stopped
$ kill 1717847
$ sh -c pactl list short sources | grep -c elgato_gchd
0
[PASS] audio source removed
--- dmesg step 11


=== summary (T-060, 08ab082 obs-plugin: OBS source "Elgato Game Capture HD" (T-060))
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
FAIL | step 10 guvcview | the picture stands still (never squeezed, cropped or with borders), and guvcview did not close or show an error? -- error v4l2_core can't grab image
PASS | step 10 guvcview | guvcview shows the N64 picture (720x480)?
PASS | step 10 guvcview | NTSC remembered for composite
PASS | step 10 guvcview | guvcview shows 'Please connect the Elgato Game Capture HD'?
PASS | step 10 guvcview | guvcview shows the N64 picture again?
PASS | step 10 guvcview | idle format: the NTSC source 720x480, 59.94 frames/s
PASS | step 11 bridge stopped | audio source removed
