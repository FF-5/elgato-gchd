# TASK T-060 — guvcview, OBS and similar applications

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** OBS plugin, `/dev/elgato-gchd`, installer accepted on hardware (T-060-obs 16/16, T-060-install 13/13); settings window: hardware test (T-060-settings)
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
14. (Maintainer, 2026-11-18) Settings in a window like OBS's V4L2
    properties (menus, sliders); a text menu only as the last resort.
15. (Maintainer, 2026-11-18) Installation scripts distribution
    independent or covering all common distributions (development on an
    Ubuntu flavour; most users elsewhere): RULES F2.
16. (Maintainer, 2026-11-18) The bitrate controls of both settings
    windows offer only what the device's capture application uses; the
    limits change with the other settings (mode, output size).

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

Settings window (`src/settings/gchd-settings-gui`, "Elgato Game Capture
HD Settings" in the application menu): GTK 3 with Python (preinstalled on
Ubuntu desktops), v4l2-ctl; menus for input, output size, bitrate mode,
profile, a bitrate slider (2000–15000 kbit/s for analog, applied 0.4 s
after the slider stops), a stretch switch; changes applied at once; when
a program captures, an input or output change is refused with an
explanation and the menu returns to the device's value; installed with
the driver part (offers python3-gi, gir1.2-gtk-3.0, v4l-utils). Checked
here on a virtual screen with a stand-in v4l2-ctl (window, values, busy
explanation). The text menu stays as `gchd-settings`.

Installer (`install-driver.sh`): asks what to install (everything,
kernel driver, OBS plugin) or takes `--all`, `--driver`, `--obs`; the
driver part also installs `/dev/elgato-gchd` (udev rule) and
`gchd-settings` (`/usr/local/bin`, offers whiptail); the OBS part offers
the build packages, builds in `build/obs-plugin`, installs per user,
stops with an explanation for a Flatpak OBS and notes a missing driver;
`--uninstall` removes everything or one part. Checked here: the OBS part
builds, installs and uninstalls the plugin (temporary home folder).

## Hardware test (maintainer)

