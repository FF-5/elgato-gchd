# TASK T-041a — Composite 480i ↔ 240p while capturing

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** mode change rewritten from the new trace; waiting for the hardware test
- **Type:** driver fix + hardware test
- **Branch:** `fix/mode-change-restart` (from `dev`)
- **TODO items:** Phase 5 T-041, T-043; Phase 3 T-023 (audio, not in this task)

## Why

Composite 480i and 240p both start and stream (merged into `dev`). The
first attempt at switching while capturing (8a4b2af) detected the change but
the NULL stop never completed (timeout after ~63 s), and the recovery path
then reported a false signal loss right after START.

The new recording `traces/composite/gchd_240p-480i_composite` shows the
complete sequence for both directions (`docs/composite.md §4`). The driver
now follows it:

1. Detection: `0x88:CA` = `76`/`7E` → 480i, `66` → 240p; on `6E` (leaving
   480i) one back-end retrigger `15`, then wait for `66`.
2. While still in START: decoder apply block (`28 = 88`/`8A`), 0.4 s, read
   `E7`, neutral back-end table (`90 = 88`, `91`–`A3 = 77`, …), retrigger
   `05`.
3. NULL and STOP with the receive queue still running.
4. Back-end table for the mode, retriggers `05`+`15` (480i) or `15`+`15`
   (240p), encoder setup, INIT A0, START. Routing and decoder tuning are no
   longer repeated, and `0x88:0A` is not written.

The video timestamp used by the signal-loss check is refreshed when START
completes.

Audio is not delivered yet (TODO T-023): only the video PES is passed to
V4L2. It comes with TS passthrough (T-020).

## Acceptance criteria

1. 480i → 240p while ffplay runs: the picture returns as 240p within a few
   seconds.
2. 240p → 480i while ffplay runs: the picture returns as 480i.
3. dmesg shows "analog mode change detected" and "analog mode now …", no
   "analog mode change failed", no "signal lost" right after a START.

## Hardware test (maintainer)

```sh
git fetch origin && git checkout fix/mode-change-restart && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
# switch the source 480i -> 240p, wait ~10 s, then 240p -> 480i
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested"
```

If ffplay stops showing a picture after a switch, close it and start it again
(same command) and note whether the picture comes back. Paste the output and
observations into the maintainer comments below.

## Findings

- 8a4b2af: stopping the receive queue before NULL/STOP prevents NULL from
  completing (see TODO "Hardware test log").
- `gchd_240p-480i_composite`: in that recording NULL takes ~0.7 s and is
  preceded by the neutral back-end table; 480i → 240p is not decoder-only.

---

## Maintainer comments

