# TODO — road to a working Linux driver

Goal: a DKMS/V4L2 driver (C, `dkms/`) that behaves like the Windows driver for
every input and mode the hardware supports. **Our own Windows USB traces are
the reference; the C++ in `src/` is a read-only starting point** (`RULES.md`
C1–C3). Each unchecked item becomes one `TASK.md` and one feature branch off
`dev`.

Legend: **[HW]** needs the maintainer's hardware, **[WIN]** needs a new
Windows trace from the maintainer, **[OFF]** can be done offline,
**(S/M/L)** rough size.

## Status notes from the maintainer

- The input change (HDMI/component/composite/S-video) is implemented in the
  DKMS driver. The C++ never had it.
- Resolution change (for example 480i ↔ 240p, HDMI mode changes) is **not
  verified**. In the last 240p ↔ 480i test the switch did not work and only
  garbled 480i output was received. This may have been caused by the 240p
  configuration (the driver programs 240p as 480p, which Windows does not do;
  see `docs/composite.md`).
- Next hardware session: composite **480i first**, then 240p.

## Phase 0 — Repository consolidation

- [x] Assess all branches (notes: `traces` branch, `traces/previous-analysis/branch-history.md`).
- [x] Merge `master` into `dev`; `dev` is the integration branch.
- [x] Copy the latest driver (`state-machine` tip) to `dkms/`; it builds with
      `W=1` against 6.8 headers.
- [x] Restore deleted analysis material (now on the `traces` branch,
      `traces/previous-analysis/`).
- [x] Rewrite `docs/`; Windows traces are the reference.
- [x] Firmware extract script writes directly to `/lib/firmware/gchd`
      (maintainer commit `9ddf703`, cherry-picked).
- [x] Comparison tools (`compare-objs.sh`, `compare-mailbox-ops.py`) — on the
      orphan `tools` branch.
- [x] Development-only files (`TODO.md`, `TASK.md`, `RULES.md`) excluded from
      `master` (RULES B2); protected files listed (RULES B4); top-level
      `README.md` restored to the `master` version.
- [x] Documentation rewritten to neutral product documentation (RULES C);
      analysis material and branch history moved out of `docs/`.
- [x] Orphan `traces` branch for trace recordings and analysis material.
- [ ] Decide what to do with the historical branches (archive tags, then
      delete?). Maintainer's decision.

## Phase 1 — Code cleanup (no behaviour change)

- [x] **T-001 [OFF] (L)** Named constants (`gchd_protocol.h`), one mailbox
      API (`gchd_mailbox.h`), transcoder field names, dead code removed,
      misleading names fixed. Verified per commit (see `TASK.md` findings).
