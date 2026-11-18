# TASK T-070 — Kernel driver only: repository layout

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** done except the licence files (maintainer go-ahead); smoke test
- **Type:** move only (no code changes)
- **Branch:** `chore/src-layout` (from `dev` 0.5.6)
- **TODO items:** T-070

## Why

Maintainer decision (2026-11-18): the project becomes the kernel driver
only. The original project (userspace program) is discontinued; its code
stays in the git history and in the original repository, its authors are
credited.

## Done

| Before | Now |
|---|---|
| `dkms/` | `src/` |
| `src/` (C++ userspace program), `CMakeLists.txt` | removed |
| `udev-rules/55-elgato-game-capture.rules` (USB access for the userspace program) | removed |
| `firmware_extract/` | `firmware/` |
| top-level `README.md` (userspace program) | the kernel driver, with origin and credits |

- Paths updated in `docs/`, `src/README.md`, RULES, CLAUDE, TODO and the
  `tools` scripts (they handle `src/` and, for older revisions, `dkms/`).
- `compare-objs.sh origin/dev WORKTREE`: identical code and data.
- After the first release: detach the fork or create a new repository
  (TODO T-093).

**Pending, needs the maintainer's explicit go-ahead in a session** (the
session's permission check blocked it): `LICENSE` → `LICENSE.MIT` (the
original MIT notice, unchanged), a new `LICENSE` with the GPL-2.0 text,
and an SPDX line (`GPL-2.0`) in every driver source file.

## Hardware test (maintainer)

Your checkout still has the old `dkms/` directory with build files in it
(ignored files stay behind after the move); remove it once:

```sh
rm -rf ~/elgato-gchd/dkms
```

Then the quick regression with the new layout (PS1 and N64 on composite):

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/smoke.sh > tests/smoke.sh
bash gchd-test.sh smoke --branch chore/src-layout
```

Paste the summary.

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

I give you permission to change RULES.md to allow editing any file from the original project. Now here's the summary:

=== summary (smoke, 6b52c1b TASK: T-070 done except licence files; smoke test)
PASS | step 2 composite, standard size, PS1 | stream 720x576 50/1 tt, ~9368 kbit/s
PASS | step 3 composite 640x480 | stream 640x480 25/1 progressive, ~6068 kbit/s
PASS | step 4 composite 480x360 | stream 480x360 25/1 progressive, ~2772 kbit/s
PASS | step 5 picture check | ffplay showed the PS1 picture with sound, correct aspect?
PASS | step 6 reopen right after close | stream 720x576 50/1 tt, ~7739 kbit/s
PASS | step 6 reopen right after close | stream 720x576 50/1 tt, ~4495 kbit/s
PASS | step 7 N64 on composite | stream 720x480 60000/1001 tt, ~7431 kbit/s

FAIL: 0  WARN: 0
