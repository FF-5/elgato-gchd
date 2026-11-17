# TASK T-041a — Composite 480i ↔ 240p while capturing

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** close path and detection fixed; waiting for the hardware test
- **Type:** driver fix + hardware test
- **Branch:** `fix/mode-change-restart` (from `dev`)
- **TODO items:** Phase 5 T-041, T-043; Phase 3 T-023 (audio, not in this task)

## Why

Test A/B answered the question: **any second START fails, also with a
stable source**, and the cause is how the first session ends:

- **Closing ffplay:** the driver joined the receive thread first and then
  sent NULL and STOP. Without the receive queue NULL never completes (the
  same was seen in the 8a4b2af test; the Windows recording keeps the
  receive transfers queued through NULL and STOP). The shutdown failed
  after 60 s (-110, xHCI: -5), the device stayed in a broken state, and
  every later START stalled. The mode-change path had been fixed earlier;
  the close path (and the system-shutdown path) had not. Both now stop the
  stream with the receive queue running and join the thread afterwards.
- **Detection (xHCI run):** the decoder status read `EE` (transitional) and
  was treated as 480i for a 240p source, so START hung. Detection now waits
  until `0x88:CA` reads `7E` (480i) or `66` (240p).

xHCI is much faster (firmware load 0.8 s instead of 6 s): keep the VM on the
USB 3.0 (xHCI) controller from now on.

## Acceptance criteria (this test)

1. Stable 240p: first session shows a picture; closing ffplay finishes
   without "shutdown … failed"; a second session shows a picture again.
2. Then with the game switching menu ↔ demo while capturing.

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
# stable 240p: picture, 20 s, close (q), wait 5 s, start ffplay again;
# if the second session works: let the game switch menu <-> demo twice
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt"
```

Paste the output into the maintainer comments below and note when the game
switched.

## Findings

- 8a4b2af: stopping the receive queue before NULL/STOP prevents NULL from
  completing.
- `gchd_240p-480i_composite`: NULL takes ~0.7 s and is preceded by the
  neutral back-end table; 480i → 240p is not decoder-only.
- 9382bcb test: the mode-change sequence completes, but every START after
  the first one in a power cycle streams only ~0.4 MB.
- `gchd_240p-480i_composite_input_part_*` (bulk data): the driver's receive
  queue handling during stop matches; no reads after STOP.
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

