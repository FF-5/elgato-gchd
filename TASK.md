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
21. (Maintainer, 2026-11-19) Two setting groups: composite/S-video share
    all settings (switching between them changes nothing), HDMI/component
    have their own. Settings without effect on HDMI/component (output
    size, stretch, and for now the bitrate) are hidden for those inputs.
22. (Maintainer, 2026-11-19) Each bitrate mode keeps its last bitrate per
    group: VBR → CBR returns to the last CBR bitrate, not the maximum.
23. (Maintainer, 2026-11-19) Labels: bitrate mode "Constant (CBR)" /
    "Variable (VBR)" only; output sizes "720x480 NTSC / 720x576 PAL",
    "640x480", "480x360" (no "Source size", no detection).
24. (Maintainer, 2026-11-19) Settings window: the status text on the
    line of the Close button, as a notification area: no message at
    start, every change (also output size) reported there and gone after
    a few seconds.
25. (Maintainer, 2026-11-19) The settings window applies a bitrate slider
    once, when it is released (users set exact values and may drag for a
    long time). OBS's properties preview changes live: applied 0.5 s
    after the last change (OBS does not report the release).
26. (Maintainer, 2026-11-19) The kept bitrate depends on the output size
    too: 480x360 (other range) keeps its own.
27. (Maintainer, 2026-11-19) An input without signal shows a black picture
    of the output size in OBS, not the last picture.

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
profile, a bitrate slider in Mbit/s (applied 0.4 s after the slider
stops), a stretch switch; changes run in a background thread (the window
never waits for the device) and are reported in the status text next to
the Close button; refresh from the device every 1.5 s; output size,
bitrate mode, bitrate and stretch hidden for HDMI/component; a mode
change sends only the mode and shows the bitrate the driver remembered
for it. Installed with the driver part (offers python3-gi,
gir1.2-gtk-3.0, v4l-utils). Checked here on a virtual screen with a
stand-in v4l2-ctl that keeps groups and per-mode bitrates (labels,
hidden rows, mode switch VBR → CBR back to 9.00, output-size message,
a 3 s drag applied once on release). The slider is applied when the
mouse button is released; keys and the mouse wheel 0.4 s after the last
step.
The text menu `gchd-settings` shows the same items.

Setting groups (driver): `grp[2]` (HDMI/component, composite/S-video)
with bitrate mode, a bitrate per mode, profile and level; new module
parameters `bitrate_cbr`, `bitrate_vbr`, `hd_bitrate_mode`,
`hd_bitrate_cbr`, `hd_bitrate_vbr`, `hd_h264_profile`, `hd_h264_level`
(uevent and udev rule); the old `bitrate` is read once for older settings
files. A mode change loads that mode's bitrate into the bitrate control;
an input change to the other group loads that group into the controls.

