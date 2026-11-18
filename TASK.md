# TASK T-020 — MPEG-TS stream with audio

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** TS works on three of four combinations; driver buffer enlarged; PS1 S-video to test
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

1. TS format, all four combinations (N64 composite/S-video, PS1
   composite/S-video): picture and sound (three confirmed in the last test).
2. No decode messages a few seconds after the start any more (they came
   from the driver buffer overflowing while ffplay/ffmpeg set up after
   analysing the stream); `ring_dropped=0` in the stats lines.
3. H.264 format unchanged.

## Hardware test (maintainer)

PS1 S-video (missing from the last test) and one N64 run:

```sh
git fetch origin && git checkout feat/ts-audio && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 3               # PS1 on S-video; 2 for composite
v4l2-ctl -d /dev/video0 -v pixelformat=MPEG
ffplay -f mpegts /dev/video0
# close; then:
ffmpeg -f mpegts -i /dev/video0 -t 20 -c copy test.ts
ffmpeg -v error -i test.ts -f null - ; echo exit=$?
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|callbacks suppressed"
```

This time the `async bulk-IN stats` lines stay in the output (they show
`ring_dropped=`). Paste the outputs into the maintainer comments below.

## Findings

- 38057b9 test: TS format works, video H.264 and MP2 audio 48 kHz stereo
  320 kbit/s, 20 s recording decodes. ffprobe showed "Program 1" with an
  empty video stream on PID `0x11` and the real video under "No Program":
  the driver's PMT (taken over from the C++) had wrong bytes. Replaced with
  the PSI tables Windows writes (`input-switching/filtered_init_switch_to_component`
  1.07-1.14 s). Decode errors only at the start and at the encoder restart
  of a 240p → 480i change.
- 1e86b93 test (S-video NTSC): video and audio inside "Program 1"; sound.
  Decode messages at the start (partial PES before the PAT) and at the
  encoder restarts. Now video and audio start with a complete PES packet
  after the PAT.
- 0d47f71 test: N64 composite and S-video, PS1 composite work in TS mode
  (PS1 S-video not in the pasted output). Decode messages at the same
  stream time in every run (dts 343080 ≈ 3.8 s, ≈ 5 s), right after
  ffplay/ffmpeg finish analysing the first ~5 MB: the driver buffer held
  only 32 entries (2 MB in TS mode, ~0.5 s in H.264 mode) and dropped
  buffers while the application set up. Raised to 512 entries (the 64 MB
  byte limit applies).

---

## Maintainer comments

