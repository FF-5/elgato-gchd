# TASK T-041a — Composite 480i ↔ 240p while capturing

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** back-end settle wait before START; waiting for the hardware test
- **Type:** driver fix + hardware test
- **Branch:** `fix/mode-change-restart` (from `dev`)
- **TODO items:** Phase 5 T-041, T-043; Phase 3 T-023 (audio, not in this task)

## Why

Last test (0c64469): even the first START after plugging in did not
complete, and every later START streamed only a few transfers. That points
to the encoder not getting video from the analog back end.

The recordings show a wait the driver did not implement. Before every
capture start Windows does:

1. retrigger: read `0x98:F5`, `FD`, `FC`, `F3`, `F5`; `0D = C8`; read `F5`;
   `0E = pulse`, `pulse & ~1`, where the pulse is `05` or `15` depending on
   bit 4 of the `F5` read just before it (44 of 44 recorded pulses);
2. poll `0x98:F5` about every 4 ms: `62` until bit 7 is set (`F2`,
   40–700 ms);
3. retrigger again (now `15`);
4. poll `0x98:F5` until it reads `72`; only then INIT A0 and START.

The driver sent the second pulse right after the first, without the `F5`
reads and waits, so whether the back end delivered video depended on
timing. It now follows the recorded sequence (initial configuration and
mode change). If a wait times out, dmesg shows
`back-end status 0x.. after N polls, waiting for ...`.

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

Note for each step whether a picture appeared. Paste the output into the
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
- 0c64469 test: START hangs or the stream stops when START is issued before
  the back end has settled (`0x98:F5` wait missing).
- `gchd_output_switch_hdmi`: output size changes with a fixed 1080p60 source
  (`docs/hdmi.md §4`, TODO T-055).

---

## Maintainer comments

