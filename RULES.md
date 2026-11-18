# RULES — guardrails for every task

**Development-only file.** `CLAUDE.md`, `RULES.md`, `TODO.md` and `TASK.md`
never reach `master` (rule B2).

These rules apply to anyone, human or AI, working on the kernel driver
(`src/`) and the documentation (`docs/`). Read them before starting a task
and re-check them before every commit.

## A. Before starting a task

1. **Read `TASK.md` completely, including the maintainer comments at the
   bottom.** Those comments override anything else in `TASK.md`.
2. **The "Maintainer comments" section of `TASK.md` belongs to the
   maintainer.** Never write in it. Lasting information from it goes to
   `RULES.md` (how to work), `TODO.md` (roadmap: open items and known
   differences only) or the `traces` branch (test results:
   `logs/hardware-test-log.md`, one row per test). When `TASK.md` is prepared
   for the next task, the section is emptied after its content has been
   transferred.
3. Check that the task is the top open item in `TODO.md`, or that the
   maintainer explicitly asked for a different one.
4. `git fetch` and start from the current `origin/dev`:
   `git checkout -b <feature-branch> origin/dev`. Branch names: `feat/…`,
   `fix/…`, `docs/…`, `refactor/…`, `diag/…`.
5. State the acceptance criteria from `TASK.md` in your own words. If you
   cannot, the task is not ready; ask instead of guessing.
6. Identify the traces the task depends on (`traces` branch) and open them.

## B. Branches, files and merges

1. **Branches:**

   | Branch | Purpose | Merged into |
   |---|---|---|
   | `master` | Stable releases. Only the maintainer decides what goes here. | — |
   | `dev` | Integration branch; base for all feature branches. | `master`, by the maintainer |
   | feature branches | One per task, from `dev`. | `dev` (`--no-ff`) |
   | `traces` (orphan) | USB trace recordings and all analysis material. | never |
   | `tools` (orphan) | Development scripts (comparison tools, generators). | never |

2. **Development-only files never reach `master`:** `CLAUDE.md`, `TODO.md`,
   `TASK.md`, `RULES.md`. When the maintainer merges `dev` into `master`, the merge
   commit removes them:

   ```sh
   git checkout master
   git merge --no-ff --no-commit dev
   git rm -q --cached CLAUDE.md TODO.md TASK.md RULES.md && rm -f CLAUDE.md TODO.md TASK.md RULES.md
   git commit
   git ls-tree --name-only master | grep -E '^(CLAUDE|TODO|TASK|RULES)\.md$' && echo "ERROR: dev-only file on master"
   ```

   The last command must print nothing. Nothing that refers to these files
   may be shipped (code, `docs/`, `src/README.md`).
3. **Tools and analysis material never enter `dev` or `master`.** New scripts
   are written on the `tools` branch (or a feature branch made from it) and
   used from a worktree or with `git show origin/tools:tools/<script>`. Traces,
   decoded trace listings, logs and analysis notes go to the `traces` branch.
4. **Files of the original project:** the project is the kernel driver
   only (maintainer decision 2026-11-18; the original userspace program
   was removed in T-070 and stays in the git history and in the original
   repository). Any file that came from the original project may be edited
   (maintainer 2026-11-18). Keep the credits to the original authors
   (`CREDITS.md`, README) and the original MIT copyright and permission
   notice: they are required by the MIT licence.
5. Never commit directly to `master`. Never force-push `dev` or `master`.
   Never rewrite published history.
6. One feature branch per task. Merge into `dev` only when the task's
   acceptance criteria are met (rule F). Before merging, re-read the diff of
   the whole branch against `dev`.
7. A feature branch is deleted (locally and on GitHub) once it is merged into
   `dev` or abandoned. If the session cannot delete remote branches, list
   them in `TODO.md` Phase 0 for the maintainer.

## C. Documentation (`docs/`, `src/README.md`)

The documentation ships with the driver and must read as product
documentation.

1. Neutral, third-person, factual wording. No first person, no
   recommendations, no opinions, no "we"/"our"/"should".
2. Document only behaviour that is verifiable from traces (label
   **Observed**) or from the driver source (label **Driver**). Unknown
   effects are labelled **Unknown**, not guessed.
3. No references to the Windows driver, other drivers or applications,
   reverse engineering, decompilation, the analysis process, repository
   history, branches, or statements from the maintainer or task notes.
   Traces may be referred to only indirectly ("observed").
4. No links to the `traces` or `tools` branches or to development-only files.
5. Register, command and constant names in the documentation match the
   driver (`gchd_protocol.h`, `gchd_mailbox.h`, `gchd_transcoder_fields.h`).
6. Development notes, open questions and discrepancies go to `TODO.md` or
   `TASK.md`, not to the documentation.

## D. Protocol fidelity

1. **Recorded USB traces of the device are the reference.** The C++ program
   of the original project (git history before T-070, or the original
   repository) is read-only material and a starting point only: it detects input and mode
   once at start-up and handles neither input changes nor resolution changes.
   The DKMS code is not a reference.
