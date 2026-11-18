# TASK T-070 — Repository layout: all code under `src/`

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** plan; implementation next
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
| `firmware_extract/` | stays (renamed in T-071) |
| `udev-rules/`, `docs/` | stay |

1. `git mv` only; no file content changes except paths:
   - top-level `CMakeLists.txt`: `ADD_SUBDIRECTORY(src/userspace src)` (the
     build output stays in `build/src`, as the top-level README says);
     the commented `src/gui` line to `src/userspace/gui` (paths only, as
     allowed by the maintainer);
   - `src/kernel/dkms.conf` and Makefile/Kbuild paths if any;
   - path references in `docs/`, `src/kernel/README.md`, RULES.md,
     CLAUDE.md, TODO.md;
   - `tools` branch: `compare-objs.sh`, `gen-transcoder-fields.py` and
     `gchd-test.sh` build from `src/kernel/` (with a fallback to `dkms/`
     for older revisions).
2. Proof: the module built before and after is identical
   (`compare-objs.sh`), and the userspace program still builds with CMake.
3. Hardware test: the `smoke` test with the new paths (one short run).

## Hardware test (maintainer)

Follows with the implementation.

## Findings

## Maintainer comments

