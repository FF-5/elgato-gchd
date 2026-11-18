# TODO — roadmap

**Development-only file** (never merged to `master`, RULES B2).

Goal: a DKMS/V4L2 driver (C, `dkms/`) that behaves like the Windows driver for
every input and mode the hardware supports. Our own USB recordings are the
reference (`traces` branch); `src/` is read-only. Each open item becomes one
`TASK.md` and one feature branch off `dev`.

**Order (maintainer, 2026-11-18):** finish composite and S-video completely
(all modes, mode and input changes, output sizes, bitrate and the other
settings); then the repository layout and installation scripts; then
guvcview/OBS support; then component the same way; then HDMI.

Legend: **[HW]** maintainer's hardware, **[WIN]** new Windows recording,
**[OFF]** offline, **(S/M/L)** size. Test results: `logs/hardware-test-log.md`
on the `traces` branch.

## Current state

- `dev` (0.5.3): composite and S-video, NTSC 480i/240p and PAL 576i/288p,
  fresh start, mode changes while capturing (NTSC ~1.5 s with encoder
  restart, PAL ~0.5 s decoder-only), 60 ↔ 50 Hz changes while capturing
  (console reset, power cycle, console swap; ~1-5 s, `docs/composite.md
  §8.3`), composite ↔ S-video without unplugging, close and reopen
  (also right after a close and after a session without a source),
  STREAMOFF → STREAMON on one handle. V4L2 reporting: standard (NTSC-M /
  PAL, QUERYSTD/G_STD), 720×480/576 by source, 29.97 / 25 fps,
  interlaced top field first; input changes from other processes refused
  while one captures. Output sizes 720×480/576 (interlaced), 640×480 and
  480×360 (progressive, transport output 2 on endpoint 0x84).
  Formats: H.264 (video only) and MPEG-TS with MP2 audio (`read()`,
  `ffplay -f mpegts /dev/video0`). Sources: N64 (NTSC), PS1 (PAL; the decoder
  reports 60 Hz for a few seconds after power-on and reset, also with the
  Windows driver; the driver waits for the 50 Hz reading).
- Test setup: VirtualBox with the USB 3.0 (xHCI) controller.

## Stage 1 — Composite and S-video complete

Video modes and switching:

- [ ] **T-054 [HW] (S)** Source loss and return while capturing (worked by
      report; check the logs once).
- [ ] **T-065 [HW] (S)** Console swap without a detected signal drop
      (N64 → running PS1: `0x88:FA` stayed `6`): on S-video one restart of
      ~7 s, on composite two restarts (240p, then 576i). Treat `E7` bit 7
      with an unlocked `CA` like a drop (wait for 50 Hz).
- [ ] **T-067 [HW] (S)** Firmware start-up failed twice in the T-055
      tests (`state-change timeout: current=0x10 expected=0x11 state=0x0011
      completion=0x0008`, IDLE completes without the completion flag), each
      after a session that ended with a failed START. Check whether it also
      happens after normal sessions; treat state 0x11 without the flag as
      IDLE reached.
- [ ] 288p capture start (recording R1 wanted; the driver uses the apply block
      of the 576i → 288p change).

Output and settings (`docs/composite.md §10`):

- [ ] **T-061 [OFF→HW] (M)** Bitrate control as recorded (CBR at the
      maximum, VBR with `v_ave_bitrate` = displayed value below it; ranges
      per output size), "stretch" (`v_in_source_type 2`), H.264
      profile/level/GOP if exposed. V4L2 controls; each change restarts the
      encoder like the recordings.
- [ ] **T-066 [OFF→HW] (M)** v4l2-compliance 1.26.1 failures left after
      4c326eb: HDMI/component `G_DV_TIMINGS` fails without a signal although
      `V4L2_IN_CAP_DV_TIMINGS` is set, `VIDIOC_DV_TIMINGS_CAP` missing
      (also breaks the buffer tests via `g_dv_timings`); a menu control
      rejects item 0 ("could not set valid menu item 0", all inputs);
      `V4L2_CID_DV_RX_POWER_PRESENT` missing (warning). The input menu
      control still switches inputs for any process while another one
      captures (S_INPUT returns EBUSY since fd8426b). Needed for
      guvcview/OBS robustness (T-060).
- [ ] **T-064 (M)** Input persistence: HDMI on first use after
      installation, afterwards the last selected input, also across module
      reloads and reboots (today only while the module stays loaded). Plan:
      uevent on input change, a new udev rule of ours stores it, module
      parameter `default_input` set from it at load time.

Stage 1 is done when composite and S-video, NTSC and PAL, all modes and
settings above work on `dev`.

## Repository layout and installation (after stage 1)

