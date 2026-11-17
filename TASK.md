# TASK T-041a — Composite 480i ↔ 240p while capturing

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** mode switches during setup and before stop handled; waiting for the hardware test
- **Type:** driver fix + hardware test
- **Branch:** `fix/mode-change-restart` (from `dev`)
- **TODO items:** Phase 5 T-041, T-043; Phase 3 T-023 (audio, not in this task)

## Why

Control test: with a stable 240p source both `dev` and this branch start and
stream without problems. The source (N64 *Star Wars Episode I: Racer*)
switches between 480i and 240p by itself (menu ↔ attract demo), and every
failure so far happened around such a switch:

1. **Switch during setup** (0c64469, 5d31e07 tests): setup takes ~9 s after
   opening (6 s of it firmware loading). The driver configured the mode seen
   at detection; the source changed meanwhile and START never completed.
   Now: right before START the decoder status is read again; if the mode
   changed, the driver logs `source changed to … during setup; configuring
   again` and configures the new mode instead of starting.
2. **Switch noticed by the signal-loss check first:** the stop then went
   straight to NULL, which does not complete while the back end is set up
   for the old mode (40 s timeouts in several tests). Windows always applies
   the decoder setting and the neutral back-end table before NULL on a mode
   change. The redetection path now does the same when the decoder reports
   the other mode (`redetection: source is now …; preparing before stop`).
3. **Still open:** after a mode change while capturing (9382bcb and 2025d5f
   tests) the restarted stream ran for ~0.6 s and stopped. This test shows
   whether that still happens with the recorded encoder block and settle
   wait.

Audio is not delivered yet (TODO T-023).

## Acceptance criteria (this test)

1. Fresh start in 480i (menu) gives a picture.
2. The menu ↔ demo switches while capturing: picture returns after each
   switch within a few seconds.

## Hardware test (maintainer)

```sh
git fetch origin && git checkout fix/mode-change-restart && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
# start while the game shows the menu; then let it run through at least
# two menu <-> demo switches
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt"
```

Note when the game switched (menu → demo, demo → menu) and whether the
picture came back each time. Paste the output into the maintainer comments
below.

## Findings

- 8a4b2af: stopping the receive queue before NULL/STOP prevents NULL from
  completing.
- `gchd_240p-480i_composite`: NULL takes ~0.7 s and is preceded by the
  neutral back-end table; 480i → 240p is not decoder-only.
- 9382bcb test: the mode-change sequence completes, but every START after
  the first one in a power cycle streams only ~0.4 MB.
- `gchd_240p-480i_composite_input_part_*` (bulk data): the driver's receive
  queue handling during stop matches; no reads after STOP.
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

