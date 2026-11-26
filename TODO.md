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

- `dev` (0.5.11): composite and S-video, NTSC 480i/240p and PAL 576i/288p,
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
  Applications (T-060): OBS plugin (source "Elgato Game Capture HD",
  device settings as properties with live preview, black picture without
  signal), `/dev/elgato-gchd` for OBS's Media Source and ffmpeg/ffplay,
  settings window and text menu, installer with three install types. One
  global setting shared by all programs; bitrate mode and bitrates kept
  per output mode (480 × 360; 640 × 480 and 480i/576i; 480p/576p; 720p at
  60; 720p at 30 and 1280 × 720; 1080), the same for every input; H.264
  profile and level per group (composite/S-video, HDMI/component);
  changes from any program apply live. Installer: the core
  (driver) always, the settings programs and the OBS plugin optional
  (T-096). Stretch SD input as in the Windows software (size kept, 16:9
  aspect ratio in the stream; the OBS plugin widens, OBS's Media Source
  cannot).
  Component (T-051): 480i, 480p, 576i, 576p, 720p, 1080i, 1080p30 with the
  recorded mode configuration and encoder values; source mode changes
  followed while capturing; output sizes per source (source size,
  1280 × 720 for 1080 sources, 640 × 480, 480 × 360) and "Allow 50/60 fps",
  also while capturing; bitrate mode and range per output (CBR/VBR range
  limits placeholders until R13); DV timings, size, frame rate and field
  order per mode; TS samples on `traces/component/samples/`.
- Test setup: VirtualBox with the USB 3.0 (xHCI) controller. Once the
  guest kernel disabled the controller's interrupt ("irq 20: nobody
  cared", "Disabling IRQ #20"): USB then very slow, captures stall; only
  a reboot helps (if it recurs: USB 2.0 controller or `irqpoll`).

## Done

Stage 1 (composite and S-video, 0.5.6), driver-only layout (T-070),
licence files (T-094), installer (T-071, 0.5.7), v4l2-compliance (T-066,
0.5.8), OBS and other applications (T-060, 0.5.9), stretch as in the
Windows software and optional installer parts (T-096, 0.5.10), component
input, all modes (T-051, 0.5.11). Test results: `logs/hardware-test-log.md` on the `traces` branch.

## Stage 3 — HDMI

- [ ] **T-050 [WIN+HW] (L)** HDMI, all modes, mode changes, output sizes
      and settings as for component (bitrate per output mode, ranges of the
      recordings; settings window and OBS plugin). Recorded so far: `hdmi/gchd_output_switch_hdmi` (1080p60
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
      `feat/installer`, `feat/output-size`, `feat/ts-audio`, `feat/app-bridge`,
      `fix/stretch`,
      `fix/analog-robustness`, `fix/close-lifecycle`, `fix/v4l2-compliance`,
      `feat/component`.
- [ ] Encoder restart while capturing timed out once after 27 s (-110;
      component 1080i at 1280 × 720, bitrate change, 2026-11-26; not seen
      in eight changes since). The driver detects again and resumes; the
      warning now names the failing step. If it recurs: look at that step.

## Known differences between the driver and the recordings

Change one at a time (RULES D5), when the mode can be tested.

| Item | Driver | Recordings |
|---|---|---|
| `system_rate` | composite/S-video (T-061) and component (T-051) as recorded; HDMI `1.075 × (bitrate + 320) + 256` | `⌊1.07285 × bitrate + 461.7⌋` (PAL; NTSC +8) |
| HDMI encoder and back end | C++ blocks (`65`/`64` pulses, `0900:0018 = 00 0A`, `v_vinpelclk = 0`, `time_scale_l = 0`, `num_units_in_tick = 0`, GOP 14, bitrate formula) | `05`/`15` pulses, `02 0A`, `v_vinpelclk = 1`, `time_scale_l = 60`, `num_units_in_tick = 1`, GOP 8, bitrate per output size |
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
| R8 | HDMI with a signal: every mode, mode changes, output sizes, bitrate | stage 3, T-050 | **needed** at stage 3 (1080p60 output sizes exist) |
| R11 | Composite, standard size and 640 × 480: bitrate slider at about ¾ and just before the right end (the CBR position), a few seconds each, values shown noted | Bitrate range above B = 14460 (VBR maximum) | needed (until then the settings windows offer the observed range: CBR 7.00–11.25, VBR average 3.00–7.23) |
| R13 | HDMI (or component), one source mode: the bitrate slider moved slowly from the highest to the lowest and back, a few seconds per step, values shown noted | where the Windows software switches between CBR and VBR on HD inputs: CBR minimum and VBR maximum for component (now placeholders: CBR from 7/11.25 of the highest, VBR up to half the highest as average) | **needed** with the HDMI recordings (maintainer, 2026-11-26) |
| R9 | One recording per picture/audio setting changed while capturing; for the audio gain: single steps with the displayed dB noted (e.g. −12, −6, 0, +6, +12 dB, pause between them) | stage 4, T-080 | needed after the release (gain registers known: `composite.md` §11) |