- [ ] **T-002 [OFF] (M)** File split proposed in `dkms/README.md` ("Planned
      layout"); fold the `*_exact` wrappers; mode-detection thresholds as named
      tables. One move per commit, each checked with `compare-mailbox-ops.py`.
      Expected USB-traffic effect: none.
- [ ] **T-003 [OFF] (S)** Make `vb2_fops` in `fops.c` static (currently a
      global `EXPORT_SYMBOL_GPL`).
- [ ] **T-005 [OFF] (S)** Code comments in `dkms/*.c` still refer to C++
      function names, "userspace" and "original driver" (about 40 lines). Rewrite
      them neutrally (RULES E10); comments only, verify code-identical.
- [ ] **T-004 [WIN] (S)** `0x1004` "stout_mode" write: the kernel writes bit 0,
      C++ `stout_mode` is bit 15. Check the Windows trace's `SOC 1004` writes and
      fix (behaviour change, own branch).

## Phase 2 — Tooling and evidence

- [ ] **T-010 [OFF] (M)** `usbmon2ops.py` on the `tools` branch: decode usbmon text (Linux)
      and USBPcap/pcapng (Windows, via `tshark`) into protocol operations
      (register R/W, mailbox write/read with device address, SCMD, firmware,
      sparam field names, bulk). Same output format for both so that DKMS
      traffic can be diffed against Windows traffic.
- [ ] **T-011 [OFF] (M)** Analyse the trace recordings uploaded to the
      `traces` branch (composite 480i first). Produce decoded listings next to
      the recordings and update `docs/` with the observed sequences.
      → current `TASK.md`
- [ ] **T-012 [OFF] (S)** Port the `debug` branch's debugfs raw bulk-IN dump
      (`traces` branch: `traces/previous-analysis/debug-branch-trace/trace.c`)
      behind a module parameter,
      so the exact device output can be saved from the kernel driver.

## Phase 3 — Kernel data path (make the output verifiable)

- [ ] **T-020 [OFF→HW] (M)** Replace the PES/H.264 extraction with MPEG-TS
      passthrough (`V4L2_PIX_FMT_MPEG`, read() + streaming, hdpvr model;
      `docs/video-output.md`). Remove the lossy ring and the one-buffer-per-URB
      delivery. Robust TS resync (several `0x47` at 188-byte spacing).
- [ ] **T-021 [OFF] (S)** Make bulk IN `0x84` optional (queue it like Windows
      when present; never fail without it).
- [ ] **T-022 [OFF] (S)** Report `V4L2_FIELD_INTERLACED` for 480i/576i and
      240p in `G_FMT`.

## Phase 4 — State machine and composite 480i (first milestone)

- [ ] **T-030 [OFF→HW] (M)** SCMD completion as traced by Windows
      (`docs/hardware.md §4.3`): the loop ends on the target state, a stale
      sticky bit costs one extra round. Fixes "START failed while state =
      0x02" (`2da03ea` log). Per-round debug log.
- [ ] **T-031 [OFF→HW] (S)** Stop sequence NULL → STOP with draining, and
      shutdown; STREAMOFF → STREAMON without replugging.
- [ ] **T-032 [OFF] (M)** Diff the DKMS composite path against the Windows
      480i trace (T-010 tool, T-011 trace, `trace_analysis.txt`). Follow
      the trace.
- [ ] **T-033 [HW] (S)** Milestone: `ffmpeg -f mpegts -i /dev/videoN -t 20 out.ts`
      decodes without errors; 720×480, field order `tt`/`bb`, H.264 + MP2.
      Merge to `dev`; propose a `master` release.
- [ ] **T-034 [HW] (S)** Investigate the ~17 s START latency at `2da03ea`
      against the Windows timing (START → first bulk data about 0.5 s).

## Phase 5 — 240p and the 480i ↔ 240p resolution change

- [ ] **T-040 [OFF] (M)** Keep the encoder at 480i for 240p (`v_format 4`,
      `v_ip_format 0`, WIN). Remove the 240p-as-480p programming.
- [ ] **T-041 [OFF→HW] (M)** Live 480i → 240p decoder-only transition
      (`docs/composite.md §4`) and the 240p → 480i restart (`§5`), exactly
      as traced, from the lock status `0x88:CA` (`7E` / `66`).
- [ ] **T-042 [OFF] (S)** Expose the 240p/480i status to userspace and emit
      `V4L2_EVENT_SOURCE_CHANGE` on change.
- [ ] **T-043 [HW] (S)** Milestone with a 240p console: clean decode;
      `-vf separatefields` gives a stable 240-line picture at 60 fps; live
      480i ↔ 240p switching keeps streaming in both directions.
- [ ] **T-044 [HW] (S)** `v_in_source_type`: Windows writes `3` for NTSC 240p.
      Confirm in the T-011 480i trace what Windows writes for 480i.

## Phase 6 — Remaining inputs and resolution changes

- [ ] **T-050 [WIN+HW] (M)** HDMI with a signal: trace Windows (1080p, 720p,
      480p), implement, and test mode changes while streaming.
- [ ] **T-051 [WIN+HW] (M)** Component, the same.
- [ ] **T-052 [HW] (M)** S-video 480i then 240p (`0x88:02=DC`, `14=D2`).
- [ ] **T-053 [WIN+HW] (S)** PAL: composite 576i/288p traces and support.
- [ ] **T-054 [HW] (S)** Source loss/return using the Windows no-signal loops.

## Known differences between the driver and recorded traces

Development notes (moved out of `docs/`). Each needs its own fix task once
the mode in question can be tested; change one at a time (RULES D5).

| Item | Driver (from the C++ port) | Recorded traces |
|---|---|---|
| 240p encoder programming | `v_format 8`, `v_ip_format 2`, full restart on 480i ↔ 240p | `v_format 4`, `v_ip_format 0`; 480i → 240p decoder-only — **fixed for composite/S-video on `fix/analog-start-per-traces`** (HDMI/component still use the C++ blocks) |
| Composite selector `0x9C:51` | `CC` | `44` (from HDMI), `EC` (from analog) |
| Decoder tuning `0x88:10` | single pass, `10 = 88` | three passes, `10 = 6F` — **already matched in the analog prepare path** |
| Composite NTSC back-end table `0x98:90–A3` | sparse (`90=68, 91=BF, 95=AB, 96=17, 97=88, A0=8C, A1=F8, A2=82, A3=59`) | all 20 registers (`01 BD 9A … 81 49`) — **fixed for composite/S-video on `fix/analog-start-per-traces`** (HDMI/component still use the C++ blocks) |
| `0x98:C0` in the B1 block | `89` | `88` — **fixed for composite/S-video on `fix/analog-start-per-traces`** (HDMI/component still use the C++ blocks) |
| `0x98:0E` retrigger base | `65`/`64` | `05`/`04` and `15`/`14` — **fixed for composite/S-video on `fix/analog-start-per-traces`** (HDMI/component still use the C++ blocks) |
| `0x94:4A`, `0x94:4B` | `AF` | `B7` (also `BB`) — **already matched in the analog prepare path** |
| `v_in_source_type` for NTSC | `0` | `3` (240p composite) — **fixed for composite/S-video on `fix/analog-start-per-traces`** (HDMI/component still use the C++ blocks) |
| `0900:0018` for HDMI | `00 0A` (bit 9 clear) | `02 0A` |
| `0x1004` "stout_mode" | bit 0 written | not yet checked (T-004) |
| 480i → 240p live change | full encoder restart (`gchd_mode_workfn`) | decoder registers only (`0x88:07/08/09/28/0A`), no SCMD |
| 288p detection | not detected (PAL always treated as 576i) | `0x88:CA = 7E`; apply block with `28=8A`, `0A=8F` in the noinput trace |
| Bulk IN `0x84` | required (`-ENODEV` if absent) | queued (8 URBs) during start |
| START completion | timed out with state already `0x02` (`2da03ea` log) | loop until target state |

## Phase 7 — Usability

- [ ] **T-060 (M)** Userspace bridge: ffmpeg → v4l2loopback raw node,
      `separatefields` for 240p, deinterlacing for 480i; udev rule plus
      systemd unit; demand-driven start.
- [ ] **T-061 (M)** V4L2 controls that program the encoder (bitrate, GOP,
      profile, audio bitrate).
- [ ] **T-062 (S)** Original-HD family (`0044/004e/0051`): needs a tester.
- [ ] **T-063 (S)** `dkms.conf` versioning and a `CHANGELOG.md` for `master`
      releases.

## Evidence wanted (maintainer, opportunistic)

- Windows: composite 480i (T-011), HDMI with signal, component with signal,
  PAL 576i/288p, a session that starts directly in 240p.
- The raw `.ts` files behind `traces/previous-analysis/reference-ts/`.
