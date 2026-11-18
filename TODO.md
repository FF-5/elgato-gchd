# TODO — roadmap

**Development-only file** (never merged to `master`, RULES B2).

Goal: a DKMS/V4L2 driver (C, `src/driver/`) that behaves like the Windows driver for
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

- `dev` (0.5.8): composite and S-video, NTSC 480i/240p and PAL 576i/288p,
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
  `v4l2-compliance` 1.26.1: 0 failures (DV timings for HDMI/component,
  power-present control; inputs only through `VIDIOC_S_INPUT`).
- Test setup: VirtualBox with the USB 3.0 (xHCI) controller.

## Done

Stage 1 (composite and S-video, 0.5.6), driver-only layout (T-070),
licence files (T-094), installer (T-071, 0.5.7), v4l2-compliance (T-066,
0.5.8). Test results: `logs/hardware-test-log.md` on the `traces` branch.

## Stage 1b — Applications (guvcview, OBS)

- [ ] **T-060 (L)** Use in OBS and other programs (maintainer decisions
      2026-11-18): (1) OBS plugin `src/obs-plugin/`, source "Elgato Game
      Capture HD" with the device settings as properties, finds the
      device itself; (2) fixed device name `/dev/elgato-gchd` (udev rule)
      for OBS's Media Source (input format mpegts) and ffmpeg/ffplay;
      (3) a settings tool (TUI) for input, output size and encoder
      settings. The driver stays a single H.264/MPEG device: the
      application device with uncompressed frames and the userspace
      bridge were tried and removed (they remain in the history of
      `feat/app-bridge`). Plugin, device name and settings tool accepted
      on hardware (T-060-obs 16/16); installer with three install types
      (everything, driver, OBS plugin; maintainer 2026-11-18) accepted
      (13/13). Settings window (GTK) instead of the text menu: hardware
      test open.

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

- [ ] **T-095 (S)** Optional download of the Elgato installer by the
      firmware script, after the user confirms (link to Elgato's terms, no
      mirroring, checksum check, URL in one place). **Maintainer: e-mail
      Elgato (Corsair) and ask whether the script may download the
      official installer from their server.** Until the answer the script
      only uses an installer the user downloaded. The script looks for the
      installer only in `firmware/` (maintainer 2026-11-18), so a download
      goes there too; the installer only calls the script and checks its
      result.
- [ ] **T-090 (M)** Pre-release test before the first `master` merge
      (after HDMI): one test file covering every accepted feature (inputs,
      standards, modes, mode and family changes, output sizes, bitrate,
      formats, close/reopen, input persistence, compliance), less detailed
      than the per-task tests.
- [ ] **T-093 (S)** After the first release (maintainer): this repository
      is a GitHub fork of the discontinued original. Either ask GitHub
      support to detach the fork (keeps history and stars), or create a
      new repository and push the branches. Consider a distinct name so it
      is not confused with the old userspace driver.
- [ ] **T-062 (S)** Original-HD family (`0044/004e/0051`): needs a tester.
- [ ] **T-063 (S)** Versioning and `CHANGELOG.md` for `master` releases.

## Code and tooling

- [ ] **T-002 [OFF] (M)** File split ("Planned layout" in `src/driver/README.md`),
      fold the `*_exact` wrappers; each move checked with
      `compare-mailbox-ops.py`.
- [ ] **T-003 [OFF] (S)** Make `vb2_fops` static.
- [ ] **T-004 [OFF] (S)** `0x1004` "stout_mode": the defaults write bit 0;
      check against the initialization recording.
- [ ] **T-005b [OFF] (S)** Code comments still name `configureDevice()`
      (RULES E10); comments only, verify code-identical.
- [ ] Branches to delete on GitHub (refused from the development session;
      state 2026-11-18): `cherry3`; merged into `dev`: `chore/src-layout`,
      `feat/bitrate`, `feat/format-reporting`, `feat/input-persistence`,
      `feat/installer`, `feat/output-size`, `feat/ts-audio`,
      `fix/analog-robustness`, `fix/close-lifecycle`, `fix/v4l2-compliance`.

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
| Repeated `0x88:02/3C`, `0x94` writes after detection | written | not written |
| 480i / 576i capture start with a locked source | no decoder apply block | apply block written (NTSC after INIT A0; PAL before, then `0x88:0A = 85`, `0x88:14 = 1B`) (`composite/gchd_*_composite_*bitrate_audio_small` start) |

## Evidence wanted (maintainer)

All Windows recordings still useful, in the order they are needed. Record
with USBPcap, reduce with the filter that keeps bulk OUT, upload to the
top of the `traces` branch; I sort and decode them. One recording per line,
a short note what was done and when (seconds) helps.

`tshark -r big.pcapng -Y "usb.transfer_type != 0x03 || usb.endpoint_address.direction == 0" -w small.pcapng`

| # | Recording | For | Status |
|---|---|---|---|
| R7 | Component with a signal: every mode (480i, 480p, 576i, 576p, 720p, 1080i), mode changes, output sizes, bitrate | stage 2, T-051 | **needed** at stage 2 |
| R8 | HDMI with a signal: every mode, mode changes, output sizes, bitrate | stage 3, T-050 | **needed** at stage 3 (1080p60 output sizes exist) |
| R9 | One recording per picture/audio setting changed while capturing; for the audio gain: single steps with the displayed dB noted (e.g. −12, −6, 0, +6, +12 dB, pause between them) | stage 4, T-080 | needed after the release (gain registers known: `composite.md` §11) |
