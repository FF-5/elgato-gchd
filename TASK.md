# TASK T-020a — Composite 480i: a stable picture in guvcview

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** delivery fix ready on `fix/v4l2-format-negotiation`; waiting for
  the hardware test
- **Type:** driver fix, hardware acceptance
- **Branch:** `fix/v4l2-format-negotiation` (stacked on
  `fix/analog-start-per-traces`)
- **TODO item:** Phase 3, T-020 (first step); Phase 4, T-033

## Why

The composite 480i path reaches streaming and the driver output is valid
H.264 (TODO "Hardware test log", third entry: ffmpeg recorded 720×480,
field order tt). guvcview still crashes; its symptoms point to guvcview
itself (see the test log). The default bitrate was twice the recorded value
and is fixed.

## Changes on the branch

1. `907af08` format negotiation (S_INPUT nominal format, no format change
   while buffers are allocated, TRY_FMT/S_FMT adjust, G_PARM/S_PARM).
2. `374fc83` delivery: frames delivered in order, no drops while waiting for
   a buffer, start at the first SPS.
3. `81bc5ec` default bitrate 11250 kbit/s at 720 wide, level 3.1.

## Acceptance criteria

1. `ffplay` shows a stable composite 480i picture for at least 60 s.
2. The decode check below prints no errors.
3. The recording is about 11–12 Mbit/s and `ffprobe` reports level 31.
4. dmesg shows no `V4L2 buffer error` and no `dropping PES` warnings.

guvcview is not an acceptance criterion while its 720-wide H.264 handling is
in question; try it once anyway and report the result.

## Hardware test (maintainer)

```sh
git fetch origin && git checkout fix/v4l2-format-negotiation && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0      # watch ~60 s, then q
sudo dmesg > dmesg-ffplay.txt
```

Recording and decode check:

```sh
ffmpeg -f v4l2 -input_format h264 -i /dev/video0 -t 10 -c copy test.h264
ffprobe -v error -show_entries stream=codec_name,profile,level,width,height,field_order test.h264
ffmpeg -v error -i test.h264 -f null - ; echo "decode errors above (none = clean)"
ls -l test.h264
```

Finally `guvcview -d /dev/video0` once. Paste the outputs into the
maintainer comments below.

## Findings

_(filled in during the task)_

---

## Maintainer comments

_Written by the maintainer only. Anything lasting is transferred to
`RULES.md` or `TODO.md` when the next task is prepared, and this section is
then emptied._