- [ ] **T-070 (M)** All code under `src/` (agreed 2026-11-18):
      `src/kernel/` (today's `dkms/`), `src/userspace/` (today's
      `src/*.cpp`, `src/gchd/`, `src/gui/`, `src/CMakeLists.txt`). The
      maintainer allows changing the source paths in the top-level
      `CMakeLists.txt` (paths only). DKMS paths and docs updated. One
      move-only branch, module checked identical with `compare-objs.sh`.
- [ ] **T-071 (M)** Scripts for non-technical users, runnable from a file
      manager (executable, with a terminal window; `pkexec` for root):
      - `firmware/` (renamed from `firmware_extract/`), bash: extracts the
        four firmware files from the official installer
        (`GameCaptureSetup*.msi`). Optional download of that installer from
        Elgato's own server after the user confirms, with a link to
        Elgato's terms; no mirroring or redistribution of the installer or
        firmware by the project; checksum check; URL kept in one place for
        updates; manual download stays as the fallback.
      - **Maintainer: e-mail Elgato (Corsair) and ask whether the script
        may download the official installer from their server and extract
        the firmware for use with this driver.** Decide on the download
        option after the answer; until then the script only extracts from
        an installer the user downloaded.
      - `install-driver.sh` at the top: installs the DKMS module (and the
        firmware step if missing), with an uninstall option.
      - `build-program.sh` at the top: builds the userspace program.

## Stage 1b — Applications (guvcview, OBS) (after T-070/T-071)

- [ ] **T-060 (L)** Make the device usable in guvcview, OBS and similar
      applications. A USB device cannot be turned into a UVC device by a
      driver: UVC is a property of the device's own USB protocol. What
      these applications need is a V4L2 device with uncompressed frames
      (YUYV/NV12) and a normal audio source. Decoding H.264 is not allowed
      in the kernel (RULES E2), so the plan is a small userspace service:
      it reads the H.264/TS node, decodes (ffmpeg/GStreamer), deinterlaces
      480i/576i and line-doubles 240p/288p, and feeds a `v4l2loopback`
      device named "Elgato Game Capture HD" plus a PipeWire/PulseAudio
      audio source; started automatically when an application opens it.
      The service keeps one output size for the whole session (scaling
      and aspect correction when the source changes between 480 and 576
      lines or between interlaced and progressive), so applications never
      see a format change (today ffplay keeps the window size of the first
      frames and shows later modes squeezed).
      Uses the MPEG-TS format with audio (done, 0.5.0).

## Stage 2 — Component

- [ ] **T-051 [WIN+HW] (L)** Component, all modes (480i, 480p, 576i,
      576p, 720p, 1080i), mode changes, output sizes and settings, as for
      stage 1. The driver still uses the C++ blocks; no recording with a
      signal yet.

## Stage 3 — HDMI

- [ ] **T-050 [WIN+HW] (L)** HDMI, all modes, mode changes, output sizes
      and settings. Recorded so far: `hdmi/gchd_output_switch_hdmi` (1080p60
      source, output sizes; `docs/hdmi.md §4`).

## Stage 4 — Picture and audio settings (after HDMI, after the release)

- [ ] **T-080 [WIN+HW] (M)** Picture settings of the Windows application
      (brightness, contrast, saturation, hue, sharpness, as offered) and
      audio settings (volume/gain, audio input selection, as offered). First
      find out for each whether it is sent to the device or applied by the
      application: one Windows recording per setting (reduced with the
      bulk-OUT filter), changing it while capturing. Settings that reach the
      device become V4L2 controls (`V4L2_CID_BRIGHTNESS`, …, audio via the
      TS stream); application-side settings are not driver features.

## Later

- [ ] **T-062 (S)** Original-HD family (`0044/004e/0051`): needs a tester.
- [ ] **T-063 (S)** Versioning and `CHANGELOG.md` for `master` releases.

## Code and tooling

- [ ] **T-002 [OFF] (M)** File split ("Planned layout" in `dkms/README.md`),
      fold the `*_exact` wrappers; each move checked with
      `compare-mailbox-ops.py`. (Together with or after T-070.)
- [ ] **T-003 [OFF] (S)** Make `vb2_fops` static.
- [ ] **T-004 [OFF] (S)** `0x1004` "stout_mode": the defaults write bit 0;
      check against the initialization recording.
- [ ] **T-005b [OFF] (S)** Code comments still name `configureDevice()`
      (RULES E10); comments only, verify code-identical.
- [ ] **T-010 [OFF] (M)** usbmon decoder on the `tools` branch, same output
      as the pcapng decoder, to diff driver traffic against Windows.
- [ ] **T-011 [OFF] (M)** Remaining recording analysis: initialization
      (`filtered_init_switch_to_component`, `gchd_win_trace_no_input`),
      repeated `0x88:02/3C` and `0x94` writes after detection.
- [ ] **T-012 [OFF] (S)** debugfs raw bulk-IN dump behind a module parameter
      (reference: `tools/archive/debug-trace.c` on the `tools` branch).
