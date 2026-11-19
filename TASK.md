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
25. (Maintainer, 2026-11-19) The settings window applies a bitrate slider
    once, when it is released (users set exact values and may drag for a
    long time). OBS's properties preview changes live: applied 0.5 s
    after the last change (OBS does not report the release).
24. (Maintainer, 2026-11-19) Settings window: the status text on the
    line of the Close button; every change (also output size) reported
    there.

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

- I don't like the note "Changes apply at once and are kept.". This should act more like a notification area. Get rid of the startup message and every notification of change should disappear after a couple of seconds.
- There's an issue with the bitrate slider and how it's settings are remembered across bitrate modes and input groups.
- The stretch function doesn't seem to be doing anything to the N64 source.

gchd-test T-060-settings 20261119-104213 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-060-settings-20261119-104213.log
== build
$ git fetch -q origin
$ git checkout -q feat/app-bridge
$ git pull -q --ff-only
commit: 0e1873d OBS plugin: live preview again, changes applied 0.5 s after the last one, Cancel restores; filled-in kept values not sent, also after a group change (T-060)
commit: 0e1873d OBS plugin: live preview again, changes applied 0.5 s after the last one, Cancel restores; filled-in kept values not sent, also after a group change (T-060)
build rc=0 warnings=0

=== step 1: install the driver part and the plugin
$ irq_clean
clean
[PASS] no USB interrupt disabled by the kernel (else: reboot first)
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
$ v4l2-ctl -d /dev/elgato-gchd -i 2 -v width=720,height=480,pixelformat=MPEG -c h264_profile=4,stretch_sd_input=0
Video input set to 2 (Composite: Camera, no signal)
$ v4l2-ctl -d /dev/elgato-gchd -c video_bitrate_mode=0
$ v4l2-ctl -d /dev/elgato-gchd -c video_bitrate=10000000
$ v4l2-ctl -d /dev/elgato-gchd -c video_bitrate_mode=1
$ v4l2-ctl -d /dev/elgato-gchd -c video_bitrate=11250000
--- dmesg step 1
[ 2156.202479] usbcore: deregistering interface driver elgato_gchd
[ 2169.639797] usbcore: registered new interface driver elgato_gchd
[ 2177.693503] usb 1-2: new high-speed USB device number 4 using xhci_hcd
[ 2178.086228] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[ 2178.086250] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[ 2178.086257] usb 1-2: Product: GameCapture HD
[ 2178.086262] usb 1-2: Manufacturer: Elgato
[ 2178.086267] usb 1-2: SerialNumber: 04107F1267
[ 2178.092375] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[ 2178.095467] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[ 2178.827762] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=0 streaming=0
[ 2178.827808] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 2178.827816] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 2178.827827] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 2178.827830] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 2178.827839] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 2178.827842] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 2178.827851] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 2178.827854] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[ 2178.827873] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 2178.827875] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=0 streaming=0
[ 2178.827884] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 2178.827888] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 2178.827898] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 2178.827901] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 2178.827911] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 2178.827914] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 2178.827924] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 2178.827996] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 2178.828012] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 2178.828019] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 2178.828028] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 2178.828032] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 2178.828041] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 2178.828049] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 2178.828058] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 2178.828067] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 2178.828076] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 2178.828096] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 2178.828105] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 2178.828122] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 2178.828131] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 2178.828135] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 2178.828144] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 2178.828148] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 2178.828157] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 2178.828160] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 2178.828170] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 2178.828172] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 2178.828182] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 2178.828185] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 2178.828195] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 2178.828198] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 2178.828207] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 2178.828221] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 2178.828230] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 2178.828233] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 2178.828242] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 2178.828246] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 2178.828255] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 2178.828260] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 2178.828271] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 2178.830481] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 2178.830500] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 2178.832370] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 2178.832380] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 2178.834117] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 2178.834127] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 2178.836467] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[ 2178.836475] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 2178.836495] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 2178.836500] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 2178.845088] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0

