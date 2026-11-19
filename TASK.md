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
17. (Maintainer, 2026-11-19) The OBS plugin's settings are OBS's own: the
    settings window and the terminal never touch them, and the plugin does
    not change the driver's remembered settings (it sets the device's
    settings back when its capture ends, except those changed meanwhile by
    another program). Driver settings can be changed by any program at any
    time, also while a program captures, and apply live. Only a second
    program streaming at the same time is refused.
18. (Maintainer, 2026-11-19) Settings window: refreshes from the device
    every 1–2 s (no Refresh button); bitrate slider always shown, in
    Mbit/s: VBR the average (half the bitrate), CBR the bitrate with the
    range 7.00–11.25 (movable; below 11.25 not recorded). Same in OBS.
19. (Maintainer, 2026-11-19, replaces the separate OBS settings of 17)
    One global setting for everything: the OBS plugin's properties show
    and change the driver's settings like the settings window and the
    terminal; changes apply live from any program. (The previous test may
    have run an older plugin build.)
20. (Maintainer, 2026-11-19) Latency benchmark: a table of latencies per
    operation (capture start, setting change, source change, ... to the
    first stream data), averaged, to find where the driver is slower than
    the Windows software.

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

1. Benchmark (a console on composite; prompts for three console resets):

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-bench.sh > tests/T-bench.sh
bash gchd-test.sh T-bench --branch feat/app-bridge
```

Paste the tables at the end (also in `timing.md` in the log folder).

2. Settings, one global setting (OBS with the plugin, a console on
   composite):

```sh
git -C elgato-gchd show origin/tools:tools/tests/T-060-settings.sh > tests/T-060-settings.sh
bash gchd-test.sh T-060-settings --branch feat/app-bridge
```

## Findings

- Benchmark platform: driver module parameter `timing` (off by default)
  logs `gchd-timing: <µs> <event>` at requests (capture start/stop,
  control, output size, input), source changes (mode, family, detected),
  every SCMD sent (IDLE, INIT, NULL, STOP, START), state completions,
  firmware ready, the first stream data and the first buffer after START.
  `tools/gchd-timing.py` turns them into tables (count, avg, min, max per
  phase); for the recordings it measures change sequences from the NULL
  command. Recordings (26 change sequences in four composite
  recordings): NULL → STOP 1150 ms on average (646–1590), NULL → START
  2295 ms (1673–2766). The recordings contain no bulk IN data, so the
  time from START to the first stream data is measured for the driver
  only. Benchmark test `T-bench`.
- T-060-settings (a45cf10): 11 PASS, 4 FAIL. (1) "(average" cut off in
  the slider label: the label is now only the number. (2) OBS started
  without a picture: the plugin's own input setting was S-Video. (3) The
  input refusal while OBS captured is unwanted (Decisions 17). (4) No
  bitrate slider in OBS's properties (it was hidden for CBR and not shown
  for VBR either): now always shown, a float slider in Mbit/s whose range
  follows mode and output size. Driver: `VIDIOC_S_INPUT` from any file
  handle while another captures (the capture continues with the new
  input); `VIDIOC_S_FMT` while buffers exist on composite/S-video applies
  another output size live through the mode worker's encoder restart (no
  new USB operations). Plugin: reads the device's settings before
  applying its own and sets them back afterwards where unchanged by others.
  Window: refresh every 1.5 s (skipped while the user drags the slider or
  a menu is open), checked on a virtual screen with a stand-in v4l2-ctl
  (a terminal-style change appears by itself).
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

gchd-test T-bench 20261119-082229 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-bench-20261119-082229.log
== build
$ git fetch -q origin
$ git checkout -q feat/app-bridge
$ git pull -q --ff-only
commit: 6143ad5 T-060: one global setting for the OBS plugin; driver timing marks for a latency benchmark
commit: 6143ad5 T-060: one global setting for the OBS plugin; driver timing marks for a latency benchmark
build rc=0 warnings=0

=== step 1: driver with timing marks
$ sh -c pkill -f 'gchd-bridge run'; true
PROMPT: Unplug the Elgato from the PC (or detach it in VirtualBox), wait 3 s, plug it in again.
$ sudo insmod /home/ff5/elgato-gchd/src/driver/elgato_gchd.ko timing=1
H.264 device: /dev/video0
module: 0.5.8 timing=1
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -v width=720,height=480,pixelformat=MPEG -c video_bitrate=11250000,video_bitrate_mode=1
PROMPT: Console on composite, game running (a moving picture).
--- dmesg step 1
[   50.454488] usbcore: deregistering interface driver elgato_gchd
[   53.016249] usb 1-2: USB disconnect, device number 3
[   61.665880] usbcore: registered new interface driver elgato_gchd
[   61.772131] usb 1-2: new high-speed USB device number 4 using xhci_hcd
[   62.150116] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[   62.150135] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[   62.150141] usb 1-2: Product: GameCapture HD
[   62.150146] usb 1-2: Manufacturer: Elgato
[   62.150150] usb 1-2: SerialNumber: 04107F1267
[   62.157108] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[   62.161170] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[   62.189179] elgato_gchd 1-2:1.0: gchd-timing: 62167764 request input 2
[   62.189180] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[   62.189213] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== step 2: capture start x5
capture start 1
capture start 2
capture start 3
capture start 4
capture start 5
--- dmesg step 2
[   71.299384] elgato_gchd 1-2:1.0: gchd-timing: 71278114 request capture-start
[   71.299393] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[   71.376030] elgato_gchd 1-2:1.0: gchd-timing: 71354774 scmd IDLE mode=00
[   71.386703] elgato_gchd 1-2:1.0: gchd-timing: 71365452 state 0x11 reached
[   71.485959] elgato_gchd 1-2:1.0: transcoder defaults loaded
[   71.486071] elgato_gchd 1-2:1.0: gchd-timing: 71464843 scmd INIT mode=00
[   71.553696] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[   71.553702] elgato_gchd 1-2:1.0: gchd-timing: 71532489 firmware ready
[   71.978264] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[   72.047032] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[   72.049191] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[   72.049193] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[   72.049202] elgato_gchd 1-2:1.0: gchd-timing: 72028087 source detected
[   72.049203] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[   72.049204] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[   72.822665] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[   72.822804] elgato_gchd 1-2:1.0: gchd-timing: 72801840 scmd INIT mode=a0
[   73.751219] elgato_gchd 1-2:1.0: back-end not settled: status 0xf2
[   73.772277] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[   73.795517] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[   73.795669] elgato_gchd 1-2:1.0: gchd-timing: 73774885 scmd INIT mode=a0
[   73.795886] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[   73.803772] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[   73.806229] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[   73.807091] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[   73.807290] elgato_gchd 1-2:1.0: gchd-timing: 73786508 scmd START
[   73.934726] elgato_gchd 1-2:1.0: gchd-timing: 73913967 state 0x02 reached
[   73.934728] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[   74.309417] elgato_gchd 1-2:1.0: gchd-timing: 74288715 stream first-data ep=0x81
[   74.309588] elgato_gchd 1-2:1.0: gchd-timing: 74288894 stream first-buffer
[   78.814328] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[   82.626941] elgato_gchd 1-2:1.0: gchd-timing: 82607358 request capture-stop
[   82.627733] elgato_gchd 1-2:1.0: gchd-timing: 82608152 scmd NULL
[   83.313745] elgato_gchd 1-2:1.0: gchd-timing: 83294223 state NULL reached
[   83.338285] elgato_gchd 1-2:1.0: gchd-timing: 83318769 scmd STOP
[   83.356325] elgato_gchd 1-2:1.0: gchd-timing: 83336809 state STOP reached
[   83.395682] elgato_gchd 1-2:1.0: gchd-timing: 83376177 scmd INIT mode=a0
[   83.396571] elgato_gchd 1-2:1.0: gchd-timing: 83377067 scmd IDLE mode=00
[   83.409831] elgato_gchd 1-2:1.0: gchd-timing: 83390328 state 0x11 reached
[   83.410015] elgato_gchd 1-2:1.0: gchd-timing: 83390512 scmd OTHER mode=01
[   83.416259] elgato_gchd 1-2:1.0: gchd-timing: 83396756 state 0x12 reached
[   83.416363] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[   87.511326] elgato_gchd 1-2:1.0: gchd-timing: 87492170 request capture-start
[   87.511327] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[   87.513374] elgato_gchd 1-2:1.0: gchd-timing: 87494218 scmd OTHER mode=00
[   87.525618] elgato_gchd 1-2:1.0: gchd-timing: 87506462 state 0x10 reached
[   87.525872] elgato_gchd 1-2:1.0: gchd-timing: 87506717 scmd IDLE mode=00
[   87.531609] elgato_gchd 1-2:1.0: gchd-timing: 87512454 state 0x11 reached
[   87.626036] elgato_gchd 1-2:1.0: transcoder defaults loaded
[   87.626294] elgato_gchd 1-2:1.0: gchd-timing: 87607146 scmd INIT mode=00
[   87.688418] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[   87.688422] elgato_gchd 1-2:1.0: gchd-timing: 87669279 firmware ready
[   87.772823] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[   87.962175] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[   88.049262] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[   88.075255] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[   88.077115] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[   88.148501] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[   88.150740] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[   88.225624] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[   88.227503] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[   88.313123] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[   88.316198] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[   88.316202] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[   88.316207] elgato_gchd 1-2:1.0: gchd-timing: 88297108 source detected
[   88.316209] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[   88.316213] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[   89.064326] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[   89.064474] elgato_gchd 1-2:1.0: gchd-timing: 89045426 scmd INIT mode=a0
[   89.840954] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[   89.856446] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[   89.856565] elgato_gchd 1-2:1.0: gchd-timing: 89837570 scmd INIT mode=a0
[   89.856663] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[   89.862108] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[   89.862733] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[   89.863180] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[   89.863298] elgato_gchd 1-2:1.0: gchd-timing: 89844303 scmd START
[   89.962704] elgato_gchd 1-2:1.0: gchd-timing: 89943715 state 0x02 reached
[   89.962707] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[   90.239818] elgato_gchd 1-2:1.0: gchd-timing: 90220844 source mode-change
[   90.239829] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[   90.335195] elgato_gchd 1-2:1.0: gchd-timing: 90316222 stream first-data ep=0x81
[   90.335607] elgato_gchd 1-2:1.0: gchd-timing: 90316640 stream first-buffer
[   90.740716] elgato_gchd 1-2:1.0: gchd-timing: 90721773 scmd NULL
[   91.706898] elgato_gchd 1-2:1.0: gchd-timing: 91688009 state NULL reached
[   91.725906] elgato_gchd 1-2:1.0: gchd-timing: 91707016 scmd STOP
[   91.742914] elgato_gchd 1-2:1.0: gchd-timing: 91724025 state STOP reached
[   91.834575] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[   91.834721] elgato_gchd 1-2:1.0: gchd-timing: 91815844 scmd INIT mode=a0
[   91.834815] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[   91.840268] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[   91.841069] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[   91.841439] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[   91.841544] elgato_gchd 1-2:1.0: gchd-timing: 91822667 scmd START
[   91.940341] elgato_gchd 1-2:1.0: gchd-timing: 91921469 state 0x02 reached
[   91.940343] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[   91.940345] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[   92.313337] elgato_gchd 1-2:1.0: gchd-timing: 92294479 stream first-data ep=0x81
[   92.313612] elgato_gchd 1-2:1.0: gchd-timing: 92294760 stream first-buffer
[   96.864602] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[   99.496308] elgato_gchd 1-2:1.0: gchd-timing: 99477764 request capture-stop
[   99.497492] elgato_gchd 1-2:1.0: gchd-timing: 99478949 scmd NULL
[  100.228882] elgato_gchd 1-2:1.0: gchd-timing: 100210362 state NULL reached
[  100.246988] elgato_gchd 1-2:1.0: gchd-timing: 100228468 scmd STOP
[  100.265721] elgato_gchd 1-2:1.0: gchd-timing: 100247202 state STOP reached
[  100.300989] elgato_gchd 1-2:1.0: gchd-timing: 100282475 scmd INIT mode=a0
[  100.303485] elgato_gchd 1-2:1.0: gchd-timing: 100284973 scmd IDLE mode=00
[  100.315592] elgato_gchd 1-2:1.0: gchd-timing: 100297080 state 0x11 reached
[  100.315935] elgato_gchd 1-2:1.0: gchd-timing: 100297424 scmd OTHER mode=01
[  100.323210] elgato_gchd 1-2:1.0: gchd-timing: 100304698 state 0x12 reached
[  100.323442] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[  104.419768] elgato_gchd 1-2:1.0: gchd-timing: 104401374 request capture-start
[  104.419770] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  104.422685] elgato_gchd 1-2:1.0: gchd-timing: 104404290 scmd OTHER mode=00
[  104.434900] elgato_gchd 1-2:1.0: gchd-timing: 104416506 state 0x10 reached
[  104.435099] elgato_gchd 1-2:1.0: gchd-timing: 104416706 scmd IDLE mode=00
[  104.441259] elgato_gchd 1-2:1.0: gchd-timing: 104422865 state 0x11 reached
[  104.508117] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  104.508225] elgato_gchd 1-2:1.0: gchd-timing: 104489833 scmd INIT mode=00
[  104.576183] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  104.576190] elgato_gchd 1-2:1.0: gchd-timing: 104557799 firmware ready
[  104.659999] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[  104.838798] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[  104.924935] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[  105.025545] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[  105.061699] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  105.063972] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[  105.139640] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[  105.142305] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[  105.229445] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  105.313517] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[  105.316058] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[  105.316062] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[  105.316065] elgato_gchd 1-2:1.0: gchd-timing: 105297693 source detected
[  105.316067] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[  105.316069] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[  106.071034] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  106.071174] elgato_gchd 1-2:1.0: gchd-timing: 106052819 scmd INIT mode=a0
[  106.217162] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  106.233737] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  106.233843] elgato_gchd 1-2:1.0: gchd-timing: 106215491 scmd INIT mode=a0
[  106.233932] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  106.254221] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  106.254984] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  106.255407] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  106.255526] elgato_gchd 1-2:1.0: gchd-timing: 106237175 scmd START
[  106.422934] elgato_gchd 1-2:1.0: gchd-timing: 106404587 state 0x02 reached
[  106.422935] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  106.796426] elgato_gchd 1-2:1.0: gchd-timing: 106778081 stream first-data ep=0x81
[  106.796650] elgato_gchd 1-2:1.0: gchd-timing: 106778310 stream first-buffer
[  111.260129] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  115.151521] elgato_gchd 1-2:1.0: gchd-timing: 115133318 request capture-stop
[  115.152423] elgato_gchd 1-2:1.0: gchd-timing: 115134222 scmd NULL
[  115.785093] elgato_gchd 1-2:1.0: gchd-timing: 115766894 state NULL reached
[  115.798401] elgato_gchd 1-2:1.0: gchd-timing: 115780204 scmd STOP
[  115.828996] elgato_gchd 1-2:1.0: gchd-timing: 115810797 state STOP reached
[  115.879917] elgato_gchd 1-2:1.0: gchd-timing: 115861725 scmd INIT mode=a0
[  115.880970] elgato_gchd 1-2:1.0: gchd-timing: 115862779 scmd IDLE mode=00
[  115.894968] elgato_gchd 1-2:1.0: gchd-timing: 115876776 state 0x11 reached
[  115.895322] elgato_gchd 1-2:1.0: gchd-timing: 115877131 scmd OTHER mode=01
[  115.901771] elgato_gchd 1-2:1.0: gchd-timing: 115883580 state 0x12 reached
[  115.901867] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[  119.959117] elgato_gchd 1-2:1.0: gchd-timing: 119940970 request capture-start
[  119.959119] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  119.962979] elgato_gchd 1-2:1.0: gchd-timing: 119944831 scmd OTHER mode=00
[  119.975088] elgato_gchd 1-2:1.0: gchd-timing: 119956941 state 0x10 reached
[  119.975329] elgato_gchd 1-2:1.0: gchd-timing: 119957181 scmd IDLE mode=00
[  119.981085] elgato_gchd 1-2:1.0: gchd-timing: 119962937 state 0x11 reached
[  120.027689] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  120.027839] elgato_gchd 1-2:1.0: gchd-timing: 120009692 scmd INIT mode=00
[  120.093831] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  120.093836] elgato_gchd 1-2:1.0: gchd-timing: 120075690 firmware ready
[  120.247404] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[  120.392771] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[  120.518324] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[  120.552248] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[  120.554615] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[  120.628320] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[  120.630433] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[  120.630434] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[  120.630437] elgato_gchd 1-2:1.0: gchd-timing: 120612295 source detected
[  120.630438] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[  120.630440] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[  121.400240] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  121.400355] elgato_gchd 1-2:1.0: gchd-timing: 121382221 scmd INIT mode=a0
[  121.524015] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  121.549823] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  121.549988] elgato_gchd 1-2:1.0: gchd-timing: 121531855 scmd INIT mode=a0
[  121.550203] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  121.579073] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  121.580018] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  121.580636] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  121.580806] elgato_gchd 1-2:1.0: gchd-timing: 121562672 scmd START
[  121.672520] elgato_gchd 1-2:1.0: gchd-timing: 121654388 state 0x02 reached
[  121.672521] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  122.044875] elgato_gchd 1-2:1.0: gchd-timing: 122026742 stream first-data ep=0x81
[  122.044999] elgato_gchd 1-2:1.0: gchd-timing: 122026870 stream first-buffer
[  124.384553] elgato_gchd 1-2:1.0: gchd-timing: 124366438 source mode-change
[  124.384568] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[  124.877699] elgato_gchd 1-2:1.0: gchd-timing: 124859590 scmd NULL
[  125.556557] elgato_gchd 1-2:1.0: gchd-timing: 125538447 state NULL reached
[  125.569557] elgato_gchd 1-2:1.0: gchd-timing: 125551449 scmd STOP
[  125.603245] elgato_gchd 1-2:1.0: gchd-timing: 125585135 state STOP reached
[  125.845197] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  125.845326] elgato_gchd 1-2:1.0: gchd-timing: 125827224 scmd INIT mode=a0
[  125.845546] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  125.852100] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  125.852960] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[  125.853519] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  125.853645] elgato_gchd 1-2:1.0: gchd-timing: 125835543 scmd START
[  125.951090] elgato_gchd 1-2:1.0: gchd-timing: 125932989 state 0x02 reached
[  125.951093] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  125.951095] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[  126.325774] elgato_gchd 1-2:1.0: gchd-timing: 126307668 stream first-data ep=0x81
[  126.325953] elgato_gchd 1-2:1.0: gchd-timing: 126307855 stream first-buffer
[  128.357655] elgato_gchd 1-2:1.0: gchd-timing: 128339563 request capture-stop
[  128.359260] elgato_gchd 1-2:1.0: gchd-timing: 128341171 scmd NULL
[  129.115969] elgato_gchd 1-2:1.0: gchd-timing: 129097880 state NULL reached
[  129.132962] elgato_gchd 1-2:1.0: gchd-timing: 129114874 scmd STOP
[  129.194867] elgato_gchd 1-2:1.0: gchd-timing: 129176778 state STOP reached
[  129.217425] elgato_gchd 1-2:1.0: gchd-timing: 129199342 scmd INIT mode=a0
[  129.218742] elgato_gchd 1-2:1.0: gchd-timing: 129200660 scmd IDLE mode=00
[  129.231699] elgato_gchd 1-2:1.0: gchd-timing: 129213616 state 0x11 reached
[  129.231910] elgato_gchd 1-2:1.0: gchd-timing: 129213829 scmd OTHER mode=01
[  129.238237] elgato_gchd 1-2:1.0: gchd-timing: 129220155 state 0x12 reached
[  129.238338] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[  133.288755] elgato_gchd 1-2:1.0: gchd-timing: 133270692 request capture-start
[  133.288757] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  133.291834] elgato_gchd 1-2:1.0: gchd-timing: 133273771 scmd OTHER mode=00
[  133.303858] elgato_gchd 1-2:1.0: gchd-timing: 133285794 state 0x10 reached
[  133.304084] elgato_gchd 1-2:1.0: gchd-timing: 133286022 scmd IDLE mode=00
[  133.309837] elgato_gchd 1-2:1.0: gchd-timing: 133291774 state 0x11 reached
[  133.370262] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  133.370373] elgato_gchd 1-2:1.0: gchd-timing: 133352311 scmd INIT mode=00
[  133.431976] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  133.431981] elgato_gchd 1-2:1.0: gchd-timing: 133413919 firmware ready
[  133.570891] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[  133.631453] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x26 masked_mode=0x6
[  133.633355] elgato_gchd 1-2:1.0: analog format status 0x6e not locked yet
[  133.696158] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[  133.725469] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x36 masked_mode=0x6
[  133.727350] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=89); waiting up to 5000 ms
[  133.833613] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  133.915395] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[  134.024461] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[  134.112690] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  134.115198] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[  134.115200] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[  134.115203] elgato_gchd 1-2:1.0: gchd-timing: 134097144 source detected
[  134.115205] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[  134.115207] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[  134.853339] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  134.853444] elgato_gchd 1-2:1.0: gchd-timing: 134835405 scmd INIT mode=a0
[  135.656028] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  135.671066] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  135.671171] elgato_gchd 1-2:1.0: gchd-timing: 135653180 scmd INIT mode=a0
[  135.671256] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  135.675960] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  135.676649] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[  135.677092] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  135.677205] elgato_gchd 1-2:1.0: gchd-timing: 135659214 scmd START
[  135.776881] elgato_gchd 1-2:1.0: gchd-timing: 135758896 state 0x02 reached
[  135.776883] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  136.151633] elgato_gchd 1-2:1.0: gchd-timing: 136133662 stream first-data ep=0x81
[  136.151900] elgato_gchd 1-2:1.0: gchd-timing: 136133936 stream first-buffer
[  140.700884] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  144.465250] elgato_gchd 1-2:1.0: gchd-timing: 144447708 request capture-stop
[  144.466284] elgato_gchd 1-2:1.0: gchd-timing: 144448745 scmd NULL
[  145.136059] elgato_gchd 1-2:1.0: gchd-timing: 145118544 state NULL reached
[  145.151892] elgato_gchd 1-2:1.0: gchd-timing: 145134380 scmd STOP
[  145.168043] elgato_gchd 1-2:1.0: gchd-timing: 145150530 state STOP reached
[  145.225775] elgato_gchd 1-2:1.0: gchd-timing: 145208270 scmd INIT mode=a0
[  145.227165] elgato_gchd 1-2:1.0: gchd-timing: 145209662 scmd IDLE mode=00
[  145.240027] elgato_gchd 1-2:1.0: gchd-timing: 145222523 state 0x11 reached
[  145.240219] elgato_gchd 1-2:1.0: gchd-timing: 145222716 scmd OTHER mode=01
[  145.246520] elgato_gchd 1-2:1.0: gchd-timing: 145229017 state 0x12 reached
[  145.246614] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 3: bitrate change x5 while capturing
$ v4l2-ctl -d /dev/video0 -c video_bitrate_mode=0,video_bitrate=8000000
$ v4l2-ctl -d /dev/video0 -c video_bitrate_mode=1,video_bitrate=11250000
$ v4l2-ctl -d /dev/video0 -c video_bitrate_mode=0,video_bitrate=8000000
$ v4l2-ctl -d /dev/video0 -c video_bitrate_mode=1,video_bitrate=11250000
$ v4l2-ctl -d /dev/video0 -c video_bitrate_mode=0,video_bitrate=8000000
$ v4l2-ctl -d /dev/video0 -c video_bitrate_mode=1,video_bitrate=11250000
--- dmesg step 3
[  149.352343] elgato_gchd 1-2:1.0: gchd-timing: 149335014 request capture-start
[  149.352344] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  149.354743] elgato_gchd 1-2:1.0: gchd-timing: 149337412 scmd OTHER mode=00
[  149.366950] elgato_gchd 1-2:1.0: gchd-timing: 149349619 state 0x10 reached
[  149.367142] elgato_gchd 1-2:1.0: gchd-timing: 149349813 scmd IDLE mode=00
[  149.372934] elgato_gchd 1-2:1.0: gchd-timing: 149355605 state 0x11 reached
[  149.447417] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  149.447565] elgato_gchd 1-2:1.0: gchd-timing: 149430239 scmd INIT mode=00
[  149.508801] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  149.508808] elgato_gchd 1-2:1.0: gchd-timing: 149491485 firmware ready
[  149.646251] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[  149.801767] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  149.804433] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[  149.883729] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[  149.912201] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[  149.914393] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[  149.983698] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[  150.056013] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[  150.137983] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  150.140530] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[  150.140531] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[  150.140533] elgato_gchd 1-2:1.0: gchd-timing: 150123235 source detected
[  150.140534] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[  150.140535] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[  150.882731] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  150.882846] elgato_gchd 1-2:1.0: gchd-timing: 150865577 scmd INIT mode=a0
[  151.386516] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  151.401378] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  151.401487] elgato_gchd 1-2:1.0: gchd-timing: 151384237 scmd INIT mode=a0
[  151.401575] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  151.406571] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  151.407559] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[  151.408005] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  151.408112] elgato_gchd 1-2:1.0: gchd-timing: 151390863 scmd START
[  151.507191] elgato_gchd 1-2:1.0: gchd-timing: 151489946 state 0x02 reached
[  151.507193] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  151.881690] elgato_gchd 1-2:1.0: gchd-timing: 151864453 stream first-data ep=0x81
[  151.881945] elgato_gchd 1-2:1.0: gchd-timing: 151864712 stream first-buffer
[  155.187692] elgato_gchd 1-2:1.0: gchd-timing: 155170574 source mode-change
[  155.187708] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[  155.671129] elgato_gchd 1-2:1.0: gchd-timing: 155654031 scmd NULL
[  156.339371] elgato_gchd 1-2:1.0: gchd-timing: 156322287 state NULL reached
[  156.352854] elgato_gchd 1-2:1.0: gchd-timing: 156335776 scmd STOP
[  156.372982] elgato_gchd 1-2:1.0: gchd-timing: 156355900 state STOP reached
[  156.667387] elgato_gchd 1-2:1.0: back-end not settled: status 0xf2
[  156.687124] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  156.687244] elgato_gchd 1-2:1.0: gchd-timing: 156670182 scmd INIT mode=a0
[  156.687349] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  156.693167] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  156.693827] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  156.694228] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  156.694347] elgato_gchd 1-2:1.0: gchd-timing: 156677285 scmd START
[  156.792998] elgato_gchd 1-2:1.0: gchd-timing: 156775940 state 0x02 reached
[  156.793000] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  156.793002] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[  157.166230] elgato_gchd 1-2:1.0: gchd-timing: 157149175 stream first-data ep=0x81
[  157.166451] elgato_gchd 1-2:1.0: gchd-timing: 157149403 stream first-buffer
[  159.327903] elgato_gchd 1-2:1.0: gchd-timing: 159310925 request control 0x9909ce
[  159.327915] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[  159.327917] elgato_gchd 1-2:1.0: gchd-timing: 159310939 restart encoder
[  159.327959] elgato_gchd 1-2:1.0: gchd-timing: 159310981 request control 0x9909cf
[  159.328756] elgato_gchd 1-2:1.0: gchd-timing: 159311777 scmd NULL
[  160.006612] elgato_gchd 1-2:1.0: gchd-timing: 159989648 state NULL reached
[  160.024103] elgato_gchd 1-2:1.0: gchd-timing: 160007139 scmd STOP
[  160.084948] elgato_gchd 1-2:1.0: gchd-timing: 160067985 state STOP reached
[  160.136196] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  160.136312] elgato_gchd 1-2:1.0: gchd-timing: 160119359 scmd INIT mode=a0
[  160.136413] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  160.142157] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  160.142686] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  160.143332] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  160.143469] elgato_gchd 1-2:1.0: gchd-timing: 160126516 scmd START
[  160.258734] elgato_gchd 1-2:1.0: gchd-timing: 160241784 state 0x02 reached
[  160.258735] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  160.258737] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[  160.858791] elgato_gchd 1-2:1.0: gchd-timing: 160841851 stream first-data ep=0x81
[  160.859194] elgato_gchd 1-2:1.0: gchd-timing: 160842261 stream first-buffer
[  165.174144] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=31 bytes=2031616 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  167.397730] elgato_gchd 1-2:1.0: gchd-timing: 167380978 request control 0x9909ce
[  167.397774] elgato_gchd 1-2:1.0: gchd-timing: 167381023 request control 0x9909cf
[  167.398449] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[  167.398457] elgato_gchd 1-2:1.0: gchd-timing: 167381706 restart encoder
[  167.399633] elgato_gchd 1-2:1.0: gchd-timing: 167382881 scmd NULL
[  168.778050] elgato_gchd 1-2:1.0: gchd-timing: 168761327 state NULL reached
[  168.792720] elgato_gchd 1-2:1.0: gchd-timing: 168775997 scmd STOP
[  168.810282] elgato_gchd 1-2:1.0: gchd-timing: 168793561 state STOP reached
[  168.877524] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  168.877643] elgato_gchd 1-2:1.0: gchd-timing: 168860929 scmd INIT mode=a0
[  168.877741] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  168.883595] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  168.884207] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  168.884676] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  168.884785] elgato_gchd 1-2:1.0: gchd-timing: 168868071 scmd START
[  169.000197] elgato_gchd 1-2:1.0: gchd-timing: 168983486 state 0x02 reached
[  169.000204] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  169.000206] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[  169.372684] elgato_gchd 1-2:1.0: gchd-timing: 169355980 stream first-data ep=0x81
[  169.372759] elgato_gchd 1-2:1.0: gchd-timing: 169356057 stream first-buffer
[  173.922830] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  175.412862] elgato_gchd 1-2:1.0: gchd-timing: 175396297 request control 0x9909ce
[  175.412902] elgato_gchd 1-2:1.0: gchd-timing: 175396337 request control 0x9909cf
[  175.413414] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[  175.413421] elgato_gchd 1-2:1.0: gchd-timing: 175396856 restart encoder
[  175.414256] elgato_gchd 1-2:1.0: gchd-timing: 175397691 scmd NULL
[  176.081418] elgato_gchd 1-2:1.0: gchd-timing: 176064862 state NULL reached
[  176.101944] elgato_gchd 1-2:1.0: gchd-timing: 176085389 scmd STOP
[  176.122375] elgato_gchd 1-2:1.0: gchd-timing: 176105819 state STOP reached
[  176.172490] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  176.172623] elgato_gchd 1-2:1.0: gchd-timing: 176156074 scmd INIT mode=a0
[  176.172854] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  176.178766] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  176.179509] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  176.180083] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  176.180230] elgato_gchd 1-2:1.0: gchd-timing: 176163682 scmd START
[  176.278396] elgato_gchd 1-2:1.0: gchd-timing: 176261850 state 0x02 reached
[  176.278397] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  176.278400] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[  176.895655] elgato_gchd 1-2:1.0: gchd-timing: 176879116 stream first-data ep=0x81
[  176.895906] elgato_gchd 1-2:1.0: gchd-timing: 176879373 stream first-buffer
[  181.209723] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=28 bytes=1835008 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  183.423694] elgato_gchd 1-2:1.0: gchd-timing: 183407287 request control 0x9909ce
[  183.423716] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[  183.423720] elgato_gchd 1-2:1.0: gchd-timing: 183407313 restart encoder
[  183.423780] elgato_gchd 1-2:1.0: gchd-timing: 183407373 request control 0x9909cf
[  183.424682] elgato_gchd 1-2:1.0: gchd-timing: 183408272 scmd NULL
[  184.796548] elgato_gchd 1-2:1.0: gchd-timing: 184780164 state NULL reached
[  184.853462] elgato_gchd 1-2:1.0: gchd-timing: 184837075 scmd STOP
[  184.866406] elgato_gchd 1-2:1.0: gchd-timing: 184850024 state STOP reached
[  184.904905] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  184.905053] elgato_gchd 1-2:1.0: gchd-timing: 184888672 scmd INIT mode=a0
[  184.905178] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  184.912387] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  184.913756] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  184.914359] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  184.914524] elgato_gchd 1-2:1.0: gchd-timing: 184898143 scmd START
[  185.010969] elgato_gchd 1-2:1.0: gchd-timing: 184994590 state 0x02 reached
[  185.010971] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  185.010973] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[  185.383820] elgato_gchd 1-2:1.0: gchd-timing: 185367440 stream first-data ep=0x81
[  185.384007] elgato_gchd 1-2:1.0: gchd-timing: 185367634 stream first-buffer
[  187.105608] elgato_gchd 1-2:1.0: gchd-timing: 187089260 request capture-stop
[  187.106642] elgato_gchd 1-2:1.0: gchd-timing: 187090298 scmd NULL
[  187.743716] elgato_gchd 1-2:1.0: gchd-timing: 187727378 state NULL reached
[  187.756500] elgato_gchd 1-2:1.0: gchd-timing: 187740164 scmd STOP
[  187.771599] elgato_gchd 1-2:1.0: gchd-timing: 187755261 state STOP reached
[  187.895577] elgato_gchd 1-2:1.0: gchd-timing: 187879246 scmd INIT mode=a0
[  187.896684] elgato_gchd 1-2:1.0: gchd-timing: 187880354 scmd IDLE mode=00
[  187.909680] elgato_gchd 1-2:1.0: gchd-timing: 187893350 state 0x11 reached
[  187.909868] elgato_gchd 1-2:1.0: gchd-timing: 187893539 scmd OTHER mode=01
[  187.915988] elgato_gchd 1-2:1.0: gchd-timing: 187899658 state 0x12 reached
[  187.916069] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 4: output size change x4 while capturing
$ v4l2-ctl -d /dev/video0 -v width=640,height=480
$ v4l2-ctl -d /dev/video0 -v width=720,height=480
$ v4l2-ctl -d /dev/video0 -v width=640,height=480
$ v4l2-ctl -d /dev/video0 -v width=720,height=480
--- dmesg step 4
[  203.587827] elgato_gchd 1-2:1.0: gchd-timing: 203571724 request capture-start
[  203.587828] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  203.591067] elgato_gchd 1-2:1.0: gchd-timing: 203574965 scmd OTHER mode=00
[  203.602925] elgato_gchd 1-2:1.0: gchd-timing: 203586822 state 0x10 reached
[  203.603253] elgato_gchd 1-2:1.0: gchd-timing: 203587151 scmd IDLE mode=00
[  203.609252] elgato_gchd 1-2:1.0: gchd-timing: 203593149 state 0x11 reached
[  203.683792] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  203.683904] elgato_gchd 1-2:1.0: gchd-timing: 203667803 scmd INIT mode=00
[  203.753009] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  203.753016] elgato_gchd 1-2:1.0: gchd-timing: 203736916 firmware ready
[  203.898530] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[  204.030371] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[  204.149039] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[  204.189425] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[  204.191972] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[  204.287999] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  204.290671] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[  204.442321] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  204.446193] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[  204.446197] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[  204.446203] elgato_gchd 1-2:1.0: gchd-timing: 204430112 source detected
[  204.446206] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[  204.446210] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[  205.225044] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  205.225170] elgato_gchd 1-2:1.0: gchd-timing: 205209088 scmd INIT mode=a0
[  205.727916] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  205.743186] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  205.743300] elgato_gchd 1-2:1.0: gchd-timing: 205727225 scmd INIT mode=a0
[  205.743393] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  205.749030] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  205.749922] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[  205.750426] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  205.750547] elgato_gchd 1-2:1.0: gchd-timing: 205734472 scmd START
[  205.865702] elgato_gchd 1-2:1.0: gchd-timing: 205849629 state 0x02 reached
[  205.865704] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  206.239937] elgato_gchd 1-2:1.0: gchd-timing: 206223860 stream first-data ep=0x81
[  206.240060] elgato_gchd 1-2:1.0: gchd-timing: 206223991 stream first-buffer
[  210.790253] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  213.560554] elgato_gchd 1-2:1.0: gchd-timing: 213544570 request output 640x480
[  213.560950] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[  213.560956] elgato_gchd 1-2:1.0: gchd-timing: 213544972 restart encoder
[  213.561931] elgato_gchd 1-2:1.0: gchd-timing: 213545946 scmd NULL
[  214.217379] elgato_gchd 1-2:1.0: gchd-timing: 214201397 state NULL reached
[  214.233626] elgato_gchd 1-2:1.0: gchd-timing: 214217644 scmd STOP
[  214.266402] elgato_gchd 1-2:1.0: gchd-timing: 214250418 state STOP reached
[  214.313224] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  214.313352] elgato_gchd 1-2:1.0: gchd-timing: 214297377 scmd INIT mode=a0
[  214.313464] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  214.319068] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  214.319836] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[  214.320529] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  214.320658] elgato_gchd 1-2:1.0: gchd-timing: 214304682 scmd START
[  214.419232] elgato_gchd 1-2:1.0: gchd-timing: 214403258 state 0x02 reached
[  214.419238] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  214.419240] elgato_gchd 1-2:1.0: analog mode now 240p, output 1
[  214.792055] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[  214.792080] elgato_gchd 1-2:1.0: gchd-timing: 214776109 stream first-data ep=0x84
[  214.792629] elgato_gchd 1-2:1.0: gchd-timing: 214776659 stream first-buffer
[  219.351302] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  219.453749] elgato_gchd 1-2:1.0: gchd-timing: 219437827 source mode-change
[  219.453754] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[  219.962272] elgato_gchd 1-2:1.0: gchd-timing: 219946356 scmd NULL
[  220.640508] elgato_gchd 1-2:1.0: gchd-timing: 220624594 state NULL reached
[  220.661699] elgato_gchd 1-2:1.0: gchd-timing: 220645786 scmd STOP
[  220.685302] elgato_gchd 1-2:1.0: gchd-timing: 220669389 state STOP reached
[  220.908287] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  220.908521] elgato_gchd 1-2:1.0: gchd-timing: 220892615 scmd INIT mode=a0
[  220.908644] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  220.914219] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  220.914899] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  220.915329] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  220.915448] elgato_gchd 1-2:1.0: gchd-timing: 220899543 scmd START
[  221.030861] elgato_gchd 1-2:1.0: gchd-timing: 221014957 state 0x02 reached
[  221.030862] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  221.030864] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[  221.401120] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[  221.401147] elgato_gchd 1-2:1.0: gchd-timing: 221385246 stream first-data ep=0x84
[  221.401667] elgato_gchd 1-2:1.0: gchd-timing: 221385754 stream first-buffer
[  221.570088] elgato_gchd 1-2:1.0: gchd-timing: 221554188 request output 720x480
[  221.578918] elgato_gchd 1-2:1.0: encoder restart: output 1 -> 0
[  221.578922] elgato_gchd 1-2:1.0: gchd-timing: 221563023 restart encoder
[  221.579136] elgato_gchd 1-2:1.0: gchd-timing: 221563237 scmd NULL
[  222.635119] elgato_gchd 1-2:1.0: gchd-timing: 222619225 state NULL reached
[  222.666545] elgato_gchd 1-2:1.0: gchd-timing: 222650652 scmd STOP
[  222.688611] elgato_gchd 1-2:1.0: gchd-timing: 222672719 state STOP reached
[  222.755306] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  222.755430] elgato_gchd 1-2:1.0: gchd-timing: 222739543 scmd INIT mode=a0
[  222.755661] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  222.760843] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  222.761366] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  222.761863] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  222.761994] elgato_gchd 1-2:1.0: gchd-timing: 222746107 scmd START
[  222.861431] elgato_gchd 1-2:1.0: gchd-timing: 222845545 state 0x02 reached
[  222.861433] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  222.861435] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[  223.233503] elgato_gchd 1-2:1.0: gchd-timing: 223217614 stream first-data ep=0x81
[  223.233675] elgato_gchd 1-2:1.0: gchd-timing: 223217792 stream first-buffer
[  227.784226] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  229.587152] elgato_gchd 1-2:1.0: gchd-timing: 229571332 request output 640x480
[  229.587220] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[  229.587227] elgato_gchd 1-2:1.0: gchd-timing: 229571406 restart encoder
[  229.590329] elgato_gchd 1-2:1.0: gchd-timing: 229574508 scmd NULL
[  230.224577] elgato_gchd 1-2:1.0: gchd-timing: 230208756 state NULL reached
[  230.238922] elgato_gchd 1-2:1.0: gchd-timing: 230223103 scmd STOP
[  230.259151] elgato_gchd 1-2:1.0: gchd-timing: 230243331 state STOP reached
[  230.610272] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  230.610405] elgato_gchd 1-2:1.0: gchd-timing: 230594594 scmd INIT mode=a0
[  230.610521] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  230.616927] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  230.617892] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  230.618381] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  230.618500] elgato_gchd 1-2:1.0: gchd-timing: 230602690 scmd START
[  230.733662] elgato_gchd 1-2:1.0: gchd-timing: 230717853 state 0x02 reached
[  230.733665] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  230.733667] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[  231.103560] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[  231.103587] elgato_gchd 1-2:1.0: gchd-timing: 231087780 stream first-data ep=0x84
[  231.103782] elgato_gchd 1-2:1.0: gchd-timing: 231087975 stream first-buffer
[  235.619100] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  237.608137] elgato_gchd 1-2:1.0: gchd-timing: 237592390 request output 720x480
[  237.608297] elgato_gchd 1-2:1.0: encoder restart: output 1 -> 0
[  237.608299] elgato_gchd 1-2:1.0: gchd-timing: 237592551 restart encoder
[  237.609620] elgato_gchd 1-2:1.0: gchd-timing: 237593872 scmd NULL
[  238.296943] elgato_gchd 1-2:1.0: gchd-timing: 238281195 state NULL reached
[  238.310974] elgato_gchd 1-2:1.0: gchd-timing: 238295226 scmd STOP
[  238.374013] elgato_gchd 1-2:1.0: gchd-timing: 238358265 state STOP reached
[  238.516175] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  238.516290] elgato_gchd 1-2:1.0: gchd-timing: 238500551 scmd INIT mode=a0
[  238.516486] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  238.521466] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  238.522011] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  238.522381] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  238.522478] elgato_gchd 1-2:1.0: gchd-timing: 238506739 scmd START
[  238.638723] elgato_gchd 1-2:1.0: gchd-timing: 238622984 state 0x02 reached
[  238.638724] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  238.638727] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[  239.011025] elgato_gchd 1-2:1.0: gchd-timing: 238995283 stream first-data ep=0x81
[  239.011214] elgato_gchd 1-2:1.0: gchd-timing: 238995478 stream first-buffer
[  239.418581] elgato_gchd 1-2:1.0: gchd-timing: 239402843 request capture-stop
[  239.420150] elgato_gchd 1-2:1.0: gchd-timing: 239404416 scmd NULL
[  240.392227] elgato_gchd 1-2:1.0: gchd-timing: 240376499 state NULL reached
[  240.437150] elgato_gchd 1-2:1.0: gchd-timing: 240421422 scmd STOP
[  240.468568] elgato_gchd 1-2:1.0: gchd-timing: 240452838 state STOP reached
[  240.592944] elgato_gchd 1-2:1.0: gchd-timing: 240577221 scmd INIT mode=a0
[  240.594101] elgato_gchd 1-2:1.0: gchd-timing: 240578380 scmd IDLE mode=00
[  240.607369] elgato_gchd 1-2:1.0: gchd-timing: 240591647 state 0x11 reached
[  240.607659] elgato_gchd 1-2:1.0: gchd-timing: 240591938 scmd OTHER mode=01
[  240.613948] elgato_gchd 1-2:1.0: gchd-timing: 240598227 state 0x12 reached
[  240.614043] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 5: input change x2 while capturing (S-video without a source, back to composite)
$ v4l2-ctl -d /dev/video0 -i 3
Video input set to 3 (S-Video: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -i 3
Video input set to 3 (S-Video: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
--- dmesg step 5
[  249.695417] elgato_gchd 1-2:1.0: gchd-timing: 249679772 request capture-start
[  249.695419] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  249.697373] elgato_gchd 1-2:1.0: gchd-timing: 249681727 scmd OTHER mode=00
[  249.709646] elgato_gchd 1-2:1.0: gchd-timing: 249694001 state 0x10 reached
[  249.710106] elgato_gchd 1-2:1.0: gchd-timing: 249694461 scmd IDLE mode=00
[  249.716162] elgato_gchd 1-2:1.0: gchd-timing: 249700517 state 0x11 reached
[  249.757965] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  249.758199] elgato_gchd 1-2:1.0: gchd-timing: 249742555 scmd INIT mode=00
[  249.819995] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  249.820001] elgato_gchd 1-2:1.0: gchd-timing: 249804357 firmware ready
[  249.951053] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[  250.137742] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[  250.280943] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[  250.359009] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  250.361664] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[  250.518242] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  250.521126] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[  250.521131] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[  250.521137] elgato_gchd 1-2:1.0: gchd-timing: 250505498 source detected
[  250.521139] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[  250.521143] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[  251.298588] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  251.298997] elgato_gchd 1-2:1.0: gchd-timing: 251283362 scmd INIT mode=a0
[  251.556420] elgato_gchd 1-2:1.0: back-end not settled: status 0xf2
[  251.583727] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  251.610136] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  251.610371] elgato_gchd 1-2:1.0: gchd-timing: 251594742 scmd INIT mode=a0
[  251.610501] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  251.616644] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  251.617271] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  251.617883] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  251.618034] elgato_gchd 1-2:1.0: gchd-timing: 251602404 scmd START
[  251.716237] elgato_gchd 1-2:1.0: gchd-timing: 251700609 state 0x02 reached
[  251.716245] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  252.088818] elgato_gchd 1-2:1.0: gchd-timing: 252073185 stream first-data ep=0x81
[  252.089033] elgato_gchd 1-2:1.0: gchd-timing: 252073407 stream first-buffer
[  252.304399] elgato_gchd 1-2:1.0: gchd-timing: 252288774 source mode-change
[  252.304405] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[  252.851498] elgato_gchd 1-2:1.0: gchd-timing: 252835877 scmd NULL
[  253.545044] elgato_gchd 1-2:1.0: gchd-timing: 253529422 state NULL reached
[  253.591351] elgato_gchd 1-2:1.0: gchd-timing: 253575730 scmd STOP
[  253.624874] elgato_gchd 1-2:1.0: gchd-timing: 253609252 state STOP reached
[  253.828771] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  253.828902] elgato_gchd 1-2:1.0: gchd-timing: 253813291 scmd INIT mode=a0
[  253.829009] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  253.834960] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  253.835715] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[  253.836303] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  253.836508] elgato_gchd 1-2:1.0: gchd-timing: 253820896 scmd START
[  253.951330] elgato_gchd 1-2:1.0: gchd-timing: 253935719 state 0x02 reached
[  253.951332] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  253.951333] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[  254.325780] elgato_gchd 1-2:1.0: gchd-timing: 254310163 stream first-data ep=0x81
[  254.325987] elgato_gchd 1-2:1.0: gchd-timing: 254310378 stream first-buffer
[  258.875763] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  259.668882] elgato_gchd 1-2:1.0: gchd-timing: 259653316 request input 3
[  259.668886] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=1 streaming=1
[  259.669730] elgato_gchd 1-2:1.0: gchd-timing: 259654163 scmd NULL
[  260.372305] elgato_gchd 1-2:1.0: gchd-timing: 260356737 state NULL reached
[  260.385865] elgato_gchd 1-2:1.0: gchd-timing: 260370299 scmd STOP
[  260.406299] elgato_gchd 1-2:1.0: gchd-timing: 260390732 state STOP reached
[  260.550941] elgato_gchd 1-2:1.0: input selected: input=3; detection scheduled
[  260.579701] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[  260.683239] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[  260.731511] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[  265.561148] elgato_gchd 1-2:1.0: gchd-timing: 265545564 request input 2
[  265.561155] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=1 streaming=1
[  265.563008] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[  265.871271] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[  266.047275] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  266.049426] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[  266.156358] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  266.158903] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[  266.158905] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[  266.158909] elgato_gchd 1-2:1.0: gchd-timing: 266143313 source detected
[  266.158910] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[  266.158913] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[  266.159061] elgato_gchd 1-2:1.0: gchd-timing: 266143465 scmd INIT mode=a0
[  267.166271] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  267.181268] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  267.181384] elgato_gchd 1-2:1.0: gchd-timing: 267165767 scmd INIT mode=a0
[  267.181477] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  267.186685] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  267.187384] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[  267.187810] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  267.187923] elgato_gchd 1-2:1.0: gchd-timing: 267172306 scmd START
[  267.287716] elgato_gchd 1-2:1.0: gchd-timing: 267272096 state 0x02 reached
[  267.287718] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  267.662542] elgato_gchd 1-2:1.0: gchd-timing: 267646906 stream first-data ep=0x81
[  267.662782] elgato_gchd 1-2:1.0: gchd-timing: 267647153 stream first-buffer
[  272.211232] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  273.579536] elgato_gchd 1-2:1.0: gchd-timing: 273563791 request input 3
[  273.579542] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=1 streaming=1
[  273.580662] elgato_gchd 1-2:1.0: gchd-timing: 273564915 scmd NULL
[  274.245438] elgato_gchd 1-2:1.0: gchd-timing: 274229673 state NULL reached
[  274.265252] elgato_gchd 1-2:1.0: gchd-timing: 274249489 scmd STOP
[  274.288148] elgato_gchd 1-2:1.0: gchd-timing: 274272383 state STOP reached
[  274.418493] elgato_gchd 1-2:1.0: input selected: input=3; detection scheduled
[  274.531316] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[  274.591589] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[  279.432511] elgato_gchd 1-2:1.0: gchd-timing: 279416661 request input 2
[  279.432516] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=1 streaming=1
[  279.437076] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[  279.729258] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[  279.886367] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  279.888576] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[  279.987246] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  279.989837] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[  280.078582] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  280.081559] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[  280.165036] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  280.167812] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[  280.167815] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[  280.167818] elgato_gchd 1-2:1.0: gchd-timing: 280151957 source detected
[  280.167820] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[  280.167822] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[  280.167974] elgato_gchd 1-2:1.0: gchd-timing: 280152113 scmd INIT mode=a0
[  281.254406] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  281.271639] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  281.271845] elgato_gchd 1-2:1.0: gchd-timing: 281255966 scmd INIT mode=a0
[  281.271963] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  281.278039] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  281.278924] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[  281.279633] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  281.279954] elgato_gchd 1-2:1.0: gchd-timing: 281264074 scmd START
[  281.377738] elgato_gchd 1-2:1.0: gchd-timing: 281361857 state 0x02 reached
[  281.377741] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  281.752527] elgato_gchd 1-2:1.0: gchd-timing: 281736632 stream first-data ep=0x81
[  281.752781] elgato_gchd 1-2:1.0: gchd-timing: 281736893 stream first-buffer
[  283.406481] elgato_gchd 1-2:1.0: gchd-timing: 283390565 source mode-change
[  283.406489] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[  283.948466] elgato_gchd 1-2:1.0: gchd-timing: 283932543 scmd NULL
[  284.606861] elgato_gchd 1-2:1.0: gchd-timing: 284590922 state NULL reached
[  284.619241] elgato_gchd 1-2:1.0: gchd-timing: 284603303 scmd STOP
[  284.705898] elgato_gchd 1-2:1.0: gchd-timing: 284689958 state STOP reached
[  284.810842] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  284.810970] elgato_gchd 1-2:1.0: gchd-timing: 284795035 scmd INIT mode=a0
[  284.811159] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  284.816661] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  284.817180] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  284.817655] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  284.817776] elgato_gchd 1-2:1.0: gchd-timing: 284801841 scmd START
[  284.916695] elgato_gchd 1-2:1.0: gchd-timing: 284900759 state 0x02 reached
[  284.916697] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  284.916699] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[  285.291087] elgato_gchd 1-2:1.0: gchd-timing: 285275139 stream first-data ep=0x81
[  285.291294] elgato_gchd 1-2:1.0: gchd-timing: 285275352 stream first-buffer
[  289.840141] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  294.863746] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  296.181867] elgato_gchd 1-2:1.0: gchd-timing: 296165769 request capture-stop
[  296.194368] elgato_gchd 1-2:1.0: gchd-timing: 296178273 scmd NULL
[  296.950586] elgato_gchd 1-2:1.0: gchd-timing: 296934482 state NULL reached
[  296.990134] elgato_gchd 1-2:1.0: gchd-timing: 296974030 scmd STOP
[  297.020975] elgato_gchd 1-2:1.0: gchd-timing: 297004870 state STOP reached
[  297.071736] elgato_gchd 1-2:1.0: gchd-timing: 297055635 scmd INIT mode=a0
[  297.073071] elgato_gchd 1-2:1.0: gchd-timing: 297056972 scmd IDLE mode=00
[  297.086007] elgato_gchd 1-2:1.0: gchd-timing: 297069907 state 0x11 reached
[  297.086197] elgato_gchd 1-2:1.0: gchd-timing: 297070098 scmd OTHER mode=01
[  297.092506] elgato_gchd 1-2:1.0: gchd-timing: 297076406 state 0x12 reached
[  297.092599] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 6: source change x3 (console reset) while capturing
PROMPT: Press RESET on the console now (change 1 of 3); wait for the picture, then Enter.
PROMPT: Press RESET on the console now (change 2 of 3); wait for the picture, then Enter.
PROMPT: Press RESET on the console now (change 3 of 3); wait for the picture, then Enter.
--- dmesg step 6
[  301.219058] elgato_gchd 1-2:1.0: gchd-timing: 301202912 request capture-start
[  301.219060] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  301.222683] elgato_gchd 1-2:1.0: gchd-timing: 301206536 scmd OTHER mode=00
[  301.234558] elgato_gchd 1-2:1.0: gchd-timing: 301218411 state 0x10 reached
[  301.234776] elgato_gchd 1-2:1.0: gchd-timing: 301218629 scmd IDLE mode=00
[  301.240623] elgato_gchd 1-2:1.0: gchd-timing: 301224477 state 0x11 reached
[  301.291897] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  301.292010] elgato_gchd 1-2:1.0: gchd-timing: 301275863 scmd INIT mode=00
[  301.352656] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  301.352659] elgato_gchd 1-2:1.0: gchd-timing: 301336511 firmware ready
[  301.479281] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[  301.650884] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[  301.684860] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[  301.688847] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[  301.777978] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[  301.805596] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  301.807888] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[  301.807889] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[  301.807892] elgato_gchd 1-2:1.0: gchd-timing: 301791739 source detected
[  301.807893] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[  301.807894] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[  302.585818] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  302.585931] elgato_gchd 1-2:1.0: gchd-timing: 302569770 scmd INIT mode=a0
[  302.706985] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  302.722013] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  302.722227] elgato_gchd 1-2:1.0: gchd-timing: 302706065 scmd INIT mode=a0
[  302.722332] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  302.744607] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  302.745566] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  302.746002] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  302.746131] elgato_gchd 1-2:1.0: gchd-timing: 302729969 scmd START
[  302.928146] elgato_gchd 1-2:1.0: gchd-timing: 302911982 state 0x02 reached
[  302.928148] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  303.301543] elgato_gchd 1-2:1.0: gchd-timing: 303285366 stream first-data ep=0x81
[  303.301737] elgato_gchd 1-2:1.0: gchd-timing: 303285568 stream first-buffer
[  307.763382] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  312.785363] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  317.360715] elgato_gchd 1-2:1.0: gchd-timing: 317344416 source mode-change
[  317.360726] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[  317.808792] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  317.882772] elgato_gchd 1-2:1.0: gchd-timing: 317866472 scmd NULL
[  318.698555] elgato_gchd 1-2:1.0: gchd-timing: 318682244 state NULL reached
[  318.723038] elgato_gchd 1-2:1.0: gchd-timing: 318706718 scmd STOP
[  318.753339] elgato_gchd 1-2:1.0: gchd-timing: 318737019 state STOP reached
[  319.013069] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  319.013192] elgato_gchd 1-2:1.0: gchd-timing: 318996886 scmd INIT mode=a0
[  319.013289] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  319.019006] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  319.019593] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[  319.020391] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  319.020519] elgato_gchd 1-2:1.0: gchd-timing: 319004212 scmd START
[  319.135609] elgato_gchd 1-2:1.0: gchd-timing: 319119301 state 0x02 reached
[  319.135611] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  319.135613] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[  319.510141] elgato_gchd 1-2:1.0: gchd-timing: 319493822 stream first-data ep=0x81
[  319.510319] elgato_gchd 1-2:1.0: gchd-timing: 319494008 stream first-buffer
[  324.058711] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  329.081956] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  334.104833] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  339.128259] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  344.150608] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  348.238967] elgato_gchd 1-2:1.0: gchd-timing: 348222506 source mode-change
[  348.238973] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[  348.733258] elgato_gchd 1-2:1.0: gchd-timing: 348716797 scmd NULL
[  349.189109] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=678 bytes=44433408 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  349.384177] elgato_gchd 1-2:1.0: gchd-timing: 349367708 state NULL reached
[  349.399676] elgato_gchd 1-2:1.0: gchd-timing: 349383207 scmd STOP
[  349.490181] elgato_gchd 1-2:1.0: gchd-timing: 349473711 state STOP reached
[  349.606398] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  349.606524] elgato_gchd 1-2:1.0: gchd-timing: 349590061 scmd INIT mode=a0
[  349.606633] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  349.612243] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  349.612862] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  349.613339] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  349.613464] elgato_gchd 1-2:1.0: gchd-timing: 349597001 scmd START
[  349.728916] elgato_gchd 1-2:1.0: gchd-timing: 349712453 state 0x02 reached
[  349.728917] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  349.728919] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[  350.101945] elgato_gchd 1-2:1.0: gchd-timing: 350085471 stream first-data ep=0x81
[  350.102154] elgato_gchd 1-2:1.0: gchd-timing: 350085688 stream first-buffer
[  354.652909] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  359.675238] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  364.696319] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  365.762331] elgato_gchd 1-2:1.0: gchd-timing: 365745825 request capture-stop
[  365.763873] elgato_gchd 1-2:1.0: gchd-timing: 365747369 scmd NULL
[  366.440938] elgato_gchd 1-2:1.0: gchd-timing: 366424428 state NULL reached
[  366.457085] elgato_gchd 1-2:1.0: gchd-timing: 366440577 scmd STOP
[  366.476705] elgato_gchd 1-2:1.0: gchd-timing: 366460196 state STOP reached
[  366.503996] elgato_gchd 1-2:1.0: gchd-timing: 366487492 scmd INIT mode=a0
[  366.505062] elgato_gchd 1-2:1.0: gchd-timing: 366488560 scmd IDLE mode=00
[  366.518352] elgato_gchd 1-2:1.0: gchd-timing: 366501848 state 0x11 reached
[  366.518573] elgato_gchd 1-2:1.0: gchd-timing: 366502070 scmd OTHER mode=01
[  366.524873] elgato_gchd 1-2:1.0: gchd-timing: 366508370 state 0x12 reached
[  366.524959] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 7: tables
--- dmesg step 7

### Driver: ms since the request

| Operation | Phase | n | avg ms | min ms | max ms |
|---|---|---|---|---|---|
| input (5) | scmd NULL | 2 | 1 | 1 | 1 |
| input (5) | source detected | 2 | 667 | 598 | 735 |
| input (5) | scmd INIT | 2 | 667 | 598 | 735 |
| input (5) | state NULL reached | 2 | 685 | 666 | 703 |
| input (5) | scmd STOP | 2 | 701 | 686 | 717 |
| input (5) | state STOP reached | 2 | 723 | 709 | 737 |
| input (5) | scmd START | 2 | 1737 | 1627 | 1847 |
| input (5) | state START reached | 2 | 1836 | 1727 | 1945 |
| input (5) | stream first-data | 2 | 2211 | 2101 | 2320 |
| input (5) | stream first-buffer | 2 | 2211 | 2102 | 2320 |
| capture-start (9) | scmd OTHER | 8 | 3 | 2 | 4 |
| capture-start (9) | state 0x10 reached | 8 | 15 | 14 | 16 |
| capture-start (9) | scmd IDLE | 9 | 22 | 15 | 77 |
| capture-start (9) | state IDLE reached | 9 | 28 | 20 | 87 |
| capture-start (9) | scmd INIT | 9 | 96 | 63 | 187 |
| capture-start (9) | firmware ready | 9 | 161 | 125 | 254 |
| capture-start (9) | source detected | 9 | 779 | 589 | 896 |
| capture-start (9) | scmd START | 9 | 2042 | 1527 | 2508 |
| capture-start (9) | state START reached | 9 | 2162 | 1709 | 2636 |
| capture-start (9) | stream first-data | 8 | 2499 | 2082 | 3011 |
| capture-start (9) | stream first-buffer | 8 | 2499 | 2083 | 3011 |
| capture-stop (9) | scmd NULL | 9 | 2 | 1 | 13 |
| capture-stop (9) | state NULL reached | 9 | 727 | 634 | 974 |
| capture-stop (9) | scmd STOP | 9 | 749 | 647 | 1019 |
| capture-stop (9) | state STOP reached | 9 | 776 | 666 | 1050 |
| capture-stop (9) | scmd INIT | 9 | 835 | 728 | 1174 |
| capture-stop (9) | scmd IDLE | 9 | 837 | 729 | 1176 |
| capture-stop (9) | state IDLE reached | 9 | 850 | 743 | 1189 |
| capture-stop (9) | scmd OTHER | 9 | 850 | 744 | 1189 |
| capture-stop (9) | state 0x12 reached | 9 | 856 | 750 | 1195 |
| source mode-change (8) | scmd NULL | 8 | 511 | 483 | 547 |
| source mode-change (8) | state NULL reached | 8 | 1238 | 1145 | 1467 |
| source mode-change (8) | scmd STOP | 8 | 1258 | 1161 | 1486 |
| source mode-change (8) | state STOP reached | 8 | 1300 | 1185 | 1503 |
| source mode-change (8) | scmd INIT | 8 | 1495 | 1368 | 1652 |
| source mode-change (8) | scmd START | 8 | 1502 | 1374 | 1660 |
| source mode-change (8) | state START reached | 8 | 1609 | 1490 | 1775 |
| source mode-change (8) | stream first-data | 8 | 1735 | 95 | 2149 |
| source mode-change (8) | stream first-buffer | 8 | 1735 | 96 | 2150 |
| control (4) | restart encoder | 4 | 0 | 0 | 1 |
| control (4) | scmd NULL | 4 | 1 | 1 | 2 |
| control (4) | state NULL reached | 4 | 1025 | 669 | 1380 |
| control (4) | scmd STOP | 4 | 1053 | 689 | 1430 |
| control (4) | state STOP reached | 4 | 1080 | 710 | 1443 |
| control (4) | scmd INIT | 4 | 1132 | 760 | 1481 |
| control (4) | scmd START | 4 | 1140 | 767 | 1491 |
| control (4) | state START reached | 4 | 1247 | 866 | 1603 |
| control (4) | stream first-data | 4 | 1737 | 1483 | 1975 |
| control (4) | stream first-buffer | 4 | 1737 | 1483 | 1975 |
| output (4) | restart encoder | 4 | 2 | 0 | 9 |
| output (4) | scmd NULL | 4 | 4 | 1 | 9 |
| output (4) | state NULL reached | 4 | 762 | 637 | 1065 |
| output (4) | scmd STOP | 4 | 781 | 652 | 1096 |
| output (4) | state STOP reached | 4 | 816 | 672 | 1119 |
| output (4) | scmd INIT | 4 | 967 | 753 | 1185 |
| output (4) | scmd START | 4 | 974 | 760 | 1192 |
| output (4) | state START reached | 4 | 1082 | 859 | 1291 |
| output (4) | stream first-data | 4 | 1454 | 1232 | 1663 |
| output (4) | stream first-buffer | 4 | 1454 | 1232 | 1664 |

### Driver and recordings: ms since the NULL command

| Operation | Phase | n | avg ms | min ms | max ms |
|---|---|---|---|---|---|
| capture-stop (driver) (9) | scmd NULL | 9 | 0 | 0 | 0 |
| capture-stop (driver) (9) | state NULL reached | 9 | 724 | 633 | 972 |
| capture-stop (driver) (9) | scmd STOP | 9 | 747 | 646 | 1017 |
| capture-stop (driver) (9) | state STOP reached | 9 | 774 | 665 | 1048 |
| capture-stop (driver) (9) | scmd INIT | 9 | 833 | 728 | 1173 |
| capture-stop (driver) (9) | scmd IDLE | 9 | 834 | 729 | 1174 |
| capture-stop (driver) (9) | state IDLE reached | 9 | 847 | 743 | 1187 |
| capture-stop (driver) (9) | scmd OTHER | 9 | 848 | 743 | 1188 |
| capture-stop (driver) (9) | state 0x12 reached | 9 | 854 | 749 | 1194 |
| source mode-change (driver) (8) | scmd NULL | 8 | 0 | 0 | 0 |
| source mode-change (driver) (8) | state NULL reached | 8 | 726 | 651 | 966 |
| source mode-change (driver) (8) | scmd STOP | 8 | 747 | 666 | 985 |
| source mode-change (driver) (8) | state STOP reached | 8 | 789 | 702 | 1002 |
| source mode-change (driver) (8) | scmd INIT | 8 | 983 | 862 | 1130 |
| source mode-change (driver) (8) | scmd START | 8 | 991 | 869 | 1138 |
| source mode-change (driver) (8) | state START reached | 8 | 1098 | 968 | 1253 |
| source mode-change (driver) (8) | stream first-data | 8 | 1471 | 1343 | 1627 |
| source mode-change (driver) (8) | stream first-buffer | 8 | 1471 | 1343 | 1628 |
| control (driver) (4) | scmd NULL | 4 | 0 | 0 | 0 |
| control (driver) (4) | state NULL reached | 4 | 1024 | 667 | 1378 |
| control (driver) (4) | scmd STOP | 4 | 1051 | 688 | 1429 |
| control (driver) (4) | state STOP reached | 4 | 1079 | 708 | 1442 |
| control (driver) (4) | scmd INIT | 4 | 1131 | 758 | 1480 |
| control (driver) (4) | scmd START | 4 | 1139 | 766 | 1490 |
| control (driver) (4) | state START reached | 4 | 1245 | 864 | 1601 |
| control (driver) (4) | stream first-data | 4 | 1736 | 1481 | 1973 |
| control (driver) (4) | stream first-buffer | 4 | 1736 | 1482 | 1973 |
| output (driver) (4) | scmd NULL | 4 | 0 | 0 | 0 |
| output (driver) (4) | state NULL reached | 4 | 758 | 634 | 1056 |
| output (driver) (4) | scmd STOP | 4 | 777 | 649 | 1087 |
| output (driver) (4) | state STOP reached | 4 | 812 | 669 | 1109 |
| output (driver) (4) | scmd INIT | 4 | 964 | 751 | 1176 |
| output (driver) (4) | scmd START | 4 | 971 | 759 | 1183 |
| output (driver) (4) | state START reached | 4 | 1078 | 857 | 1282 |
| output (driver) (4) | stream first-data | 4 | 1450 | 1230 | 1654 |
| output (driver) (4) | stream first-buffer | 4 | 1450 | 1231 | 1655 |
| input (driver) (2) | scmd NULL | 2 | 0 | 0 | 0 |
| input (driver) (2) | state NULL reached | 2 | 684 | 665 | 703 |
| input (driver) (2) | scmd STOP | 2 | 700 | 685 | 716 |
| input (driver) (2) | state STOP reached | 2 | 722 | 707 | 737 |
| change (recordings) (26) | scmd NULL | 26 | 0 | 0 | 0 |
| change (recordings) (26) | scmd STOP | 26 | 1150 | 646 | 1590 |
| change (recordings) (26) | scmd INIT | 26 | 2294 | 1672 | 2766 |
| change (recordings) (26) | scmd START | 26 | 2295 | 1673 | 2766 |

$ cat /home/ff5/gchd-test-logs/T-bench-20261119-082229/timing.md
### Driver: ms since the request

| Operation | Phase | n | avg ms | min ms | max ms |
|---|---|---|---|---|---|
| input (5) | scmd NULL | 2 | 1 | 1 | 1 |
| input (5) | source detected | 2 | 667 | 598 | 735 |
| input (5) | scmd INIT | 2 | 667 | 598 | 735 |
| input (5) | state NULL reached | 2 | 685 | 666 | 703 |
| input (5) | scmd STOP | 2 | 701 | 686 | 717 |
| input (5) | state STOP reached | 2 | 723 | 709 | 737 |
| input (5) | scmd START | 2 | 1737 | 1627 | 1847 |
| input (5) | state START reached | 2 | 1836 | 1727 | 1945 |
| input (5) | stream first-data | 2 | 2211 | 2101 | 2320 |
| input (5) | stream first-buffer | 2 | 2211 | 2102 | 2320 |
| capture-start (9) | scmd OTHER | 8 | 3 | 2 | 4 |
| capture-start (9) | state 0x10 reached | 8 | 15 | 14 | 16 |
| capture-start (9) | scmd IDLE | 9 | 22 | 15 | 77 |
| capture-start (9) | state IDLE reached | 9 | 28 | 20 | 87 |
| capture-start (9) | scmd INIT | 9 | 96 | 63 | 187 |
| capture-start (9) | firmware ready | 9 | 161 | 125 | 254 |
| capture-start (9) | source detected | 9 | 779 | 589 | 896 |
| capture-start (9) | scmd START | 9 | 2042 | 1527 | 2508 |
| capture-start (9) | state START reached | 9 | 2162 | 1709 | 2636 |
| capture-start (9) | stream first-data | 8 | 2499 | 2082 | 3011 |
| capture-start (9) | stream first-buffer | 8 | 2499 | 2083 | 3011 |
| capture-stop (9) | scmd NULL | 9 | 2 | 1 | 13 |
| capture-stop (9) | state NULL reached | 9 | 727 | 634 | 974 |
| capture-stop (9) | scmd STOP | 9 | 749 | 647 | 1019 |
| capture-stop (9) | state STOP reached | 9 | 776 | 666 | 1050 |
| capture-stop (9) | scmd INIT | 9 | 835 | 728 | 1174 |
| capture-stop (9) | scmd IDLE | 9 | 837 | 729 | 1176 |
| capture-stop (9) | state IDLE reached | 9 | 850 | 743 | 1189 |
| capture-stop (9) | scmd OTHER | 9 | 850 | 744 | 1189 |
| capture-stop (9) | state 0x12 reached | 9 | 856 | 750 | 1195 |
| source mode-change (8) | scmd NULL | 8 | 511 | 483 | 547 |
| source mode-change (8) | state NULL reached | 8 | 1238 | 1145 | 1467 |
| source mode-change (8) | scmd STOP | 8 | 1258 | 1161 | 1486 |
| source mode-change (8) | state STOP reached | 8 | 1300 | 1185 | 1503 |
| source mode-change (8) | scmd INIT | 8 | 1495 | 1368 | 1652 |
| source mode-change (8) | scmd START | 8 | 1502 | 1374 | 1660 |
| source mode-change (8) | state START reached | 8 | 1609 | 1490 | 1775 |
| source mode-change (8) | stream first-data | 8 | 1735 | 95 | 2149 |
| source mode-change (8) | stream first-buffer | 8 | 1735 | 96 | 2150 |
| control (4) | restart encoder | 4 | 0 | 0 | 1 |
| control (4) | scmd NULL | 4 | 1 | 1 | 2 |
| control (4) | state NULL reached | 4 | 1025 | 669 | 1380 |
| control (4) | scmd STOP | 4 | 1053 | 689 | 1430 |
| control (4) | state STOP reached | 4 | 1080 | 710 | 1443 |
| control (4) | scmd INIT | 4 | 1132 | 760 | 1481 |
| control (4) | scmd START | 4 | 1140 | 767 | 1491 |
| control (4) | state START reached | 4 | 1247 | 866 | 1603 |
| control (4) | stream first-data | 4 | 1737 | 1483 | 1975 |
| control (4) | stream first-buffer | 4 | 1737 | 1483 | 1975 |
| output (4) | restart encoder | 4 | 2 | 0 | 9 |
| output (4) | scmd NULL | 4 | 4 | 1 | 9 |
| output (4) | state NULL reached | 4 | 762 | 637 | 1065 |
| output (4) | scmd STOP | 4 | 781 | 652 | 1096 |
| output (4) | state STOP reached | 4 | 816 | 672 | 1119 |
| output (4) | scmd INIT | 4 | 967 | 753 | 1185 |
| output (4) | scmd START | 4 | 974 | 760 | 1192 |
| output (4) | state START reached | 4 | 1082 | 859 | 1291 |
| output (4) | stream first-data | 4 | 1454 | 1232 | 1663 |
| output (4) | stream first-buffer | 4 | 1454 | 1232 | 1664 |

### Driver and recordings: ms since the NULL command

| Operation | Phase | n | avg ms | min ms | max ms |
|---|---|---|---|---|---|
| capture-stop (driver) (9) | scmd NULL | 9 | 0 | 0 | 0 |
| capture-stop (driver) (9) | state NULL reached | 9 | 724 | 633 | 972 |
| capture-stop (driver) (9) | scmd STOP | 9 | 747 | 646 | 1017 |
| capture-stop (driver) (9) | state STOP reached | 9 | 774 | 665 | 1048 |
| capture-stop (driver) (9) | scmd INIT | 9 | 833 | 728 | 1173 |
| capture-stop (driver) (9) | scmd IDLE | 9 | 834 | 729 | 1174 |
| capture-stop (driver) (9) | state IDLE reached | 9 | 847 | 743 | 1187 |
| capture-stop (driver) (9) | scmd OTHER | 9 | 848 | 743 | 1188 |
| capture-stop (driver) (9) | state 0x12 reached | 9 | 854 | 749 | 1194 |
| source mode-change (driver) (8) | scmd NULL | 8 | 0 | 0 | 0 |
| source mode-change (driver) (8) | state NULL reached | 8 | 726 | 651 | 966 |
| source mode-change (driver) (8) | scmd STOP | 8 | 747 | 666 | 985 |
| source mode-change (driver) (8) | state STOP reached | 8 | 789 | 702 | 1002 |
| source mode-change (driver) (8) | scmd INIT | 8 | 983 | 862 | 1130 |
| source mode-change (driver) (8) | scmd START | 8 | 991 | 869 | 1138 |
| source mode-change (driver) (8) | state START reached | 8 | 1098 | 968 | 1253 |
| source mode-change (driver) (8) | stream first-data | 8 | 1471 | 1343 | 1627 |
| source mode-change (driver) (8) | stream first-buffer | 8 | 1471 | 1343 | 1628 |
| control (driver) (4) | scmd NULL | 4 | 0 | 0 | 0 |
| control (driver) (4) | state NULL reached | 4 | 1024 | 667 | 1378 |
| control (driver) (4) | scmd STOP | 4 | 1051 | 688 | 1429 |
| control (driver) (4) | state STOP reached | 4 | 1079 | 708 | 1442 |
| control (driver) (4) | scmd INIT | 4 | 1131 | 758 | 1480 |
| control (driver) (4) | scmd START | 4 | 1139 | 766 | 1490 |
| control (driver) (4) | state START reached | 4 | 1245 | 864 | 1601 |
| control (driver) (4) | stream first-data | 4 | 1736 | 1481 | 1973 |
| control (driver) (4) | stream first-buffer | 4 | 1736 | 1482 | 1973 |
| output (driver) (4) | scmd NULL | 4 | 0 | 0 | 0 |
| output (driver) (4) | state NULL reached | 4 | 758 | 634 | 1056 |
| output (driver) (4) | scmd STOP | 4 | 777 | 649 | 1087 |
| output (driver) (4) | state STOP reached | 4 | 812 | 669 | 1109 |
| output (driver) (4) | scmd INIT | 4 | 964 | 751 | 1176 |
| output (driver) (4) | scmd START | 4 | 971 | 759 | 1183 |
| output (driver) (4) | state START reached | 4 | 1078 | 857 | 1282 |
| output (driver) (4) | stream first-data | 4 | 1450 | 1230 | 1654 |
| output (driver) (4) | stream first-buffer | 4 | 1450 | 1231 | 1655 |
| input (driver) (2) | scmd NULL | 2 | 0 | 0 | 0 |
| input (driver) (2) | state NULL reached | 2 | 684 | 665 | 703 |
| input (driver) (2) | scmd STOP | 2 | 700 | 685 | 716 |
| input (driver) (2) | state STOP reached | 2 | 722 | 707 | 737 |
| change (recordings) (26) | scmd NULL | 26 | 0 | 0 | 0 |
| change (recordings) (26) | scmd STOP | 26 | 1150 | 646 | 1590 |
| change (recordings) (26) | scmd INIT | 26 | 2294 | 1672 | 2766 |
| change (recordings) (26) | scmd START | 26 | 2295 | 1673 | 2766 |
[PASS] timing marks recorded
--- dmesg step 7


=== summary (T-bench, 6143ad5 T-060: one global setting for the OBS plugin; driver timing marks for a latency benchmark)
PASS | step 7 tables | timing marks recorded

Second log:
gchd-test T-060-settings 20261119-083138 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-060-settings-20261119-083138.log
== build
$ git fetch -q origin
$ git checkout -q feat/app-bridge
$ git pull -q --ff-only
commit: 6143ad5 T-060: one global setting for the OBS plugin; driver timing marks for a latency benchmark
commit: 6143ad5 T-060: one global setting for the OBS plugin; driver timing marks for a latency benchmark
build rc=0 warnings=0

=== step 1: install the driver part and the plugin
$ sudo rmmod elgato_gchd
$ bash /home/ff5/elgato-gchd/install-driver.sh --all --yes --in-terminal
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

[1m==> OBS plugin[0m
[32mOBS plugin installed in /home/ff5/.config/obs-studio/plugins/obs-elgato-gchd. Restart OBS and add the source 'Elgato Game Capture HD'.[0m

PROMPT: Unplug the Elgato and plug it in again; wait 5 s.
$ v4l2-compliance -d /dev/elgato-gchd
v4l2-compliance 1.26.1, 64 bits, 64-bit time_t

Compliance test for elgato-gchd device /dev/elgato-gchd:

Driver Info:
	Driver name      : elgato-gchd
	Card type        : Elgato Game Capture HD
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

Required ioctls:
	test VIDIOC_QUERYCAP: OK
	test invalid ioctls: OK

Allow for multiple opens:
	test second /dev/elgato-gchd open: OK
	test VIDIOC_QUERYCAP: OK
	test VIDIOC_G/S_PRIORITY: OK
	test for unlimited opens: OK

Debug ioctls:
	test VIDIOC_DBG_G/S_REGISTER: OK (Not Supported)
	test VIDIOC_LOG_STATUS: OK (Not Supported)

Input ioctls:
	test VIDIOC_G/S_TUNER/ENUM_FREQ_BANDS: OK (Not Supported)
	test VIDIOC_G/S_FREQUENCY: OK (Not Supported)
	test VIDIOC_S_HW_FREQ_SEEK: OK (Not Supported)
	test VIDIOC_ENUMAUDIO: OK (Not Supported)
	test VIDIOC_G/S/ENUMINPUT: OK
	test VIDIOC_G/S_AUDIO: OK (Not Supported)
	Inputs: 4 Audio Inputs: 0 Tuners: 0

Output ioctls:
	test VIDIOC_G/S_MODULATOR: OK (Not Supported)
	test VIDIOC_G/S_FREQUENCY: OK (Not Supported)
	test VIDIOC_ENUMAUDOUT: OK (Not Supported)
	test VIDIOC_G/S/ENUMOUTPUT: OK (Not Supported)
	test VIDIOC_G/S_AUDOUT: OK (Not Supported)
	Outputs: 0 Audio Outputs: 0 Modulators: 0

Input/Output configuration ioctls:
	test VIDIOC_ENUM/G/S/QUERY_STD: OK
	test VIDIOC_ENUM/G/S/QUERY_DV_TIMINGS: OK
	test VIDIOC_DV_TIMINGS_CAP: OK
	test VIDIOC_G/S_EDID: OK (Not Supported)

Control ioctls (Input 0):
	test VIDIOC_QUERY_EXT_CTRL/QUERYMENU: OK
	test VIDIOC_QUERYCTRL: OK
	test VIDIOC_G/S_CTRL: OK
	test VIDIOC_G/S/TRY_EXT_CTRLS: OK
	test VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: OK
	test VIDIOC_G/S_JPEGCOMP: OK (Not Supported)
	Standard Controls: 8 Private Controls: 1

Format ioctls (Input 0):
	test VIDIOC_ENUM_FMT/FRAMESIZES/FRAMEINTERVALS: OK
	test VIDIOC_G/S_PARM: OK
	test VIDIOC_G_FBUF: OK (Not Supported)
	test VIDIOC_G_FMT: OK
	test VIDIOC_TRY_FMT: OK
	test VIDIOC_S_FMT: OK
	test VIDIOC_G_SLICED_VBI_CAP: OK (Not Supported)
	test Cropping: OK (Not Supported)
	test Composing: OK (Not Supported)
	test Scaling: OK (Not Supported)

Codec ioctls (Input 0):
	test VIDIOC_(TRY_)ENCODER_CMD: OK (Not Supported)
	test VIDIOC_G_ENC_INDEX: OK (Not Supported)
	test VIDIOC_(TRY_)DECODER_CMD: OK (Not Supported)

Buffer ioctls (Input 0):
		warn: v4l2-test-buffers.cpp(693): VIDIOC_CREATE_BUFS not supported
	test VIDIOC_REQBUFS/CREATE_BUFS/QUERYBUF: OK
	test CREATE_BUFS maximum buffers: OK
	test VIDIOC_EXPBUF: OK (Not Supported)
	test Requests: OK (Not Supported)

Control ioctls (Input 1):
	test VIDIOC_QUERY_EXT_CTRL/QUERYMENU: OK
	test VIDIOC_QUERYCTRL: OK
	test VIDIOC_G/S_CTRL: OK
	test VIDIOC_G/S/TRY_EXT_CTRLS: OK
	test VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: OK
	test VIDIOC_G/S_JPEGCOMP: OK (Not Supported)
	Standard Controls: 8 Private Controls: 1

Format ioctls (Input 1):
	test VIDIOC_ENUM_FMT/FRAMESIZES/FRAMEINTERVALS: OK
	test VIDIOC_G/S_PARM: OK
	test VIDIOC_G_FBUF: OK (Not Supported)
	test VIDIOC_G_FMT: OK
	test VIDIOC_TRY_FMT: OK
	test VIDIOC_S_FMT: OK
	test VIDIOC_G_SLICED_VBI_CAP: OK (Not Supported)
	test Cropping: OK (Not Supported)
	test Composing: OK (Not Supported)
	test Scaling: OK (Not Supported)

Codec ioctls (Input 1):
	test VIDIOC_(TRY_)ENCODER_CMD: OK (Not Supported)
	test VIDIOC_G_ENC_INDEX: OK (Not Supported)
	test VIDIOC_(TRY_)DECODER_CMD: OK (Not Supported)

Buffer ioctls (Input 1):
		warn: v4l2-test-buffers.cpp(693): VIDIOC_CREATE_BUFS not supported
	test VIDIOC_REQBUFS/CREATE_BUFS/QUERYBUF: OK
	test CREATE_BUFS maximum buffers: OK
	test VIDIOC_EXPBUF: OK (Not Supported)
	test Requests: OK (Not Supported)

Control ioctls (Input 2):
	test VIDIOC_QUERY_EXT_CTRL/QUERYMENU: OK
	test VIDIOC_QUERYCTRL: OK
	test VIDIOC_G/S_CTRL: OK
	test VIDIOC_G/S/TRY_EXT_CTRLS: OK
	test VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: OK
	test VIDIOC_G/S_JPEGCOMP: OK (Not Supported)
	Standard Controls: 8 Private Controls: 1

Format ioctls (Input 2):
	test VIDIOC_ENUM_FMT/FRAMESIZES/FRAMEINTERVALS: OK
	test VIDIOC_G/S_PARM: OK
	test VIDIOC_G_FBUF: OK (Not Supported)
	test VIDIOC_G_FMT: OK
	test VIDIOC_TRY_FMT: OK
	test VIDIOC_S_FMT: OK
	test VIDIOC_G_SLICED_VBI_CAP: OK (Not Supported)
	test Cropping: OK (Not Supported)
	test Composing: OK (Not Supported)
	test Scaling: OK (Not Supported)

Codec ioctls (Input 2):
	test VIDIOC_(TRY_)ENCODER_CMD: OK (Not Supported)
	test VIDIOC_G_ENC_INDEX: OK (Not Supported)
	test VIDIOC_(TRY_)DECODER_CMD: OK (Not Supported)

Buffer ioctls (Input 2):
		warn: v4l2-test-buffers.cpp(693): VIDIOC_CREATE_BUFS not supported
	test VIDIOC_REQBUFS/CREATE_BUFS/QUERYBUF: OK
	test CREATE_BUFS maximum buffers: OK
	test VIDIOC_EXPBUF: OK (Not Supported)
	test Requests: OK (Not Supported)

Control ioctls (Input 3):
	test VIDIOC_QUERY_EXT_CTRL/QUERYMENU: OK
	test VIDIOC_QUERYCTRL: OK
	test VIDIOC_G/S_CTRL: OK
	test VIDIOC_G/S/TRY_EXT_CTRLS: OK
	test VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: OK
	test VIDIOC_G/S_JPEGCOMP: OK (Not Supported)
	Standard Controls: 8 Private Controls: 1

Format ioctls (Input 3):
	test VIDIOC_ENUM_FMT/FRAMESIZES/FRAMEINTERVALS: OK
	test VIDIOC_G/S_PARM: OK
	test VIDIOC_G_FBUF: OK (Not Supported)
	test VIDIOC_G_FMT: OK
	test VIDIOC_TRY_FMT: OK
	test VIDIOC_S_FMT: OK
	test VIDIOC_G_SLICED_VBI_CAP: OK (Not Supported)
	test Cropping: OK (Not Supported)
	test Composing: OK (Not Supported)
	test Scaling: OK (Not Supported)

Codec ioctls (Input 3):
	test VIDIOC_(TRY_)ENCODER_CMD: OK (Not Supported)
	test VIDIOC_G_ENC_INDEX: OK (Not Supported)
	test VIDIOC_(TRY_)DECODER_CMD: OK (Not Supported)

Buffer ioctls (Input 3):
		warn: v4l2-test-buffers.cpp(693): VIDIOC_CREATE_BUFS not supported
	test VIDIOC_REQBUFS/CREATE_BUFS/QUERYBUF: OK
	test CREATE_BUFS maximum buffers: OK
	test VIDIOC_EXPBUF: OK (Not Supported)
	test Requests: OK (Not Supported)

Total for elgato-gchd device /dev/elgato-gchd: 115, Succeeded: 115, Failed: 0, Warnings: 4
[PASS] v4l2-compliance: no failures
$ v4l2-ctl -d /dev/elgato-gchd -i 2 -v width=720,height=480,pixelformat=MPEG -c video_bitrate=11250000,video_bitrate_mode=1,h264_profile=4,stretch_sd_input=0
Video input set to 2 (Composite: Camera, no signal)
--- dmesg step 1
[  594.080441] usbcore: deregistering interface driver elgato_gchd
[  607.552025] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[  607.556937] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[  607.556956] usbcore: registered new interface driver elgato_gchd
[  613.958820] usb 1-2: USB disconnect, device number 4
[  619.081276] usb 1-2: new high-speed USB device number 5 using xhci_hcd
[  619.475808] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[  619.475827] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[  619.475833] usb 1-2: Product: GameCapture HD
[  619.475838] usb 1-2: Manufacturer: Elgato
[  619.475842] usb 1-2: SerialNumber: 04107F1267
[  619.482207] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[  619.486163] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[  621.012760] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=0 streaming=0
[  621.012807] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[  621.012810] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[  621.012844] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[  621.012847] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[  621.012856] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[  621.012858] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[  621.012864] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[  621.012865] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[  621.012871] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[  621.012874] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=0 streaming=0
[  621.012879] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[  621.012881] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[  621.012887] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[  621.012889] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[  621.012898] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[  621.012899] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[  621.012906] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[  621.012989] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[  621.012996] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[  621.013000] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[  621.013032] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[  621.013035] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[  621.013047] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[  621.013052] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[  621.013066] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[  621.013077] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[  621.013091] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[  621.013121] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[  621.013139] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[  621.013154] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[  621.013160] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[  621.013163] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[  621.013180] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[  621.013182] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[  621.013189] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[  621.013191] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[  621.013198] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[  621.013200] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[  621.013208] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[  621.013213] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[  621.013221] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[  621.013227] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[  621.013236] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[  621.013248] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[  621.013255] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[  621.013258] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[  621.013265] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[  621.013269] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[  621.013280] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[  621.013284] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[  621.013296] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[  621.017514] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[  621.017523] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[  621.018301] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[  621.018308] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[  621.019185] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[  621.019193] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[  621.020374] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[  621.020382] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[  621.020398] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[  621.020404] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[  621.024880] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0

=== step 2: window: values, slider, refresh
PROMPT: Open 'Elgato Game Capture HD Settings' from the application menu.
[PASS] Input Composite, Source size, Constant (CBR), slider at '11.25 Mbit/s' (no other words), High, Stretch off; no Refresh button?
PROMPT: Bitrate mode Variable (VBR); move the slider to both ends, then to about 4.00.
[PASS] VBR slider from 3.00 to 7.23 Mbit/s?
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
video_bitrate: 8060000
[PASS] VBR about 8000 kbit/s (4.00 shown)
PROMPT: Output size 480x360.
[PASS] mode Variable (VBR) and locked, slider 2.00 to 7.50? -- It shouldn't say (VBR only) in output size selection
PROMPT: Output size Source size, Bitrate mode Constant (CBR), slider to the left end.
[FAIL] CBR slider from 7.00 to 11.25? -- When I move it to the right it shows 11.25 for a bit but then it settles at 11.24
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
video_bitrate: 7000000
[PASS] CBR 7000 kbit/s
$ v4l2-ctl -d /dev/elgato-gchd -i 3 -c video_bitrate=11250000
Video input set to 3 (S-Video: Camera, no signal)
PROMPT: Leave the window open; wait 3 s.
[FAIL] the window now shows Input S-Video and CBR 11.25 by itself (changed from the terminal)? -- no, 11.25 settle to 11.2
$ v4l2-ctl -d /dev/elgato-gchd -i 2
Video input set to 2 (Composite: Camera, no signal)
--- dmesg step 2
[  942.900207] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[  942.900217] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 1064.515673] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[ 1064.515708] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== step 3: OBS: the same settings, changes live in both directions
PROMPT: In the settings window: Input Composite, Source size, Bitrate mode Variable (VBR), slider about 5.00.
PROMPT: Close OBS if open. Start OBS; open the properties of the source 'Elgato Game Capture HD'.
[FAIL] OBS's properties show the same values as the settings window (Composite, Source size, VBR, Bitrate slider about 5.00 Mbit/s)? -- I accidentaly changed settings in our GUI settings and they didn't change in OBS or in the other direction, OBS changes don't reflect on the GUI settings.
PROMPT: In OBS's properties choose Output size 640x480 -> OK. Wait about 8 s.
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
[PASS] OBS's change reached the device
[PASS] OBS shows the picture at 640x480 without a long interruption, and the settings window shows 640x480 by itself?
PROMPT: In the settings window choose Input 'S-Video'; wait 5 s.
[PASS] no message box, OBS's picture went to S-Video (no signal) at once? -- This setting appears to propagate everywhere, so I guess bitrate is the only setting that fails to propagate so far.
PROMPT: In the settings window choose Input 'Composite' and Output size Source size; wait 8 s.
[PASS] OBS shows the console again at the source size?
PROMPT: Close OBS completely; wait 5 s.
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate_mode
video_bitrate_mode: 1 (Constant Bitrate)
[FAIL] settings stay after OBS closed (VBR) (expected /video_bitrate_mode: 0/, got: video_bitrate_mode: 1 (Constant Bitrate))
$ sh -c grep -h '\[gchd\]' /home/ff5/.config/obs-studio/logs/$(ls -t /home/ff5/.config/obs-studio/logs | head -1) | tail -15
grep: /home/ff5/.config/obs-studio/logs/2026-11-19: No such file or directory
grep: 08-42-12.txt: No such file or directory
PROMPT: In the settings window choose Bitrate mode Constant (CBR), slider 11.25; close the window.
--- dmesg step 3
[ 1300.689640] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[ 1302.016704] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1302.016956] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1302.026257] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1302.026780] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 1302.027306] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 1302.140259] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1302.140263] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[ 1307.066653] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1312.093533] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1317.121087] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1319.330046] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[ 1320.470954] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1320.471293] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1320.477898] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1320.478572] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 1320.479063] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 1320.594379] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1320.594388] elgato_gchd 1-2:1.0: analog mode now 240p, output 1
[ 1320.963430] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[ 1325.484810] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1330.523099] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1331.620960] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[ 1333.445240] elgato_gchd 1-2:1.0: back-end not settled: status 0xf2
[ 1333.471734] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1333.471984] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1333.478797] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1333.479682] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 1333.482626] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 1333.594663] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1333.594666] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[ 1333.963306] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[ 1338.482292] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1343.518978] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1347.040883] elgato_gchd 1-2:1.0: encoder restart: output 1 -> 0
[ 1347.962319] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1347.962600] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1347.974594] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1347.975836] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 1347.976575] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 1348.086042] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1348.086045] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[ 1352.438692] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[ 1352.985983] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=102 bytes=6684672 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1353.521018] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1353.521462] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1353.533122] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1353.533693] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 1353.535056] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 1353.648234] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1353.648238] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[ 1354.012779] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[ 1358.532250] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1363.569376] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1365.635713] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[ 1367.247786] elgato_gchd 1-2:1.0: back-end not settled: status 0xf2
[ 1367.276441] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1367.276788] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1367.296300] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1367.296910] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 1367.298460] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 1367.399901] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1367.399904] elgato_gchd 1-2:1.0: analog mode now 240p, output 1
[ 1367.768860] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[ 1372.335452] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1377.372606] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=14417920 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1382.412097] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=21954560 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1387.450867] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 ep84_bytes=29491200 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1392.489589] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 ep84_bytes=37027840 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1396.544738] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[ 1396.659987] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=1 streaming=1
[ 1397.528708] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=680 bytes=44564480 ep84_bytes=44564480 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=19
[ 1398.374037] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1398.374411] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1398.381450] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1398.381984] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 1398.382518] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 1398.483178] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1398.483187] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[ 1398.849199] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[ 1400.258384] elgato_gchd 1-2:1.0: input selected: input=3; detection scheduled
[ 1400.366633] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 1400.443785] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 1495.689855] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=1 streaming=1
[ 1495.690939] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[ 1495.971332] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 1496.074726] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 1496.165731] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 1496.234320] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 1496.241128] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[ 1496.267388] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 1496.269981] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[ 1496.339360] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 1496.342104] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[ 1496.414765] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 1496.420922] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[ 1496.420927] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[ 1496.420930] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[ 1496.420932] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[ 1497.122802] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 1497.151298] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1497.151673] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1497.163372] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1497.163925] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 1497.166948] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 1497.276112] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1499.183613] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[ 1499.186339] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[ 1499.186342] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[ 1499.186358] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[ 1499.186360] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[ 1500.060812] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 1500.088414] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1500.089228] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1500.104531] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1500.105104] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 1500.106667] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 1500.213335] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1500.213343] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[ 1502.150544] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1502.150903] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1502.159999] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1502.160546] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 1502.161542] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 1502.289946] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1502.289949] elgato_gchd 1-2:1.0: analog mode now 240p, output 1
[ 1502.659530] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[ 1504.907791] elgato_gchd 1-2:1.0: encoder restart: output 1 -> 0
[ 1505.765385] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1505.765995] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1505.772472] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1505.772987] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 1505.773619] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 1505.888111] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1505.888114] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[ 1510.771764] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1515.799725] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1520.825609] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1525.610750] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[ 1525.850370] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1527.029647] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1527.029852] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1527.035522] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1527.036032] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 1527.036505] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 1527.152140] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1527.152142] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[ 1528.974460] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== summary (T-060-settings, 6143ad5 T-060: one global setting for the OBS plugin; driver timing marks for a latency benchmark)
PASS | step 1 install the driver part and the plugin | v4l2-compliance: no failures
PASS | step 2 window: values, slider, refresh | Input Composite, Source size, Constant (CBR), slider at '11.25 Mbit/s' (no other words), High, Stretch off; no Refresh button?
PASS | step 2 window: values, slider, refresh | VBR slider from 3.00 to 7.23 Mbit/s?
PASS | step 2 window: values, slider, refresh | VBR about 8000 kbit/s (4.00 shown)
PASS | step 2 window: values, slider, refresh | mode Variable (VBR) and locked, slider 2.00 to 7.50? -- It shouldn't say (VBR only) in output size selection
FAIL | step 2 window: values, slider, refresh | CBR slider from 7.00 to 11.25? -- When I move it to the right it shows 11.25 for a bit but then it settles at 11.24
PASS | step 2 window: values, slider, refresh | CBR 7000 kbit/s
FAIL | step 2 window: values, slider, refresh | the window now shows Input S-Video and CBR 11.25 by itself (changed from the terminal)? -- no, 11.25 settle to 11.2
FAIL | step 3 OBS: the same settings, changes live in both directions | OBS's properties show the same values as the settings window (Composite, Source size, VBR, Bitrate slider about 5.00 Mbit/s)? -- I accidentaly changed settings in our GUI settings and they didn't change in OBS or in the other direction, OBS changes don't reflect on the GUI settings.
PASS | step 3 OBS: the same settings, changes live in both directions | OBS's change reached the device
PASS | step 3 OBS: the same settings, changes live in both directions | OBS shows the picture at 640x480 without a long interruption, and the settings window shows 640x480 by itself?
PASS | step 3 OBS: the same settings, changes live in both directions | no message box, OBS's picture went to S-Video (no signal) at once? -- This setting appears to propagate everywhere, so I guess bitrate is the only setting that fails to propagate so far.
PASS | step 3 OBS: the same settings, changes live in both directions | OBS shows the console again at the source size?
FAIL | step 3 OBS: the same settings, changes live in both directions | settings stay after OBS closed (VBR) (expected /video_bitrate_mode: 0/, got: video_bitrate_mode: 1 (Constant Bitrate))
