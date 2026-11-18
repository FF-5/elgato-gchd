# TODO — roadmap

**Development-only file** (never merged to `master`, RULES B2).

Goal: a DKMS/V4L2 driver (C, `src/`) that behaves like the Windows driver for
every input and mode the hardware supports. Our own USB recordings are the
reference (`traces` branch). The project is the kernel driver only
(maintainer decision 2026-11-18, T-070). Each open item becomes one
`TASK.md` and one feature branch off `dev`.

**Order (maintainer, 2026-11-18):** finish composite and S-video completely
(all modes, mode and input changes, output sizes, bitrate and the other
settings); then the repository layout and installation scripts; then
guvcview/OBS support; then component the same way; then HDMI.

Legend: **[HW]** maintainer's hardware, **[WIN]** new Windows recording,
**[OFF]** offline, **(S/M/L)** size. Test results: `logs/hardware-test-log.md`
on the `traces` branch.

## Current state

- `dev` (0.5.7): composite and S-video, NTSC 480i/240p and PAL 576i/288p,
  fresh start, mode changes while capturing (NTSC ~1.5 s with encoder
  restart, PAL ~0.5 s decoder-only), 60 ↔ 50 Hz changes while capturing
  (console reset, power cycle, console swap; ~1-5 s, `docs/composite.md
  §8.3`), composite ↔ S-video without unplugging, close and reopen
  (also right after a close and after a session without a source),
  STREAMOFF → STREAMON on one handle. V4L2 reporting: standard (NTSC-M /
  PAL, QUERYSTD/G_STD), 720×480/576 by source, 29.97 / 25 fps,
  interlaced top field first; input changes from other processes refused
  while one captures. Output sizes 720×480/576 (interlaced), 640×480 and
  480×360 (progressive, transport output 2 on endpoint 0x84). Bitrate
  (2-15 Mbit/s, CBR/VBR) and stretch as V4L2 controls, changeable while
  capturing (encoder restart). All settings remembered across replug,
  module reload and reboot (module parameters, udev rule of ours writing
  modprobe.d). Console swaps, source off/on and capture start in every
  mode (incl. 288p) checked; encoder values match the PAL and NTSC
  recordings.
  Formats: H.264 (video only) and MPEG-TS with MP2 audio (`read()`,
  `ffplay -f mpegts /dev/video0`). Sources: N64 (NTSC), PS1 (PAL; the decoder
  reports 60 Hz for a few seconds after power-on and reset, also with the
  Windows driver; the driver waits for the 50 Hz reading).
- Test setup: VirtualBox with the USB 3.0 (xHCI) controller.

## Stage 1 — Composite and S-video complete

All planned composite and S-video items are done (0.5.6). The v4l2
compliance items (T-066) moved to stage 1b, where they matter.

Stage 1 is done when composite and S-video, NTSC and PAL, all modes and
settings above work on `dev`.

## Repository layout and installation (after stage 1)

- [x] **T-070** Kernel driver only (maintainer 2026-11-18): driver in `src/`,
      userspace program, `CMakeLists.txt` and the original udev rule
      removed (git history and the original repository keep them),
      `firmware_extract/` → `firmware/`, top-level README rewritten for the
      driver with origin and credits.
- [x] **T-094** Licence files: `LICENSE` (GPL-2.0) and `LICENSE.MIT` (the
      original notice), done by the maintainer; README licence section and
      SPDX lines in every driver source file.
