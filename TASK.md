# TASK T-041a — Composite 480i ↔ 240p while capturing

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** recorded encoder block before START; waiting for the hardware test
- **Type:** driver fix + hardware test
- **Branch:** `fix/mode-change-restart` (from `dev`)
- **TODO items:** Phase 5 T-041, T-043; Phase 3 T-023 (audio, not in this task)

## Why

Last test (2025d5f): every START still streamed for only ~0.5 s, then the
device stopped sending and NULL could not complete. Two findings:

1. **Encoder programming before START did not match the recordings.**
   Every recording (composite, S-video, HDMI, every restart) writes the same
   120 encoder fields in the same order before INIT A0 / START. The driver
   wrote a different set: `dma_sel_out = 1` (transport output DMA
   selection), `sout_mode`, `v_ip_format_bl` (twice), `ach_sel_bl`,
   `v_idr_interval_h_bl` and two unnamed fields were missing; the four
   stream-id fields (written by Windows only at initialization) were extra;
   the order differed and `num_units_in_tick_bl` was 1 instead of 0. The
   driver now writes exactly the recorded block (checked mechanically: 120
   of 120 fields in order).
2. **Back-end settle wait.** After the first pulse the status already read
   `72` (settled), and the driver waited 1.5 s for a flag. `72` now ends
   the wait; a second pulse is sent only when the flag appears, as in the
   recordings.

Open: at the start the decoder read `CA = 66` and the driver configured
240p, then saw 480i 0.3 s after START. Check in this test whether the
source is 480i or 240p when you start.

Audio is not delivered yet (TODO T-023).

## Acceptance criteria

1. 480i → 240p and 240p → 480i while ffplay runs: the picture returns.
2. Closing ffplay and starting it again gives a picture.
3. dmesg shows "analog mode now …", no "analog mode change failed", no
   "signal lost" right after a START.

## Hardware test (maintainer)

Unplug and replug the device first (the device may still be in the stuck
state from the last test).

```sh
git fetch origin && git checkout fix/mode-change-restart && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
# switch the source 480i -> 240p, wait ~10 s, then 240p -> 480i
# close ffplay, wait 5 s, start the same ffplay command again
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested"
```

Note which mode the source outputs when you start, and for each step whether
a picture appeared. Paste the output into the
maintainer comments below. Important lines: `back-end status …` (only
printed if a wait timed out), `bulk-IN first reads`, and the
`async bulk-IN stats` lines after each START.

## Findings

- 8a4b2af: stopping the receive queue before NULL/STOP prevents NULL from
  completing.
- `gchd_240p-480i_composite`: NULL takes ~0.7 s and is preceded by the
  neutral back-end table; 480i → 240p is not decoder-only.
- 9382bcb test: the mode-change sequence completes, but every START after
  the first one in a power cycle streams only ~0.4 MB.
- `gchd_240p-480i_composite_input_part_*` (bulk data): the driver's receive
  queue handling during stop matches; no reads after STOP.
- 2025d5f test: encoder programming before START differed from every
  recording (missing `dma_sel_out` etc.); fixed.
- 0c64469 test: START hangs or the stream stops when START is issued before
  the back end has settled (`0x98:F5` wait missing).
- `gchd_output_switch_hdmi`: output size changes with a fixed 1080p60 source
  (`docs/hdmi.md §4`, TODO T-055).

---

## Maintainer comments

