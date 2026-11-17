# TASK T-041a — Composite 480i ↔ 240p while capturing

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** fix ready; waiting for the hardware test
- **Type:** driver fix + hardware test
- **Branch:** `fix/mode-change-restart` (from `dev`)
- **TODO items:** Phase 5 T-041, T-043

## Why

Composite 480i and 240p both start and stream (merged into `dev`). Switching
240p → 480i while capturing failed (TODO "Hardware test log"): the stop took
~70 s, then the driver put the device into IDLE (0x11), which INIT 0xa0 does
not leave, so START timed out. The driver now restarts from the stopped state
(0x01) as in the traces, and the stop no longer waits on 1 s timeouts.

480i → 240p still uses the same restart path; the traces show a
decoder-only change for that direction (next step once the restart works).

## Acceptance criteria

1. 240p → 480i while ffplay runs: the picture returns as 480i within a few
   seconds.
2. 480i → 240p while ffplay runs: the picture returns as 240p.
3. dmesg shows "analog mode change detected" and "analog mode now …", and no
   "START failed".

## Hardware test (maintainer)

```sh
git fetch origin && git checkout fix/mode-change-restart && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
# switch the source 240p -> 480i, wait, then 480i -> 240p
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested"
```

ffplay may need to be restarted after a switch if it does not recover by
itself; note whether it did. Paste the output and observations into the
maintainer comments below.

## Findings

_(filled in during the task)_

---

## Maintainer comments

_Written by the maintainer only. Anything lasting is transferred to
`RULES.md` or `TODO.md` when the next task is prepared, and this section is
then emptied._