OBS plugin: OBS 30 calls the source's update for every change in the
properties dialog, every slider step, and with the old settings on
Cancel; a plugin cannot see a slider's release (deferring the updates to
OK, `OBS_PROPERTIES_DEFER_UPDATE`, was tried and dropped: no preview). A
worker thread does all device calls: applies 0.5 s after the last change
the values that differ from the device, then reads the device back;
every 1.5 s it takes over changes made elsewhere into the settings (only
the changed keys) and reloads an open dialog (not while a change of the
user waits). The dialog fills in the mode, bitrate and profile the driver
keeps when the user changes the mode or the input group (from the
plugin's last reads); such filled-in values are not sent. Checked
offline: callback sequence (CBR → 9.00, VBR → 4.00, HDMI → its own
values); what is sent for a drag, a mode change with and without a moved
slider, Cancel of each, group changes with and without a profile change;
stream decoding.

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
- T-060-settings (6d2e9a0): 19 PASS, 5 FAIL, 1 WARN. (1) At 3972 s of
  uptime the guest kernel logged "irq 20: nobody cared" and "Disabling
  IRQ #20" (VirtualBox, the USB controller's interrupt). From then on USB
  ran without its interrupt: input configuration took 76 s instead of
  under 1 s, OBS showed no picture, input changes took long, the settings
  window froze waiting for v4l2-ctl, OBS could not close and had to be
  killed, and "analog mode change failed: -11" (timeout). Not caused by
  the driver; only a reboot re-enables the interrupt (if it recurs: the
  VirtualBox USB 2.0 (EHCI) controller instead of USB 3.0, or the kernel
  option `irqpoll`). The test now checks dmesg for it at the start and
  end. (2) 12040 instead of 12000 kbit/s: the slider position (6.02); the
  test now accepts ±60 kbit/s. (3) OBS calls the source's update for each
  slider step while the properties are open, so every step was applied
  (encoder restarts, ioctls in OBS's UI thread): now a worker applies
  0.5 s after the last change (replaced: applied on OK, decision 25).
  Requests (decisions 21–25) implemented.
- T-060-settings (0e1873d): 39 PASS, 8 FAIL. (1) The kept bitrate per
  mode did not show: after a mode change the window showed the previous
  bitrate (VBR 4.50 after CBR 9.01; CBR 8.00 after VBR 4.00). The driver
  switched its bitrate, but the bitrate control was set from inside the
  mode control's handler, which did not change the value read back. Now
  the bitrate control is volatile (reads give the bitrate in use) and
  nothing is set from inside a handler. (2) 480x360 at 2.00 then 640x480
  showed VBR 2.00 (below the 3.00 minimum): 480x360 now keeps its own
  bitrate (`bitrate_small`, decision 26). (3) Status text stayed; now a
  notification that disappears after 3 s, nothing at start (decision 24).
  (4) An input without signal froze the last picture in OBS: now black
  (decision 27). (5) Stretch on/off showed no visible change on the N64
  (maintainer). The recordings show only `v_in_source_type` 2 instead of
  3; whether the device changes the pixels or only signals another
  aspect ratio in the stream (which OBS would ignore) is not known: the
  test now captures a picture and the stream's aspect ratio with stretch
  off and on, and the plugin logs the sample aspect ratio. (6) Cancel
  after choosing HDMI in OBS restored Composite and 640x480 but left CBR
  11.25 instead of VBR 6.02 in the bitrate control; probably the same
  stale control value as (1) (the control now always reads the bitrate
  in use; the test checks it again).
  OBS: live preview with the 0.5 s delay worked (maintainer).
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

- Sometimes when switching to CBR (I don't know if always) it goes straight to 11.25. This was working well on the last test, this time not so much.
- Stretch it's not doing anything. I tested both PAL and NTSC sources and nothing, even through ffplay. I believe it was working at some point. Can you check at which point it was functional?

gchd-test T-060-settings 20261119-113637 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-060-settings-20261119-113637.log
== build
$ git fetch -q origin
$ git checkout -q feat/app-bridge
$ git pull -q --ff-only
commit: 41b2df0 Kept bitrate shown after a mode change (volatile bitrate control), 480x360 keeps its own bitrate; OBS: black picture without signal, picture/aspect ratio logged; settings window: notifications that disappear after 3 s (T-060)
commit: 41b2df0 Kept bitrate shown after a mode change (volatile bitrate control), 480x360 keeps its own bitrate; OBS: black picture without signal, picture/aspect ratio logged; settings window: notifications that disappear after 3 s (T-060)
build rc=0 warnings=0

=== step 1: install the driver part and the plugin
$ irq_clean
clean
[PASS] no USB interrupt disabled by the kernel (else: reboot first)
$ sudo rmmod elgato_gchd
rmmod: ERROR: Module elgato_gchd is not currently loaded
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
$ v4l2-ctl -d /dev/elgato-gchd -i 2 -v width=720,height=480,pixelformat=MPEG -c h264_profile=4,stretch_sd_input=0
Video input set to 2 (Composite: Camera, no signal)
$ v4l2-ctl -d /dev/elgato-gchd -c video_bitrate_mode=0
$ v4l2-ctl -d /dev/elgato-gchd -c video_bitrate=10000000
$ v4l2-ctl -d /dev/elgato-gchd -c video_bitrate_mode=1
$ v4l2-ctl -d /dev/elgato-gchd -c video_bitrate=11250000
$ v4l2-ctl -d /dev/elgato-gchd -v width=480,height=360 -c video_bitrate=15000000
$ v4l2-ctl -d /dev/elgato-gchd -v width=720,height=480
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
video_bitrate: 15000000
[FAIL] 720x480: CBR 11250 again (expected /video_bitrate: 11250000/, got: video_bitrate: 15000000)
--- dmesg step 1
[   79.551598] mc: Linux media interface: v0.10
[   79.561499] videodev: Linux video capture interface: v2.00
[   79.566779] usbcore: registered new interface driver elgato_gchd
[   88.864598] usb 1-2: new high-speed USB device number 3 using xhci_hcd
[   89.253039] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[   89.253058] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[   89.253064] usb 1-2: Product: GameCapture HD
[   89.253069] usb 1-2: Manufacturer: Elgato
[   89.253073] usb 1-2: SerialNumber: 04107F1267
[   89.262335] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[   89.265375] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[   93.428441] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[   93.428509] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[   93.428520] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[   93.428538] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[   93.428542] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[   93.428558] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[   93.428562] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[   93.428578] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[   93.428582] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[   93.428586] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[   93.428601] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[   93.428607] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[   93.428622] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[   93.428626] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[   93.428641] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[   93.428646] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[   93.428662] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[   93.428736] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[   93.428752] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[   93.428762] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[   93.428777] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[   93.428782] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[   93.428797] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[   93.428810] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[   93.428824] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[   93.428835] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[   93.428850] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[   93.428879] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[   93.428894] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[   93.428923] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[   93.428939] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[   93.428948] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[   93.428961] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[   93.428966] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[   93.428980] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[   93.428984] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[   93.428998] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[   93.429001] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[   93.429014] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[   93.429019] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[   93.429034] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[   93.429038] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[   93.429051] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[   93.429067] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[   93.429083] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[   93.429088] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[   93.429101] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[   93.429106] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[   93.429121] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[   93.429126] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[   93.429140] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[   93.431426] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[   93.431439] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[   93.433505] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[   93.433527] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[   93.435727] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[   93.435745] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[   93.438447] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[   93.438485] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[   93.438495] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[   93.448651] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[   93.448661] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== step 2: window: values, labels, slider, status line
PROMPT: Open 'Elgato Game Capture HD Settings' from the application menu.
[PASS] Input Composite, Output size '720x480 NTSC / 720x576 PAL', Constant (CBR), slider at '11.25 Mbit/s', High, Stretch off; no text next to the Close button?
PROMPT: Open the Output size list (then close it with Escape).
[PASS] the list is '720x480 NTSC / 720x576 PAL', '640x480', '480x360' (no 'Source size')?
PROMPT: Hold the slider and move it slowly back and forth for about 10 s, ending at about 9.00; watch the status text; then release it.
[PASS] no 'Changing'/'changed' message while you held the slider; 'Bitrate changed.' once after the release, on the line of the Close button, gone after about 3 s?
PROMPT: Wait 3 s.
$ rate_near 9000
near (9000000)
[PASS] CBR about 9000 kbit/s
PROMPT: Bitrate mode Variable (VBR); wait 3 s.
[PASS] the slider shows the last VBR bitrate, 5.00 Mbit/s (range 3.00 to 7.23)?
$ rate_near 10000
near (10020000)
[PASS] VBR 10000 kbit/s remembered
PROMPT: Move the slider to about 4.00; then Bitrate mode Constant (CBR); wait 3 s.
[PASS] the slider is back at about 9.00 (the last CBR bitrate), not at 11.25?
$ rate_near 9000
near (9000000)
[PASS] CBR 9000 kbit/s remembered
PROMPT: Move the slider fully to the right and release it; wait 3 s.
[PASS] the slider stays at 11.25 (not 11.24)?
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
video_bitrate: 11250000
[PASS] CBR 11250 kbit/s exactly
PROMPT: Output size 480x360; wait 3 s.
[PASS] 'Output size changed.'; mode Variable (VBR) and locked, slider range 2.00 to 7.50 at 7.50 (480x360 keeps its own bitrate)? -- I believed it opened at 2.00. On a previous test run the only notification I saw was bitrate mode change, so I think all notifications are being sent. The only notification that needs to be sent is the one the user interacts with and not side effects from that change.
PROMPT: Move the slider to about 2.00 and release it; then Output size 640x480; wait 3 s.
[PASS] 'Output size changed.'; Variable (VBR) at about 4.00 again (the VBR bitrate of the other sizes)?
$ rate_near 8000
near (8000000)
[PASS] VBR 8000 kbit/s for 640x480
PROMPT: Output size 480x360; wait 3 s.
[PASS] back at about 2.00 (480x360's own bitrate)?
$ rate_near 4000
near (4000000)
[PASS] 480x360 kept 4000 kbit/s
PROMPT: Output size '720x480 NTSC / 720x576 PAL'; Bitrate mode Constant (CBR); wait 3 s.
[PASS] slider at 11.25 again (CBR remembered)?
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
video_bitrate: 11250000
[PASS] CBR 11250 remembered
--- dmesg step 2


=== step 3: groups: composite/S-video share, HDMI/component have their own
PROMPT: Stretch on; Bitrate mode Variable (VBR). Then Input S-Video; wait 5 s.
[PASS] S-Video: Output size, VBR about 4.00 and Stretch on unchanged?
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate_mode
video_bitrate_mode: 0 (Variable Bitrate)
[PASS] S-video: VBR kept
PROMPT: Input HDMI; wait 5 s.
[PASS] only Input and H.264 profile are shown (no Output size, Bitrate mode, Bitrate, Stretch); status text on the Close button line?
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate_mode,video_bitrate,h264_profile
video_bitrate_mode: 1 (Constant Bitrate)
video_bitrate: 10000000
h264_profile: 4 (High)
PROMPT: H.264 profile Main (HDMI). Then Input Component; wait 5 s.
[PASS] Component also shows H.264 profile Main?
PROMPT: Input Composite; wait 5 s.
[PASS] Composite: profile High, Variable (VBR) about 4.00, Stretch on, all rows shown again?
$ rate_near 8000
near (8000000)
[PASS] composite group kept its VBR bitrate
$ v4l2-ctl -d /dev/elgato-gchd -C h264_profile
h264_profile: 4 (High)
[PASS] composite group kept High
$ v4l2-ctl -d /dev/elgato-gchd -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ v4l2-ctl -d /dev/elgato-gchd -C h264_profile
h264_profile: 2 (Main)
[PASS] HDMI group kept Main
$ v4l2-ctl -d /dev/elgato-gchd -c h264_profile=4
$ v4l2-ctl -d /dev/elgato-gchd -i 3 -c video_bitrate_mode=1
Video input set to 3 (S-Video: Camera, no signal)
$ v4l2-ctl -d /dev/elgato-gchd -c video_bitrate=10000000
PROMPT: Leave the window open; wait 3 s.
[FAIL] the window now shows Input S-Video and CBR 10.00 by itself (changed from the terminal)? -- Only input changed. Bitrate remained at VBR 5.00.
$ v4l2-ctl -d /dev/elgato-gchd -i 2 -c stretch_sd_input=0
Video input set to 2 (Composite: Camera, no signal)
--- dmesg step 3
[  450.484010] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[  450.484049] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[  473.126217] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[  473.126286] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[  490.990965] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[  490.991058] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[  506.941653] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[  506.941728] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[  528.967497] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=0 streaming=0
[  528.967535] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[  528.974922] elgato_gchd 1-2:1.0: input switch requested: old=0 new=3 hw_initialized=0 streaming=0
[  528.974940] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[  600.766311] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[  600.766386] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== step 4: OBS: the same settings, changes in both directions
PROMPT: In the settings window: Bitrate mode Variable (VBR), slider 5.00.
$ rate_near 10000
near (10000000)
[PASS] window: VBR about 10000 kbit/s (5.00 shown)
PROMPT: Close OBS if open. Start OBS (the scene with the source 'Elgato Game Capture HD'); open the source's properties.
$ obs_log 5
11:46:20.792: [gchd] capture started (source shown)
11:46:24.082: [gchd] capturing from /dev/video0 (video yes, audio yes)
11:46:24.095: [gchd] picture 720x480, sample aspect ratio 10:11
$ obs_log 5
11:46:20.792: [gchd] capture started (source shown)
11:46:24.082: [gchd] capturing from /dev/video0 (video yes, audio yes)
11:46:24.095: [gchd] picture 720x480, sample aspect ratio 10:11
[FAIL] plugin read the device's settings (VBR) (expected /properties: device settings read: input 2, output 0, mode 0/, got: 11:46:20.792: [gchd] capture started (source shown) 11:46:24.082: [gchd] capturing from /dev/video0 (video yes, audio yes) 11:46:24.095: [gchd] picture 720x480,)
[PASS] OBS: Composite, '720x480 NTSC / 720x576 PAL', Variable (VBR), about 5.00, Stretch off; mode list says just 'Constant (CBR)' / 'Variable (VBR)'?
PROMPT: In OBS's properties choose Bitrate mode Constant (CBR).
[FAIL] OBS's slider jumps to 10.00 (the last CBR bitrate), not 11.25? -- It changed to 11.25.
PROMPT: Back to Variable (VBR); hold the slider and move it back and forth for about 10 s, ending at about 6.00; release; wait 3 s (properties still open).
$ rate_near 12000
near (12000000)
[PASS] applied while the properties are open (VBR about 12000)
$ obs_log 6
11:46:24.095: [gchd] picture 720x480, sample aspect ratio 10:11
11:46:43.802: [gchd] properties: device settings read: input 2, output 0, mode 0, 5.00 Mbit/s
11:47:27.371: [gchd] settings applied: input 2, output 0, mode 1, 11.25 Mbit/s, profile 4, stretch 0
11:48:03.301: [gchd] settings applied: input 2, output 0, mode 0, 5.00 Mbit/s, profile 4, stretch 0
11:48:24.624: [gchd] settings applied: input 2, output 0, mode 0, 5.99 Mbit/s, profile 4, stretch 0
11:48:25.227: [gchd] settings applied: input 2, output 0, mode 0, 6.00 Mbit/s, profile 4, stretch 0
[PASS] the preview in the properties kept running while you moved the slider (short pause after each stop is fine)?
PROMPT: Press OK. Wait 3 s.
$ rate_near 12000
near (12000000)
[PASS] OBS: VBR about 12000 kbit/s (6.00 shown)
[PASS] the settings window shows about 6.00 by itself?
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
$ rate_near 12000
near (12000000)
[PASS] bitrate not overwritten by the size change
[PASS] OBS shows the picture at 640x480, and the settings window shows 640x480 by itself?
PROMPT: In OBS's properties choose Input HDMI (do not press OK); wait 3 s.
[PASS] OBS hides Output size, Stretch, Bitrate mode and Bitrate; the preview turned black (HDMI, no signal)?
PROMPT: Press Cancel. Wait 5 s.
$ v4l2-ctl -d /dev/elgato-gchd -I
Video input : 2 (Composite: ok)
[PASS] Cancel set Composite again
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
[PASS] Cancel kept 640x480
$ rate_near 12000
near (12000000)
[PASS] Cancel kept the bitrate
PROMPT: In the settings window: Stretch on, Output size '720x480 NTSC / 720x576 PAL', slider 4.00; wait 8 s.
$ v4l2-ctl -d /dev/elgato-gchd -C stretch_sd_input
stretch_sd_input: 1
[PASS] window: stretch on
PROMPT: In OBS open the source's properties again.
[PASS] OBS's properties show '720x480 NTSC / 720x576 PAL', about 4.00, Stretch on?
PROMPT: Close the properties with Cancel. In the settings window choose Input 'S-Video'; wait 5 s.
[PASS] no message box; OBS's picture turned black (same size) within about 1 s?
PROMPT: In the settings window choose Input 'Composite' and Stretch off; wait 8 s.
[PASS] OBS shows the console again?
PROMPT: Close OBS completely; wait 5 s.
$ rate_near 8000
near (8000000)
[PASS] settings stay after OBS closed (VBR about 8000 kbit/s)
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate_mode
video_bitrate_mode: 0 (Variable Bitrate)
[PASS] VBR stays
$ obs_log 20
11:46:24.095: [gchd] picture 720x480, sample aspect ratio 10:11
11:46:43.802: [gchd] properties: device settings read: input 2, output 0, mode 0, 5.00 Mbit/s
11:47:27.371: [gchd] settings applied: input 2, output 0, mode 1, 11.25 Mbit/s, profile 4, stretch 0
11:48:03.301: [gchd] settings applied: input 2, output 0, mode 0, 5.00 Mbit/s, profile 4, stretch 0
11:48:24.624: [gchd] settings applied: input 2, output 0, mode 0, 5.99 Mbit/s, profile 4, stretch 0
11:48:25.227: [gchd] settings applied: input 2, output 0, mode 0, 6.00 Mbit/s, profile 4, stretch 0
11:49:15.640: [gchd] settings applied: input 2, output 1, mode 0, 6.00 Mbit/s, profile 4, stretch 0
11:49:17.998: [gchd] picture 640x480, sample aspect ratio 1:1
11:49:46.555: [gchd] properties: device settings read: input 2, output 1, mode 0, 6.00 Mbit/s
11:49:52.099: [gchd] settings applied: input 0, output 1, mode 0, 6.00 Mbit/s, profile 4, stretch 0
11:49:52.100: [gchd] properties: device settings read: input 0, output 1, mode 1, 10.00 Mbit/s
11:49:52.635: [gchd] no signal: black picture
11:50:12.821: [gchd] settings applied: input 2, output 1, mode 0, 6.00 Mbit/s, profile 4, stretch 0
11:50:47.301: [gchd] picture 640x480, sample aspect ratio 4:3
11:50:58.661: [gchd] picture 720x480, sample aspect ratio 40:33
11:51:27.251: [gchd] properties: device settings read: input 2, output 0, mode 0, 4.00 Mbit/s
11:51:53.967: [gchd] no signal: black picture
11:52:27.516: [gchd] picture 720x480, sample aspect ratio 10:11
11:52:35.489: [gchd] no signal: black picture
11:52:57.494: [gchd] capture stopped (source hidden or settings changed)
--- dmesg step 4
[  639.067734] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  639.224285] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  639.298325] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  639.695196] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[  639.697590] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[  639.697595] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[  639.697597] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[  639.697599] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[  640.652735] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  640.815213] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  640.836877] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  640.837238] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  640.870917] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  640.871540] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  640.872438] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  641.009758] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  645.951421] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=39 bytes=2555904 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  650.954715] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=84 bytes=5505024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  656.060422] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=127 bytes=8323072 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  660.046745] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[  661.159024] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=169 bytes=11075584 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  662.258286] elgato_gchd 1-2:1.0: back-end not settled: status 0xf2
[  662.284074] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  662.284359] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  662.291777] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  662.292529] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[  662.293496] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  662.423674] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  662.423677] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[  667.296485] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=34 bytes=2228224 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  672.356625] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=72 bytes=4718592 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  677.423178] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=111 bytes=7274496 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  682.436828] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=150 bytes=9830400 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  687.468438] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=186 bytes=12189696 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  691.225364] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[  692.518658] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=223 bytes=14614528 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  693.597203] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  693.597688] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  693.607689] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  693.608314] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  693.609170] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  693.703423] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  693.703426] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[  698.618818] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=32 bytes=2097152 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  703.636228] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=72 bytes=4718592 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  705.667518] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[  707.202554] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  707.202901] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  707.208441] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  707.209005] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  707.211437] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  707.310727] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  707.310735] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[  712.230536] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  717.253289] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  722.276700] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  724.479657] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[  726.120777] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  726.121126] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  726.126768] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  726.127220] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[  726.129824] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  726.227104] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  726.227108] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[  731.150551] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  736.174467] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  741.196813] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  741.614516] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[  742.471501] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  742.471837] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  742.481346] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  742.481871] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[  742.483318] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  742.597213] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  742.597217] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[  747.570179] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=32 bytes=2097152 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  752.620758] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=70 bytes=4587520 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  755.356864] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[  757.658371] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  757.660292] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  757.669360] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  757.670489] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  757.671287] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  757.781348] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  757.781350] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[  762.721737] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=41 bytes=2686976 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  762.945723] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[  764.603616] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  764.604005] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  764.611902] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  764.612491] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  764.613131] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  764.710055] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  764.710058] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[  769.685403] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=40 bytes=2621440 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  774.759793] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=86 bytes=5636096 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  779.851400] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=128 bytes=8388608 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  784.876795] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=170 bytes=11141120 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  788.369487] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[  789.935400] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=210 bytes=13762560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  790.411764] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  790.412084] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  790.421969] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  790.424829] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[  790.425371] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  790.551443] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  790.551446] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[  795.514137] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=35 bytes=2293760 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  800.524904] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=76 bytes=4980736 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  805.527189] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=115 bytes=7536640 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  810.539501] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=155 bytes=10158080 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  813.989501] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[  815.594467] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  815.597213] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  815.604277] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  815.604925] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[  815.605976] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  815.717483] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  815.717486] elgato_gchd 1-2:1.0: analog mode now 240p, output 1
[  816.084467] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[  819.589387] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[  820.694757] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=43 bytes=2818048 ep84_bytes=2818048 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  822.232435] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  822.232800] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  822.238704] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  822.239206] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  822.239874] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  822.355319] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  822.355323] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[  822.722066] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[  827.289967] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=54 bytes=3538944 ep84_bytes=3538944 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  832.361745] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=106 bytes=6946816 ep84_bytes=6946816 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  837.368110] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=156 bytes=10223616 ep84_bytes=10223616 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  842.428923] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=198 bytes=12976128 ep84_bytes=12976128 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  847.468828] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=251 bytes=16449536 ep84_bytes=16449536 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  848.829601] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=1 streaming=1
[  850.467036] elgato_gchd 1-2:1.0: input selected: input=0; detection scheduled
[  850.482838] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  850.499060] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  850.781834] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  851.061454] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  851.337443] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  851.613909] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  851.892302] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  852.176550] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  852.460298] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  852.737619] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  853.007298] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  853.289095] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  853.569259] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  853.846636] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  854.125655] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  854.404332] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  854.683138] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  854.967404] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  855.239667] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  855.513368] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  855.789497] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  856.075495] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  856.363909] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  856.641484] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  856.914423] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  857.186470] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  857.462091] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  857.741272] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  858.034900] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  858.311099] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  858.608347] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  858.880597] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  859.156790] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  859.430391] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  859.704672] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  859.981908] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  860.261106] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  860.544404] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  860.816565] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  861.092865] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  861.370097] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  861.645095] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  861.923131] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  862.196850] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  862.471178] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  862.743647] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  863.020907] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  863.301139] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  863.580818] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  863.857659] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  864.131742] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  864.409213] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  864.686773] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  864.964801] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  865.246427] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  865.528235] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  865.798679] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  866.079442] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  866.355067] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  866.639926] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  866.912169] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  867.183460] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  867.455347] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  867.731311] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  868.012019] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  868.287312] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  868.564220] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  868.842921] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  869.126544] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  869.404751] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  869.683052] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  869.967373] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  870.248833] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  870.532295] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  870.828280] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  871.110115] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[  871.187477] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=1 streaming=1
[  871.188409] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[  871.224916] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[  871.506695] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[  871.585741] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[  871.620361] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  871.622524] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[  871.690817] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  871.698061] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[  871.775013] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  871.782000] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[  871.854389] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  871.856654] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[  871.856657] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[  871.856659] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[  871.856681] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[  872.813788] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  872.847284] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  872.847719] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  872.858580] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  872.860334] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[  872.860838] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  872.956595] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  873.213693] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1, settings changed
[  875.709859] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  875.710153] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  875.716938] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  875.719412] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[  875.720181] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  875.832941] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  875.832945] elgato_gchd 1-2:1.0: analog mode now 240p, output 1
[  876.199800] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[  880.744336] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=48 bytes=3145728 ep84_bytes=3145728 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  883.913801] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[  885.828674] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=82 bytes=5373952 ep84_bytes=5373952 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  886.387640] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  886.387995] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  886.395801] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  886.396356] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  886.396916] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  886.527041] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  886.527045] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[  886.893922] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[  891.401151] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=47 bytes=3080192 ep84_bytes=3080192 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  896.404439] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=92 bytes=6029312 ep84_bytes=6029312 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  901.452708] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=142 bytes=9306112 ep84_bytes=9306112 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  903.234287] elgato_gchd 1-2:1.0: encoder restart: output 1 -> 1, settings changed
[  904.992490] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  904.992944] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  905.001205] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  905.001836] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  905.002462] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  905.099165] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  905.099169] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[  905.466292] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[  910.002146] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=37 bytes=2424832 ep84_bytes=2424832 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  913.526166] elgato_gchd 1-2:1.0: encoder restart: output 1 -> 0
[  914.982197] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  914.982540] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  914.991647] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  914.992385] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  914.993266] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  915.121705] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  915.121708] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[  917.025559] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[  919.237555] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  919.237897] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  919.246531] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  919.247388] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[  919.248080] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  919.343586] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  919.343588] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[  924.261160] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=36 bytes=2359296 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  927.215523] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[  928.609176] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  928.609561] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  928.616031] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  928.616970] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[  928.617539] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  928.732534] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  928.732539] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[  933.708711] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=33 bytes=2162688 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  938.812827] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=71 bytes=4653056 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  943.814737] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=109 bytes=7143424 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  948.108100] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[  948.887043] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=141 bytes=9240576 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  950.502753] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  950.503162] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  950.513879] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  950.514600] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  950.517497] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  950.626419] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  950.626424] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[  955.605945] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=41 bytes=2686976 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  960.668660] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=86 bytes=5636096 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  965.676646] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=130 bytes=8519680 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  970.281138] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=1 streaming=1
[  970.764658] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=174 bytes=11403264 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=3
[  971.806361] elgato_gchd 1-2:1.0: input selected: input=3; detection scheduled
[  971.916080] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[  972.002886] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[  999.131579] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=1 streaming=1
[  999.132414] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[  999.394698] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[  999.497538] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[  999.581489] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  999.583848] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[  999.601299] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  999.604317] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[  999.678872] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  999.684826] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[  999.709027] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  999.715090] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[  999.793332] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  999.803391] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[  999.803397] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[  999.803400] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[  999.803401] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[ 1000.718930] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 1000.744862] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1000.745228] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1000.754314] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1000.755116] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 1000.755882] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 1000.870683] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1003.587917] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[ 1005.108449] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1005.109505] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1005.116874] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1005.117373] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 1005.120630] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 1005.250382] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1005.250386] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[ 1010.172673] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=32 bytes=2097152 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1013.379194] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[ 1013.852708] elgato_gchd: input 2 signal lost; scheduling redetection
[ 1014.169292] elgato_gchd 1-2:1.0: signal present: input=2; first video TS packet on PID 0x1011
[ 1015.213872] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=59 bytes=3866624 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1015.603982] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1015.604329] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1015.610147] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1015.610928] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 1015.615171] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 1015.729605] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1015.729607] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[ 1018.770670] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 1018.811349] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 1018.817742] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[ 1018.817748] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 1018.817751] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 1018.817753] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 1019.160758] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 1019.190021] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1019.190309] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1019.200353] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1019.202281] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 1019.203346] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 1019.297744] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1022.034030] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 1022.037774] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[ 1022.037780] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 1022.037784] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 1022.037786] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 1022.319714] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 1022.349865] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1022.350818] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1022.358727] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1022.359228] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 1022.360809] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 1022.472667] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1027.380642] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=38 bytes=2490368 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1032.404681] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=78 bytes=5111808 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1035.860411] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[WARN] dmesg: elgato_gchd: input 2 signal lost; scheduling redetection

