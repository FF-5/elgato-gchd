# CLAUDE.md — how to work on this repository

**Development-only file** (never merged to `master`, RULES B2).

This repository develops `elgato_gchd`, a Linux V4L2/DKMS kernel driver (C,
in `dkms/`) for the Elgato Game Capture HD, plus its product documentation
(`docs/`). The C++ userspace driver in `src/` is read-only reference
material.

## At the start of every session

1. `git fetch origin` and check out the branch named in `TASK.md` (or
   `origin/dev` if none). Pull the latest version: the maintainer edits
   `TASK.md` directly on GitHub.
2. Read, in this order:
   1. **`RULES.md`**: the rules. They are binding; re-check them before
      every commit.
   2. **`TASK.md`**: the current task, its acceptance criteria and the test
      steps, and at the end the **maintainer comments** (test results, logs,
      instructions). Those comments override the rest of `TASK.md`.
   3. **`TODO.md`**: the roadmap and the known
      differences between the driver and the traces.
3. Act on the maintainer comments first: analyse logs, fix the driver,
   update the test steps.

## While working

- Feature branches start from `dev` and merge back into `dev` (`--no-ff`)
  only when the task's acceptance criteria are met; `master` is the
  maintainer's (RULES B).
- Never write in the maintainer comments section of `TASK.md`. Lasting
  information goes to `RULES.md` (how to work) or `TODO.md` (facts, plans,
  test results). When preparing `TASK.md` for the next task, transfer the
  comments and empty the section (RULES A2).
- Protocol changes come from USB traces on the `traces` branch; the trace
  file and time go in the commit message, never in code comments (RULES D3,
  E10).
- Build-check every change:
  `make -C /usr/src/linux-headers-6.8.0-*-generic M=$PWD/dkms modules W=1`
  (no warnings). Headers can be installed with
  `apt-get install linux-headers-6.8.0-<n>-generic`.
- Refactors are proven code-identical with the scripts on the `tools`
  branch (`git show origin/tools:tools/compare-objs.sh`).
- Documentation (`docs/`, `dkms/README.md`) is neutral product
  documentation (RULES C).
- Never modify `src/`, `CMakeLists.txt`, the original files in
  `udev-rules/`, `CREDITS.md`, `LICENSE` or the top-level `README.md`
  (RULES B4); new files of our own in `udev-rules/` are allowed.

## Branches

| Branch | Content |
|---|---|
| `dev` | Integration branch |
| feature branches | One per task |
| `traces` (orphan) | USB recordings sorted by input, decoded listings, analysis notes |
| `tools` (orphan) | Development scripts (pcapng decoder, comparison tools) |
| `master` | Releases, maintainer only |

## Hardware testing

Only the maintainer has the device. Testing: build and load the module,
select the input with `v4l2-ctl -i N`, start the viewer (`guvcview`, or
`ffplay` for the H.264 stream), check the picture,
paste `dmesg` and application output into the maintainer comments of
`TASK.md`. Write exact commands in `TASK.md` "Hardware test".

## Bring-up order

Composite and S-video (all resolutions), then component (all resolutions),
HDMI last; within an input, 480i first (RULES D7).
