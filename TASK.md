# TASK T-041a — Composite 480i ↔ 240p while capturing

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** stop/restart works; mode switches while capturing to be tested
- **Type:** driver fix + hardware test
- **Branch:** `fix/mode-change-restart` (from `dev`)
- **TODO items:** Phase 5 T-041, T-043; Phase 3 T-023 (audio, not in this task)

## Why

Last test (39f63f2, stable 240p, xHCI): two sessions in a row work, the
close finishes cleanly, detection waits for a locked mode. The restart that
a mode change needs therefore works now. What remains for this task is the
mode change itself while capturing (480i ↔ 240p, `docs/composite.md §4`),
which has not been tested since the stop/restart fixes.

## Acceptance criteria (this test)

1. Starting in either mode gives a picture.
2. Each menu ↔ demo switch while capturing: the picture returns within a
   few seconds, without closing ffplay.
3. Closing and reopening ffplay afterwards still works.

## Hardware test (maintainer)

VirtualBox with the USB 3.0 (xHCI) controller.

```sh
git fetch origin && git checkout fix/mode-change-restart && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
# let the game switch menu <-> demo at least twice (both directions);
# then close ffplay and start it once more
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats"
```

Note roughly when the game switched and whether the picture came back each
time (and whether ffplay had to be restarted). Paste the output into the
maintainer comments below.

## Findings

- 8a4b2af: stopping the receive queue before NULL/STOP prevents NULL from
  completing.
- `gchd_240p-480i_composite`: NULL takes ~0.7 s and is preceded by the
  neutral back-end table; 480i → 240p is not decoder-only.
- 9382bcb test: the mode-change sequence completes, but every START after
  the first one in a power cycle streams only ~0.4 MB.
- `gchd_240p-480i_composite_input_part_*` (bulk data): the driver's receive
  queue handling during stop matches; no reads after STOP.
- 39f63f2 test: close and reopen work (stable 240p, xHCI).
- f90494a test: any second START fails because closing ffplay stopped the
  stream after joining the receive thread (NULL never completes); detection
  accepted `CA = EE`.
- 3df7591 test: after the setup-time mode check cancelled the queued
  receive transfers, every START stalled.
- Control test: stable 240p works on `dev` and on this branch; the source
  switches 480i ↔ 240p by itself.
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