=== step 2: window: values, labels, slider, status line
PROMPT: Open 'Elgato Game Capture HD Settings' from the application menu.
[PASS] Input Composite, Output size '720x480 NTSC / 720x576 PAL', Constant (CBR), slider at '11.25 Mbit/s', High, Stretch off?
[PASS] the status text sits on the same line as the Close button? -- I don't like the note "Changes apply at once and are kept.". This should act more like a notification area. Get rid of the startup message and every notification of change should disappear after a couple of seconds.
PROMPT: Open the Output size list (then close it with Escape).
[PASS] the list is '720x480 NTSC / 720x576 PAL', '640x480', '480x360' (no 'Source size')?
PROMPT: Hold the slider and move it slowly back and forth for about 10 s, ending at about 9.00; watch the status text; then release it.
[PASS] no 'Changing'/'changed' message while you held the slider, 'Bitrate changed.' once after the release? -- As written before, this notification message should disappear after a short while.
PROMPT: Wait 3 s.
$ rate_near 9000
near (9010000)
[PASS] CBR about 9000 kbit/s
PROMPT: Bitrate mode Variable (VBR); wait 3 s.
[FAIL] the slider shows the last VBR bitrate, 5.00 Mbit/s (range 3.00 to 7.23)? -- Upon change it showed 4.50 and not 5.00
$ rate_near 10000
video_bitrate 9420000
[FAIL] VBR 10000 kbit/s remembered (expected /^near/, got: video_bitrate 9420000)
PROMPT: Move the slider to about 4.00; then Bitrate mode Constant (CBR); wait 3 s.
[FAIL] the slider is back at about 9.00 (the last CBR bitrate), not at 11.25? -- No, it's showing 8.00. It seems it's just showing double of the last VBR setting or VBR gets half of CBR setting on change.
$ rate_near 9000
video_bitrate 8000000
[FAIL] CBR 9000 kbit/s remembered (expected /^near/, got: video_bitrate 8000000)
PROMPT: Move the slider fully to the right and release it; wait 3 s.
[PASS] the slider stays at 11.25 (not 11.24)?
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate
video_bitrate: 11250000
[PASS] CBR 11250 kbit/s exactly
PROMPT: Output size 480x360; wait 3 s.
[PASS] status text 'Output size changed.'; mode Variable (VBR) and locked, slider 2.00 to 7.50?
PROMPT: Output size 640x480; wait 3 s.
[FAIL] status text 'Output size changed.'; mode still Variable (VBR) at about 4.00? -- No, before changing to 640x480 I set 480x360 to 2.00 and when changing to 640x480 it opened also with VBR 2.00
PROMPT: Output size '720x480 NTSC / 720x576 PAL'; Bitrate mode Constant (CBR); wait 3 s.
[FAIL] slider at 11.25 again (CBR remembered)? -- I think the slider is just remembering it's global position and not output size and input group dependant.
--- dmesg step 2


