# TASK T-020a — Composite 480i: a stable picture in guvcview

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** delivery fix ready on `fix/v4l2-format-negotiation`; waiting for
  the hardware test
- **Type:** driver fix, hardware acceptance
- **Branch:** `fix/v4l2-format-negotiation` (stacked on
  `fix/analog-start-per-traces`)
- **TODO item:** Phase 3, T-020 (first step); Phase 4, T-033

## Why

The composite 480i path now reaches streaming: START completes in about
0.1 s and data flows (TODO "Hardware test log"). guvcview shows a picture
briefly and then crashes. The driver delivered at most one frame per USB
completion, dropped frames when no buffer was queued, could deliver frames
out of order, and started at an arbitrary picture.

## Changes on the branch

1. `907af08` format negotiation: S_INPUT sets the input's nominal format;
   the negotiated format never changes while buffers are allocated;
   TRY_FMT/S_FMT adjust instead of failing; G_PARM/S_PARM; analog formats
   are reported interlaced.
2. `374fc83` delivery: every frame is delivered in order while buffers are
   queued (no drops when a buffer is late); delivery starts at the first
   picture carrying an SPS after each (re)start.

## Acceptance criteria

1. guvcview shows a stable composite 480i picture for at least 60 s without
   crashing.
2. The ffmpeg check below records a stream that `ffprobe` reports as H.264
   720×480 without decode errors.
3. dmesg shows no `V4L2 buffer error` and no `dropping PES` warnings.

If guvcview still crashes while the ffmpeg recording is clean, the next step
is to compare guvcview's H.264 handling with the buffer contents (one PES per
buffer, field-coded pictures).

## Hardware test (maintainer)

```sh
git fetch origin && git checkout fix/v4l2-format-negotiation && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
guvcview -d /dev/video0
sudo dmesg > dmesg-guvcview.txt
```

Then, with guvcview closed:

```sh
sudo rmmod elgato_gchd; sudo dmesg -C; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffmpeg -f v4l2 -input_format h264 -i /dev/video0 -t 10 -c copy test.h264
ffprobe -v error -show_entries stream=codec_name,width,height,field_order test.h264
ffmpeg -v error -i test.h264 -f null -
```

Paste the outputs (guvcview, dmesg, ffprobe, the last ffmpeg command) into
the maintainer comments below.

## Findings

_(filled in during the task)_

---

## Maintainer comments

_Written by the maintainer only. Anything lasting is transferred to
`RULES.md` or `TODO.md` when the next task is prepared, and this section is
then emptied._

