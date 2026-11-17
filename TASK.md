# TASK T-041a — Composite 480i ↔ 240p while capturing

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** test whether any second START fails (stable source)
- **Type:** driver fix + hardware test
- **Branch:** `fix/mode-change-restart` (from `dev`)
- **TODO items:** Phase 5 T-041, T-043; Phase 3 T-023 (audio, not in this task)

## Why

In every test so far only the **first START after plugging in** streamed.
Every later START (after a mode change, after redetection, after closing and
reopening ffplay, even after reloading the firmware) delivered 1–7 transfers
and stopped, and NULL never completed afterwards. In the last test the
"source changed during setup" check worked, but it ran after the receive
transfers had been queued; they were cancelled without data, and from then
on every START stalled.

The open question: is this tied to the source switching, or does any second
START fail? The control test never had a second START. This test answers it
with a stable source and no mode change, on the same build:

- If the second session also stalls: the restart itself is broken (driver,
  or the USB passthrough of the VM). Test B then separates the two.
- If the second session works: the problem is specific to mode changes.

Change in this build: the mode check now runs before the receive transfers
are queued, so they are no longer queued and cancelled for nothing.

## Acceptance criteria (this test)

1. Test A answered: does a second session (close and reopen ffplay, no
   replug) show a picture with a stable 240p source?
2. If not: test B with the VirtualBox USB 3.0 (xHCI) controller.

## Hardware test (maintainer)

Test A: stable 240p source (as in the control test), this branch.

```sh
git fetch origin && git checkout fix/mode-change-restart && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
# picture appears: let it run 20 s, close the window (q), wait 5 s, then
# start the same ffplay command again (no replug); wait up to 30 s
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt"
```

Test B (only if the second session in test A has no picture): shut the VM
down, set *Settings → USB → USB 3.0 (xHCI) Controller*, start the VM, and
repeat test A. If a Linux machine without virtualisation (or a live USB
stick) is available, that is even better than xHCI.

Paste the outputs (labelled A, B) into the maintainer comments below.

## Findings

- 8a4b2af: stopping the receive queue before NULL/STOP prevents NULL from
  completing.
- `gchd_240p-480i_composite`: NULL takes ~0.7 s and is preceded by the
  neutral back-end table; 480i → 240p is not decoder-only.
- 9382bcb test: the mode-change sequence completes, but every START after
  the first one in a power cycle streams only ~0.4 MB.
- `gchd_240p-480i_composite_input_part_*` (bulk data): the driver's receive
  queue handling during stop matches; no reads after STOP.
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