=== step 3: groups: composite/S-video share, HDMI/component have their own
PROMPT: Stretch on; Bitrate mode Variable (VBR). Then Input S-Video; wait 5 s.
[PASS] S-Video: Output size, VBR about 4.00 and Stretch on unchanged? -- The only problem is still the bitrate slider position.
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate_mode
video_bitrate_mode: 0 (Variable Bitrate)
[PASS] S-video: VBR kept
PROMPT: Input HDMI; wait 5 s.
[PASS] only Input and H.264 profile are shown (no Output size, Bitrate mode, Bitrate, Stretch); status text on the Close button line?
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate_mode,video_bitrate,h264_profile
video_bitrate_mode: 1 (Constant Bitrate)
video_bitrate: 11250000
h264_profile: 4 (High)
PROMPT: H.264 profile Main (HDMI). Then Input Component; wait 5 s.
[PASS] Component also shows H.264 profile Main?
PROMPT: Input Composite; wait 5 s.
[PASS] Composite: profile High, Variable (VBR) about 4.00, Stretch on, all rows shown again? -- Still the same problem with the bitrate slider.
$ rate_near 8000
video_bitrate 4000000
[FAIL] composite group kept its VBR bitrate (expected /^near/, got: video_bitrate 4000000)
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
[PASS] the window now shows Input S-Video and CBR 10.00 by itself (changed from the terminal)?
$ v4l2-ctl -d /dev/elgato-gchd -i 2 -c stretch_sd_input=0
Video input set to 2 (Composite: Camera, no signal)
--- dmesg step 3
[ 3014.769082] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 3014.769161] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 3076.429915] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[ 3076.429987] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 3109.361096] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[ 3109.361187] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[ 3125.874717] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[ 3125.874928] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 3156.810770] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=0 streaming=0
[ 3156.810797] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[ 3156.818362] elgato_gchd 1-2:1.0: input switch requested: old=0 new=3 hw_initialized=0 streaming=0
[ 3156.818378] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 3185.636767] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[ 3185.636819] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== step 4: OBS: the same settings, changes in both directions
PROMPT: In the settings window: Bitrate mode Variable (VBR), slider 5.00.
$ rate_near 10000
near (10000000)
[PASS] window: VBR about 10000 kbit/s (5.00 shown)
PROMPT: Close OBS if open. Start OBS (the scene with the source 'Elgato Game Capture HD'); open the source's properties.
$ obs_log 5
11:00:00.651: [gchd] capture started (source shown)
11:00:03.641: [gchd] capturing from /dev/video0 (video yes, audio yes)
11:00:04.783: [gchd] properties: device settings read: input 2, output 0, mode 0, 5.00 Mbit/s
$ obs_log 5
11:00:00.651: [gchd] capture started (source shown)
11:00:03.641: [gchd] capturing from /dev/video0 (video yes, audio yes)
11:00:04.783: [gchd] properties: device settings read: input 2, output 0, mode 0, 5.00 Mbit/s
[PASS] plugin read the device's settings (VBR)
[PASS] OBS: Composite, '720x480 NTSC / 720x576 PAL', Variable (VBR), about 5.00, Stretch off; mode list says just 'Constant (CBR)' / 'Variable (VBR)'?
PROMPT: In OBS's properties choose Bitrate mode Constant (CBR).
[PASS] OBS's slider jumps to 10.00 (the last CBR bitrate), not 11.25?
PROMPT: Back to Variable (VBR); hold the slider and move it back and forth for about 10 s, ending at about 6.00; release; wait 3 s (properties still open).
$ rate_near 12000
near (12040000)
[PASS] applied while the properties are open (VBR about 12000)
$ obs_log 6
11:00:16.027: [gchd] properties: device settings read: input 2, output 0, mode 0, 5.00 Mbit/s
11:00:52.643: [gchd] properties: device settings read: input 2, output 0, mode 0, 5.00 Mbit/s
11:00:57.245: [gchd] settings applied: input 2, output 0, mode 1, 11.25 Mbit/s, profile 4, stretch 0
11:00:57.246: [gchd] properties: device settings read: input 2, output 0, mode 1, 10.00 Mbit/s
11:01:42.347: [gchd] settings applied: input 2, output 0, mode 0, 5.00 Mbit/s, profile 4, stretch 0
11:01:54.217: [gchd] settings applied: input 2, output 0, mode 0, 6.02 Mbit/s, profile 4, stretch 0
[PASS] the preview in the properties kept running while you moved the slider (short pause after each stop is fine)?
PROMPT: Press OK. Wait 3 s.
$ rate_near 12000
near (12040000)
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
near (12040000)
[PASS] bitrate not overwritten by the size change
[PASS] OBS shows the picture at 640x480, and the settings window shows 640x480 by itself?
PROMPT: In OBS's properties choose Input HDMI (do not press OK); wait 3 s.
[PASS] OBS hides Output size, Stretch, Bitrate mode and Bitrate; the preview shows HDMI (no signal)? -- Video output froze on the last received fram from GCHD. When changing input to one with no source I would expect a black screen the size of the output.
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
video_bitrate 11250000
[FAIL] Cancel kept the bitrate (expected /^near/, got: video_bitrate 11250000)
PROMPT: In the settings window: Stretch on, Output size '720x480 NTSC / 720x576 PAL', slider 4.00; wait 8 s.
$ v4l2-ctl -d /dev/elgato-gchd -C stretch_sd_input
stretch_sd_input: 1
[PASS] window: stretch on
PROMPT: In OBS open the source's properties again.
[PASS] OBS's properties show '720x480 NTSC / 720x576 PAL', about 4.00, Stretch on?
PROMPT: Close the properties with Cancel. In the settings window choose Input 'S-Video'; wait 5 s.
[PASS] no message box, OBS's picture went to S-Video (no signal) at once? -- Output still freezes at the last received frame.
PROMPT: In the settings window choose Input 'Composite' and Stretch off; wait 8 s.
[PASS] OBS shows the console again? -- I get the impression that stretch is doing nothing to the output. The image doesn't change.
PROMPT: Close OBS completely; wait 5 s.
$ rate_near 8000
near (8060000)
[PASS] settings stay after OBS closed (VBR about 8000 kbit/s)
$ v4l2-ctl -d /dev/elgato-gchd -C video_bitrate_mode
video_bitrate_mode: 0 (Variable Bitrate)
[PASS] VBR stays
$ obs_log 20
11:00:04.783: [gchd] properties: device settings read: input 2, output 0, mode 0, 5.00 Mbit/s
11:00:16.027: [gchd] properties: device settings read: input 2, output 0, mode 0, 5.00 Mbit/s
11:00:52.643: [gchd] properties: device settings read: input 2, output 0, mode 0, 5.00 Mbit/s
11:00:57.245: [gchd] settings applied: input 2, output 0, mode 1, 11.25 Mbit/s, profile 4, stretch 0
11:00:57.246: [gchd] properties: device settings read: input 2, output 0, mode 1, 10.00 Mbit/s
11:01:42.347: [gchd] settings applied: input 2, output 0, mode 0, 5.00 Mbit/s, profile 4, stretch 0
11:01:54.217: [gchd] settings applied: input 2, output 0, mode 0, 6.02 Mbit/s, profile 4, stretch 0
11:03:13.214: [gchd] properties: device settings read: input 2, output 0, mode 0, 6.02 Mbit/s
11:03:16.232: [gchd] settings applied: input 2, output 1, mode 0, 6.02 Mbit/s, profile 4, stretch 0
11:03:38.609: [gchd] properties: device settings read: input 2, output 1, mode 0, 6.02 Mbit/s
11:03:45.931: [gchd] settings applied: input 0, output 1, mode 0, 6.02 Mbit/s, profile 4, stretch 0
11:03:45.933: [gchd] properties: device settings read: input 0, output 1, mode 1, 11.25 Mbit/s
11:03:55.846: [gchd] properties: device settings read: input 0, output 1, mode 1, 11.25 Mbit/s
11:05:48.488: [gchd] settings applied: input 2, output 1, mode 0, 6.02 Mbit/s, profile 4, stretch 0
11:05:48.488: [gchd] properties: device settings read: input 2, output 1, mode 1, 12.04 Mbit/s
11:05:54.777: [gchd] properties: device settings read: input 2, output 1, mode 1, 12.04 Mbit/s
11:05:58.592: [gchd] settings applied: input 0, output 1, mode 1, 11.25 Mbit/s, profile 4, stretch 0
11:06:03.924: [gchd] settings applied: input 2, output 1, mode 1, 11.25 Mbit/s, profile 4, stretch 0
11:07:34.546: [gchd] properties: device settings read: input 2, output 0, mode 0, 4.03 Mbit/s
11:11:02.499: [gchd] capture stopped (source hidden or settings changed)
$ irq_clean
clean
[PASS] no USB interrupt disabled by the kernel
PROMPT: In the settings window choose Bitrate mode Constant (CBR), slider 11.25; close the window.
--- dmesg step 4
[ 3576.238345] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3576.495592] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1, settings changed
[ 3578.165860] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3578.166115] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3578.172841] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3578.173354] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 3578.173907] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3578.272339] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3578.272343] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[ 3578.640265] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[ 3583.198801] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3588.231022] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=14417920 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3591.686265] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[ 3593.545955] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3593.546320] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3593.554641] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3593.555413] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 3593.556086] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3593.685299] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3593.685302] elgato_gchd 1-2:1.0: analog mode now 240p, output 1
[ 3594.056139] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[ 3598.568448] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3603.600809] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3605.373577] elgato_gchd 1-2:1.0: encoder restart: output 1 -> 0
[ 3606.228488] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3606.228746] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3606.238205] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3606.238935] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 3606.239485] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3606.351136] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3606.351140] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[ 3611.273136] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3616.296991] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3621.174638] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[ 3621.319310] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3622.028913] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3622.029294] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3622.035942] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3622.036495] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 3622.037503] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3622.168665] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3622.168669] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[ 3622.705955] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[ 3624.318942] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3624.319314] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3624.328858] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3624.329486] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 3624.331188] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3624.441772] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3624.441776] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[ 3629.363119] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3634.385449] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3638.323322] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[ 3639.115455] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3639.115936] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3639.128819] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3639.129623] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 3639.133447] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3639.222197] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3639.222200] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[ 3642.162987] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[ 3643.233944] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3643.234256] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3643.245603] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3643.246411] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 3643.248680] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3643.357010] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3643.357015] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[ 3648.282016] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=67 bytes=4390912 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3649.866164] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[ 3650.706120] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3650.706475] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3650.712952] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3650.713716] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 3650.715652] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3650.846066] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3650.846070] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[ 3655.739366] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=76 bytes=4980736 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3656.834044] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[ 3658.395472] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3658.395829] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3658.401691] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3658.402317] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 3658.403060] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3658.518223] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3658.518226] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[ 3663.413910] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=76 bytes=4980736 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3668.451866] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=160 bytes=10485760 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3673.486830] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=244 bytes=15990784 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3678.523098] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=328 bytes=21495808 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3683.559440] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=412 bytes=27000832 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3687.734843] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[ 3688.596151] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=496 bytes=32505856 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3689.394815] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3689.395093] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3689.405703] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3689.406237] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 3689.407500] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3689.503147] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3689.503151] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[ 3694.457203] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=77 bytes=5046272 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3694.482029] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=1 streaming=1
[ 3695.262229] elgato_gchd 1-2:1.0: input selected: input=3; detection scheduled
[ 3695.382213] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 3695.448989] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 3737.779733] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=1 streaming=1
[ 3737.780967] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[ 3738.038256] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 3738.140865] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 3738.248007] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 3738.251071] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[ 3738.270328] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 3738.272625] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[ 3738.343404] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 3738.347078] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[ 3738.426788] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 3738.430949] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[ 3738.430954] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[ 3738.430958] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[ 3738.430959] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[ 3739.418859] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 3739.449268] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3739.449604] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3739.455754] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3739.456915] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 3739.460283] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3739.556535] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3741.119715] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[ 3741.948936] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3741.949667] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3741.957231] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3741.958269] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 3741.958924] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3742.055197] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3742.055200] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[ 3747.010143] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=77 bytes=5046272 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3748.418581] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[ 3749.236736] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3749.237004] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3749.246872] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3749.247445] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 3749.248099] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3749.343617] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3749.343627] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[ 3751.727951] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[ 3753.657172] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3753.657539] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3753.663167] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3753.666563] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 3753.667219] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3753.783082] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3753.783090] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[ 3758.392881] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[ 3758.674631] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=76 bytes=4980736 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3759.357863] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3759.360613] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3759.369494] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3759.370380] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 3759.371347] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3759.480805] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3759.480808] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[ 3764.377466] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=76 bytes=4980736 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3765.156237] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[ 3766.187105] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3766.187422] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3766.194923] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3766.195986] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 3766.196817] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3766.293229] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3766.293231] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[ 3766.677220] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[ 3769.550988] elgato_gchd 1-2:1.0: encoder restart: output 1 -> 1, settings changed
[ 3770.507537] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3770.508034] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3770.515506] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3770.516093] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 3770.517289] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3770.630678] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3770.630682] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[ 3771.016391] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[ 3775.530961] elgato_gchd 1-2:1.0: encoder restart: output 1 -> 1, settings changed
[ 3775.533717] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=76 bytes=4980736 ep84_bytes=4980736 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3776.440104] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3776.440352] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3776.446676] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3776.447377] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 3776.447963] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3776.562864] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3776.562867] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[ 3776.950206] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[ 3779.478827] elgato_gchd 1-2:1.0: encoder restart: output 1 -> 1, settings changed
[ 3780.300307] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3780.300733] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3780.307851] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3780.308473] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 3780.309125] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3780.408818] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3780.408821] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[ 3780.789991] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[ 3782.804418] elgato_gchd 1-2:1.0: encoder restart: output 1 -> 1, settings changed
[ 3783.537735] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3783.538078] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3783.549276] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3783.549872] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 3783.551570] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3783.677318] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3783.677323] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[ 3784.061979] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[ 3785.051102] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[ 3786.738252] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3786.739018] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3786.747922] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3786.748571] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 3786.749160] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3786.844620] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3786.844622] elgato_gchd 1-2:1.0: analog mode now 240p, output 1
[ 3787.168414] elgato_gchd 1-2:1.0: encoder restart: output 1 -> 1, settings changed
[ 3787.228376] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[ 3788.634952] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3788.635398] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3788.644718] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3788.645368] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 3788.646500] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3788.759153] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3788.759157] elgato_gchd 1-2:1.0: analog mode now 240p, output 1
[ 3789.141852] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[ 3790.125473] elgato_gchd 1-2:1.0: encoder restart: output 1 -> 1, settings changed
[ 3791.178285] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3791.178778] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3791.186652] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3791.187202] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 3791.187785] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3791.286391] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3791.286393] elgato_gchd 1-2:1.0: analog mode now 240p, output 1
[ 3791.668621] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[ 3795.688060] elgato_gchd 1-2:1.0: encoder restart: output 1 -> 0
[ 3796.278523] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=74 bytes=4849664 ep84_bytes=4849664 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3796.665706] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3796.666110] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3796.675691] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3796.676410] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 3796.677440] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3796.773484] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3796.773487] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[ 3799.727830] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[ 3800.566630] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3800.566882] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3800.572753] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3800.573416] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 3800.574740] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3800.691535] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3800.691541] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[ 3803.704863] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 0, settings changed
[ 3804.437382] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3804.437952] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3804.449576] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3804.450067] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 3804.450897] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3804.564566] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3804.564572] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[ 3809.457998] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=76 bytes=4980736 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3814.495274] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=160 bytes=10485760 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3815.992252] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[ 3817.602069] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3817.602303] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3817.608377] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3817.609443] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 3817.609945] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3817.710121] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3817.710125] elgato_gchd 1-2:1.0: analog mode now 480i, output 0
[ 3822.663663] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=77 bytes=5046272 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3827.700022] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=161 bytes=10551296 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3832.738830] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=245 bytes=16056320 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3837.774562] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=329 bytes=21561344 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3842.809647] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=413 bytes=27066368 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3847.844220] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=497 bytes=32571392 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3849.215051] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[ 3850.698462] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 3850.699008] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3850.710064] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3850.710884] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 3850.712306] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3850.821620] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 3850.821622] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[ 3855.718793] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=76 bytes=4980736 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3860.753712] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=160 bytes=10485760 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3865.789797] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=244 bytes=15990784 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3870.825703] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=328 bytes=21495808 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 3873.667897] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== summary (T-060-settings, 0e1873d OBS plugin: live preview again, changes applied 0.5 s after the last one, Cancel restores; filled-in kept values not sent, also after a group change (T-060))
PASS | step 1 install the driver part and the plugin | no USB interrupt disabled by the kernel (else: reboot first)
PASS | step 1 install the driver part and the plugin | v4l2-compliance: no failures
PASS | step 2 window: values, labels, slider, status line | Input Composite, Output size '720x480 NTSC / 720x576 PAL', Constant (CBR), slider at '11.25 Mbit/s', High, Stretch off?
PASS | step 2 window: values, labels, slider, status line | the status text sits on the same line as the Close button? -- I don't like the note "Changes apply at once and are kept.". This should act more like a notification area. Get rid of the startup message and every notification of change should disappear after a couple of seconds.
PASS | step 2 window: values, labels, slider, status line | the list is '720x480 NTSC / 720x576 PAL', '640x480', '480x360' (no 'Source size')?
PASS | step 2 window: values, labels, slider, status line | no 'Changing'/'changed' message while you held the slider, 'Bitrate changed.' once after the release? -- As written before, this notification message should disappear after a short while.
PASS | step 2 window: values, labels, slider, status line | CBR about 9000 kbit/s
FAIL | step 2 window: values, labels, slider, status line | the slider shows the last VBR bitrate, 5.00 Mbit/s (range 3.00 to 7.23)? -- Upon change it showed 4.50 and not 5.00
FAIL | step 2 window: values, labels, slider, status line | VBR 10000 kbit/s remembered (expected /^near/, got: video_bitrate 9420000)
FAIL | step 2 window: values, labels, slider, status line | the slider is back at about 9.00 (the last CBR bitrate), not at 11.25? -- No, it's showing 8.00. It seems it's just showing double of the last VBR setting or VBR gets half of CBR setting on change.
FAIL | step 2 window: values, labels, slider, status line | CBR 9000 kbit/s remembered (expected /^near/, got: video_bitrate 8000000)
PASS | step 2 window: values, labels, slider, status line | the slider stays at 11.25 (not 11.24)?
PASS | step 2 window: values, labels, slider, status line | CBR 11250 kbit/s exactly
PASS | step 2 window: values, labels, slider, status line | status text 'Output size changed.'; mode Variable (VBR) and locked, slider 2.00 to 7.50?
FAIL | step 2 window: values, labels, slider, status line | status text 'Output size changed.'; mode still Variable (VBR) at about 4.00? -- No, before changing to 640x480 I set 480x360 to 2.00 and when changing to 640x480 it opened also with VBR 2.00
FAIL | step 2 window: values, labels, slider, status line | slider at 11.25 again (CBR remembered)? -- I think the slider is just remembering it's global position and not output size and input group dependant.
PASS | step 3 groups: composite/S-video share, HDMI/component have their own | S-Video: Output size, VBR about 4.00 and Stretch on unchanged? -- The only problem is still the bitrate slider position.
PASS | step 3 groups: composite/S-video share, HDMI/component have their own | S-video: VBR kept
PASS | step 3 groups: composite/S-video share, HDMI/component have their own | only Input and H.264 profile are shown (no Output size, Bitrate mode, Bitrate, Stretch); status text on the Close button line?
PASS | step 3 groups: composite/S-video share, HDMI/component have their own | Component also shows H.264 profile Main?
PASS | step 3 groups: composite/S-video share, HDMI/component have their own | Composite: profile High, Variable (VBR) about 4.00, Stretch on, all rows shown again? -- Still the same problem with the bitrate slider.
FAIL | step 3 groups: composite/S-video share, HDMI/component have their own | composite group kept its VBR bitrate (expected /^near/, got: video_bitrate 4000000)
PASS | step 3 groups: composite/S-video share, HDMI/component have their own | composite group kept High
PASS | step 3 groups: composite/S-video share, HDMI/component have their own | HDMI group kept Main
PASS | step 3 groups: composite/S-video share, HDMI/component have their own | the window now shows Input S-Video and CBR 10.00 by itself (changed from the terminal)?
PASS | step 4 OBS: the same settings, changes in both directions | window: VBR about 10000 kbit/s (5.00 shown)
PASS | step 4 OBS: the same settings, changes in both directions | plugin read the device's settings (VBR)
PASS | step 4 OBS: the same settings, changes in both directions | OBS: Composite, '720x480 NTSC / 720x576 PAL', Variable (VBR), about 5.00, Stretch off; mode list says just 'Constant (CBR)' / 'Variable (VBR)'?
PASS | step 4 OBS: the same settings, changes in both directions | OBS's slider jumps to 10.00 (the last CBR bitrate), not 11.25?
PASS | step 4 OBS: the same settings, changes in both directions | applied while the properties are open (VBR about 12000)
PASS | step 4 OBS: the same settings, changes in both directions | the preview in the properties kept running while you moved the slider (short pause after each stop is fine)?
PASS | step 4 OBS: the same settings, changes in both directions | OBS: VBR about 12000 kbit/s (6.00 shown)
PASS | step 4 OBS: the same settings, changes in both directions | the settings window shows about 6.00 by itself?
PASS | step 4 OBS: the same settings, changes in both directions | OBS's output size reached the device
PASS | step 4 OBS: the same settings, changes in both directions | bitrate not overwritten by the size change
PASS | step 4 OBS: the same settings, changes in both directions | OBS shows the picture at 640x480, and the settings window shows 640x480 by itself?
PASS | step 4 OBS: the same settings, changes in both directions | OBS hides Output size, Stretch, Bitrate mode and Bitrate; the preview shows HDMI (no signal)? -- Video output froze on the last received fram from GCHD. When changing input to one with no source I would expect a black screen the size of the output.
PASS | step 4 OBS: the same settings, changes in both directions | Cancel set Composite again
PASS | step 4 OBS: the same settings, changes in both directions | Cancel kept 640x480
FAIL | step 4 OBS: the same settings, changes in both directions | Cancel kept the bitrate (expected /^near/, got: video_bitrate 11250000)
PASS | step 4 OBS: the same settings, changes in both directions | window: stretch on
PASS | step 4 OBS: the same settings, changes in both directions | OBS's properties show '720x480 NTSC / 720x576 PAL', about 4.00, Stretch on?
PASS | step 4 OBS: the same settings, changes in both directions | no message box, OBS's picture went to S-Video (no signal) at once? -- Output still freezes at the last received frame.
PASS | step 4 OBS: the same settings, changes in both directions | OBS shows the console again? -- I get the impression that stretch is doing nothing to the output. The image doesn't change.
PASS | step 4 OBS: the same settings, changes in both directions | settings stay after OBS closed (VBR about 8000 kbit/s)
PASS | step 4 OBS: the same settings, changes in both directions | VBR stays
PASS | step 4 OBS: the same settings, changes in both directions | no USB interrupt disabled by the kernel
