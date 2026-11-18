# TASK T-020 — MPEG-TS stream with audio

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** implemented; waiting for the hardware test
- **Type:** driver feature
- **Branch:** `feat/ts-audio` (from `dev`)
- **TODO items:** T-020, T-023 (stage 1)

## Why

Composite and S-video work for all NTSC and PAL modes (`dev` 0.4.1). The
driver still delivers only the H.264 video, one PES packet per buffer, and
drops the audio: the device multiplexes MPEG-1 Layer II audio (PID `0x010F`)
into its transport stream. Applications such as ffplay, ffmpeg and OBS can
use the complete transport stream directly, with sound. It is also the
input for the planned guvcview/OBS bridge (T-060).

## Implementation

1. A second pixel format on the same node: `V4L2_PIX_FMT_MPEG`, next to
   `V4L2_PIX_FMT_H264` (still the default). The selected format is kept
   while the device stays connected, like the input.
2. MPEG buffers carry 348 whole TS packets (65 424 bytes) as received from
   the device, without null packets (PID `0x1FFF`), starting with a PAT
   packet after every stream start or restart: video, audio (`0x010F`),
   PAT/PMT, PCR.
3. ffmpeg's V4L2 input does not know the TS format, so the standard way for
   encoder cards applies: select the format once with `v4l2-ctl`, then read
   the device like a file (`read()`), e.g. `ffplay -f mpegts /dev/video0`.

## Acceptance criteria

1. `ffplay -f mpegts /dev/video0` shows the picture **with sound**
   (composite and S-video, NTSC and PAL).
2. A 20 s recording decodes without errors; `ffprobe` reports H.264 video
   and MP2 audio.
3. The H.264 format still works as before.

## Hardware test (maintainer)

```sh
git fetch origin && git checkout feat/ts-audio && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
v4l2-ctl -d /dev/video0 -v pixelformat=MPEG
v4l2-ctl -d /dev/video0 -V                 # should show "MPEG"
ffplay -f mpegts /dev/video0               # picture and sound?
# close; then a 20 s recording and checks:
ffmpeg -f mpegts -i /dev/video0 -t 20 -c copy test.ts
ffprobe test.ts
ffmpeg -v error -i test.ts -f null - && echo DECODE-OK
# back to H.264 (as before):
v4l2-ctl -d /dev/video0 -v pixelformat=H264
ffplay -f v4l2 -input_format h264 -i /dev/video0
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
```

Paste the outputs (ffplay, ffprobe, the DECODE-OK line, dmesg) into the
maintainer comments below and say whether there was sound.

## Findings

_(filled in during the task)_

---

## Maintainer comments

