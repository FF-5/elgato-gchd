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
**Before the first `master` merge (maintainer, 2026-11-26):** after HDMI
the picture and audio settings (T-080), then stage 5 in its order: user
profiles, uninstall with the firmware, test suite, documentation,
performance, pre-release test, security, repository cleanup with
detaching the fork, and last, once the repository is clean, the
installer download (after Elgato's answer) (maintainer, 2026-11-26/28).

Legend: **[HW]** maintainer's hardware, **[WIN]** new Windows recording,
**[OFF]** offline, **(S/M/L)** size. Test results: `logs/hardware-test-log.md`
on the `traces` branch.

## Current state

- `dev` (0.5.14): composite and S-video, NTSC 480i/240p and PAL 576i/288p,
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
  60; 720p at 30 and 1280 × 720; 1080), the same for every input; the
  bitrate as one slider (below 10.00 VBR shown as the average, from 10.00
  CBR); H.264 High profile at the recorded level per output (no profile or
  level controls); changes from any program apply live. Installer: the core
  (driver) always, the settings programs and the OBS plugin optional
  (T-096). Stretch SD input as in the Windows software (size kept, 16:9
  aspect ratio in the stream; the OBS plugin widens, OBS's Media Source
  cannot).
  Component (T-051): 480i, 480p, 576i, 576p, 720p, 1080i, 1080p30 with the
  recorded mode configuration and encoder values; source mode changes
  followed while capturing; output sizes per source (source size,
  1280 × 720 for 1080 sources, 640 × 480, 480 × 360) and "Allow 50/60 fps",
  also while capturing; DV timings, size, frame rate and field order per
  mode; TS samples on `traces/component/samples/`.
  HDMI (T-050): 480p, 576p, 720p50/60, 1080p25/30/50/60 (1080p50/60 at
  25/30 frames per second) with the recorded receiver configuration (no
  back end) and encoder values, tested with a laptop (cold start, after
  composite, every mode with samples on `traces/hdmi/samples/`); output
  sizes, "Allow 50/60 fps", source mode changes while capturing; "HDMI Color
  Range" (Standard/Expanded) while capturing without a restart; BT.601 for
  SD RGB sources (480p tested 2026-11-27). 1080i over HDMI not tested on
  hardware.
  Picture and audio (T-080): brightness, contrast, saturation, hue on
  every input (HDMI RGB sources after the RGB conversion), analog audio
  gain −12 … +12 dB (measured 1 dB per step with a steady tone), live
  without an encoder restart, kept from the capture start and after the
  signal returns, remembered across replug.
- Test setup: VirtualBox with the USB 3.0 (xHCI) controller. Once the
  guest kernel disabled the controller's interrupt ("irq 20: nobody
  cared", "Disabling IRQ #20"): USB then very slow, captures stall; only
  a reboot helps (if it recurs: USB 2.0 controller or `irqpoll`).

## Done

Stage 1 (composite and S-video, 0.5.6), driver-only layout (T-070),
licence files (T-094), installer (T-071, 0.5.7), v4l2-compliance (T-066,
0.5.8), OBS and other applications (T-060, 0.5.9), stretch as in the
Windows software and optional installer parts (T-096, 0.5.10), component
input, all modes (T-051, 0.5.11), HDMI, all modes, and the bitrate as one
slider (T-050, 0.5.12), picture settings and analog audio gain (T-080,
0.5.13), user profiles (T-091, 0.5.14). Test results: `logs/hardware-test-log.md` on the `traces` branch.

## Stage 4 — Picture and audio settings (after HDMI, before the release)

- [x] **T-080 (M)** Picture settings (brightness, contrast, saturation,
      hue; HDMI/component in the colour conversion `0x9C:92`–`AA`,
      composite/S-video in the decoder `0x88:10`–`15`) and the analog audio
      gain (−12 … +12 dB, `0x94:41/4A/4B`, 1 dB per step measured) as V4L2
      controls, live without an encoder restart, remembered, in the
      settings window, text menu and OBS plugin (0.5.13, 2026-11-27).

## Stage 5 — Release preparation (before the first `master` merge)

In this order, each its own task and branch:

