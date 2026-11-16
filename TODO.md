# TODO — road to a working Linux driver

Goal: a DKMS/V4L2 driver that behaves like the Windows driver for every input
and mode the hardware supports, starting with composite 480i. Ordering
follows `RULES.md` C7. Each unchecked item becomes one `TASK.md` and one
feature branch off `dev`.

Legend: **[HW]** needs the maintainer's hardware, **[OFF]** can be done
offline, **(S/M/L)** rough size.

## Phase 0 — Repository consolidation

- [x] Assess all branches (`docs/branch-history.md`).
- [x] Merge `master` into `dev`; `dev` becomes the integration branch.
- [x] Copy the latest driver (`state-machine` tip) to `dkms/`; it builds clean
      with `W=1` against 6.8 headers.
- [x] Restore deleted evidence to `docs/evidence/` (Windows trace analyses,
      C++ usbmon logs, kernel logs, reference-TS analysis, debug trace tool).
- [x] Rewrite `docs/` against the C++ source and the traces; record
      discrepancies.
- [ ] Decide what to do with the historical branches once `dev` works
      (archive tags such as `archive/state-machine`, then delete?). Maintainer's
      decision.

## Phase 1 — Ground truth and tooling

- [ ] **T-001 [OFF] (M)** `tools/usbmon2ops.py`: turn a usbmon text log
      into protocol operations (register R/W, mailbox write/read with
      device address, SCMD, firmware chunk, bulk). Diff the DKMS no-input init
      (`kernel_noinput_*.log`) against the C++ init (`userspace_noinput.log`)
      and write `docs/analysis/kernel-vs-cpp-init.md`. → current `TASK.md`
- [ ] **T-002 [OFF] (S)** Make the C++ reference build on current GCC
      (`#include <cstdint>` in `settings.cpp` and others). Minimal fix in a
      `fix/` branch, no behaviour change.
- [ ] **T-003 [HW] (S)** Maintainer: run the C++ driver with a composite
      **480i** source, about 20 s to a `.ts` file, with usbmon running. Commit
      the usbmon log and the first 5 MB of the TS to `docs/evidence/`. Confirm
      the TS decodes cleanly in ffmpeg. This is the known-good baseline for
      everything after it.
- [ ] **T-004 [OFF] (S)** Port the `debug` branch's debugfs raw bulk-IN dump
      (`docs/evidence/debug-branch-trace/trace.c`) into `dkms/` behind a
      module parameter. It allows saving exactly what the device sent.

## Phase 2 — Kernel data path (make the output verifiable)

- [ ] **T-010 [OFF→HW] (M)** Replace the PES/H.264 extraction with MPEG-TS
      passthrough: `V4L2_PIX_FMT_MPEG`, `V4L2_CAP_READWRITE` + streaming,
      vb2 buffers filled with whole 188-byte packets (hdpvr model,
      `docs/video-output.md`). Remove `gchd_ring_*`, `gchd_ts()` PES assembly,
      and the "one buffer per URB" delivery. Keep TS sync recovery robust
      (require consecutive `0x47` at 188-byte spacing).
- [ ] **T-011 [OFF] (S)** Make bulk IN `0x84` optional: queue it when present
      (Windows does), never fail without it (C++ does not use it).
- [ ] **T-012 [OFF] (S)** Report `V4L2_FIELD_INTERLACED` for 480i/576i
      (and for 240p, see Phase 5) in `G_FMT`.

## Phase 3 — State machine exactly as C++

- [ ] **T-020 [OFF→HW] (M)** Re-port `completeStateChange()`,
      `stateConfirmedScmd()` and `scmd()` literally (including the first-iteration
      early return, the double `2008` read, the `0074`/`01B0` acknowledge, and
      HD-New receipt only when `(mode & A0) != A0`). Fixes the "START failed
      while state = 0x02" seen at `2da03ea`. Add a per-round debug log.
- [ ] **T-021 [OFF→HW] (S)** Port `stopStream()` (NULL → STOP with draining)
      and `uninitDevice()` exactly; verify that STREAMOFF → STREAMON works
      without replugging.
- [ ] **T-022 [HW] (S)** Investigate the 17 s START latency at `2da03ea`.
      Compare the timing with the C++ usbmon from T-003.

## Phase 4 — Composite 480i end to end (first real milestone)

- [ ] **T-030 [OFF] (M)** Using T-001's tool, diff the DKMS composite path
      against the C++ composite path (`userspace_composite_input_1.log`, T-003
      log). Fix every divergence that is not justified by a Windows trace.
