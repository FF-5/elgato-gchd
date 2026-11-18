# TASK T-070 — Repository layout: all code under `src/`

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** waiting for the release decision (see findings)
- **Type:** move only (no code changes)
- **Branch:** `chore/src-layout` (from `dev` 0.5.6)
- **TODO items:** T-070

## Why

Agreed with the maintainer (2026-11-18): all code under `src/`, kernel
driver and userspace program side by side, before the installation
scripts (T-071) and the guvcview/OBS service (T-060).

## Plan

| Today | New |
|---|---|
| `dkms/` | `src/kernel/` |
| `src/*.cpp`, `src/*.hpp`, `src/gchd/`, `src/gui/`, `src/CMakeLists.txt` | `src/userspace/` (content unchanged) |
| `firmware_extract/` | `firmware/` (maintainer 2026-11-18) |
| `udev-rules/`, `docs/` | stay |

1. `git mv` only; no file content changes except paths:
   - top-level `CMakeLists.txt`: `ADD_SUBDIRECTORY(src/userspace src)` (the
     build output stays in `build/src`, as the top-level README says);
     the commented `src/gui` line to `src/userspace/gui` (paths only, as
     allowed by the maintainer);
   - `src/kernel/dkms.conf` and Makefile/Kbuild paths if any;
   - path references in `docs/`, `src/kernel/README.md`, RULES.md,
     CLAUDE.md, TODO.md;
   - top-level `README.md`: one section per program; the kernel-module
     section written new, the userspace section unchanged except paths
     and the build method (RULES B4 as updated 2026-11-18);
   - `tools` branch: `compare-objs.sh`, `gen-transcoder-fields.py` and
     `gchd-test.sh` build from `src/kernel/` (with a fallback to `dkms/`
     for older revisions).
2. Proof: the module built before and after is identical
   (`compare-objs.sh`), and the userspace program still builds with CMake.
3. Hardware test: the `smoke` test with the new paths (one short run).

## Hardware test (maintainer)

Follows with the implementation.

## Findings

- Maintainer plan (2026-11-18), recorded in TODO/RULES: `firmware_extract/`
  → `firmware/` (T-070); top-level README with one section per program
  (RULES B4 updated); two top-level scripts (install the kernel module,
  build the userspace program) using `build/` with
  `build/objects/kernel|userspace/` for intermediate files, redundant
  userspace build files removed afterwards (T-071; `.gitignore` already
  ignores `build`); T-092 before the first release: userspace compile
  errors only.
- Release question (kernel module only, on a branch of its own, or both
  programs on `master`). My recommendation:
  1. **If the aim is to contribute to the original project:** `master`
     with both programs, the kernel module as the main product and the
     userspace program kept as it is, marked "not maintained" in the
     README. A contribution there is "add the kernel driver"; removing a
     program its maintainers wrote is their decision, not part of the
     contribution. The cost of keeping it is small (T-092, one build
     script).
  2. **If it becomes our own project:** a repository of its own with the
     kernel module only (driver, docs, firmware script, udev rules,
     installer; credits and a link to the original project kept, GPL).
     Clean README, releases and issues for the module only.
  3. **Not recommended:** a long-lived `kernel-module` branch used as the
     real `master`. Visitors and `git clone` get `master` (the old README
     and program); releases, tags and the default branch disagree; the
     two branches drift apart.
  The move in this task is the same in 1 and 2 except for
  `src/userspace/`; it waits for the decision.

## Maintainer comments