Settings window: OBS with the plugin, a console on composite.

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-060-settings.sh > tests/T-060-settings.sh
bash gchd-test.sh T-060-settings --branch feat/app-bridge
```

Paste the summary; on FAILs the log.

## Findings

- Bitrate in the recordings (composite: `gchd_288p_standard_bitrate_change`,
  `gchd_288p_mobile_bitrate_change`, `gchd_NTSC_composite_output_bitrate_audio`,
  `gchd_PAL_composite_bitrate_audio`): CBR (`v_rate_mode 0`,
  `v_max_cpb_delay 3`) only at B = 11250 (slider's right end, shown 11.3);
  every other position VBR; standard size and 640×480: B 6000 (shown 3.0)
  to 14460 (shown 7.2); the middle of the slider (shown about 7.0) is VBR
  with B 13980–14460; 480×360 always VBR, 4000–15000 (going back to
  640×480 sets CBR 11250). No CBR minimum (CBR is one value). VBR maximum
  above 14460 unknown: slider positions between the middle and the CBR
  end not recorded (TODO R11). Both settings windows now offer CBR 11250
  or VBR in the range of the output size (480×360 VBR only), slider only
  for VBR, the average shown as in the application. The driver itself
  still accepts CBR at any value and 2000–15000 kbit/s (unchanged).
- T-060-settings (0d5409b): steps 1–2 PASS (window installed with menu
  entry, shows the settings, every change applied and remembered).
  Step 3 FAIL ×2: OBS was not capturing (no hardware start and no input
  request from the plugin in the kernel log), so the window rightly
  accepted the input change, and OBS showed an old picture. The
  maintainer had the settings window, OBS and OBS's settings window open.
  Changes: a refused change now opens a message box (before: only the
  status line); the plugin logs capture start, stop, retries and why
  opening failed; the test checks the hardware start before the busy
  test and keeps the plugin's lines from the OBS log.
- T-060-install (9c9f814): 13/13 PASS (uninstall all, driver only,
  plugin only, plugin uninstall only, menu "everything", OBS lists the
  source and shows the picture). Maintainer: settings belong in a window
  like OBS's V4L2 properties; the text menu only as the last resort
  (Decisions 14): settings window added.
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

There were some issues and we need to discuss some changes. I wrote a lengthy note for one of the tests I hope it's intact.

gchd-test T-060-settings 20261119-065844 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-060-settings-20261119-065844.log
== build
$ git fetch -q origin
$ git checkout -q feat/app-bridge
$ git pull -q --ff-only
commit: a45cf10 settings window and OBS plugin: bitrate limited to the observed settings (T-060)
commit: a45cf10 settings window and OBS plugin: bitrate limited to the observed settings (T-060)
build rc=0 warnings=0

=== step 1: install the driver part (window, menu entry)
$ sudo rmmod elgato_gchd
$ bash /home/ff5/elgato-gchd/install-driver.sh --driver --yes --in-terminal
[1mElgato Game Capture HD driver 0.5.8[0m

[1m==> 1/3 Firmware[0m
[32mFirmware found in /lib/firmware/gchd.[0m

[1m==> 2/3 Building the driver[0m
[32mBuilt /home/ff5/elgato-gchd/build/elgato_gchd.ko.[0m

[1m==> 3/3 Installing[0m
Sign command: /usr/bin/kmodsign
Signing key: /var/lib/shim-signed/mok/MOK.priv
Public certificate (MOK): /var/lib/shim-signed/mok/MOK.der

Building module:
Cleaning build area...
make -j4 KERNELRELEASE=6.8.0-146-generic KDIR=/lib/modules/6.8.0-146-generic/build...
Signing module /var/lib/dkms/elgato-gchd/0.5.8/build/elgato_gchd.ko
Cleaning build area...

elgato_gchd.ko.zst:
Running module version sanity check.
 - Original module
   - No original module exists within this kernel
 - Installation
   - Installing to /lib/modules/6.8.0-146-generic/updates/dkms/
depmod...
[32mInstalled with DKMS (rebuilt automatically after kernel updates).[0m
[32mudev rules installed (settings are remembered; device name /dev/elgato-gchd).[0m
[32mSettings installed: 'Elgato Game Capture HD Settings' in the application menu (gchd-settings-gui); text menu: gchd-settings[0m
[32mDriver loaded. Plug in the Game Capture HD; it appears as a /dev/video device.[0m
Watch: v4l2-ctl -d /dev/elgato-gchd -v pixelformat=MPEG && ffplay -f mpegts /dev/elgato-gchd

$ ls /usr/local/bin/gchd-settings-gui
/usr/local/bin/gchd-settings-gui
[PASS] settings window installed
$ ls /usr/local/share/applications/elgato-gchd-settings.desktop
/usr/local/share/applications/elgato-gchd-settings.desktop
[PASS] menu entry installed
PROMPT: Unplug the Elgato and plug it in again; wait 5 s.
$ v4l2-ctl -d /dev/elgato-gchd -i 2 -v width=720,height=480 -c video_bitrate=11250000,video_bitrate_mode=1,h264_profile=4,stretch_sd_input=0
Video input set to 2 (Composite: Camera, no signal)
--- dmesg step 1
[103119.069751] usbcore: deregistering interface driver elgato_gchd
[103129.475229] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[103129.483646] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[103129.483667] usbcore: registered new interface driver elgato_gchd
[103410.039382] usb 1-2: USB disconnect, device number 111
[103415.181768] usb 1-2: new high-speed USB device number 112 using xhci_hcd
[103415.577094] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[103415.577101] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[103415.577103] usb 1-2: Product: GameCapture HD
[103415.577104] usb 1-2: Manufacturer: Elgato
[103415.577105] usb 1-2: SerialNumber: 04107F1267
[103415.579342] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[103415.581262] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[103416.297782] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[103416.297805] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== step 2: window: show and change the settings
PROMPT: Open 'Elgato Game Capture HD Settings' from the application menu (or run gchd-settings-gui).
[PASS] the window shows Device /dev/elgato-gchd, Input Composite, Output size Source size, Bitrate mode Constant (CBR) 11250, the slider greyed out ('11250 kbit/s constant'), High, Stretch off?
PROMPT: In the window: Bitrate mode Variable (VBR); try to move the slider to both ends.
[FAIL] the slider goes from 6000 to 14460 kbit/s and shows the average (half)? -- it only show "(average" and the rest is cut. I would suggestin VBR mode just showing the average bitrate with no words like "average"
PROMPT: Output size 480x360.
[PASS] Bitrate mode is now Variable (VBR) and cannot be changed, and the slider goes from 4000 to 15000?
PROMPT: In the window: Output size 640x480, Bitrate mode Variable (VBR), Bitrate slider to about 8000, H.264 profile Main, Stretch on.
$ v4l2-ctl -d /dev/elgato-gchd -V
Format Video Capture:
	Width/Height      : 640/480
	Pixel Format      : 'MPEG' (MPEG-1/2/4)
	Field             : None
	Bytes per Line    : 0
	Size Image        : 65424
	Colorspace        : Rec. 709
	Transfer Function : Default (maps to Rec. 709)
	YCbCr/HSV Encoding: Default (maps to Rec. 709)
	Quantization      : Default (maps to Full Range)
	Flags             : 
[PASS] output size 640x480
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
video_bitrate: 8020000
[PASS] bitrate about 8000 kbit/s
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate_mode
video_bitrate_mode: 0 (Variable Bitrate)
[PASS] bitrate mode VBR
$ v4l2-ctl -d /dev/elgato-gchd -C h264_profile
h264_profile: 2 (Main)
[PASS] profile Main
$ v4l2-ctl -d /dev/elgato-gchd -C stretch_sd_input
stretch_sd_input: 1
[PASS] stretch on
$ cat /etc/modprobe.d/elgato-gchd-settings.conf
options elgato_gchd input=2 output=1 format=1 bitrate=8020 bitrate_mode=0 stretch=1 h264_profile=2 h264_level=0 std_composite=1 std_svideo=0
[PASS] settings remembered (modprobe.d)
PROMPT: In the window set them back: Source size, Bitrate mode Constant (CBR), High, Stretch off. Leave the window open.
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
video_bitrate: 11250000
[PASS] CBR 11250 again
--- dmesg step 2


=== step 3: window while OBS captures
PROMPT: Close OBS completely if it is open. Then start OBS; the scene with the source 'Elgato Game Capture HD' must show the PS1/N64 picture. Wait until the picture moves.
$ sudo dmesg
[103915.194277] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1731439 comm=gchd-capture
[103915.194292] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[103915.194348] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[103915.194553] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[103915.202234] elgato_gchd 1-2:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.203425] elgato_gchd 1-2:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.204887] elgato_gchd 1-2:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.206163] elgato_gchd 1-2:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.207536] elgato_gchd 1-2:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.208828] elgato_gchd 1-2:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.210454] elgato_gchd 1-2:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.212225] elgato_gchd 1-2:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.213405] elgato_gchd 1-2:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.214926] elgato_gchd 1-2:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.216184] elgato_gchd 1-2:1.0: firmware chunk offset=327680 size=4016 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.261577] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[103915.262766] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
[103915.351735] elgato_gchd 1-2:1.0: transcoder defaults loaded
[103915.351912] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[103915.352459] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x04
[103915.353936] elgato_gchd 1-2:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.355179] elgato_gchd 1-2:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.356342] elgato_gchd 1-2:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.357557] elgato_gchd 1-2:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.358955] elgato_gchd 1-2:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.360160] elgato_gchd 1-2:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.361896] elgato_gchd 1-2:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.366246] elgato_gchd 1-2:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.367591] elgato_gchd 1-2:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.368966] elgato_gchd 1-2:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.370173] elgato_gchd 1-2:1.0: firmware chunk offset=327680 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.371477] elgato_gchd 1-2:1.0: firmware chunk offset=360448 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.372712] elgato_gchd 1-2:1.0: firmware chunk offset=393216 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.373964] elgato_gchd 1-2:1.0: firmware chunk offset=425984 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.375463] elgato_gchd 1-2:1.0: firmware chunk offset=458752 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.376872] elgato_gchd 1-2:1.0: firmware chunk offset=491520 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.378044] elgato_gchd 1-2:1.0: firmware chunk offset=524288 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.381854] elgato_gchd 1-2:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.383473] elgato_gchd 1-2:1.0: firmware chunk offset=589824 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.384891] elgato_gchd 1-2:1.0: firmware chunk offset=622592 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.386184] elgato_gchd 1-2:1.0: firmware chunk offset=655360 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.387570] elgato_gchd 1-2:1.0: firmware chunk offset=688128 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.388909] elgato_gchd 1-2:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.390218] elgato_gchd 1-2:1.0: firmware chunk offset=753664 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.391627] elgato_gchd 1-2:1.0: firmware chunk offset=786432 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.392924] elgato_gchd 1-2:1.0: firmware chunk offset=819200 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.394904] elgato_gchd 1-2:1.0: firmware chunk offset=851968 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.396413] elgato_gchd 1-2:1.0: firmware chunk offset=884736 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.397853] elgato_gchd 1-2:1.0: firmware chunk offset=917504 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.399278] elgato_gchd 1-2:1.0: firmware chunk offset=950272 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.400730] elgato_gchd 1-2:1.0: firmware chunk offset=983040 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.402064] elgato_gchd 1-2:1.0: firmware chunk offset=1015808 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.403363] elgato_gchd 1-2:1.0: firmware chunk offset=1048576 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.404709] elgato_gchd 1-2:1.0: firmware chunk offset=1081344 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.406047] elgato_gchd 1-2:1.0: firmware chunk offset=1114112 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.407357] elgato_gchd 1-2:1.0: firmware chunk offset=1146880 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.408731] elgato_gchd 1-2:1.0: firmware chunk offset=1179648 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.410086] elgato_gchd 1-2:1.0: firmware chunk offset=1212416 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.411697] elgato_gchd 1-2:1.0: firmware chunk offset=1245184 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.415835] elgato_gchd 1-2:1.0: firmware chunk offset=1277952 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.417348] elgato_gchd 1-2:1.0: firmware chunk offset=1310720 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.418753] elgato_gchd 1-2:1.0: firmware chunk offset=1343488 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.420131] elgato_gchd 1-2:1.0: firmware chunk offset=1376256 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.421445] elgato_gchd 1-2:1.0: firmware chunk offset=1409024 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.422879] elgato_gchd 1-2:1.0: firmware chunk offset=1441792 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.424252] elgato_gchd 1-2:1.0: firmware chunk offset=1474560 size=26224 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103915.433511] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[103915.532469] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[103915.598678] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103915.601060] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e 0x98:F5=0x62
[103915.601064] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103915.659472] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[103915.686693] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103915.689704] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0xf2
[103915.689709] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103915.751843] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103915.754463] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103915.754468] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103915.815803] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103915.819390] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103915.819395] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103915.878628] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103915.882647] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103915.882652] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103915.939259] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103915.941485] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103915.941489] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103916.000388] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103916.002850] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103916.002853] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103916.062678] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103916.067761] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103916.067771] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103916.124823] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103916.126952] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103916.126955] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103916.184183] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103916.187314] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103916.187318] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103916.245788] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103916.312622] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103916.373603] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103916.442960] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103916.510672] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103916.576875] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103916.644403] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103916.710466] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103916.775752] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103916.848455] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103916.910913] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103916.978382] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103917.042947] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103917.105245] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103917.167554] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103917.233091] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103917.296903] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103917.358971] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103917.424845] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103917.491980] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103917.559611] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103917.627272] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103917.695493] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103917.766361] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103917.828663] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103917.893408] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103917.965179] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103918.033221] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103918.099712] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103918.164830] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103918.231887] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103918.300449] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103918.372724] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103918.440614] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103918.506276] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103918.569809] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103918.639958] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103918.699545] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103918.765990] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103918.835694] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103918.898788] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103918.961195] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103919.028168] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103919.093536] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103919.159361] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103919.228411] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103919.293457] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103919.355665] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103919.419163] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103919.481470] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103919.542286] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103919.606855] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103919.667387] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103919.728739] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103919.789735] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103919.854469] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103919.917861] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103919.985643] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103920.050010] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103920.114235] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103920.173864] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103920.235001] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103920.298530] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103920.361800] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103920.425342] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103920.487713] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103920.549815] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103920.615323] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103920.618290] gchd_probe_video_source: 67 callbacks suppressed
[103920.618292] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103920.618295] gchd_detection_worker: 67 callbacks suppressed
[103920.618296] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103920.676401] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103920.679418] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103920.679422] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103920.740642] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103920.742910] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103920.742914] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103920.806751] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103920.809195] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103920.809200] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103920.873813] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103920.876293] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103920.876296] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103920.943870] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103920.949577] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103920.949587] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103921.009203] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103921.015596] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103921.015601] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103921.072906] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103921.075106] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103921.075110] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103921.134380] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103921.137791] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103921.137794] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103921.201592] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103921.204272] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103921.204277] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103921.266596] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103921.326934] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103921.388168] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103921.453754] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103921.519522] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103921.585631] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103921.650239] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103921.716382] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103921.782880] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103921.852799] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103921.917677] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103921.977968] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103922.043736] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103922.107073] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103922.171292] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103922.233960] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103922.295743] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103922.358962] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103922.421598] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103922.486798] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103922.549565] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103922.611817] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103922.675460] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103922.739238] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103922.804387] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103922.871423] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103922.932592] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103922.990843] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103923.051964] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103923.117767] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103923.178346] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103923.239116] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103923.304691] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103923.372240] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103923.435732] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103923.495815] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103923.558546] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103923.618732] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103923.677002] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103923.736814] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103923.796632] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103923.858124] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103923.921318] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103923.985596] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103924.051193] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103924.116016] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103924.178703] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103924.241079] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103924.302870] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103924.373537] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103924.441757] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103924.508364] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103924.574417] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103924.638785] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103924.702752] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103924.764332] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103924.826580] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103924.890993] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103924.954339] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103925.019153] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103925.080402] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103925.144904] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103925.215653] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103925.279787] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103925.341913] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103925.404429] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103925.473760] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103925.538329] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103925.601993] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103925.663877] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103925.666526] gchd_probe_video_source: 69 callbacks suppressed
[103925.666529] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103925.666533] gchd_detection_worker: 69 callbacks suppressed
[103925.666534] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103925.726124] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103925.731388] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103925.731395] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103925.792181] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103925.794812] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103925.794817] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103925.855328] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103925.857633] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103925.857636] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103925.916891] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103925.919124] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103925.919127] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103925.974924] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103925.977888] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103925.977894] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103926.040368] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103926.043285] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103926.043289] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103926.101753] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103926.104113] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103926.104118] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103926.160916] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103926.165841] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103926.165847] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103926.223618] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103926.225967] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103926.225971] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103926.282607] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103926.341384] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103926.407913] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103926.470862] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103926.538318] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103926.605907] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103926.672757] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103926.742054] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103926.807213] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103926.870654] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103926.938299] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103927.001361] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103927.069886] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103927.136577] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103927.204195] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103927.269308] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103927.333612] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103927.396563] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103927.461537] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103927.524679] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103927.590362] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103927.654328] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103927.719167] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103927.780621] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103927.841442] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103927.907704] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103927.973777] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103928.034680] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103928.094999] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103928.157905] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103928.223412] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103928.288156] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103928.350869] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103928.415939] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103928.476166] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103928.538222] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103928.602136] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103928.664395] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103928.725806] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103928.791161] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103928.855827] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103928.917652] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103928.979761] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103929.043550] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103929.104353] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103929.163702] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103929.223904] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103929.289107] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103929.352339] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103929.414363] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103929.478788] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103929.542139] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103929.603858] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103929.667535] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103929.732516] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103929.799710] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103929.865720] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103929.933849] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103930.001869] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103930.071902] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103930.135363] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103930.201406] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103930.265405] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103930.326164] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103930.387805] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103930.448497] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103930.508995] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103930.572674] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103930.637146] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103930.700826] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103930.703697] gchd_probe_video_source: 69 callbacks suppressed
[103930.703700] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103930.703710] gchd_detection_worker: 69 callbacks suppressed
[103930.703711] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103930.762722] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103930.767287] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103930.767294] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103930.824246] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103930.826524] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103930.826527] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103930.888362] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103930.891205] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103930.891211] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103930.952209] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103930.955684] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103930.955692] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103931.016896] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103931.020007] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103931.020012] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103931.076749] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103931.080705] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103931.080714] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103931.139011] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103931.141429] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103931.141431] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103931.202221] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103931.205435] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103931.205443] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103931.265645] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103931.268035] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103931.268040] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103931.330835] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103931.406781] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103931.467830] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103931.527901] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103931.591224] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103931.655954] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103931.720331] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103931.780703] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103931.841678] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103931.906405] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103931.969886] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103932.034536] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103932.096708] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103932.157339] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103932.216302] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103932.277239] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103932.345756] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103932.410518] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103932.474152] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103932.539490] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103932.604267] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103932.670839] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103932.733224] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103932.796650] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103932.860492] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103932.928639] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103932.989293] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103933.054929] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103933.117735] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103933.179531] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103933.241723] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103933.302758] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103933.360834] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103933.426827] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103933.492477] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103933.556785] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103933.619245] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103933.685027] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103933.749325] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103933.815273] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103933.879122] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103933.941887] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103934.006163] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103934.074080] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103934.137569] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103934.200958] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103934.259389] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103934.321727] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103934.386197] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103934.447405] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103934.508932] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103934.574148] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103934.635844] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103934.694822] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103934.756680] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103934.819382] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103934.879432] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103934.946030] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103935.008022] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103935.070151] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103935.131387] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103935.191337] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103935.251029] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103935.310690] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103935.371688] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103935.432470] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103935.494371] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103935.559593] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103935.621427] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103935.683792] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103935.744292] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103935.750182] gchd_probe_video_source: 70 callbacks suppressed
[103935.750186] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103935.750190] gchd_detection_worker: 70 callbacks suppressed
[103935.750191] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103935.810695] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103935.816421] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103935.816426] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103935.875778] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103935.878305] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103935.878310] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103935.938672] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103935.940789] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103935.940792] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103936.002962] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103936.005481] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103936.005487] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103936.066356] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103936.069718] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103936.069724] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103936.127693] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103936.132049] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103936.132059] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103936.189982] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103936.193537] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103936.193541] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103936.254712] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103936.257374] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103936.257379] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103936.319776] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103936.325805] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103936.325810] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103936.389676] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103936.455470] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103936.516398] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103936.581398] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103936.651416] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103936.712363] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103936.779372] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103936.842416] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103936.904715] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103936.972482] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103937.041813] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103937.108424] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103937.170721] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103937.232995] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103937.294997] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103937.356189] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103937.421587] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103937.484778] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103937.550158] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103937.609701] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103937.673043] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103937.738849] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103937.803712] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103937.868411] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103937.935084] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103938.000367] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103938.062355] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103938.127837] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103938.186544] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103938.244437] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103938.308229] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103938.372296] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103938.434774] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103938.494157] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103938.557543] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103938.621779] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103938.687426] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103938.756123] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103938.817949] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103938.887336] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103938.959547] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103939.026762] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103939.095923] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103939.162200] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103939.224072] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103939.282890] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103939.345300] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103939.410913] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103939.476019] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103939.542503] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103939.606871] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103939.669039] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103939.733928] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103939.796445] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103939.860966] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103939.934904] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103940.004865] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103940.075001] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103940.139886] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103940.205636] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103940.267873] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103940.331019] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103940.393246] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103940.460205] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103940.533386] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103940.599795] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103940.661951] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103940.723020] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103940.785904] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103940.790220] gchd_probe_video_source: 68 callbacks suppressed
[103940.790223] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103940.790226] gchd_detection_worker: 68 callbacks suppressed
[103940.790227] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103940.852330] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103940.855622] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103940.855626] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103940.918602] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103940.920967] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103940.920971] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103940.979672] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103940.985642] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103940.985648] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103941.044001] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103941.047209] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103941.047216] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103941.103690] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103941.107132] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103941.107137] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103941.168680] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103941.172144] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103941.172150] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103941.229076] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103941.232018] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103941.232025] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103941.289410] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103941.292246] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103941.292251] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103941.355083] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103941.357612] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103941.357617] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103941.416470] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103941.476991] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103941.541318] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103941.607157] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103941.669490] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103941.729524] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103941.791556] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103941.857857] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103941.920807] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103941.986456] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103942.053028] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103942.119377] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103942.181637] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103942.240937] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103942.304924] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103942.367252] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103942.428028] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103942.489661] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103942.552041] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103942.615279] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103942.677194] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103942.739080] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103942.803081] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103942.869271] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103942.932273] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103942.995052] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103943.056572] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103943.117986] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103943.178911] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103943.242144] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103943.303620] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103943.366598] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103943.429876] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103943.495094] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103943.560798] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103943.624579] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103943.687076] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103943.750830] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103943.810672] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103943.869560] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103943.929637] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103943.989053] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103944.055004] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103944.116961] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103944.182128] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103944.245004] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103944.308145] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103944.371717] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103944.435590] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103944.499174] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103944.564990] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103944.629000] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103944.699656] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103944.767739] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103944.829986] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103944.893344] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103944.959371] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103945.022638] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103945.084055] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103945.150091] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103945.214111] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103945.274421] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103945.339230] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103945.406159] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103945.471438] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103945.534282] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103945.599445] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103945.659893] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103945.723126] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103945.790677] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103945.794494] gchd_probe_video_source: 69 callbacks suppressed
[103945.794497] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103945.794502] gchd_detection_worker: 69 callbacks suppressed
[103945.794503] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103945.858412] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103945.862192] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103945.862203] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103945.923126] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103945.925475] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103945.925478] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103945.984324] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103945.987020] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103945.987025] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103946.048940] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103946.053517] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103946.053521] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103946.114468] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103946.117433] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103946.117439] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103946.178472] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103946.183633] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103946.183646] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103946.241304] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103946.244229] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103946.244233] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103946.307805] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103946.311188] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103946.311196] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103946.372317] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103946.375003] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103946.375006] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103946.434487] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103946.494331] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103946.556660] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103946.620920] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103946.691266] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103946.756412] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103946.824924] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103946.885995] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103946.949399] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103947.012481] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103947.083591] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103947.147305] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103947.208917] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103947.273631] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103947.336260] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103947.401485] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103947.461951] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103947.524011] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103947.584914] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103947.643707] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103947.705709] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103947.768925] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103947.833003] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103947.893821] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103947.955554] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103948.019941] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103948.084822] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103948.146005] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103948.209855] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103948.278689] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103948.349856] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103948.414834] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103948.475741] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103948.537826] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103948.604811] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103948.667589] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103948.727349] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103948.794711] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103948.859991] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103948.924060] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103948.986873] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103949.047335] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103949.109802] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103949.176024] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103949.239510] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103949.305913] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103949.370281] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103949.431011] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103949.499997] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103949.566894] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103949.635049] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103949.703403] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103949.765573] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103949.834602] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103949.901249] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103949.968201] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103950.030409] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103950.093150] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103950.156966] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103950.218174] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103950.279832] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103950.343698] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103950.408238] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103950.471696] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103950.534085] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103950.596623] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103950.659933] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103950.729197] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103950.796187] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103950.801231] gchd_probe_video_source: 68 callbacks suppressed
[103950.801235] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103950.801239] gchd_detection_worker: 68 callbacks suppressed
[103950.801240] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103950.859318] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103950.863114] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103950.863123] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103950.919589] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103950.922088] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103950.922092] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103950.982607] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103950.985639] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103950.985644] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103951.045352] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103951.048388] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103951.048405] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103951.108573] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103951.111228] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103951.111231] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103951.171089] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103951.173806] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103951.173812] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103951.231114] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103951.233836] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103951.233840] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103951.291632] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103951.293892] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103951.293895] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103951.355765] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103951.358348] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103951.358353] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103951.418673] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103951.478263] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103951.545559] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103951.609496] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103951.679489] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103951.750363] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103951.815851] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103951.878848] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103951.943625] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103952.007361] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103952.067262] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103952.136660] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103952.207357] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103952.270643] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103952.331848] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103952.394110] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103952.456050] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103952.517639] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103952.579738] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103952.642486] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103952.705005] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103952.765766] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103952.827860] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103952.887989] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103952.956948] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103953.018381] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103953.079494] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103953.141209] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103953.203509] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103953.271704] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103953.335060] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103953.393420] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103953.453885] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103953.517801] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103953.578966] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103953.641026] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103953.699077] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103953.760282] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103953.821961] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103953.887482] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103953.947637] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103954.011978] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103954.082679] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103954.146159] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103954.209562] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103954.273651] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103954.342965] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103954.406888] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103954.468072] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103954.534642] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103954.594057] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103954.656812] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103954.717716] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103954.786312] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103954.855618] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103954.918240] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103954.989706] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103955.055536] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103955.121844] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103955.182190] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103955.241454] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103955.301423] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103955.362325] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103955.427405] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103955.491737] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103955.557819] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103955.623666] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103955.683183] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103955.744516] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103955.812439] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103955.817380] gchd_probe_video_source: 69 callbacks suppressed
[103955.817386] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103955.817391] gchd_detection_worker: 69 callbacks suppressed
[103955.817392] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103955.878535] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103955.881460] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103955.881466] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103955.947312] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103955.951495] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103955.951502] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103956.013183] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[103956.015452] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103956.015455] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103956.072330] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[103956.074529] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[103956.074533] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[103956.115065] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[103956.173327] elgato_gchd 1-2:1.0: shutdown: device in STOP after RESET; IDLE and RESET again
[103956.173703] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[103956.174030] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
[103956.189729] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0011 before, 0x0012 now)
[103956.189979] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1731446 comm=gchd-capture
[103956.189989] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[103956.190027] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[103956.190143] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[103956.190144] elgato_gchd 1-2:1.0: waiting 1500 ms after the previous shutdown
[103957.717647] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[103957.717913] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
[103957.806099] elgato_gchd 1-2:1.0: transcoder defaults loaded
[103957.806584] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[103957.806926] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x04
[103957.808418] elgato_gchd 1-2:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.809614] elgato_gchd 1-2:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.810767] elgato_gchd 1-2:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.811991] elgato_gchd 1-2:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.813236] elgato_gchd 1-2:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.817102] elgato_gchd 1-2:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.818417] elgato_gchd 1-2:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.819681] elgato_gchd 1-2:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.820914] elgato_gchd 1-2:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.822404] elgato_gchd 1-2:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.823825] elgato_gchd 1-2:1.0: firmware chunk offset=327680 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.825059] elgato_gchd 1-2:1.0: firmware chunk offset=360448 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.826359] elgato_gchd 1-2:1.0: firmware chunk offset=393216 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.827656] elgato_gchd 1-2:1.0: firmware chunk offset=425984 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.828948] elgato_gchd 1-2:1.0: firmware chunk offset=458752 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.833669] elgato_gchd 1-2:1.0: firmware chunk offset=491520 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.834967] elgato_gchd 1-2:1.0: firmware chunk offset=524288 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.836210] elgato_gchd 1-2:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.837751] elgato_gchd 1-2:1.0: firmware chunk offset=589824 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.838975] elgato_gchd 1-2:1.0: firmware chunk offset=622592 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.840255] elgato_gchd 1-2:1.0: firmware chunk offset=655360 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.841493] elgato_gchd 1-2:1.0: firmware chunk offset=688128 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.842823] elgato_gchd 1-2:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.844125] elgato_gchd 1-2:1.0: firmware chunk offset=753664 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.845515] elgato_gchd 1-2:1.0: firmware chunk offset=786432 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.847106] elgato_gchd 1-2:1.0: firmware chunk offset=819200 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.848479] elgato_gchd 1-2:1.0: firmware chunk offset=851968 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.849791] elgato_gchd 1-2:1.0: firmware chunk offset=884736 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.851209] elgato_gchd 1-2:1.0: firmware chunk offset=917504 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.852629] elgato_gchd 1-2:1.0: firmware chunk offset=950272 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.854013] elgato_gchd 1-2:1.0: firmware chunk offset=983040 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.855288] elgato_gchd 1-2:1.0: firmware chunk offset=1015808 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.856721] elgato_gchd 1-2:1.0: firmware chunk offset=1048576 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.858076] elgato_gchd 1-2:1.0: firmware chunk offset=1081344 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.859328] elgato_gchd 1-2:1.0: firmware chunk offset=1114112 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.860675] elgato_gchd 1-2:1.0: firmware chunk offset=1146880 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.862098] elgato_gchd 1-2:1.0: firmware chunk offset=1179648 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.863464] elgato_gchd 1-2:1.0: firmware chunk offset=1212416 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.865711] elgato_gchd 1-2:1.0: firmware chunk offset=1245184 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.867077] elgato_gchd 1-2:1.0: firmware chunk offset=1277952 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.868504] elgato_gchd 1-2:1.0: firmware chunk offset=1310720 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.870014] elgato_gchd 1-2:1.0: firmware chunk offset=1343488 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.871506] elgato_gchd 1-2:1.0: firmware chunk offset=1376256 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.872938] elgato_gchd 1-2:1.0: firmware chunk offset=1409024 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.874225] elgato_gchd 1-2:1.0: firmware chunk offset=1441792 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.875624] elgato_gchd 1-2:1.0: firmware chunk offset=1474560 size=26224 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[103957.881571] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[103958.210897] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[103958.270856] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[103958.283550] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[103958.286082] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[103958.286087] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[103958.286090] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[103958.286097] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[103959.206973] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[103959.207120] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[103959.986732] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[103960.011826] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[103960.011967] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[103960.012161] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[103960.021347] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[103960.022402] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[103960.023084] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[103960.023237] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[103960.023528] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[103960.024773] elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0000 ack=1
[103960.025859] elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[103960.026290] elgato_gchd 1-2:1.0: START poll=1 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[103960.026688] elgato_gchd 1-2:1.0: START poll=2 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[103960.027165] elgato_gchd 1-2:1.0: START poll=3 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[103960.027690] elgato_gchd 1-2:1.0: START poll=4 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[103960.029068] elgato_gchd 1-2:1.0: START poll=5 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[103960.032035] elgato_gchd 1-2:1.0: START poll=6 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[103960.032425] elgato_gchd 1-2:1.0: START poll=7 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[103960.032764] elgato_gchd 1-2:1.0: START poll=8 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[103960.033085] elgato_gchd 1-2:1.0: START poll=9 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[103960.033416] elgato_gchd 1-2:1.0: START poll=10 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[103960.033730] elgato_gchd 1-2:1.0: START poll=11 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[103960.034143] elgato_gchd 1-2:1.0: START poll=12 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[103960.034508] elgato_gchd 1-2:1.0: START poll=13 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[103960.034926] elgato_gchd 1-2:1.0: START poll=14 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[103960.035477] elgato_gchd 1-2:1.0: START poll=15 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[103960.074952] elgato_gchd 1-2:1.0: START poll=100 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[103960.117350] elgato_gchd 1-2:1.0: START poll=190 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0010 ack=1
[103960.117870] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[103960.489931] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[103960.535198] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[103960.581956] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[103960.625149] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[103960.664324] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[103960.670084] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[103961.202448] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[103961.202832] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[103961.883047] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[103961.883426] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[103962.266330] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[103962.266544] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[103962.268883] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[103962.283041] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[103962.283592] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[103962.284627] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[103962.284830] elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[103962.285087] elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
[103962.285880] elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0150 ack=1
[103962.289398] elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[103962.290056] elgato_gchd 1-2:1.0: START poll=1 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[103962.292710] elgato_gchd 1-2:1.0: START poll=2 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[103962.293351] elgato_gchd 1-2:1.0: START poll=3 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[103962.293834] elgato_gchd 1-2:1.0: START poll=4 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[103962.294168] elgato_gchd 1-2:1.0: START poll=5 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[103962.294508] elgato_gchd 1-2:1.0: START poll=6 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[103962.294878] elgato_gchd 1-2:1.0: START poll=7 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[103962.295234] elgato_gchd 1-2:1.0: START poll=8 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[103962.295615] elgato_gchd 1-2:1.0: START poll=9 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[103962.295991] elgato_gchd 1-2:1.0: START poll=10 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[103962.296349] elgato_gchd 1-2:1.0: START poll=11 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[103962.296760] elgato_gchd 1-2:1.0: START poll=12 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[103962.297094] elgato_gchd 1-2:1.0: START poll=13 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[103962.297470] elgato_gchd 1-2:1.0: START poll=14 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[103962.297879] elgato_gchd 1-2:1.0: START poll=15 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[103962.347837] elgato_gchd 1-2:1.0: START poll=100 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[103962.390941] elgato_gchd 1-2:1.0: START poll=150 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0150 ack=1
[103962.392050] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[103962.392055] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[103962.761833] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[103962.806534] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[103962.853238] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[103962.896068] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[103962.941684] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[103967.311655] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[103972.333445] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[103977.356223] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[103982.380059] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[103987.403378] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[PASS] OBS captures (capture device started)
$ sh -c grep -h '\[gchd\]' /home/ff5/.config/obs-studio/logs/$(ls -t /home/ff5/.config/obs-studio/logs | head -1) | tail -5
grep: /home/ff5/.config/obs-studio/logs/2026-11-19: No such file or directory
grep: 07-12-09.txt: No such file or directory
PROMPT: In the settings window choose Input 'S-Video'.
[FAIL] a message box said 'Input not changed' (a program is capturing), and after OK the Input shows Composite again? -- First upon opening OBS I had no video because the input was set to S-Video from the previous test. Second, I wouldn't expect the plugin setting to prevent me from changing input setting, I would only expect a block for a setting change that comes from outside OBS. I guess our GUI has no connection to the plugin settings, meaning video stream is limited to 1 program only but settings are separate between driver and plugin. If possible when handling settings in our GUI I would want that if I have the GUI open that if a change is done through the terminal while the GUI is open then the GUI can periodically refresh its state to reflect changes made from a terminal. Besides I don't know what the refresh button does in the GUI, we might not need it.
$ v4l2-ctl -d /dev/elgato-gchd -I
Video input : 3 (S-Video: no signal)
[FAIL] input still composite (expected /Video input : 2/, got: Video input : 3 (S-Video: no signal))
PROMPT: In the settings window choose Bitrate mode Variable (VBR) and move the slider to about 6000; wait 5 s.
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
video_bitrate: 6000000
[PASS] bitrate changed while capturing
[PASS] OBS kept showing the moving picture (a short pause is normal)?
PROMPT: In the settings window choose Bitrate mode Constant (CBR); close the settings window.
PROMPT: In OBS open the properties of the source 'Elgato Game Capture HD': choose Bitrate mode Variable (VBR), then Output size 480x360 and back to the source size.
[FAIL] OBS: the bitrate slider appears only for VBR (6000-14460), the mode is hidden for 480x360 (slider 4000-15000)? -- there's no slider and I gave a wrong answer before. We need to work on how everything is supposed to work.
PROMPT: Set Bitrate mode Constant (CBR), Output size Source size -> OK; close OBS.
$ sh -c grep -h '\[gchd\]' /home/ff5/.config/obs-studio/logs/$(ls -t /home/ff5/.config/obs-studio/logs | head -1) | tail -20
grep: /home/ff5/.config/obs-studio/logs/2026-11-19: No such file or directory
grep: 07-12-09.txt: No such file or directory
--- dmesg step 3
[104721.910091] elgato_gchd 1-2:1.0: shutdown: device in STOP after RESET; IDLE and RESET again
[104721.925896] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0011 before, 0x0012 now)
[104721.926125] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[104721.926155] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[104721.926244] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[104721.926245] elgato_gchd 1-2:1.0: waiting 1500 ms after the previous shutdown
[104723.553054] elgato_gchd 1-2:1.0: transcoder defaults loaded
[104723.647224] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[104723.984674] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[104724.062923] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[104724.065690] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[104724.065695] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[104724.065697] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[104724.065700] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[104725.032680] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[104725.552599] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[104725.579990] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[104725.580232] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[104725.589792] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[104725.591280] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[104725.591865] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[104725.703721] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[104725.958613] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[104727.524251] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[104727.524633] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[104727.534886] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[104727.535571] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[104727.536393] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[104727.630318] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[104727.630321] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[104732.555785] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=7
[104733.097042] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[104733.097745] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[104733.098004] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[104733.098009] elgato_gchd 1-2:1.0: waiting 1500 ms after the previous shutdown
[104734.793223] elgato_gchd 1-2:1.0: transcoder defaults loaded
[104734.883263] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[104735.051684] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[104735.157490] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x36 masked_mode=0x6
[104735.160223] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[104735.218267] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[104735.249332] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x26 masked_mode=0x6
[104735.251808] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[104735.251815] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[104735.251818] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[104735.251820] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[104736.179590] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[104736.798245] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[104736.821073] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[104736.821430] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[104736.826296] elgato_gchd 1-2:1.0: source changed to 480i during setup; configuring again
[104736.883642] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[104736.913477] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[104736.915715] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[104736.915719] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[104736.915722] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[104736.915724] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[104737.187277] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[104737.211219] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[104737.211808] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[104737.220346] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[104737.222067] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[104737.223038] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[104737.317499] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[104742.238676] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=36 bytes=2359296 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=3
[104743.221877] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[104743.223225] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[104743.223407] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[104743.223409] elgato_gchd 1-2:1.0: waiting 1499 ms after the previous shutdown
[104744.884443] elgato_gchd 1-2:1.0: transcoder defaults loaded
[104744.964406] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[104745.066246] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[104745.189237] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[104745.229817] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x26 masked_mode=0x6
[104745.233901] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[104745.293310] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[104745.332607] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x36 masked_mode=0x6
[104745.337827] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[104745.337836] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[104745.337840] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[104745.337844] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[104746.393705] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[104746.705673] elgato_gchd 1-2:1.0: back-end not settled: status 0xf2
[104746.723036] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[104746.753862] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[104746.754215] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[104746.763547] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[104746.764075] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[104746.764693] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[104746.860102] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[104747.111352] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 2
[104749.502628] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[104749.502995] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[104749.509276] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[104749.509952] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[104749.510903] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[104749.626806] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[104749.626810] elgato_gchd 1-2:1.0: analog mode now 480i, output 2
[104750.082823] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[104754.589843] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=40 bytes=2621440 ep84_bytes=2621440 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104759.592880] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=83 bytes=5439488 ep84_bytes=5439488 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104764.661574] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=128 bytes=8388608 ep84_bytes=8388608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104769.673626] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=169 bytes=11075584 ep84_bytes=11075584 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104770.279048] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[104772.632147] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[104772.632463] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[104772.640680] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[104772.642041] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[104772.642537] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[104772.757161] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[104772.757165] elgato_gchd 1-2:1.0: analog mode now 240p, output 2
[104773.120923] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[104777.645316] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=41 bytes=2686976 ep84_bytes=2686976 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104782.718650] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=88 bytes=5767168 ep84_bytes=5767168 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104787.733947] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=134 bytes=8781824 ep84_bytes=8781824 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104792.749822] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=181 bytes=11862016 ep84_bytes=11862016 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104797.797748] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=228 bytes=14942208 ep84_bytes=14942208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104801.281552] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[104802.798662] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=265 bytes=17367040 ep84_bytes=17367040 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104803.781773] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[104803.782318] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[104803.789550] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[104803.792236] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[104803.792933] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[104803.906520] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[104803.906529] elgato_gchd 1-2:1.0: analog mode now 480i, output 2
[104804.297341] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[104808.878624] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=37 bytes=2424832 ep84_bytes=2424832 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104813.907499] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=85 bytes=5570560 ep84_bytes=5570560 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104818.959126] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=129 bytes=8454144 ep84_bytes=8454144 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104824.005086] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=175 bytes=11468800 ep84_bytes=11468800 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104829.030976] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=14417920 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104834.109482] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=262 bytes=17170432 ep84_bytes=17170432 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104835.144049] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[104837.489533] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[104837.489909] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[104837.497507] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[104837.498759] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[104837.499535] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[104837.612876] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[104837.612880] elgato_gchd 1-2:1.0: analog mode now 240p, output 2
[104837.978402] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[104842.509727] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=41 bytes=2686976 ep84_bytes=2686976 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104847.551381] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=87 bytes=5701632 ep84_bytes=5701632 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104852.622226] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=134 bytes=8781824 ep84_bytes=8781824 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104857.694684] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=181 bytes=11862016 ep84_bytes=11862016 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104862.765316] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=228 bytes=14942208 ep84_bytes=14942208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104866.310675] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[104867.796874] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=266 bytes=17432576 ep84_bytes=17432576 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104868.461522] elgato_gchd 1-2:1.0: back-end not settled: status 0xf2
[104868.489675] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[104868.490281] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[104868.498150] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[104868.498896] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[104868.500034] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[104868.612671] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[104868.612674] elgato_gchd 1-2:1.0: analog mode now 480i, output 2
[104869.006509] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[104873.597928] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=37 bytes=2424832 ep84_bytes=2424832 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104878.621778] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=80 bytes=5242880 ep84_bytes=5242880 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104883.691018] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=121 bytes=7929856 ep84_bytes=7929856 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104887.140519] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[104887.140919] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[104887.141047] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[104887.141049] elgato_gchd 1-2:1.0: waiting 1499 ms after the previous shutdown
[104888.827281] elgato_gchd 1-2:1.0: transcoder defaults loaded
[104888.916666] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[104889.028664] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[104889.160268] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[104889.252067] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[104889.341874] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[104889.393884] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[104889.397617] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[104889.478825] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[104889.484786] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[104889.563215] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[104889.637143] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[104889.640010] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[104889.640013] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[104889.640015] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[104889.640017] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[104890.637525] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[104890.825810] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[104890.854135] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[104890.854547] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[104890.889575] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[104890.890805] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[104890.891952] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[104891.010390] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[104895.909028] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=39 bytes=2555904 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=3
[104896.894256] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[104896.895790] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[104896.895974] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[104896.895976] elgato_gchd 1-2:1.0: waiting 1499 ms after the previous shutdown
[104898.564533] elgato_gchd 1-2:1.0: transcoder defaults loaded
[104898.640832] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[104898.746181] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[104898.945806] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[104899.048993] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[104899.051714] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[104899.111459] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[104899.156826] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[104899.160894] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[104899.227119] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[104899.229681] elgato_gchd 1-2:1.0: analog format status 0x6e not locked yet
[104899.287492] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[104899.317899] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[104899.320318] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[104899.320322] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[104899.320325] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[104899.320327] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[104900.246663] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[104901.030298] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[104901.059915] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[104901.060272] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[104901.066402] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[104901.067331] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[104901.067904] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[104901.185208] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[104906.106431] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104911.129946] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104916.152777] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104921.174662] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104926.197466] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104930.441845] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[104931.220939] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=680 bytes=44564480 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104932.210784] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[104932.211151] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[104932.218905] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[104932.219736] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[104932.220358] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[104932.333725] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[104932.333728] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[104937.255468] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104942.278700] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104947.300786] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104952.323080] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104957.346791] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104962.369404] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=680 bytes=44564480 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104963.707732] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[104965.160948] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[104965.161227] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[104965.167237] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[104965.167807] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[104965.168461] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[104965.267215] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[104965.267217] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[104970.191545] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104975.213472] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[104977.979716] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== summary (T-060-settings, a45cf10 settings window and OBS plugin: bitrate limited to the observed settings (T-060))
PASS | step 1 install the driver part (window, menu entry) | settings window installed
PASS | step 1 install the driver part (window, menu entry) | menu entry installed
PASS | step 2 window: show and change the settings | the window shows Device /dev/elgato-gchd, Input Composite, Output size Source size, Bitrate mode Constant (CBR) 11250, the slider greyed out ('11250 kbit/s constant'), High, Stretch off?
FAIL | step 2 window: show and change the settings | the slider goes from 6000 to 14460 kbit/s and shows the average (half)? -- it only show "(average" and the rest is cut. I would suggestin VBR mode just showing the average bitrate with no words like "average"
PASS | step 2 window: show and change the settings | Bitrate mode is now Variable (VBR) and cannot be changed, and the slider goes from 4000 to 15000?
PASS | step 2 window: show and change the settings | output size 640x480
PASS | step 2 window: show and change the settings | bitrate about 8000 kbit/s
PASS | step 2 window: show and change the settings | bitrate mode VBR
PASS | step 2 window: show and change the settings | profile Main
PASS | step 2 window: show and change the settings | stretch on
PASS | step 2 window: show and change the settings | settings remembered (modprobe.d)
PASS | step 2 window: show and change the settings | CBR 11250 again
PASS | step 3 window while OBS captures | OBS captures (capture device started)
FAIL | step 3 window while OBS captures | a message box said 'Input not changed' (a program is capturing), and after OK the Input shows Composite again? -- First upon opening OBS I had no video because the input was set to S-Video from the previous test. Second, I wouldn't expect the plugin setting to prevent me from changing input setting, I would only expect a block for a setting change that comes from outside OBS. I guess our GUI has no connection to the plugin settings, meaning video stream is limited to 1 program only but settings are separate between driver and plugin. If possible when handling settings in our GUI I would want that if I have the GUI open that if a change is done through the terminal while the GUI is open then the GUI can periodically refresh its state to reflect changes made from a terminal. Besides I don't know what the refresh button does in the GUI, we might not need it.
FAIL | step 3 window while OBS captures | input still composite (expected /Video input : 2/, got: Video input : 3 (S-Video: no signal))
PASS | step 3 window while OBS captures | bitrate changed while capturing
PASS | step 3 window while OBS captures | OBS kept showing the moving picture (a short pause is normal)?
FAIL | step 3 window while OBS captures | OBS: the bitrate slider appears only for VBR (6000-14460), the mode is hidden for 480x360 (slider 4000-15000)? -- there's no slider and I gave a wrong answer before. We need to work on how everything is supposed to work.