- [x] **T-091 (M)** User profiles: named sets of all settings (input,
      output size, bitrate of that size, stretch, Allow 50/60 fps, HDMI
      colour range, picture settings, audio gain), per user in
      `~/.config/elgato-gchd/profiles`, with `gchd-profile` (installed with
      the core); Load, Save (new name or replace, asks), Delete in the
      settings window, text menu and OBS plugin (Qt question boxes); the
      settings window as "Elgato Game Capture HD" in the system settings
      menu (0.5.14, 2026-11-28).
- [ ] **T-101 (S)** Uninstall removes the firmware too (maintainer,
      2026-11-27): `install-driver.sh --uninstall` also deletes the firmware
      files it extracted (`/lib/firmware/gchd/`), after asking (default yes;
      `--yes` without asking); today it keeps them.
- [ ] **T-102 (L)** Test suite shipped with the driver (maintainer,
      2026-11-27), replacing the single pre-release test file. Lives on
      its own orphan branch `tests`, separate from development and releases
      (maintainer, 2026-11-27; RULES B3); other people fetch it from there
      to test what the maintainer cannot. Grows from `tools/gchd-test.sh` and the per-task
      test files (their helpers: `capture` with size, frame rate and field
      order, dmesg check, `prompt`, `confirm`, summary and log).
      - **Driver, per input** (composite, S-video, component, HDMI): deep
        tests of everything for that input (every standard and source
        mode, mode and family changes while capturing, output sizes,
        stretch/allow options, bitrate modes and limits, picture settings,
        audio gain with a level measurement, colour range, formats,
        close/reopen, unplug while capturing, settings persistence,
        compliance). Fully automated: the user is only asked to change the
        cable/input or to change the source mode while a viewer window is
        open (as with the PS3 tests).
      - **Settings window, text menu, OBS plugin**: interactive tests in
        which the user operates the interface and the script checks the
        result through the driver (as the T-080 window/OBS steps).
      - **One launcher** asking what to run: a single input or program,
        the full deep test, or the full fast test (a chosen subset of the
        deep tests that checks general function in a short time; each test
        is tagged deep or fast in one place).
      - **Cases needing outside testers** shown in the launcher with a
        direct link to their GitHub issue: the original GCHD family
        (`0044/004e/0051`, T-062) and HDMI 1080i; the issues are created
        at the first `master` merge (T-100) and the links kept in one place
        (the repository URL may change with T-093).
      - A summary at the end to paste into an issue (device, kernel,
        driver version, PASS/FAIL per test, log path).
      - Documentation for testers (how to run, what each part needs:
        sources, cables, OBS), neutral product docs (RULES C).
- [ ] **T-097 [OFF] (L)** Documentation rewrite (maintainer, 2026-11-26):
      `docs/` becomes (1) a reference of the device's communication
      protocol, every command and register with what it does, and (2) a
      description of what the driver's code does (structure, state
      machine, workers, data path, settings). Written to help others
      understand the device and the code, not how the results were found:
      no "Observed"/"Driver"/"Unknown" labels, no references to
      recordings, traces or the analysis; also the READMEs and code
      comments (RULES C, E10).
