# Video output

## 1. Stream produced by the device

The device contains a hardware H.264 encoder (Fujitsu MB86H57/H58 or
MB86M01). In the encoding state (SCMD state `0x02`) it sends an MPEG-2
transport stream on bulk IN endpoint `0x81`. Encoding is done entirely on the
device.

Stream parameters as programmed by the driver (`dkms/transcoder.c`):

| Item | Value |
|---|---|
| Container | MPEG transport stream, 188-byte packets, null packets inserted (`auto_null = 2`) |
| Video | H.264, PID `0x1011`, stream id `0xE0`; B-frames enabled (`v_gop_struct = 3`) |
| Audio | MPEG-1 Layer II, PID `0x010F`, stream id `0xC0` |
| PSI | PAT (program 1 → PMT PID `0x0110`), PMT, SIT on PID `0x001F`; PCR PID `0x0100` |
| Output size | Equal to the input format, for example 720 × 480 for NTSC analog sources |

## 2. Picture structure for standard-definition analog sources (Observed)

| Property | Value |
|---|---|
| Size | 720 × 480 (NTSC family) |
| Coding | H.264, field-coded interlaced |
| Field rate | 60000/1001 fields per second (NTSC) |

A 240p source is carried in the same structure: each field contains one
complete 240-line picture, so the stream has 60000/1001 progressive pictures
per second stored as fields. Displaying it without blending fields requires
separating the fields rather than deinterlacing, for example with FFmpeg:

```sh
ffmpeg -i capture.ts -vf separatefields,scale=720:480:flags=neighbor out.mkv
```

A 480i source is displayed with a regular deinterlacer, for example
`-vf bwdif`.

## 3. Driver interface

The driver registers a V4L2 capture device (`/dev/videoN`) with the inputs
HDMI (0), Component (1), Composite (2) and S-Video (3).

| Item | Current behaviour |
|---|---|
| Pixel format | `V4L2_PIX_FMT_H264` |
| Buffer content | Payload of one video PES packet (PID `0x1011`) per buffer, starting with the first packet that contains an SPS; audio is not delivered |
| Timestamps | Decoding timestamp of the PES packet (DTS, or PTS if no DTS), converted to the monotonic clock and anchored at the first buffer of a stream |
| Hardware start | `VIDIOC_STREAMON`; firmware is loaded and the input configured at this point |
| Source changes | `V4L2_EVENT_SOURCE_CHANGE` is queued when the signal appears or is lost, and after a composite or S-video source changes between 480i and 240p or between 576i and 288p |
| Analog mode change | 480i ↔ 240p: the encoder is stopped and restarted for the new mode (`composite.md` §4); the stream resumes with a new SPS after about 1.5 s. 576i ↔ 288p: only the decoder is reconfigured (`composite.md` §8); the stream continues |

The interface is experimental and expected to change.
