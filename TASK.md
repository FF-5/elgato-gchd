# TASK T-096 — Stretch SD input as in the Windows software

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** waiting for evidence from the Windows software (TODO R12)
- **Type:** fix (driver and/or OBS plugin)
- **Branch:** `fix/stretch` (from `dev` 0.5.9), created when the evidence
  is in
- **TODO items:** T-096 (stage 1b)

## Why

"Stretch SD input" is meant for games that offer a 16:9 output by
squashing the picture horizontally into 4:3, so that a widescreen TV
stretches it back. In the Windows software the output size stays the same
and the picture is visibly stretched (maintainer, 2026-11-19). With the
driver, OBS's Media Source shows no difference.

## What is known

- The USB recording `traces/composite/gchd_288p_stretch_input_small`:
  the only change is the encoder field `v_in_source_type = 2` instead of
  `3` (`docs/composite.md §10`). The driver sends the same.
- Driver capture (T-060-settings, c8c8944, NTSC N64, 720 × 480): the
  encoded size stays 720 × 480; the stream's sample aspect ratio changes
  from 10:11 (display 4:3) to 40:33 (display 16:9), also when stretch is
  switched while capturing. Players that follow the aspect ratio
  (ffplay at the start of a stream) show it stretched; OBS's Media Source
  and OBS itself ignore it.
- The OBS plugin widens a picture with a 16:9 aspect ratio itself
  (720 × 480 → 872 × 480, 720 × 576 → 1048 × 576), so the source in OBS
  gets wider (accepted as a stop-gap, T-060). The maintainer is not sure
  this matches the Windows software.

## Evidence needed (TODO R12)

With a picture of known shape (a menu, a circle; best a game with a 16:9
option), in the Windows software:

1. Screenshots of the preview with stretch off and on.
2. A short recording saved by the software with stretch off and one with
   it on (the files themselves: their size and aspect ratio show whether
   the software rescales the picture or only keeps the aspect ratio).

## Goal (acceptance criteria, draft — after the evidence)

1. With stretch on, the picture looks as in the Windows software in the
   OBS plugin, OBS's Media Source (`/dev/elgato-gchd`) and ffplay.
2. With stretch off, nothing changes from today.
3. Output size settings unchanged.

## Hardware test (maintainer)

Written once the evidence is in.

## Findings

## Maintainer comments