- [ ] **T-098 [OFF+HW] (M)** Performance review (maintainer: "extract
      everything out of the module"). Measure first: CPU time per second
      of capture (`perf`, the `timing` parameter), latency of control
      changes and restarts, per input and output size. Candidates: the
      data path (bulk-IN URBs, TS packet parsing and PID filtering, ring
      buffer copies, `read()`/vb2 copies), polling intervals, mailbox round
      trips. SIMD (SSE2) in a kernel module needs `kernel_fpu_begin()` /
      `_end()` around it, which saves the FPU state and must not sleep; it
      pays off only for long copy or scan loops, and the kernel's `memcpy`
      already uses the fastest copy the CPU offers. Expected gain small
      (the work is mostly USB transfers); every change measured before and
      after and proven to give the same stream.
- [ ] **T-090 (M)** Pre-release test: the full deep test of T-102 (every
      input, the settings window, text menu and OBS plugin) and the
      installer, on the release candidate.
- [ ] **T-099 [OFF] (M)** Security review of all code, last before the
      merge (maintainer, 2026-11-26): driver (USB data from the device
      treated as untrusted: lengths and bounds in the bulk-IN and TS
      parsing, mailbox replies, control and ioctl arguments, module
      parameters, unplug races, locking), userspace (installer and
      firmware script: quoting, temporary files, `sudo` use, downloads;
      udev rule `RUN` command; settings programs; OBS plugin: buffer sizes,
      parsing of driver output and sysfs). Findings listed by severity;
      fixed and re-tested before the merge.
- [ ] **T-100 (S)** Repository cleanup for the release (maintainer,
      2026-11-26): `master` gets no development material (the merge removes
      `CLAUDE.md`, `TODO.md`, `TASK.md`, `RULES.md`, RULES B2; `traces` and
      `tools` never enter `dev` or `master`, RULES B3); the merged feature
      branches deleted; `traces` and `tools` deleted from GitHub by the
      maintainer once nothing needs them (the development session cannot
      delete remote branches; keep a private copy if the recordings may be
      wanted later, e.g. `git clone --mirror`). Also (maintainer,
      2026-11-27): the guidelines this project is developed with (RULES,
      the agent instructions of CLAUDE.md, the test runner workflow) made
      usable by people who continue the work, also with an AI agent: they
      stay development files on `dev` (never on `master`, which holds only
      the product, RULES B2), cleaned of the development history and
      maintainer notes; the `master` README points to `dev` for
      contributors. At the merge, GitHub
      issues for the cases that need outside testers (original GCHD
      family, HDMI 1080i), each saying exactly which part of the T-102
      suite to run (fetched from the `tests` branch); the launcher links
      to them.
- [ ] **T-093 (S)** With the repository cleanup, before the first release
      (maintainer, 2026-11-27): this repository
      is a GitHub fork of the discontinued original. Either ask GitHub
      support to detach the fork (keeps history and stars), or create a
      new repository and push the branches. Consider a distinct name so it
      is not confused with the old userspace driver.
- [ ] **T-095 (S)** (Before the first release, maintainer 2026-11-27; last, after the cleanup and the fork detachment, once the repository is clean and professional looking, maintainer 2026-11-28.) Optional download of the Elgato installer by the
      firmware script, after the user confirms (link to Elgato's terms, no
      mirroring, checksum check, URL in one place). **Maintainer: e-mail
      Elgato (Corsair) and ask whether the script may download the
      official installer from their server.** Until the answer the script
      only uses an installer the user downloaded. The script looks for the
      installer only in `firmware/` (maintainer 2026-11-18), so a download
      goes there too; the installer only calls the script and checks its
      result. Comes after the security review (T-099): its download code
      gets the same review in this task.

## Later

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
      `feat/component`, `feat/hdmi`.
- [ ] HDMI 1080i: not testable (the PS3 fails HDCP on the device; no
      laptop offers 1080i); detection value and encoder values from the
      component 1080i and the older code (maintainer, 2026-11-27).
- [ ] Encoder restart while capturing timed out once after 27 s (-110;
      component 1080i at 1280 × 720, bitrate change, 2026-11-26; not seen
      in eight changes since). The driver detects again and resumes; the
      warning now names the failing step. If it recurs: look at that step.

## Known differences between the driver and the recordings

Change one at a time (RULES D5), when the mode can be tested.

| Item | Driver | Recordings |
|---|---|---|
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
| R15 | Composite (and S-video), capturing: each picture slider 0 → −100 → 0 → +100 → 0 (hue ±180), a few seconds per position | registers of the picture settings on the analog decoder path | received 2026-11-27 (composite): `0x88:10`–`15`, analysis §8 |
| R16 | Composite, capturing, a steady sound if possible: audio gain from 0 dB in single +1 dB steps to +12 dB, 3 s each, values noted; then down to −12 dB the same way | the positive gain codes and `0x94:41 = 57` | received 2026-11-27: the driver's codes match every step, analysis §8 |
| R9 | One recording per picture/audio setting changed while capturing | stage 4, T-080 | recorded 2026-11-27 (brightness, contrast, saturation, hue, analog audio gain; `traces/`); positive steps confirmed by a level measurement (T-080, 2026-11-27) |
