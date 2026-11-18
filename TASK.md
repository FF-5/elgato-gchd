# TASK T-060 — guvcview, OBS and similar applications

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** OBS plugin, `/dev/elgato-gchd`, settings tool accepted on hardware (T-060-obs 16/16); installer with three install types: hardware test (T-060-install)
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
12. (Maintainer, 2026-11-18, final for now) OBS: both the plugin and
    OBS's Media Source (fixed device name `/dev/elgato-gchd` by a udev
    rule) with a settings tool (TUI). The application device and the
    bridge are removed (simpler driver: one H.264/MPEG device; they remain
    in the history of this branch). Folders: `src/driver/` (kernel
    driver), `src/obs-plugin/`, plus `src/settings/` for the tool. The
    plugin cannot be stand-alone: the USB protocol, firmware, detection
    and encoder setup are in the kernel driver; the plugin only reads the
    stream and changes settings.
13. (Maintainer, 2026-11-18) The installer offers three install types:
    everything, the kernel driver part only, the OBS plugin only.

## Implementation

- Driver (`src/driver/`): as on `dev` (one device "Elgato Game Capture
  HD"), plus the remembered standard per input (`std_composite`,
  `std_svideo`; saved when the standard is detected; udev rule), so the
  format before the first detection is the right one.
- `udev-rules/55-elgato-gchd-device.rules`: `/dev/elgato-gchd` for the
  driver's video device; installed by `install-driver.sh`.
- `src/obs-plugin/`: OBS source "Elgato Game Capture HD": finds the
  device by its driver in sysfs; properties input, output size
  (composite/S-video), bitrate, bitrate mode, H.264 profile, stretch
  (applied with V4L2 ioctls, capture restarted); decodes the transport
  stream with FFmpeg, video and audio with common timestamps; OBS
  deinterlacing Yadif 2x; captures while shown.
- `src/settings/gchd-settings`: text menu (whiptail, v4l2-ctl) for the
  same settings on `/dev/elgato-gchd`; input and output size only while
  no program captures (it says so).
- Test runner: driver in `src/driver/`.

Offline: driver W=1 clean; plugin built against libobs 30.0.2 without
warnings, stream test (PAL; PAL then NTSC: size follows, audio
continuous); shellcheck clean (tool, installer warnings only the known
SC2033 false positive).

Installer (`install-driver.sh`): asks what to install (everything,
kernel driver, OBS plugin) or takes `--all`, `--driver`, `--obs`; the
driver part also installs `/dev/elgato-gchd` (udev rule) and
`gchd-settings` (`/usr/local/bin`, offers whiptail); the OBS part offers
the build packages, builds in `build/obs-plugin`, installs per user,
stops with an explanation for a Flatpak OBS and notes a missing driver;
`--uninstall` removes everything or one part. Checked here: the OBS part
builds, installs and uninstalls the plugin (temporary home folder).

## Hardware test (maintainer)

Installer: firmware present (or the .msi in `firmware/`), Elgato plugged
in, OBS installed; PS1 or N64 on composite for the last check.

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-060-install.sh > tests/T-060-install.sh
bash gchd-test.sh T-060-install --branch feat/app-bridge
```

Paste the summary; on FAILs the log.

## Findings

- T-060-obs, second run (4db2b4a): 16/16 PASS: plugin built and
  installed, device found automatically, PS1 picture (50 frames/s) and
  sound, 640×480 from the properties, PAL → NTSC swap followed by itself,
  unplug/replug, capture device off while hidden, Media Source on
  `/dev/elgato-gchd` with sound, settings tool.
- T-060-obs, first run (interrupted by the maintainer): step 6 FAIL: the
  capture continued for about 110 s after hiding the source, until it
  was shown again. OBS calls the plugin's hide only when no view shows
  the source (properties preview, projector, multiview, another shown
  scene count as showing); passed in the second run. Plugin README notes
  it. The maintainer saw an error at the unplug/replug in that run; the
  kernel log of step 5 is clean (disconnect, probe, capture restarted by
  OBS): asked where the error appeared.
- Run 08ab082 (application device, T-060): 32/33 PASS; FAIL: guvcview
  reports "v4l2_core can't grab image" while the picture stands still
  after the NTSC swap (no frames; guvcview does not close). The
  application device was then removed (Decisions 12).
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

Everything seems to work.

=== summary (T-060-install, 9c9f814 install-driver.sh: three install types (everything, kernel driver, OBS plugin), per-part uninstall; settings tool and device rule with the driver (T-060))
PASS | step 1 uninstall everything first | plugin removed
PASS | step 1 uninstall everything first | settings tool removed
PASS | step 2 kernel driver only (--driver) | driver installed
PASS | step 2 kernel driver only (--driver) | udev rules installed
PASS | step 2 kernel driver only (--driver) | settings tool installed
PASS | step 2 kernel driver only (--driver) | no plugin installed by --driver
PASS | step 2 kernel driver only (--driver) | /dev/elgato-gchd present
PASS | step 3 OBS plugin only (--obs) | plugin installed
PASS | step 4 uninstall the plugin only (--uninstall --obs) | plugin removed
PASS | step 4 uninstall the plugin only (--uninstall --obs) | driver still installed
PASS | step 5 menu: everything (choice 1) | driver installed
PASS | step 5 menu: everything (choice 1) | plugin installed
PASS | step 5 menu: everything (choice 1) | OBS lists the source and shows the picture?

FAIL: 0  WARN: 0

A comment, I imagined a TUI being the extreme last option for configuration control. I had imagined something similar to what OBS has with v4l2 devices, a window with sliders, menus, etc.
