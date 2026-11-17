# TASK T-043a — Smooth composite 480i, then composite 240p

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** timestamp fix ready; waiting for the hardware test
- **Type:** driver fix + hardware test
- **Branch:** `fix/buffer-timestamps` (from `dev`)
- **TODO items:** Phase 3 T-020 (delivery quality), Phase 5 T-041/T-043

## Why

Composite 480i is accepted and merged into `dev` (TODO "Hardware test log").
ffplay stuttered now and then: buffers carried the delivery time, and each
64 KB bulk transfer holds several fields, so fields arrived in bursts with
nearly identical timestamps. Buffers now carry the stream's own decoding
timestamps (`6aaf465`).

Next in the bring-up order is composite 240p. The encoder is already
programmed as for 480i (as in the traces). A 480i ↔ 240p change while
capturing still uses a full encoder restart in the driver (TODO T-041); the
traces show a decoder-only change for 480i → 240p.

## Acceptance criteria

1. Composite 480i in ffplay: no periodic stutter over 60 s.
2. Composite 240p (source already in 240p when capture starts): ffplay shows
   a stable picture; the recording decodes without errors and `ffprobe`
   reports 720×480, `field_order tt`.
3. Report what happens when the source switches 480i → 240p and back while
   ffplay runs (expected to be imperfect until T-041).

## Hardware test (maintainer)

```sh
git fetch origin && git checkout fix/buffer-timestamps && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0     # 480i, ~60 s
```

Then with the source in 240p (reload the module first):

```sh
sudo rmmod elgato_gchd; sudo dmesg -C; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0     # 240p; then switch 480i <-> 240p
sudo dmesg > dmesg-240p.txt
ffmpeg -f v4l2 -input_format h264 -i /dev/video0 -t 10 -c copy test240p.h264
ffprobe -v error -show_entries stream=codec_name,level,width,height,field_order test240p.h264
ffmpeg -v error -i test240p.h264 -f null - ; echo "decode errors above (none = clean)"
```

To view the 240p recording without field blending:
`ffplay -vf separatefields,scale=720:480:flags=neighbor test240p.h264`.

Paste the outputs and observations into the maintainer comments below.

## Findings

_(filled in during the task)_

---

## Maintainer comments

_Written by the maintainer only. Anything lasting is transferred to
`RULES.md` or `TODO.md` when the next task is prepared, and this section is
then emptied._

