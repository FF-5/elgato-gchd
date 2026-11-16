# RULES — guardrails for every task

These rules apply to anyone, human or AI, working on the kernel driver
(`dkms/`) and the documentation (`docs/`). Read them before starting a task
and re-check them before every commit.

## A. Before starting a task

1. **Read `TASK.md` completely, including the maintainer comments at the
   bottom.** Those comments override anything else in `TASK.md`.
2. Check that the task is the top open item in `TODO.md`, or that the
   maintainer explicitly asked for a different one.
3. `git fetch` and start from the current `origin/dev`:
   `git checkout -b <feature-branch> origin/dev`. Branch names: `feat/…`,
   `fix/…`, `docs/…`, `diag/…`.
4. State the acceptance criteria from `TASK.md` in your own words. If you
   cannot, the task is not ready; ask instead of guessing.
5. Identify which protocol sources the task touches and open them
   (`docs/README.md` "Sources of truth").

## B. Branch and merge discipline

1. Never commit directly to `master`. The maintainer alone decides what goes
   to `master`.
2. Never develop on the historical branches (`state-machine`,
   `async-bulk-in-queue`, `diagnostic-input-baseline`, `debug*`, `v4l2-*`,
   `video-output-lazy-init`, `cherry3`). Cherry-pick from them deliberately and
   name the source commit in the commit message.
3. One feature branch per task. Merge into `dev` only when the task's
   acceptance criteria are met (rule E). Use a merge commit (`--no-ff`) so each
   task stays visible in history.
4. Never force-push `dev` or `master`. Never rewrite published history.
5. Before merging, re-read the diff of the whole branch against `dev`.

## C. Protocol fidelity (the most common source of past failures)

1. **Never modify the C++ code (`src/`) or its build files (`CMakeLists.txt`).**
   This project writes the DKMS driver in C. The C++ is read-only material.
2. **Our own Windows USB traces are the reference.** The C++ is a starting
   point: much of it worked, but it detects input and mode once at start-up
   and handles neither input changes nor resolution changes. The DKMS code is
   not a reference for anything.
3. When a Windows trace covers the scenario, follow the trace, even where the
   C++ differs. When no trace covers it, the C++ may be used as a stopgap, and
   the gap is recorded as a "trace wanted" item in `TODO.md`.
4. **Do not invent register writes, values, delays, or orderings.** Every
   protocol operation in the kernel cites its source in a comment:
   `/* WIN trace_analysis.txt t=25.892 */` or, as a stopgap,
   `/* C++ configure_composite.cpp:configureComposite */`.
5. **Do not "simplify" sequences.** Repeated writes, reads whose values are
   discarded, odd orderings and asymmetric transitions (for example 480i → 240p
   vs 240p → 480i) are kept exactly as recorded.
6. Change one protocol detail per experiment, never several at once, and keep
   a before/after capture.
7. Session-specific values (`0900:0014` shadow, `0900:001C` ready word,
   `0900:01B0`) are read and restored, never hard-coded.
8. Bring up modes in this order: **480i composite → 240p composite → 576i →
   HDMI/component SD → HD modes → S-video**. Do not debug a mode until the
   previous simpler mode works end to end. 240p is not attempted while 480i is
   broken. Resolution changes (480i ↔ 240p, HDMI mode changes) are tested as
   soon as the mode on each side works.
9. **Refactors do not change behaviour.** A cleanup task (renaming, macros,
   moving code) must produce the same USB traffic. Verify this mechanically:
   identical disassembly of the module before and after, or a documented list
   of every intended difference (procedure in `TASK.md` of the cleanup task
   and in `tools/compare-objs.sh`).

## D. Kernel-code rules

1. The module must build **without warnings** against Linux 6.8 headers
   (the maintainer's VM: Ubuntu 24.04, 6.8.0-x), and against the current
   distribution kernel when one is available:
   `make -C /usr/src/linux-headers-6.8.0-*-generic M=$PWD/dkms modules W=1`.
2. No H.264 decoding, scaling or deinterlacing in the kernel. The kernel
   moves bytes; userspace decodes ([`docs/video-output.md`](docs/video-output.md)).
3. No `msleep()` or blocking USB calls in URB completion handlers or while
   holding spinlocks. Control transfers only from process context.
4. Every wait on the device has a timeout and a clear error message naming
   the step (`dev_err(..., "START: state 0x%02x after %d rounds", ...)`).
5. Unplug at any time must not oops: test with `usb_kill_urb` paths and
   `d->disconnected` checks.
6. Use meaningful names that match `docs/commands.md` (for example
   `GCHD_DEC_REG_LOCK_STATUS` for `0x88:CA`). Do not encode guesses in names
   (`..._SVIDEO` for a 480i value).
7. Keep diagnostics behind module parameters or `dev_dbg`. Default runtime
   logging stays quiet.
8. Keep `dkms/dkms.conf` `PACKAGE_VERSION` in sync with any user-visible
   behaviour change.
9. **No magic numbers in protocol code.** Registers, banks, request codes,
   mailbox ports/devices, device registers, SCMD commands/states and bit masks
   are named constants in `elgato_gchd.h` (or a dedicated header). Repeated USB
   operations go through named helpers or macros (for example
   `GCHD_DEC_WRITE(d, GCHD_DEC_REG_LOCK, 0x8A)`). Raw byte tables copied from a
   trace may stay as data arrays if they cite their source. Constant names
   follow `docs/commands.md`.
10. Prefer `static inline` functions over function-like macros when type
    checking is useful. Use macros only where they shorten repetitive
    call sites without hiding control flow, and never hide a `return` or
    `goto` inside a macro unless its name says so (for example `GCHD_TRY()`).

## E. Definition of done for a task

A task may be merged to `dev` only when all of these hold:

1. The module builds cleanly (D1).
2. The acceptance criteria in `TASK.md` are met **on hardware**, or the task
   is explicitly marked "build-only" / "docs-only" in `TASK.md`.
3. For any task that changes streamed data: a saved TS (or H.264) sample
   decodes with `ffmpeg -v error -i sample.ts -f null -` and **no errors**,
   and `ffprobe` reports the expected size/field order. Attach the command
   output to the commit message or `docs/evidence/`.
4. `docs/` is updated to match what was learned (new evidence goes to
   `docs/evidence/` with a row in its README).
5. `TODO.md` is updated (item checked, new items added), and `TASK.md` is
   rewritten for the next task, keeping the maintainer-comments section.

## F. While working

1. Make small commits with a clear "why" in the message.
2. If an assumption turns out to be wrong, stop and write it down in
   `TASK.md` "Findings" before continuing.
3. If a task grows beyond its acceptance criteria, split it and add the rest
   to `TODO.md` instead of widening the branch.
4. Never delete evidence (traces, logs, captures) from the repository. Old
   logs move to `docs/evidence/`; they are not removed.
5. Hardware tests are done by the maintainer. Provide exact commands to run
   and exactly which output to send back (dmesg filter, file to upload).
6. Keep secrets, serial numbers and personal paths out of committed logs
   where practical. Existing logs are kept as they are.
