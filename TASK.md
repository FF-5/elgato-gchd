# TASK T-041a — Composite 480i ↔ 240p while capturing

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** control test (`dev` vs. this branch) and source mode needed
- **Type:** driver fix + hardware test
- **Branch:** `fix/mode-change-restart` (from `dev`)
- **TODO items:** Phase 5 T-041, T-043; Phase 3 T-023 (audio, not in this task)

## Why

The last three tests on this branch failed at or right after the first
START, including on a freshly plugged-in device. Before going further it has
to be clear whether this is caused by the changes on this branch or by the
source:

- In the 0c64469 and 5d31e07 tests the decoder reported 480i (`CA = 7E`) at
  detection, START never completed, and ~15 s later it reported 240p
  (`CA = 66`). In the 2025d5f test it reported 240p at detection and 480i
  0.3 s after START. START succeeds only when the back end is configured for
  the mode the source really outputs; a source that changes between 480i and
  240p (or a decoder that reports the wrong one) produces exactly these logs.
- `dev` (240p and 480i fresh starts accepted earlier) has none of the changes
  of this branch.

## Acceptance criteria (this test)

1. Known source mode, stated by the maintainer.
2. `dev` and this branch both started from a freshly plugged-in device with
   that source, without switching anything.

## Hardware test (maintainer)

Please first tell me **what the source is** (device, and whether it outputs
240p, 480i, or changes between them by itself, e.g. menu vs. game).

Test A: `dev`, source left in one mode, nothing switched.

```sh
git fetch origin && git checkout --detach origin/dev
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
# wait up to 30 s for the window; note whether a picture appears and stays
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested"
```

Test B: the same with this branch (`git checkout fix/mode-change-restart &&
git pull`, same commands; unplug and replug again before `insmod`).

Paste both outputs, labelled A and B, into the maintainer comments below.

The delay before the ffplay window opens: ffplay first analyses the stream
for up to 5 s before it shows anything. Adding
`-fflags nobuffer -probesize 32k -analyzeduration 500000` shortens that; it
is not needed for this test.

## Findings

- 8a4b2af: stopping the receive queue before NULL/STOP prevents NULL from
  completing.
- `gchd_240p-480i_composite`: NULL takes ~0.7 s and is preceded by the
  neutral back-end table; 480i → 240p is not decoder-only.
- 9382bcb test: the mode-change sequence completes, but every START after
  the first one in a power cycle streams only ~0.4 MB.
- `gchd_240p-480i_composite_input_part_*` (bulk data): the driver's receive
  queue handling during stop matches; no reads after STOP.
- 0c64469 / 5d31e07 / 2025d5f tests: decoder status flips between 480i and
  240p around the first START; START only completes for the matching mode.
- 2025d5f test: encoder programming before START differed from every
  recording (missing `dma_sel_out` etc.); fixed.
- 0c64469 test: START hangs or the stream stops when START is issued before
  the back end has settled (`0x98:F5` wait missing).
- `gchd_output_switch_hdmi`: output size changes with a fixed 1080p60 source
  (`docs/hdmi.md §4`, TODO T-055).

---

## Maintainer comments

