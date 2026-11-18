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
  `STATE APP-NODE WIDTH HEIGHT NUM DEN FEED-NODE`, notified on changes.
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

Many errors.

gchd-test T-060 20261118-182357 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-060-20261118-182357.log
== build
$ git fetch -q origin
$ git checkout -q feat/app-bridge
$ git pull -q --ff-only
commit: 1c46631 TODO: T-060 with the driver's application device
commit: 1c46631 TODO: T-060 with the driver's application device
build rc=0 warnings=0

=== step 1: driver with three devices
$ sh -c pkill -f 'gchd-bridge run'; sudo modprobe -r v4l2loopback; true
PROMPT: Unplug the Elgato from the PC (or detach it in VirtualBox), wait 3 s, plug it in again.
$ sudo insmod /home/ff5/elgato-gchd/src/elgato_gchd.ko
module: 0.5.8 
$ v4l2-ctl --list-devices
Elgato Game Capture HD (platform:v4l2loopback-000):
	/dev/video42

Elgato Game Capture HD (HDNew,  (usb-0000:00:0c.0-2):
	/dev/video0
	/dev/video1
	/dev/video2

$ v4l2-ctl --list-devices
Elgato Game Capture HD (platform:v4l2loopback-000):
	/dev/video42

Elgato Game Capture HD (HDNew,  (usb-0000:00:0c.0-2):
	/dev/video0
	/dev/video1
	/dev/video2
[FAIL] H.264 device (expected /Elgato Game Capture HD \(H.264\)/, got: Elgato Game Capture HD (platform:v4l2loopback-000): 	/dev/video42  Elgato Game Capture HD (HDNew,  (usb-0000:00:0c.0-2): 	/dev/video0 	/dev/video1 	/dev/video2)
$ app_attr
idle video2 1280 720 60 1 video1
[FAIL] application device registered, idle (expected /^idle video[0-9]+ 720 (480|576) /, got: idle video2 1280 720 60 1 video1)
$ v4l2-ctl -d /dev/video2 --list-formats-ext
ioctl: VIDIOC_ENUM_FMT
	Type: Video Capture

	[0]: 'YUYV' (YUYV 4:2:2)
		Size: Discrete 1280x720
			Interval: Discrete 0.017s (60.000 fps)
[PASS] application device offers YUYV
--- dmesg step 1
[87240.977571] usbcore: deregistering interface driver elgato_gchd
[87250.784523] usb 1-2: new high-speed USB device number 86 using xhci_hcd
[87251.176960] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[87251.176967] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[87251.176968] usb 1-2: Product: GameCapture HD
[87251.176970] usb 1-2: Manufacturer: Elgato
[87251.176970] usb 1-2: SerialNumber: 04107F1267
[87251.634909] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[87251.638003] elgato_gchd 1-2:1.0: application device video2: 1280x720, 60/1 frames/s
[87251.638007] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[87251.638029] usbcore: registered new interface driver elgato_gchd

=== step 2: bridge idle: capture device off
PROMPT: PS1 on composite, game running.
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ bash /home/ff5/elgato-gchd/service/gchd-bridge status
driver state:   idle video2 720 480 60000 1001 video1
stream device:  none
service:        running (pid 88524)
$ bash /home/ff5/elgato-gchd/service/gchd-bridge status
driver state:   idle video2 720 480 60000 1001 video1
stream device:  none
service:        running (pid 88524)
[PASS] bridge running
$ sh -c sudo dmesg | grep -c 'starting hardware boot'
0
[PASS] capture device off while no application streams
--- dmesg step 2
[87280.509211] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[87280.509250] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[87280.509290] elgato_gchd 1-2:1.0: application device: 720x480, 60000/1001 frames/s

=== step 3: first read: source detected (PAL), device registered again with 720x576
$ cat /home/ff5/gchd-test-logs/T-060-20261118-182357/read1.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 331444 kb/s, 59.94 fps, 59.94 tbr, 1000k tbn
frame=    0 fps=0.0 q=0.0 Lsize=       0kB time=N/A bitrate=N/A speed=N/A    
$ app_attr
idle video2 720 480 60000 1001 video1
$ app_attr
idle video2 720 480 60000 1001 video1
[FAIL] application device now 720x576, 50 frames/s (expected / 720 576 50 1 /, got: idle video2 720 480 60000 1001 video1)
$ tail -8 /home/ff5/gchd-test-logs/T-060-20261118-182357/bridge.log
--- dmesg step 3


=== step 4: read: 720x576 YUYV, about 50 frames/s
$ cat /home/ff5/gchd-test-logs/T-060-20261118-182357/read2.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 331444 kb/s, 59.94 fps, 59.94 tbr, 1000k tbn
frame=    0 fps=0.0 q=0.0 Lsize=       0kB time=N/A bitrate=N/A speed=N/A    
$ cat /home/ff5/gchd-test-logs/T-060-20261118-182357/read2.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 331444 kb/s, 59.94 fps, 59.94 tbr, 1000k tbn
frame=    0 fps=0.0 q=0.0 Lsize=       0kB time=N/A bitrate=N/A speed=N/A    
[FAIL] stream YUYV 720x576 (expected /yuyv422.*720x576/, got:   Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 331444 kb/s, 59.94 fps, 59.94 tbr, 1000k tbn frame=    0 fps=0.0 q=0.0 Lsize=       0kB ti)
$ cat /home/ff5/gchd-test-logs/T-060-20261118-182357/read2.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 331444 kb/s, 59.94 fps, 59.94 tbr, 1000k tbn
frame=    0 fps=0.0 q=0.0 Lsize=       0kB time=N/A bitrate=N/A speed=N/A    
[FAIL] frame count >= 450 in 15 s (expected /frame= *([4-9][5-9][0-9]|[5-9][0-9]{2}|[0-9]{4})/, got:   Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 331444 kb/s, 59.94 fps, 59.94 tbr, 1000k tbn frame=    0 fps=0.0 q=0.0 Lsize=       0kB ti)
$ sudo dmesg

[FAIL] capture device started (expected /starting hardware boot/, got: )
--- dmesg step 4


=== step 5: application closed: capture device off
$ sudo dmesg

[FAIL] hardware shut down after the application stopped (expected /hardware shutdown done/, got: )
$ app_attr
idle video2 720 480 60000 1001 video1
[PASS] state idle
--- dmesg step 5


=== step 6: audio while capturing
$ sink_streams

[FAIL] the bridge plays into the audio sink (expected /stream into elgato_gchd/, got: )
$ ffmpeg -hide_banner -nostdin -f pulse -i elgato_gchd_audio -t 4 -af volumedetect -f null -
[aist#0:0/pcm_s16le @ 0x64e4ada56640] Guessed Channel Layout: stereo
Input #0, pulse, from 'elgato_gchd_audio':
  Duration: N/A, start: 1795022820.764074, bitrate: 1536 kb/s
  Stream #0:0: Audio: pcm_s16le, 48000 Hz, 2 channels, s16, 1536 kb/s
[Parsed_volumedetect_0 @ 0x64e4ada58000] n_samples: 0
Stream mapping:
  Stream #0:0 -> #0:0 (pcm_s16le (native) -> pcm_s16le (native))
Output #0, null, to 'pipe:':
  Metadata:
    encoder         : Lavf60.16.100
  Stream #0:0: Audio: pcm_s16le, 48000 Hz, stereo, s16, 1536 kb/s
    Metadata:
      encoder         : Lavc60.31.102 pcm_s16le
size=       0kB time=00:00:00.00 bitrate=N/A speed=N/A    
size=N/A time=00:00:00.59 bitrate=N/A speed=1.07x    
size=N/A time=00:00:01.10 bitrate=N/A speed=1.02x    
size=N/A time=00:00:01.61 bitrate=N/A speed=1.02x    
size=N/A time=00:00:02.13 bitrate=N/A speed=0.94x    
size=N/A time=00:00:02.74 bitrate=N/A speed=0.976x    
size=N/A time=00:00:03.35 bitrate=N/A speed=1.01x    
size=N/A time=00:00:03.90 bitrate=N/A speed=1.02x    
[out#0/null @ 0x64e4ada53b00] video:0kB audio:711kB subtitle:0kB other streams:0kB global headers:0kB muxing overhead: unknown
size=N/A time=00:00:03.95 bitrate=N/A speed=0.978x    
[Parsed_volumedetect_0 @ 0x64e4ada5aa80] n_samples: 369600
[Parsed_volumedetect_0 @ 0x64e4ada5aa80] mean_volume: -91.0 dB
[Parsed_volumedetect_0 @ 0x64e4ada5aa80] max_volume: -91.0 dB
[Parsed_volumedetect_0 @ 0x64e4ada5aa80] histogram_91db: 369600
[PASS] audio source delivers samples
--- dmesg step 6


=== step 7: input change on the application device while reading
$ v4l2-ctl -d /dev/video2 -i 3
Video input set to 3 (S-Video: Camera, ok)
$ app_attr
streaming video2 720 480 60000 1001 video1
$ cat /home/ff5/gchd-test-logs/T-060-20261118-182357/read3.txt
  Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 331444 kb/s, 59.94 fps, 59.94 tbr, 1000k tbn
frame=    0 fps=0.0 q=0.0 Lsize=       0kB time=N/A bitrate=N/A speed=N/A    
$ app_attr
idle video2 720 480 60000 1001 video1
[PASS] S-video (no source, NTSC size): device re-registered with 720x480
$ v4l2-ctl -d /dev/video2 -i 2
Video input set to 2 (Composite: Camera, ok)
$ app_attr
idle video2 720 480 60000 1001 video1
[FAIL] composite again: 720x576 (expected / 720 576 50 1 /, got: idle video2 720 480 60000 1001 video1)
$ v4l2-ctl -d /dev/video0 --get-input
Video input : 2 (Composite: no signal)
[PASS] input composite on the H.264 device too
--- dmesg step 7
[87412.964495] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[87412.964529] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[87452.976902] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[87452.976935] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== step 8: guvcview
PROMPT: Start in a second terminal: guvcview -d /dev/video2   (leave it open)
[FAIL] guvcview shows the PS1 picture, smooth, without interlacing lines? -- the terminal says that v4l2_core could not grab image
PROMPT: Swap the PS1 for the N64 (NTSC) while guvcview stays open; wait about 15 s.
$ app_attr
streaming video2 720 480 60000 1001 video1
[FAIL] guvcview reconnected by itself and shows the N64 picture (720x480)? -- still no image and same error on the terminal
PROMPT: Close guvcview.
$ tail -20 /home/ff5/gchd-test-logs/T-060-20261118-182357/bridge.log
--- dmesg step 8


=== step 9: bridge stopped
$ kill 88524
$ sh -c pactl list short sources | grep -c elgato_gchd
0
[PASS] audio source removed
--- dmesg step 9


=== summary (T-060, 1c46631 TODO: T-060 with the driver's application device)
FAIL | step 1 driver with three devices | H.264 device (expected /Elgato Game Capture HD \(H.264\)/, got: Elgato Game Capture HD (platform:v4l2loopback-000): 	/dev/video42  Elgato Game Capture HD (HDNew,  (usb-0000:00:0c.0-2): 	/dev/video0 	/dev/video1 	/dev/video2)
FAIL | step 1 driver with three devices | application device registered, idle (expected /^idle video[0-9]+ 720 (480|576) /, got: idle video2 1280 720 60 1 video1)
PASS | step 1 driver with three devices | application device offers YUYV
PASS | step 2 bridge idle: capture device off | bridge running
PASS | step 2 bridge idle: capture device off | capture device off while no application streams
FAIL | step 3 first read: source detected (PAL), device registered again with 720x576 | application device now 720x576, 50 frames/s (expected / 720 576 50 1 /, got: idle video2 720 480 60000 1001 video1)
FAIL | step 4 read: 720x576 YUYV, about 50 frames/s | stream YUYV 720x576 (expected /yuyv422.*720x576/, got:   Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 331444 kb/s, 59.94 fps, 59.94 tbr, 1000k tbn frame=    0 fps=0.0 q=0.0 Lsize=       0kB ti)
FAIL | step 4 read: 720x576 YUYV, about 50 frames/s | frame count >= 450 in 15 s (expected /frame= *([4-9][5-9][0-9]|[5-9][0-9]{2}|[0-9]{4})/, got:   Stream #0:0: Video: rawvideo (YUY2 / 0x32595559), yuyv422, 720x480, 331444 kb/s, 59.94 fps, 59.94 tbr, 1000k tbn frame=    0 fps=0.0 q=0.0 Lsize=       0kB ti)
FAIL | step 4 read: 720x576 YUYV, about 50 frames/s | capture device started (expected /starting hardware boot/, got: )
FAIL | step 5 application closed: capture device off | hardware shut down after the application stopped (expected /hardware shutdown done/, got: )
PASS | step 5 application closed: capture device off | state idle
FAIL | step 6 audio while capturing | the bridge plays into the audio sink (expected /stream into elgato_gchd/, got: )
PASS | step 6 audio while capturing | audio source delivers samples
PASS | step 7 input change on the application device while reading | S-video (no source, NTSC size): device re-registered with 720x480
FAIL | step 7 input change on the application device while reading | composite again: 720x576 (expected / 720 576 50 1 /, got: idle video2 720 480 60000 1001 video1)
PASS | step 7 input change on the application device while reading | input composite on the H.264 device too
FAIL | step 8 guvcview | guvcview shows the PS1 picture, smooth, without interlacing lines? -- the terminal says that v4l2_core could not grab image
FAIL | step 8 guvcview | guvcview reconnected by itself and shows the N64 picture (720x480)? -- still no image and same error on the terminal
PASS | step 9 bridge stopped | audio source removed
