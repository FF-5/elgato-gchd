# TASK T-020 — MPEG-TS stream with audio

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** implementation (offline), then hardware test
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

## Plan

1. A second pixel format on the same node: `V4L2_PIX_FMT_MPEG` (MPEG-TS),
   next to the existing `V4L2_PIX_FMT_H264`. Buffers carry whole 188-byte
   TS packets as received (video, audio, PAT/PMT, PCR), aligned to packet
   boundaries; null packets are dropped.
2. TS resynchronisation: a packet start is accepted only when `0x47`
   appears at 188-byte spacing several times.
3. The H.264 format stays unchanged as the default, so existing commands
   keep working.
4. Mode changes: after a restart the stream continues; buffers resume with
   PAT/PMT.

## Acceptance criteria

1. `ffplay -f v4l2 -input_format mpegts -i /dev/video0` shows the picture
   with sound (composite and S-video, NTSC and PAL).
2. A 20 s recording with ffmpeg decodes without errors (video H.264 and
   audio MP2); `ffprobe` reports both streams.
3. The H.264 format still works as before.

## Hardware test (maintainer)

_(written when the implementation is ready)_

## Findings

_(filled in during the task)_

---

## Maintainer comments