- [ ] Branches to delete on GitHub (refused from the development session):
      `fix/mode-change-restart`, `feat/svideo`, `feat/pal`,
      `fix/pal-svideo`, `feat/component` (superseded; component comes
      after the restructure),
      `async-bulk-in-queue`, `cherry3`, `debug`, `debug-saved-enable-state`,
      `diagnostic-input-baseline`, `state-machine`, `v4l2-dkms-driver`,
      `v4l2-h264-access-units`, `video-output-lazy-init`.

## Known differences between the driver and the recordings

Change one at a time (RULES D5), when the mode can be tested.

| Item | Driver | Recordings |
|---|---|---|
| `system_rate` | composite/S-video as recorded (T-061); HDMI/component `1.075 × (bitrate + 320) + 256` | `⌊1.07285 × bitrate + 461.7⌋` (PAL; NTSC +8) |
| HDMI/component encoder and back end | C++ blocks (`65`/`64` pulses, `0900:0018 = 00 0A`, `v_vinpelclk = 0`, `time_scale_l = 0`, `num_units_in_tick = 0`, GOP 14, bitrate formula) | `05`/`15` pulses, `02 0A`, `v_vinpelclk = 1`, `time_scale_l = 60`, `num_units_in_tick = 1`, GOP 8, bitrate per output size |
| Composite prefix | `02 = C9 … 28 = 88` written on the first composite start after loading the module | written only after S-video (HDMI → composite writes none of them) |
| Initial analog detection | waits until `0x88:CA` reads `7E` or `66`; family re-checked before START | unknown (no recording of a 480i start) |
| Receive queue at restart | URBs queued after INIT A0, before START | queued before the first encoder write |
| Bulk IN `0x84` | required (`-ENODEV` if absent); parsed while transport output 2 runs | queued (8 URBs); carries transport output 2 (scaled outputs) |
| Repeated `0x88:02/3C`, `0x94` writes after detection | written | not written (T-011) |

## Evidence wanted (maintainer)

All Windows recordings still useful, in the order they are needed. Record
with USBPcap, reduce with the filter that keeps bulk OUT, upload to the
top of the `traces` branch; I sort and decode them. One recording per line,
a short note what was done and when (seconds) helps.

`tshark -r big.pcapng -Y "usb.transfer_type != 0x03 || usb.endpoint_address.direction == 0" -w small.pcapng`

| # | Recording | For | Status |
|---|---|---|---|
| R1 | Composite PAL: PS1 started while the game already shows 288p (capture start straight into 288p) | stage 1, "288p capture start" | **needed** (only open stage-1 start case) |
| R2 | Bitrate slider with an NTSC source (N64): standard and mobile, a few positions incl. min and max | T-061 | **needed** if the PAL recordings leave the NTSC values open (decided in T-061) |
| R3 | Windows application closed while capturing, and closed without a source | close sequence (T-047 was fixed without a recording) | optional (close works) |
| R4 | Composite NTSC 480i capture start (N64 menu) | initial detection (known differences table) | optional (480i starts work) |
| R5 | PS1 on S-video, switch to composite while capturing (T-053b) | composite ↔ S-video | not needed (works; the switch sequence is in `input-switching/gchd_PAL_composite_svideo_small` 96 s) |
| R6 | Output 640×480 / 480×360 with an NTSC source | T-055 | not needed (works, NTSC tested) |
| R7 | Component with a signal: every mode (480i, 480p, 576i, 576p, 720p, 1080i), mode changes, output sizes, bitrate | stage 2, T-051 | **needed** at stage 2 |
| R8 | HDMI with a signal: every mode, mode changes, output sizes, bitrate | stage 3, T-050 | **needed** at stage 3 (1080p60 output sizes exist) |
| R9 | One recording per picture/audio setting changed while capturing | stage 4, T-080 | needed after the release |
| R10 | Windows initialization with composite selected from the start (application start, no other input before) | initialization (driver uses the component recording) | optional |

## Testing (maintainer request, 2026-11-18)

Hardware tests run through an interactive script instead of copied
commands: `tools/gchd-test.sh` on the `tools` branch with one test file
per task (`tools/tests/T-xxx.sh`). The script builds and loads the module,
runs every command, checks the stream automatically (ffprobe: size, field
order, frame rate, decode errors; dmesg: failures) and writes one log
file. It prompts only for physical actions (cables, consoles) and visual
checks. The maintainer pastes the log (or its summary) into `TASK.md`.

- [ ] **T-090 (M)** Pre-release test before the first `master` merge
      (after HDMI): one test file covering every accepted feature (inputs,
      standards, modes, mode and family changes, output sizes, bitrate,
      formats, close/reopen, input persistence, compliance), less detailed
      than the per-task tests.