=== step 5: stretch SD input: what changes in the stream
PROMPT: The console on composite, showing a picture with a clear shape (a menu, a circle). OBS closed, settings window: Composite, '720x480 NTSC / 720x576 PAL'.
$ stretch_shot 0
width=720 height=480 sample_aspect_ratio=10:11 display_aspect_ratio=15:11 width=720 height=480 sample_aspect_ratio=10:11 display_aspect_ratio=15:11 [h264 @ 0x5c1307759a40] error while decoding MB 39 11, bytestream -8
picture /home/ff5/gchd-test-logs/stretch-0.png
$ stretch_shot 1
width=720 height=480 sample_aspect_ratio=40:33 display_aspect_ratio=20:11 width=720 height=480 sample_aspect_ratio=40:33 display_aspect_ratio=20:11 [h264 @ 0x61513d52bb40] error while decoding MB 21 29, bytestream -14
picture /home/ff5/gchd-test-logs/stretch-1.png
[FAIL] open ~/gchd-test-logs/stretch-0.png and stretch-1.png: does the picture differ in shape or borders (not only the game moving)? Note what differs. -- There are no stretch files created at all.
$ v4l2-ctl -d /dev/elgato-gchd -c stretch_sd_input=0
$ irq_clean
clean
[PASS] no USB interrupt disabled by the kernel
PROMPT: In the settings window choose Bitrate mode Constant (CBR), slider 11.25; close the window.
--- dmesg step 5
[ 1073.386907] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 1073.488296] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 1073.555186] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 1073.678866] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 1073.823508] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 1073.936914] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 1073.980530] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 1073.982741] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[ 1074.068701] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[ 1074.070761] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[ 1074.160353] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[ 1074.163162] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[ 1074.259397] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[ 1074.261912] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[ 1074.261915] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[ 1074.261917] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[ 1074.261920] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[ 1075.045542] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 1076.014040] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 1076.035858] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1076.036212] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1076.042565] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1076.043364] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 1076.043960] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 1076.158524] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1077.340630] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[ 1079.518646] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1079.518856] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1079.524452] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1079.525215] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 1079.525917] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 1079.641393] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1079.641395] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[ 1082.235240] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 1082.355113] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 1082.355115] elgato_gchd 1-2:1.0: waiting 1381 ms after the previous shutdown
[ 1083.967949] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 1084.030105] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 1084.220344] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 1084.376731] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 1084.439360] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 1084.441453] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 1084.549114] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 1084.551273] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 1084.681121] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 1084.684011] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[ 1084.684015] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 1084.684018] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 1084.684021] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 1085.459933] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 1085.578078] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 1085.593268] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1085.593512] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1085.615831] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1085.616549] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 1085.617022] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 1085.732705] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1090.004942] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== summary (T-060-settings, 41b2df0 Kept bitrate shown after a mode change (volatile bitrate control), 480x360 keeps its own bitrate; OBS: black picture without signal, picture/aspect ratio logged; settings window: notifications that disappear after 3 s (T-060))
PASS | step 1 install the driver part and the plugin | no USB interrupt disabled by the kernel (else: reboot first)
PASS | step 1 install the driver part and the plugin | v4l2-compliance: no failures
FAIL | step 1 install the driver part and the plugin | 720x480: CBR 11250 again (expected /video_bitrate: 11250000/, got: video_bitrate: 15000000)
PASS | step 2 window: values, labels, slider, status line | Input Composite, Output size '720x480 NTSC / 720x576 PAL', Constant (CBR), slider at '11.25 Mbit/s', High, Stretch off; no text next to the Close button?
PASS | step 2 window: values, labels, slider, status line | the list is '720x480 NTSC / 720x576 PAL', '640x480', '480x360' (no 'Source size')?
PASS | step 2 window: values, labels, slider, status line | no 'Changing'/'changed' message while you held the slider; 'Bitrate changed.' once after the release, on the line of the Close button, gone after about 3 s?
PASS | step 2 window: values, labels, slider, status line | CBR about 9000 kbit/s
PASS | step 2 window: values, labels, slider, status line | the slider shows the last VBR bitrate, 5.00 Mbit/s (range 3.00 to 7.23)?
PASS | step 2 window: values, labels, slider, status line | VBR 10000 kbit/s remembered
PASS | step 2 window: values, labels, slider, status line | the slider is back at about 9.00 (the last CBR bitrate), not at 11.25?
PASS | step 2 window: values, labels, slider, status line | CBR 9000 kbit/s remembered
PASS | step 2 window: values, labels, slider, status line | the slider stays at 11.25 (not 11.24)?
PASS | step 2 window: values, labels, slider, status line | CBR 11250 kbit/s exactly
PASS | step 2 window: values, labels, slider, status line | 'Output size changed.'; mode Variable (VBR) and locked, slider range 2.00 to 7.50 at 7.50 (480x360 keeps its own bitrate)? -- I believed it opened at 2.00. On a previous test run the only notification I saw was bitrate mode change, so I think all notifications are being sent. The only notification that needs to be sent is the one the user interacts with and not side effects from that change.
PASS | step 2 window: values, labels, slider, status line | 'Output size changed.'; Variable (VBR) at about 4.00 again (the VBR bitrate of the other sizes)?
PASS | step 2 window: values, labels, slider, status line | VBR 8000 kbit/s for 640x480
PASS | step 2 window: values, labels, slider, status line | back at about 2.00 (480x360's own bitrate)?
PASS | step 2 window: values, labels, slider, status line | 480x360 kept 4000 kbit/s
PASS | step 2 window: values, labels, slider, status line | slider at 11.25 again (CBR remembered)?
PASS | step 2 window: values, labels, slider, status line | CBR 11250 remembered
PASS | step 3 groups: composite/S-video share, HDMI/component have their own | S-Video: Output size, VBR about 4.00 and Stretch on unchanged?
PASS | step 3 groups: composite/S-video share, HDMI/component have their own | S-video: VBR kept
PASS | step 3 groups: composite/S-video share, HDMI/component have their own | only Input and H.264 profile are shown (no Output size, Bitrate mode, Bitrate, Stretch); status text on the Close button line?
PASS | step 3 groups: composite/S-video share, HDMI/component have their own | Component also shows H.264 profile Main?
PASS | step 3 groups: composite/S-video share, HDMI/component have their own | Composite: profile High, Variable (VBR) about 4.00, Stretch on, all rows shown again?
PASS | step 3 groups: composite/S-video share, HDMI/component have their own | composite group kept its VBR bitrate
PASS | step 3 groups: composite/S-video share, HDMI/component have their own | composite group kept High
PASS | step 3 groups: composite/S-video share, HDMI/component have their own | HDMI group kept Main
FAIL | step 3 groups: composite/S-video share, HDMI/component have their own | the window now shows Input S-Video and CBR 10.00 by itself (changed from the terminal)? -- Only input changed. Bitrate remained at VBR 5.00.
PASS | step 4 OBS: the same settings, changes in both directions | window: VBR about 10000 kbit/s (5.00 shown)
FAIL | step 4 OBS: the same settings, changes in both directions | plugin read the device's settings (VBR) (expected /properties: device settings read: input 2, output 0, mode 0/, got: 11:46:20.792: [gchd] capture started (source shown) 11:46:24.082: [gchd] capturing from /dev/video0 (video yes, audio yes) 11:46:24.095: [gchd] picture 720x480,)
PASS | step 4 OBS: the same settings, changes in both directions | OBS: Composite, '720x480 NTSC / 720x576 PAL', Variable (VBR), about 5.00, Stretch off; mode list says just 'Constant (CBR)' / 'Variable (VBR)'?
FAIL | step 4 OBS: the same settings, changes in both directions | OBS's slider jumps to 10.00 (the last CBR bitrate), not 11.25? -- It changed to 11.25.
PASS | step 4 OBS: the same settings, changes in both directions | applied while the properties are open (VBR about 12000)
PASS | step 4 OBS: the same settings, changes in both directions | the preview in the properties kept running while you moved the slider (short pause after each stop is fine)?
PASS | step 4 OBS: the same settings, changes in both directions | OBS: VBR about 12000 kbit/s (6.00 shown)
PASS | step 4 OBS: the same settings, changes in both directions | the settings window shows about 6.00 by itself?
PASS | step 4 OBS: the same settings, changes in both directions | OBS's output size reached the device
PASS | step 4 OBS: the same settings, changes in both directions | bitrate not overwritten by the size change
PASS | step 4 OBS: the same settings, changes in both directions | OBS shows the picture at 640x480, and the settings window shows 640x480 by itself?
PASS | step 4 OBS: the same settings, changes in both directions | OBS hides Output size, Stretch, Bitrate mode and Bitrate; the preview turned black (HDMI, no signal)?
PASS | step 4 OBS: the same settings, changes in both directions | Cancel set Composite again
PASS | step 4 OBS: the same settings, changes in both directions | Cancel kept 640x480
PASS | step 4 OBS: the same settings, changes in both directions | Cancel kept the bitrate
PASS | step 4 OBS: the same settings, changes in both directions | window: stretch on
PASS | step 4 OBS: the same settings, changes in both directions | OBS's properties show '720x480 NTSC / 720x576 PAL', about 4.00, Stretch on?
PASS | step 4 OBS: the same settings, changes in both directions | no message box; OBS's picture turned black (same size) within about 1 s?
PASS | step 4 OBS: the same settings, changes in both directions | OBS shows the console again?
PASS | step 4 OBS: the same settings, changes in both directions | settings stay after OBS closed (VBR about 8000 kbit/s)
PASS | step 4 OBS: the same settings, changes in both directions | VBR stays
WARN | step 4 OBS: the same settings, changes in both directions | dmesg: elgato_gchd: input 2 signal lost; scheduling redetection
FAIL | step 5 stretch SD input: what changes in the stream | open ~/gchd-test-logs/stretch-0.png and stretch-1.png: does the picture differ in shape or borders (not only the game moving)? Note what differs. -- There are no stretch files created at all.
PASS | step 5 stretch SD input: what changes in the stream | no USB interrupt disabled by the kernel