- [ ] **T-031 [HW] (S)** Milestone test: `ffmpeg -f mpegts -i /dev/videoN -t 20 out.ts`
      decodes without errors; `ffprobe` shows 720×480, `field_order tt`/`bb`,
      H.264 + MP2 audio. Merge to `dev`; propose a `master` release.
- [ ] **T-032 [HW] (S)** Composite 576i (PAL) the same way, if a PAL source is
      available. Record the first PAL Windows trace if possible.

## Phase 5 — 240p / 288p (the original goal)

- [ ] **T-040 [OFF] (M)** Remove the 240p-as-480p programming: the encoder
      stays at `v_format 4`, `v_ip_format 0` for 240p (WIN). Rename
      `GCHD_REPLY_STATUS_COMPOSITE/SVIDEO` → `GCHD_DEC_LOCK_240P` (`0x66`) /
      `GCHD_DEC_LOCK_480I` (`0x7E`).
- [ ] **T-041 [OFF→HW] (M)** Implement the Windows live 480i → 240p decoder
      transition (`composite.md §4`: `0x88:07/08/09`, `28=8A`, `06=08`,
      `0A=8F→98`, back-end `0D/0E` pulse; no SCMD, no transcoder writes) and
      the 240p → 480i restart (`§5`: NULL, STOP, back end, INIT A0, START).
- [ ] **T-042 [OFF] (S)** Expose the 240p/480i status to userspace (read-only
      control or input status) and emit `V4L2_EVENT_SOURCE_CHANGE` on change.
- [ ] **T-043 [HW] (S)** Milestone test with a 240p console: TS decodes
      cleanly; `ffmpeg … -vf separatefields` shows a stable 240-line picture at
      60 fps; live 480i ↔ 240p switching (for example a PS1 menu → game) keeps
      streaming.
- [ ] **T-044 [OFF] (S)** Resolve `v_in_source_type`: Windows writes `3` for
      NTSC 240p, C++ writes `0` for NTSC. Test both on hardware and record the
      visible difference.

## Phase 6 — Remaining inputs

- [ ] **T-050 [HW] (M)** HDMI 720p/1080i/1080p/480p following the C++ path;
      compare with the C++ usbmon.
- [ ] **T-051 [HW] (M)** Component 480i … 1080i following the C++ path.
- [ ] **T-052 [HW] (M)** S-video (Windows-only evidence:
      `0x88:02=DC`, `14=D2`), 480i then 240p.
- [ ] **T-053 [HW] (S)** Input autodetect from `0900:001C` bits 8–11 and
      source-loss/return handling with the Windows no-signal loops.
- [ ] **T-054 [HW] (S)** Investigate the C++/Windows discrepancies listed in
      `docs/composite.md §3` one at a time (selector `51=CC` vs `44`, tuning
      `10=88` vs `6F`, back-end table values, `C0=89` vs `88`, `0E` base).

## Phase 7 — Usability

- [ ] **T-060 (M)** Userspace bridge: ffmpeg → v4l2loopback raw node, with
      automatic deinterlace/separatefields for 240p; udev rule plus systemd
      unit; demand-driven start (see `video-output-lazy-init` notes, now in
      `docs/video-output.md`).
- [ ] **T-061 (M)** V4L2 controls that really program the encoder: bitrate
      (CBR/VBR), GOP, profile; audio bitrate.
- [ ] **T-062 (S)** Audio: document using the MP2 track from the TS; consider
      an ALSA interface only if users need it.
- [ ] **T-063 (S)** Original-HD family (`0044/004e/0051`): review the code
      paths that differ (6-byte SCMD, `0xBD` mailbox); needs a tester.

## Phase 8 — Code health

- [ ] **T-070 (S)** Make `vb2_fops` in `fops.c` static (currently a global
      `EXPORT_SYMBOL_GPL`).
- [ ] **T-071 (M)** Rename constants to the `docs/commands.md` names
      (`GCHD_MAIL_PORT_COMPONENT` → `GCHD_DEV_FRONTEND_9C`,
      `GCHD_SCMD_MODE_HDNEW` → `GCHD_SCMD_INIT_NO_RELOAD`, …).
- [ ] **T-072 (M)** Simplify the lifecycle/detection workers once Phases 3–5
      work; audit locking and unplug paths.
- [ ] **T-073 (S)** Bump `dkms.conf` version and add a `CHANGELOG.md` for
      `master` releases.

## Evidence wanted (maintainer, opportunistic)

- PAL (50 Hz) Windows trace: composite 576i and 288p.
- Windows trace with HDMI **with** a signal (1080p and 720p).
- Windows trace of a 240p session that starts directly in 240p (no HDMI step
  before it).
- Raw `.ts` files behind `docs/evidence/reference-ts/` (only the analysis is
  in git).
