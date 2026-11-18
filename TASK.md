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

Ok, these are the things I have in mind:
- change firmware_extract to just firmware
- the consolidation of the source code under src as we discussed
- top level README.md we will start touching. We will have essentially 2 programs so the README.md should have sections for each. Leave the section for the userspace mostly untouched except for path changes and eventually build method. Try and mostly work on the kernel module section.
- There are going to be 2 scripts at the top level: a kernel module installer, and the userspace program build. Therefore whatever the userspace uses to build might get made redundant by the build script, so check any build redundancies after creating that script.
- I want the install and build scripts to use a build folder at the top level for all intermediary objects and compiled elements. The compiled program should land in build and all intermediary objects should be created in build/objects/kernel (or userspace if C++ also creates intermediate objects). Make sure the gitignore includes build and everything inside it.
- add a TODO item before merging to master, to fix the compile problem of the userspace code. Make sure that this only addresses the compile problems and not any logic in the code. This is necessary for the build script so that we deliver a working thing.
- a question to you. Is it best to contribute only the kernel module or all? I mean I'm just working on the kernel module and not doing anything about userspace. The module is going to be more complete than the userspace implementation and probably more practical. Is it best, instead of merging to master, we push our releases to a kernel-module or a better named branch without any of the userspace README.md, source and other files? We would essentially be treating this branch as our own master branch that we are going to contribute to the main project. Because I don't really see the benefit of keeping code that I'm not going to maintain and it's usability is worse than ours. Tell me what you think is best for us in this case.
