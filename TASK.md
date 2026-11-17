# TASK T-011 — Analyse the recorded USB traces

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** waiting for the trace recordings on the `traces` branch
- **Type:** offline analysis + docs ("docs-only" for RULES F2)
- **Branches:** analysis output on `traces`; documentation changes on
  `docs/<topic>` from `dev`
- **TODO item:** Phase 2, T-011

## Why

The recorded traces are the protocol reference (RULES D1). The driver's
composite path still follows the C++ port in several places that differ from
earlier trace analyses (TODO "Known differences"). New recordings, starting
with composite 480i, decide what the driver must send.

## Inputs

- Recordings uploaded by the maintainer to `traces/` on the `traces` branch.
- `traces/previous-analysis/` (earlier decoded listings) for comparison.

## Deliverables

1. For every recording, a decoded listing next to it on the `traces` branch
   (`<recording>.ops.txt`): one line per protocol operation (register R/W,
   mailbox write/read with device and register, SCMD, firmware chunk count,
   bulk transfer summary), with timestamps. The decoder script lives on the
   `tools` branch.
2. A short summary per recording (`<recording>.summary.md`): scenario,
   phases with time ranges, SCMD sequence, input selector and analog status
   values, encoder field writes that differ from the driver.
3. A comparison against the driver's sequence for the same scenario, as a
   list of differences added to `TODO.md` ("Known differences").
4. `docs/` updated with the newly observed sequences, following RULES C
   (neutral wording, Observed/Driver/Unknown labels).

## Acceptance criteria

- Every uploaded recording has a decoded listing and a summary.
- Every difference between the driver and the recordings is listed in
  `TODO.md` with the trace file and timestamp.
- Documentation changes pass RULES C (no references to the analysis process,
  Windows, or development files).
- No change to `dkms/` in this task.

## Findings

_(filled in during the task)_

---

## Maintainer comments

_Add anything here that the next task must take into account: hardware
availability, what you saw on screen, priorities, things not to touch.
Comments here override the task text above._

- Do not touch the C++ code. The work is the DKMS driver in C only.
- The C++ is a starting point, not the reference; the recorded traces are the
  reference.
- The C++ handles neither input change nor resolution change. Input change is
  in the DKMS driver; resolution change still needs testing with composite and
  HDMI.
- Next hardware session: 480i first, then 240p.
- Documentation ships with the driver: neutral, professional, only verifiable
  behaviour; no Windows references, no reverse-engineering references, nothing
  taken from prompts.
- Tools never go to `dev`/`master` (use the `tools` branch); analysis
  material goes to the `traces` branch.
- Never modify `udev-rules/`, `CMakeLists.txt`, `CREDITS.md`, `LICENSE`, the
  top-level `README.md` or `src/`.