2. When a trace covers the scenario, follow the trace. When no trace covers
   it, the C++ may be used as a stopgap; record the gap as a "trace wanted"
   item in `TODO.md`.
3. **Do not invent register writes, values, delays, or orderings.** Every new
   protocol operation must come from a trace. The trace file and time range
   are named in the **commit message** and, where useful, in `TODO.md` /
   `TASK.md` — never in the code.
4. **Do not "simplify" sequences.** Repeated writes, reads whose values are
   discarded, odd orderings and asymmetric transitions (for example 480i → 240p
   vs 240p → 480i) are kept exactly as recorded.
5. Change one protocol detail per experiment, never several at once, and keep
   a before/after capture.
6. Session-specific values (`0900:0014` shadow, `0900:001C` ready word,
   `0900:01B0`) are read and restored, never hard-coded.
7. Bring up inputs in this order: **composite and S-video first** (480i,
   240p, 576i, 288p), **then component** (all its resolutions), **HDMI
   last**. Within an input, start with 480i. Do not debug a mode until the
   previous simpler mode works end to end. Resolution changes (480i ↔ 240p,
   576i ↔ 288p, HDMI mode changes) are tested as soon as the modes on both
   sides work.
8. **Refactors do not change behaviour.** Verify mechanically with the
   `tools` branch: `compare-objs.sh` (identical machine code) or, when an idiom
   change makes machine code differ, `compare-mailbox-ops.py` (identical USB
   operations per function). Record the tool and result in the commit message.

## E. Kernel-code rules

1. The module builds **without warnings** with `W=1` against Linux 6.8
   headers (Ubuntu 24.04, 6.8.0-x), and against the current distribution
   kernel when available:
   `make -C /usr/src/linux-headers-6.8.0-*-generic M=$PWD/src modules W=1`.
2. No H.264 decoding, scaling or deinterlacing in the kernel.
3. No `msleep()` or blocking USB calls in URB completion handlers or while
   holding spinlocks. Control transfers only from process context.
4. Every wait on the device has a timeout and an error message naming the
   step.
5. Unplug at any time must not oops.
6. **No magic numbers in protocol code.** Requests, banks, registers, ports,
   devices, SCMD values and bit masks are named in `gchd_protocol.h`; encoder
   fields in `gchd_transcoder_fields.h`. Device registers without a documented
   role stay as hex literals behind the device access macro; never add aliases
   that only restate a number. Names follow `docs/commands.md`; do not encode
   guesses in names.
7. Repeated USB operations go through `gchd_mailbox.h`. Macros may return
   from the caller only if they belong to that documented family
   (`GCHD_*_W`, `GCHD_*_R`, `GCHD_*_RX`, `GCHD_BRIDGE_*`, `GCHD_TC_SET`,
   `GCHD_TRY`). No local helper macros in `.c` files.
8. Diagnostics stay behind module parameters or `dev_dbg`.
9. Keep `src/dkms.conf` `PACKAGE_VERSION` in sync with user-visible
   behaviour changes.
10. **Code comments describe only the code itself and what it does.** No
    references to traces, trace files, the Windows driver, the C++ code,
    "userspace", "the original driver" or "the reference". Sources belong in
    commit messages (D3).

## F. Definition of done for a task

A task may be merged to `dev` only when all of these hold:

1. The module builds cleanly (E1).
2. The acceptance criteria in `TASK.md` are met **on hardware**, or the task
   is explicitly marked "build-only" / "docs-only" in `TASK.md`.
3. For any task that changes streamed data: a saved TS sample decodes with
   `ffmpeg -v error -i sample.ts -f null -` without errors, and `ffprobe`
   reports the expected size and field order. The sample and output go to the
   `traces` branch.
4. `docs/` is updated to match what was learned, following section C.
5. `TODO.md` is updated (open items only; finished items are removed), the
   test result is in `logs/hardware-test-log.md` on the `traces` branch, and
   `TASK.md` is rewritten for the next task, keeping the maintainer-comments
   section.

## G. While working

1. Make small commits with a clear "why" in the message.
2. If an assumption turns out to be wrong, stop and write it down in
   `TASK.md` "Findings" before continuing.
3. If a task grows beyond its acceptance criteria, split it and add the rest
   to `TODO.md` instead of widening the branch.
4. Never delete traces, logs or captures; they live on the `traces` branch.
5. Hardware tests are done by the maintainer: build and load the module,
   select the input (`v4l2-ctl -i N`) before starting the viewer, check the
   picture, and paste the `dmesg` and viewer output into the maintainer
   comments of `TASK.md`. Viewers: `guvcview` and, for the H.264 stream,
   `ffplay` (guvcview 2.0.8 mishandles 720-pixel-wide H.264). Provide exact
   commands in `TASK.md` "Hardware test".
6. Do not copy statements from task descriptions or conversations into the
   documentation; verify them against traces or code first, and record
   unverified information in `TODO.md`.
