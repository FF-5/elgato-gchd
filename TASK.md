# TASK T-102 — Test suite

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** plan agreed (2026-11-28); first the logging task T-103, then piece (a)
- **Type:** test suite (stage 5), on the orphan branch `tests`
- **Branch:** `feat/test-suite` (from `dev` 0.5.15) for TASK/TODO; the
  suite itself on `tests` (RULES B3)
- **TODO items:** T-102 (stage 5), T-090 (runs it)

## Why

One test file per task tested what changed. Before the release the whole
driver and its programs need deep tests, a quick check, and a way for
other people to test what the maintainer cannot (the original GCHD
models, HDMI 1080i) and report it in an issue (maintainer, 2026-11-27).

## Goal (acceptance criteria, draft)

1. Orphan branch `tests`, separate from development and releases; a
   tester fetches only it.
2. One launcher, `gchd-test`, with a menu: one input (composite,
   S-video, component, HDMI) or program (settings window, text menu, OBS
   plugin, installer), the full deep test, the full fast test, and the
   cases that need outside testers with a direct link to their GitHub
   issue.
3. Driver tests per input, automated: the tester is only asked to change
   cables/inputs or the source's mode (with a viewer window open, as with
   the PS3). Everything for that input: each source mode the tester has
   and mode changes while capturing, output sizes, stretch / Allow
   50/60 fps / colour range, bitrate modes and limits, picture settings
   and audio gain, formats, close and reopen, unplug while capturing,
   settings kept after replug and module reload, v4l2-compliance.
4. Program tests (settings window, text menu, OBS plugin, installer): the
   tester uses the program as told; the suite checks the result through
   the driver.
5. Each test case is tagged deep or fast in one place; the fast test runs
   the fast ones of every part (general function in a short time).
6. At the end a summary to paste into an issue: device (USB ID), kernel,
   distribution, driver version, PASS/FAIL/SKIP per case, the log file.
7. A README for testers: what each part needs (sources, cables, OBS),
   how to run it, how to report.

## Plan (proposed; open points below)

1. **Layout** of `tests`:
   `gchd-test` (launcher), `lib/runner.sh` (the helpers of today's
   `gchd-test.sh`: steps, `expect`, `capture`, `prompt`, `confirm`,
   dmesg check, summary), `lib/links.conf` (issue links, in one place:
   the repository URL may change with T-093), `parts/` one file per part
   (`composite`, `svideo`, `component`, `hdmi`, `window`, `menu`, `obs`,
   `install`), `README.md`.
2. **Test cases** as functions registered with a tag:
   `case fast "composite: capture 480i" t_composite_capture`; a part runs
   its cases in order; the fast test only those tagged fast.
3. **What is tested:** one driver build for everybody, no separate
   development version. The detailed log messages the tests rely on move
   behind a `debug` module parameter (T-103), off by default and
   switchable while the driver runs
   (`/sys/module/elgato_gchd/parameters/debug`). The fast test checks
   function only, on the driver as installed, with the default (quiet)
   logging; the deep test switches `debug` on for its run and off at the
   end, so it gets the detailed messages from a release build too. The
   maintainer can still build and install a checkout first (`--repo DIR
   --branch NAME`, as today).
4. **Sources the tester has:** at the start of each part the suite asks
   which sources and modes are available (e.g. composite: NTSC 480i, PAL
   576i, 240p/288p; component: 480i … 1080p; HDMI: which modes the
   source can output); cases for missing ones are SKIP, not FAIL.
5. **Automatic checks where eyes were needed so far:** picture settings
   measured on captured frames (`ffmpeg` `signalstats`: brightness moves
   the average luma, saturation −100 gives zero saturation, contrast
   widens the luma spread); audio gain measured as the level difference
   with a steady sound (when the tester has one, else only "louder /
   quieter"). `confirm` stays for what cannot be measured.
6. **Outside-tester cases:** the menu lists "Original Game Capture HD
   (0044/004e/0051)" and "HDMI 1080i" with their issue links; each runs
   the matching part and ends with the summary for the issue. The issues
   are created at the first `master` merge (T-100); until then the links
   point to the repository's issue list.
7. **Delivered in pieces**, each tested on hardware before the next:
   (a) framework, launcher, composite deep and fast; (b) S-video,
   component, HDMI; (c) settings window, text menu, OBS plugin,
   installer; (d) outside-tester cases, README, issue texts for T-100.

## Decisions (maintainer, 2026-11-28)

- What is tested: the maintainer asked whether the deep test needs a
  development build with the log messages. Agreed instead (proposal,
  2026-11-28): the same build for everyone, the detailed messages behind
  a `debug` parameter (T-103): quiet by default for users, on for the
  deep test and for troubleshooting (an issue can ask for a log with
  `debug` on). A second build would be one more thing to install and to
  keep equal to the release.
- Everything that can be measured is checked automatically (picture from
  captured frames, audio levels); the tester checks only the rest.
- `tools/gchd-test.sh` and `tools/tests/` stay for task tests until the
  suite replaces them.

## Hardware test (maintainer)

Not yet.

## Maintainer comments

