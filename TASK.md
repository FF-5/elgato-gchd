# TASK T-043b — Composite 240p start; stutter diagnosis

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** fixes ready; waiting for the hardware test
- **Type:** driver fix + hardware test
- **Branch:** `fix/composite-240p-start` (stacked on `fix/buffer-timestamps`)
- **TODO items:** Phase 5 T-043, Phase 3 T-020

## Why

Last test (TODO "Hardware test log"): with a 240p source the decoder
reported 240p (`0x88:CA = 66`) but the driver configured 480i, and START
never completed. The driver now configures 240p as recorded in the traces.
The 480i stutter is still unexplained; the 5 s statistics line now shows
whether frames are discarded by the driver (`ring_dropped`).

## Acceptance criteria

1. Composite 240p, source in 240p before capture starts: dmesg shows
   "configuring 240p", START completes, ffplay shows a picture, the
   recording decodes without errors.
2. Composite 480i still works as before.
3. The statistics lines during 480i playback are reported (`ring_dropped`,
   `frames_waiting`).

## Hardware test (maintainer)

240p (source in 240p before starting):

```sh
git fetch origin && git checkout fix/composite-240p-start && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
sudo dmesg > dmesg-240p.txt
ffmpeg -f v4l2 -input_format h264 -i /dev/video0 -t 10 -c copy test240p.h264
ffmpeg -v error -i test240p.h264 -f null - ; echo "decode errors above (none = clean)"
```

480i (reload the module first), watch for stutter for ~60 s:

```sh
sudo rmmod elgato_gchd; sudo dmesg -C; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
sudo dmesg | grep -E "configuring|START transition|bulk-IN stats|no analog family"
```

Paste the outputs and what ffplay showed into the maintainer comments below.

## Findings

_(filled in during the task)_

---

## Maintainer comments

_Written by the maintainer only. Anything lasting is transferred to
`RULES.md` or `TODO.md` when the next task is prepared, and this section is
then emptied._

