# Video output: what the device emits and how Linux should expose it

## What the hardware produces

The Game Capture HD has a hardware H.264 encoder (Fujitsu MB86H57/H58 on the
original HD, MB86M01 on HD-New). After `SCMD START` it streams a finished
**MPEG-2 transport stream** on bulk IN `0x81`. The host never encodes anything.

With the C++ transcoder settings (`src/gchd/transcoder.cpp`):

| Item | Value |
|---|---|
| Container | MPEG-TS, 188-byte packets, null packets inserted (`auto_null = 2`) |
| Video | H.264 High profile, PID `0x1011`, stream id `0xE0`, B-frames (`v_gop_struct = 3`), GOP `(fps+10)/5` |
| Audio | MPEG-1 Layer II, PID `0x010F`, stream id `0xC0`; 384 kbit/s in the defaults, 320 kbit/s after `transcoderSetup` (C++ default setting) |
| PSI | PAT (program 1 → PMT `0x0110`), PMT with HDMV descriptors, SIT on `0x001F`, PCR PID `0x0100` |
| SD analog | 720×480 (NTSC) or 720×576 (PAL), **field-coded interlaced**, 60 or 50 fields/s |

The reference recordings in `evidence/reference-ts/` (PIDs `0x64`/`0x66`,
probably made with the Elgato software) show the same picture structure:
720×480, `field_order = tt`, High profile, level 3.1, `has_b_frames = 4`,
60000/1001 field rate. A second elementary stream is typed as MPEG-2 video
with no frames; its purpose is unknown.

## The Windows "MP4 codec" question

The Windows package contains a software H.264 decoder (and an MP4 muxer) for
**preview and recording**: it decodes the hardware-encoded stream so that the
Elgato application can display it, and remuxes it into MP4. No decompiled DLL
is present anywhere in this repository's history, so the exact component
cannot be confirmed. Its role is clear from the data path either way: nothing
on the USB side needs a host-side codec.

Consequences for Linux:

- A kernel driver **cannot and should not** contain an H.264 decoder.
- The compressed stream can be handed to userspace unchanged. FFmpeg/GStreamer
  decode it trivially; the C++ project already relied on that (`gchd` →
  FIFO/file/UDP → OBS/VLC/ffmpeg).

## Recommended Linux interface

### Kernel: pass the transport stream through (hdpvr model)

Expose the TS unchanged on the V4L2 capture node as `V4L2_PIX_FMT_MPEG`
(`V4L2_CAP_READWRITE | V4L2_CAP_STREAMING`). Mainline already uses this model
for a nearly identical device, the Hauppauge HD PVR (`drivers/media/usb/hdpvr`,
USB H.264 TS encoder), as well as `pvrusb2`, `cx18` and `ivtv`.

Why this beats the current in-kernel PES → H.264 extraction (`gchd_ts()` in
`dkms/core.c`):

| Current DKMS (`V4L2_PIX_FMT_H264`, one PES per buffer) | TS passthrough (`V4L2_PIX_FMT_MPEG`) |
|---|---|
| Audio is discarded | Audio preserved |
| PTS/DTS discarded; buffers stamped with `ktime_get_ns()` | Exact device timestamps preserved; B-frame reordering works |
| A lossy ring drops the **oldest** PES when full, and `gchd_deliver()` hands out at most one PES per USB completion. At 60 fields/s with B-frames this drops reference pictures → corrupted ("malformed") decode | Byte stream; nothing to reorder or drop as long as the reader keeps up |
| Field-coded pictures arrive as separate PES; consumers expecting one frame per buffer misbehave | Demuxer (FFmpeg) handles field pairing |
| Fragile resync on any `0x47` byte | Resync on 188-byte boundaries with several consecutive sync bytes |
| ~400 lines of parsing in kernel | Copy bytes into vb2 buffers / `read()` |

`ffplay /dev/videoN`, `ffmpeg -f mpegts -i /dev/videoN …`, VLC and
GStreamer (`v4l2src ! tsdemux`) all accept such a node directly. This is the
"pipe the TS into the V4L2 device" idea, done by the kernel itself.

### Userspace bridge for raw-frame consumers

Applications that only accept raw frames (browsers, some webcam apps) need a
decoded node. Keep this in userspace (as `VIDEO_OUTPUT.md` on the
`video-output-lazy-init` branch already concluded):

```
/dev/videoN (MPEG-TS)  →  ffmpeg (decode + deinterlace)  →  v4l2loopback /dev/videoM (YUYV/NV12)
```

Example for a 480i source:

```sh
ffmpeg -f mpegts -i /dev/videoN -vf bwdif=mode=send_field -pix_fmt yuyv422 -f v4l2 /dev/videoM
```

Example for a **240p** source (each field is a complete picture, so do not
blend fields):

```sh
ffmpeg -f mpegts -i /dev/videoN -vf separatefields,scale=720:480:flags=neighbor -r 60000/1001 -pix_fmt yuyv422 -f v4l2 /dev/videoM
```

Demand-driven start/stop of the bridge (a udev rule plus a systemd unit, or a
small daemon) is a later packaging task. It does not affect the kernel design.

## 240p handling, end to end

1. **Hardware:** program the encoder exactly as for 480i (`v_format 4`,
   `v_ip_format 0`). Windows does this ([composite.md](composite.md)).
2. **Kernel:** report the format honestly. The stream is 720×480 interlaced.
   Expose the 240p lock (`0x88:CA = 66`) as information (for example a
   read-only control or `V4L2_EVENT_SOURCE_CHANGE` plus a status in
   `VIDIOC_G_INPUT` / `ENUMINPUT`). Do not pretend it is progressive.
3. **Userspace:** pick `separatefields` (sharp 240p, 60 fps) vs a deinterlacer
   (480i) based on that status, or by user choice.