- [x] **T-071** `install-driver.sh`: firmware check (extraction from the
      user's `GameCaptureSetup*.msi`, stops on failure), build in `build/`,
      DKMS install, udev rule, `--uninstall`; accepted 2026-11-18.
- [ ] **T-095 (S)** Optional download of the Elgato installer by the
      firmware script, after the user confirms (link to Elgato's terms, no
      mirroring, checksum check, URL in one place). **Maintainer: e-mail
      Elgato (Corsair) and ask whether the script may download the
      official installer from their server.** Until the answer the script
      only uses an installer the user downloaded. The script looks for the
      installer only in `firmware/` (maintainer 2026-11-18), so a download
      goes there too; the installer only calls the script and checks its
      result.

## Stage 1b — Applications (guvcview, OBS) (after T-071)

- [ ] **T-066 [OFF→HW] (M)** v4l2-compliance 1.26.1 failures left after
      4c326eb: HDMI/component `G_DV_TIMINGS` fails without a signal although
      `V4L2_IN_CAP_DV_TIMINGS` is set, `VIDIOC_DV_TIMINGS_CAP` missing
      (also breaks the buffer tests via `g_dv_timings`); a menu control
      rejects item 0 ("could not set valid menu item 0", all inputs);
      `V4L2_CID_DV_RX_POWER_PRESENT` missing (warning). The input menu
      control still switches inputs for any process while another one
      captures (S_INPUT returns EBUSY since fd8426b). Needed for
      guvcview/OBS robustness (T-060).

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

- [ ] **T-091 (M)** User profiles (maintainer, 2026-11-18): named sets of
      all settings (input, output size, format, bitrate, mode, stretch,
      H.264 profile/level, later picture/audio settings), several per
      input, recalled at once; the original software has only fixed
      profiles. Builds on the remembered settings of T-064 (settings kept
      per input).

- [ ] **T-062 (S)** Original-HD family (`0044/004e/0051`): needs a tester.
- [ ] **T-063 (S)** Versioning and `CHANGELOG.md` for `master` releases.

## Code and tooling

- [ ] **T-002 [OFF] (M)** File split ("Planned layout" in `src/README.md`),
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
| 480i / 576i capture start with a locked source | no decoder apply block | apply block written (NTSC after INIT A0; PAL before, then `0x88:0A = 85`, `0x88:14 = 1B`) (`composite/gchd_*_composite_*bitrate_audio_small` start) |

## Evidence wanted (maintainer)

All Windows recordings still useful, in the order they are needed. Record
with USBPcap, reduce with the filter that keeps bulk OUT, upload to the
top of the `traces` branch; I sort and decode them. One recording per line,
a short note what was done and when (seconds) helps.

`tshark -r big.pcapng -Y "usb.transfer_type != 0x03 || usb.endpoint_address.direction == 0" -w small.pcapng`

| # | Recording | For | Status |
|---|---|---|---|
| R1 | Composite PAL capture start with a 288p source | stage 1, "288p capture start" | done (`composite/gchd_PAL_composite_small`) |
| R2 | Bitrate slider with an NTSC source | T-061 | done (`composite/gchd_NTSC_composite_output_bitrate_audio_small`) |
| R3 | Windows application closed while capturing, and closed without a source | close sequence (T-047 was fixed without a recording) | optional (close works) |
| R4 | Composite NTSC 480i capture start | initial detection | done (PS2, same NTSC recording) |
| R5 | PS1 on S-video, switch to composite while capturing (T-053b) | composite ↔ S-video | not needed (works; the switch sequence is in `input-switching/gchd_PAL_composite_svideo_small` 96 s) |
| R6 | Output 640×480 / 480×360 with an NTSC source | T-055 | not needed (works, NTSC tested) |
| R7 | Component with a signal: every mode (480i, 480p, 576i, 576p, 720p, 1080i), mode changes, output sizes, bitrate | stage 2, T-051 | **needed** at stage 2 |
| R8 | HDMI with a signal: every mode, mode changes, output sizes, bitrate | stage 3, T-050 | **needed** at stage 3 (1080p60 output sizes exist) |
| R9 | One recording per picture/audio setting changed while capturing; for the audio gain: single steps with the displayed dB noted (e.g. −12, −6, 0, +6, +12 dB, pause between them) | stage 4, T-080 | needed after the release (gain registers known: `composite.md` §11) |
| R10 | Windows initialization with composite selected from the start (application start, no other input before) | initialization (driver uses the component recording) | optional |

## Testing (maintainer request, 2026-11-18)

Hardware tests run through an interactive script instead of copied
commands: `tools/gchd-test.sh` on the `tools` branch with one test file
per task (`tools/tests/T-xxx.sh`). The script builds and loads the module,
runs every command, checks the stream automatically (ffprobe: size, field
order, frame rate, decode errors; dmesg: failures) and writes one log
file. It prompts only for physical actions (cables, consoles) and visual
checks. The maintainer pastes the log (or its summary) into `TASK.md`.

- [x] **Release decision (maintainer, 2026-11-18):** the project is the
      kernel driver only; the userspace program was removed (T-070); the
      former T-092 (userspace compile fix) is dropped.
- [ ] **T-093 (S)** After the first release (maintainer): this repository
      is a GitHub fork of the discontinued original. Either ask GitHub
      support to detach the fork (keeps history and stars), or create a
      new repository and push the branches. Consider a distinct name so it
      is not confused with the old userspace driver.
- [ ] **T-090 (M)** Pre-release test before the first `master` merge
      (after HDMI): one test file covering every accepted feature (inputs,
      standards, modes, mode and family changes, output sizes, bitrate,
      formats, close/reopen, input persistence, compliance), less detailed
      than the per-task tests.
