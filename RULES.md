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

1. **The C++ implementation and the Windows traces are the protocol
   references. The DKMS code is not.** When the kernel driver disagrees with
   both, the kernel is wrong until hardware evidence says otherwise.
2. **Do not invent register writes, values, delays, or orderings.** Every
   protocol operation added to the kernel must cite its source in a code
   comment: `/* C++ configure.cpp:configureComposite */` or
   `/* WIN trace_analysis.txt t=25.892 */`.
3. **Do not "simplify" sequences.** Repeated writes, reads whose values are
   discarded, odd orderings and asymmetric transitions (for example 480i → 240p
   vs 240p → 480i) are kept exactly as recorded.
4. When C++ and Windows disagree, keep the C++ behaviour unless the task is
   specifically about that discrepancy. Change one discrepancy per
   experiment, never several at once.
5. Session-specific values (`0900:0014` shadow, `0900:001C` ready word,
   `0900:01B0`) are read and restored, never hard-coded.
6. Follow the C++ state-machine logic literally (`completeStateChange`). The
   loop exit condition is the target state, not a single completion bit.
7. Bring up modes in this order: **480i composite → 576i → HDMI/component SD
   → HD modes → 240p → S-video**. Never debug a mode until the previous
   simpler mode works end to end. 240p must not be attempted while 480i is
   broken.

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
