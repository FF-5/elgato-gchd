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

Settings, one global setting (OBS with the plugin, a console on
composite):

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-060-settings.sh > tests/T-060-settings.sh
bash gchd-test.sh T-060-settings --branch feat/app-bridge
```

## Findings

- T-bench (6143ad5), first latency tables (`logs/timing-2026-11-19.md`
  on `traces`): request → first buffer: bitrate change 1.74 s (n 4),
  output size 1.45 s (n 4), console reset (mode change) about 1.74 s from
  the detection (NULL sent 0.51 s after it), input back to composite
  2.21 s, capture start 2.50 s (firmware ready 0.16 s, source detected
  0.78 s, START 2.04 s). NULL → START: driver 0.97–1.14 s, Windows
  recordings 2.30 s (NULL → STOP 1.15 s): the driver's command sequence is
  about twice as fast; START → first data 0.37–0.49 s (not in the
  recordings: bulk IN filtered). Largest remaining driver phases: capture
  start between source detection and START (1.26 s: encoder start, input
  configuration, encoder programming) and the 0.51 s mode-change
  confirmation. The slower feel in OBS is likely after the driver
  (decoder waiting for a keyframe every 12–14 frames, OBS buffering).
- T-060-settings (6143ad5): 13 PASS, 4 FAIL. CBR slider settled at 11.24:
  11250 kbit/s is not a multiple of the 20 kbit/s step, and Python rounds
  halves to even (562.5 → 562); now CBR 10 kbit/s and VBR 20 kbit/s per
  0.01 Mbit/s, rounded half up. Bitrate did not propagate: OBS's dialog
  did not show the device's values (cause not visible: the test's OBS log
  command failed on the space in the file name), and the plugin applied
  all its settings on OK, so an output size change wrote its stale CBR
  over the window's VBR. Now the plugin applies only the settings that
  changed since its last sync with the device (at creation, when the
  properties open, after each apply) and logs what it read. Limit: OBS
  copies the settings before the dialog syncs; changing something and
  then pressing Cancel writes that older copy back. "(VBR only)" removed
  from the size name. Input and output size propagated in both
  directions (maintainer).
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

gchd-test T-060-settings 20261119-090336 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-060-settings-20261119-090336.log
== build
$ git fetch -q origin
$ git checkout -q feat/app-bridge
$ git pull -q --ff-only
commit: 6d2e9a0 OBS plugin: apply only changed settings, sync from the device at creation and when the properties open; CBR 11.25 exact (10 kbit/s steps, half up) in the window and the plugin (T-060)
commit: 6d2e9a0 OBS plugin: apply only changed settings, sync from the device at creation and when the properties open; CBR 11.25 exact (10 kbit/s steps, half up) in the window and the plugin (T-060)
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
[ 2516.163437] usbcore: deregistering interface driver elgato_gchd
[ 2526.827903] usbcore: registered new interface driver elgato_gchd
[ 2543.996254] workqueue: hub_event hogged CPU for >10000us 4 times, consider switching to WQ_UNBOUND
[ 2544.110387] usb 1-2: new high-speed USB device number 6 using xhci_hcd
[ 2544.527946] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[ 2544.527976] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[ 2544.527984] usb 1-2: Product: GameCapture HD
[ 2544.527991] usb 1-2: Manufacturer: Elgato
[ 2544.527997] usb 1-2: SerialNumber: 04107F1267
[ 2544.534974] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[ 2544.536560] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[ 2547.282810] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=0 streaming=0
[ 2547.282854] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 2547.282860] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 2547.282873] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 2547.282877] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 2547.282889] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 2547.282893] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 2547.282905] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 2547.282909] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[ 2547.282922] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 2547.282924] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=0 streaming=0
[ 2547.282936] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 2547.282941] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 2547.282952] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 2547.282956] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 2547.282967] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 2547.282970] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 2547.282982] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 2547.282998] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 2547.283010] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 2547.283017] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 2547.283039] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 2547.283044] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 2547.283056] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 2547.283066] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 2547.283077] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 2547.283087] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 2547.283099] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 2547.283122] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 2547.283134] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 2547.283216] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 2547.283228] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 2547.283233] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 2547.283245] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 2547.283251] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 2547.283263] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 2547.283267] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 2547.283279] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 2547.283283] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 2547.283297] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 2547.283300] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 2547.283311] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 2547.283314] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 2547.283440] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 2547.283456] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 2547.283467] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 2547.283471] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 2547.283481] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 2547.283485] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 2547.283497] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 2547.283502] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 2547.283513] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 2547.286375] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 2547.286395] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 2547.290303] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 2547.290315] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 2547.291682] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 2547.291689] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 2547.292670] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[ 2547.292677] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 2547.292693] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 2547.292702] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 2547.296744] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0

=== step 2: window: values, slider, refresh
PROMPT: Open 'Elgato Game Capture HD Settings' from the application menu.
[PASS] Input Composite, Source size, Constant (CBR), slider at '11.25 Mbit/s' (no other words), High, Stretch off; no Refresh button?
PROMPT: Bitrate mode Constant (CBR); move the slider fully to the right and release it; wait 3 s.
[PASS] the slider stays at 11.25 (not 11.24)?
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
video_bitrate: 11250000
[PASS] CBR 11250 kbit/s exactly
PROMPT: Bitrate mode Variable (VBR); move the slider to both ends, then to about 4.00.
[PASS] VBR slider from 3.00 to 7.23 Mbit/s?
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
video_bitrate: 8000000
[PASS] VBR about 8000 kbit/s (4.00 shown)
PROMPT: Output size 480x360.
[PASS] mode Variable (VBR) and locked, slider 2.00 to 7.50, and the size reads just '480x360'? -- Everytime a setting is changed it's advertised at the lower left corner. Can this text appear a little bit lower? Hopefully at the same height as the text in the Close button. On output size change the change is not advertised in this text.
PROMPT: Output size Source size, Bitrate mode Constant (CBR), slider to the left end.
[PASS] CBR slider from 7.00 to 11.25?
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
video_bitrate: 7000000
[PASS] CBR 7000 kbit/s
$ v4l2-ctl -d /dev/elgato-gchd -i 3 -c video_bitrate=11250000
Video input set to 3 (S-Video: Camera, no signal)
PROMPT: Leave the window open; wait 3 s.
[PASS] the window now shows Input S-Video and CBR 11.25 by itself (changed from the terminal)?
$ v4l2-ctl -d /dev/elgato-gchd -i 2
Video input set to 2 (Composite: Camera, no signal)
--- dmesg step 2
[ 2935.702172] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 2935.702206] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 2970.505552] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[ 2970.505640] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== step 3: OBS: the same settings, changes in both directions
PROMPT: In the settings window: Input Composite, Source size, Bitrate mode Variable (VBR), slider 5.00, Stretch off.
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
video_bitrate: 10000000
[PASS] window: VBR 10000 kbit/s (5.00 shown)
PROMPT: Close OBS if open. Start OBS (the scene with the source 'Elgato Game Capture HD'); open the source's properties.
$ obs_log 5
09:29:22.394: [gchd] settings applied: input 2, output 0, mode 1, 8.38 Mbit/s, profile 4, stretch 0
09:29:25.011: [gchd] settings applied: input 2, output 0, mode 0, 7.00 Mbit/s, profile 4, stretch 0
09:29:40.386: [gchd] capturing from /dev/video0 (video yes, audio yes)
09:30:02.128: [gchd] settings applied: input 2, output 0, mode 0, 5.00 Mbit/s, profile 4, stretch 0
09:30:08.947: [gchd] properties: device settings read: input 2, output 0, mode 0, 5.00 Mbit/s
$ obs_log 5
09:29:22.394: [gchd] settings applied: input 2, output 0, mode 1, 8.38 Mbit/s, profile 4, stretch 0
09:29:25.011: [gchd] settings applied: input 2, output 0, mode 0, 7.00 Mbit/s, profile 4, stretch 0
09:29:40.386: [gchd] capturing from /dev/video0 (video yes, audio yes)
09:30:02.128: [gchd] settings applied: input 2, output 0, mode 0, 5.00 Mbit/s, profile 4, stretch 0
09:30:08.947: [gchd] properties: device settings read: input 2, output 0, mode 0, 5.00 Mbit/s
[PASS] plugin read the device's settings (VBR, 5.00)
[PASS] OBS's properties show Composite, Source size, Variable (VBR), Bitrate 5.00 Mbit/s, Stretch off? -- In the plugin the bitrate mode menu lists CBR "Constant (CBR), 11250kbit/s". It should just read "Constant (CBR)".
PROMPT: In OBS's properties move the Bitrate slider to 6.00 -> OK. Wait 3 s.
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
video_bitrate: 12040000
[FAIL] OBS: VBR 12000 kbit/s (6.00 shown) (expected /video_bitrate: 12000000/, got: video_bitrate: 12040000)
[PASS] the settings window shows 6.00 by itself? -- There's no input at times even though the source is running.
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
[PASS] OBS's output size reached the device
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
video_bitrate: 12040000
[FAIL] bitrate not overwritten by the size change (still 12000) (expected /video_bitrate: 12000000/, got: video_bitrate: 12040000)
[FAIL] OBS shows the picture at 640x480, and the settings window shows 640x480 by itself? -- There's no input in OBS but the setting did propagate to the settings GUI.
PROMPT: In the settings window: Stretch on, Output size Source size, Bitrate slider 4.00; wait 8 s.
$ v4l2-ctl -d /dev/elgato-gchd -C stretch_sd_input
stretch_sd_input: 1
[PASS] window: stretch on
PROMPT: In OBS open the source's properties again.
[PASS] OBS's properties show Source size, Bitrate 4.00, Stretch on?
PROMPT: Close the properties with Cancel. In the settings window choose Input 'S-Video'; wait 5 s.
[FAIL] no message box, OBS's picture went to S-Video (no signal) at once? -- Nothing happened and the settings window froze, maybe related to the input problem now.
PROMPT: In the settings window choose Input 'Composite' and Stretch off; wait 8 s.
[PASS] OBS shows the console again? -- Eventually the settings window switch to S-Video and changing to Composite also took a while.
PROMPT: Close OBS completely; wait 5 s.
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
video_bitrate: 12040000
[FAIL] settings stay after OBS closed (VBR 8000 kbit/s) (expected /video_bitrate: 8000000/, got: video_bitrate: 12040000)
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate_mode
video_bitrate_mode: 0 (Variable Bitrate)
[PASS] VBR stays
$ obs_log 20
09:34:17.134: [gchd] settings applied: input 2, output 0, mode 0, 5.68 Mbit/s, profile 4, stretch 0
09:34:17.455: [gchd] settings applied: input 2, output 0, mode 0, 5.71 Mbit/s, profile 4, stretch 0
09:34:17.548: [gchd] settings applied: input 2, output 0, mode 0, 5.73 Mbit/s, profile 4, stretch 0
09:34:17.758: [gchd] settings applied: input 2, output 0, mode 0, 5.74 Mbit/s, profile 4, stretch 0
09:34:17.973: [gchd] settings applied: input 2, output 0, mode 0, 5.80 Mbit/s, profile 4, stretch 0
09:34:18.072: [gchd] settings applied: input 2, output 0, mode 0, 5.82 Mbit/s, profile 4, stretch 0
09:34:18.171: [gchd] settings applied: input 2, output 0, mode 0, 5.85 Mbit/s, profile 4, stretch 0
09:34:18.491: [gchd] settings applied: input 2, output 0, mode 0, 5.87 Mbit/s, profile 4, stretch 0
09:34:18.589: [gchd] settings applied: input 2, output 0, mode 0, 5.89 Mbit/s, profile 4, stretch 0
09:34:18.699: [gchd] settings applied: input 2, output 0, mode 0, 5.90 Mbit/s, profile 4, stretch 0
09:34:19.009: [gchd] settings applied: input 2, output 0, mode 0, 5.93 Mbit/s, profile 4, stretch 0
09:34:19.436: [gchd] settings applied: input 2, output 0, mode 0, 5.96 Mbit/s, profile 4, stretch 0
09:34:21.194: [gchd] settings applied: input 2, output 0, mode 0, 5.97 Mbit/s, profile 4, stretch 0
09:34:23.273: [gchd] settings applied: input 2, output 0, mode 0, 6.00 Mbit/s, profile 4, stretch 0
09:34:23.902: [gchd] settings applied: input 2, output 0, mode 0, 6.02 Mbit/s, profile 4, stretch 0
09:36:06.713: [gchd] properties: device settings read: input 2, output 0, mode 0, 6.02 Mbit/s
09:36:10.096: [gchd] settings applied: input 2, output 1, mode 0, 6.02 Mbit/s, profile 4, stretch 0
09:37:52.574: [gchd] properties: device settings read: input 2, output 0, mode 0, 4.01 Mbit/s
09:38:23.934: [gchd] settings applied: input 2, output 1, mode 0, 6.02 Mbit/s, profile 4, stretch 0
09:41:40.959: [gchd] properties: device settings read: input 2, output 1, mode 0, 6.02 Mbit/s
PROMPT: In the settings window choose Bitrate mode Constant (CBR), slider 11.25; close the window.
--- dmesg step 3
[ 3969.946930] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 3970.086621] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 3970.154567] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 3970.479301] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 3970.554760] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 3970.556884] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[ 3970.556887] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[ 3970.556890] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[ 3970.556892] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[ 3971.454214] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 3972.051432] irq 20: nobody cared (try booting with the "irqpoll" option)
[ 3972.051439] CPU: 3 PID: 0 Comm: swapper/3 Tainted: G        W  OE      6.8.0-146-generic #146-Ubuntu
[ 3972.051442] Hardware name: innotek GmbH VirtualBox/VirtualBox, BIOS VirtualBox 12/01/2006
[ 3972.051443] Call Trace:
[ 3972.051444]  <IRQ>
[ 3972.051447]  dump_stack_lvl+0x76/0xa0
[ 3972.051452]  dump_stack+0x10/0x20
[ 3972.051454]  __report_bad_irq+0x30/0xd0
[ 3972.051457]  note_interrupt+0x2b7/0x300
[ 3972.051459]  handle_irq_event+0x79/0x80
[ 3972.051483]  handle_fasteoi_irq+0x7d/0x200
[ 3972.051485]  __common_interrupt+0x51/0xf0
[ 3972.051488]  common_interrupt+0x44/0xb0
[ 3972.051490]  asm_common_interrupt+0x27/0x40
[ 3972.051493] RIP: 0010:handle_softirqs+0x74/0x340
[ 3972.051496] Code: 01 00 00 c7 45 c0 0a 00 00 00 44 89 65 ac 4c 89 7d b8 40 88 7d c7 48 89 5d b0 65 66 c7 05 4a 22 32 64 00 00 fb 0f 1f 44 00 00 <41> ba ff ff ff ff 45 0f bc d5 41 83 c2 01 0f 84 9b 00 00 00 48 c7
[ 3972.051497] RSP: 0018:ffffcf4ec0160f58 EFLAGS: 00000282
[ 3972.051499] RAX: 000000010038052a RBX: ffff8988808d2900 RCX: 0000000000000000
[ 3972.051500] RDX: 0000000000000000 RSI: 0000000000000000 RDI: 0000000000000000
[ 3972.051501] RBP: ffffcf4ec0160fb8 R08: 0000000000000040 R09: 0000000000000000
[ 3972.051502] R10: 0000000000000000 R11: 0000000000000000 R12: 0000000004200042
[ 3972.051503] R13: 0000000000000040 R14: 0000000000000000 R15: 000000010038052c
[ 3972.051505]  ? native_apic_msr_read+0x1a/0x60
[ 3972.051507]  ? _raw_spin_unlock+0xe/0x40
[ 3972.051509]  __irq_exit_rcu+0xd9/0x100
[ 3972.051511]  irq_exit_rcu+0xe/0x20
[ 3972.051512]  common_interrupt+0xa4/0xb0
[ 3972.051514]  </IRQ>
[ 3972.051514]  <TASK>
[ 3972.051515]  asm_common_interrupt+0x27/0x40
[ 3972.051516] RIP: 0010:pv_native_safe_halt+0xb/0x10
[ 3972.051518] Code: 22 d7 31 ff e9 06 3a 01 00 66 0f 1f 44 00 00 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 eb 07 0f 00 2d c9 4a 3b 00 fb f4 <c3> cc cc cc cc 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 83
[ 3972.051519] RSP: 0018:ffffcf4ec00bbe78 EFLAGS: 00000246
[ 3972.051520] RAX: 0000000000000000 RBX: 0000000000000000 RCX: 0000000000000000
[ 3972.051520] RDX: 0000000000000000 RSI: 0000000000000000 RDI: 0000000000000000
[ 3972.051521] RBP: ffffcf4ec00bbe80 R08: 0000000000000000 R09: 0000000000000000
[ 3972.051521] R10: 0000000000000000 R11: 0000000000000000 R12: ffff8988808d2900
[ 3972.051522] R13: 0000000000000000 R14: 0000000000000000 R15: 0000000000000000
[ 3972.051523]  ? default_idle+0x9/0x30
[ 3972.051525]  arch_cpu_idle+0x9/0x10
[ 3972.051526]  default_idle_call+0x2c/0xf0
[ 3972.051528]  cpuidle_idle_call+0x11c/0x1f0
[ 3972.051530]  do_idle+0x97/0x100
[ 3972.051532]  cpu_startup_entry+0x2a/0x30
[ 3972.051533]  start_secondary+0x129/0x160
[ 3972.051535]  secondary_startup_64_no_verify+0x184/0x18b
[ 3972.051538]  </TASK>
[ 3972.051539] handlers:
[ 3972.051540] [<00000000c0eff474>] usb_hcd_irq
[ 3972.051544] [<00000000b3c36ff2>] vgdrvLinuxISR [vboxguest]
[ 3972.051561] Disabling IRQ #20
[ 4053.210252] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 4065.688442] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 4065.896675] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 4068.601353] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 4068.602920] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 4069.016545] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 4070.785493] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 4070.785513] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[ 4092.832366] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 4093.040538] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 4095.749662] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 4095.750155] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 4096.160434] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 4097.930443] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 4097.930449] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[ 4097.930452] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[ 4100.840413] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=26 bytes=1703936 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 4119.144386] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 4119.352621] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 4122.057600] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 4122.058752] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 4122.473396] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 4124.242291] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 4124.242299] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[ 4127.153645] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=39 bytes=2555904 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=1 start_pending=0 ring_dropped=0 frames_waiting=0
[ 4129.856461] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[ 4132.249432] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=93 bytes=6094848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 4137.347360] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=138 bytes=9043968 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 4142.441268] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=181 bytes=11862016 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 4147.536354] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 4152.633608] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=270 bytes=17694720 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 4157.729521] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=318 bytes=20840448 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 4162.825519] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=354 bytes=23199744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 4167.920401] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=390 bytes=25559040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 4173.016471] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=426 bytes=27918336 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 4178.114449] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=462 bytes=30277632 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=1 start_pending=0 ring_dropped=0 frames_waiting=0
[ 4183.208813] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=494 bytes=32374784 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 4247.896440] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 4248.104492] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 4250.810418] elgato_gchd 1-2:1.0: source changed to 240p during setup; configuring again
[ 4250.810424] elgato_gchd 1-2:1.0: analog mode change failed: -11
[ 4253.825837] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 4269.841308] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 4271.192511] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[ 4271.192518] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 4271.192521] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 4271.192523] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 4336.192503] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 4348.672559] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 4348.880721] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 4351.585881] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 4351.586469] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 4352.000573] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 4353.768619] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 4356.680576] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=29 bytes=1900544 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=1 start_pending=0 ring_dropped=0 frames_waiting=0
[ 4368.331466] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 4384.345628] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 4385.696537] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[ 4385.696544] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[ 4385.696546] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[ 4385.696548] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[ 4463.488620] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 4475.969609] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 4476.177807] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 4478.880705] elgato_gchd 1-2:1.0: source changed to 480i during setup; configuring again
[ 4481.584647] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 4497.600633] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 4498.953435] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[ 4498.953440] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[ 4498.953443] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[ 4498.953445] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[ 4576.744622] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 4589.224651] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 4589.433429] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 4592.136706] elgato_gchd 1-2:1.0: source changed to 480i during setup; configuring again
[ 4594.840629] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 4603.061726] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=1 streaming=1
[ 4610.856640] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[ 4612.209465] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[ 4612.209478] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 4612.209481] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 4612.209491] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 4677.209718] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 4689.691693] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 4689.896679] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 4692.601018] elgato_gchd 1-2:1.0: source changed to 240p during setup; configuring again
[ 4692.808767] elgato_gchd 1-2:1.0: input selected: input=3; detection scheduled
[ 4706.338942] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=1 streaming=1
[ 4707.785645] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 4742.833326] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[ 4756.664765] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 4790.153749] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 4806.169357] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 4807.521799] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[ 4807.521805] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 4807.521807] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 4807.521809] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 4883.562400] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 4896.264160] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 4896.489865] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 4899.257841] elgato_gchd 1-2:1.0: source changed to 240p during setup; configuring again
[ 4906.466872] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[WARN] dmesg: elgato_gchd 1-2:1.0: analog mode change failed: -11

=== summary (T-060-settings, 6d2e9a0 OBS plugin: apply only changed settings, sync from the device at creation and when the properties open; CBR 11.25 exact (10 kbit/s steps, half up) in the window and the plugin (T-060))
PASS | step 1 install the driver part and the plugin | v4l2-compliance: no failures
PASS | step 2 window: values, slider, refresh | Input Composite, Source size, Constant (CBR), slider at '11.25 Mbit/s' (no other words), High, Stretch off; no Refresh button?
PASS | step 2 window: values, slider, refresh | the slider stays at 11.25 (not 11.24)?
PASS | step 2 window: values, slider, refresh | CBR 11250 kbit/s exactly
PASS | step 2 window: values, slider, refresh | VBR slider from 3.00 to 7.23 Mbit/s?
PASS | step 2 window: values, slider, refresh | VBR about 8000 kbit/s (4.00 shown)
PASS | step 2 window: values, slider, refresh | mode Variable (VBR) and locked, slider 2.00 to 7.50, and the size reads just '480x360'? -- Everytime a setting is changed it's advertised at the lower left corner. Can this text appear a little bit lower? Hopefully at the same height as the text in the Close button. On output size change the change is not advertised in this text.
PASS | step 2 window: values, slider, refresh | CBR slider from 7.00 to 11.25?
PASS | step 2 window: values, slider, refresh | CBR 7000 kbit/s
PASS | step 2 window: values, slider, refresh | the window now shows Input S-Video and CBR 11.25 by itself (changed from the terminal)?
PASS | step 3 OBS: the same settings, changes in both directions | window: VBR 10000 kbit/s (5.00 shown)
PASS | step 3 OBS: the same settings, changes in both directions | plugin read the device's settings (VBR, 5.00)
PASS | step 3 OBS: the same settings, changes in both directions | OBS's properties show Composite, Source size, Variable (VBR), Bitrate 5.00 Mbit/s, Stretch off? -- In the plugin the bitrate mode menu lists CBR "Constant (CBR), 11250kbit/s". It should just read "Constant (CBR)".
FAIL | step 3 OBS: the same settings, changes in both directions | OBS: VBR 12000 kbit/s (6.00 shown) (expected /video_bitrate: 12000000/, got: video_bitrate: 12040000)
PASS | step 3 OBS: the same settings, changes in both directions | the settings window shows 6.00 by itself? -- There's no input at times even though the source is running.
PASS | step 3 OBS: the same settings, changes in both directions | OBS's output size reached the device
FAIL | step 3 OBS: the same settings, changes in both directions | bitrate not overwritten by the size change (still 12000) (expected /video_bitrate: 12000000/, got: video_bitrate: 12040000)
FAIL | step 3 OBS: the same settings, changes in both directions | OBS shows the picture at 640x480, and the settings window shows 640x480 by itself? -- There's no input in OBS but the setting did propagate to the settings GUI.
PASS | step 3 OBS: the same settings, changes in both directions | window: stretch on
PASS | step 3 OBS: the same settings, changes in both directions | OBS's properties show Source size, Bitrate 4.00, Stretch on?
FAIL | step 3 OBS: the same settings, changes in both directions | no message box, OBS's picture went to S-Video (no signal) at once? -- Nothing happened and the settings window froze, maybe related to the input problem now.
PASS | step 3 OBS: the same settings, changes in both directions | OBS shows the console again? -- Eventually the settings window switch to S-Video and changing to Composite also took a while.
FAIL | step 3 OBS: the same settings, changes in both directions | settings stay after OBS closed (VBR 8000 kbit/s) (expected /video_bitrate: 8000000/, got: video_bitrate: 12040000)
PASS | step 3 OBS: the same settings, changes in both directions | VBR stays
WARN | step 3 OBS: the same settings, changes in both directions | dmesg: elgato_gchd 1-2:1.0: analog mode change failed: -11
