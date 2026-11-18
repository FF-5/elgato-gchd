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
| Size | 720 × 480 (60 Hz family), 720 × 576 (50 Hz family) |
| Coding | H.264, field-coded interlaced, top field first |
| Frame rate | 30000/1001 frames (60000/1001 fields) per second; 25 frames (50 fields) per second |

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
| Pixel formats | `V4L2_PIX_FMT_H264` (default) and `V4L2_PIX_FMT_MPEG`; the selected format is kept while the device stays connected |
| Buffer content, H.264 | Payload of one video PES packet (PID `0x1011`) per buffer, starting with the first packet that contains an SPS; no audio |
| Buffer content, MPEG | 348 transport-stream packets of 188 bytes (65 424 bytes) per buffer, as sent by the device without null packets, starting with a PAT packet after each stream start; contains video, audio (PID `0x010F`) and PSI |
| I/O | Streaming (mmap) and `read()`; with `read()` the MPEG format is read like a transport-stream file |
| Timestamps | H.264: decoding timestamp of the PES packet (DTS, or PTS if no DTS), converted to the monotonic clock and anchored at the first buffer of a stream. MPEG: time the buffer was completed (timing is carried in the stream) |
| Format of composite and S-video | Size 720 × 480 or 720 × 576 as the source delivers it (`VIDIOC_G_FMT` reports the current source, also while capturing; `VIDIOC_S_FMT` cannot change it); `V4L2_FIELD_INTERLACED_TB` (also for 240p/288p sources), frame rate (`VIDIOC_G_PARM`, `VIDIOC_ENUM_FRAMEINTERVALS`) 30000/1001 or 25/1. Before a source has been detected: the values of the standard last detected on that input while the module is loaded, NTSC before the first detection |
| Analog standard | `VIDIOC_QUERYSTD`: `V4L2_STD_NTSC_M`, `V4L2_STD_PAL` or `V4L2_STD_PAL_60` (60 Hz with PAL colour) of the detected source, `V4L2_STD_UNKNOWN` while no source is detected. `VIDIOC_G_STD`: the last detected standard. `VIDIOC_S_STD` accepts these standards and changes nothing (the source decides). HDMI and component: `ENODATA` |
| Input status | `VIDIOC_ENUMINPUT`: `V4L2_IN_ST_NO_SIGNAL` for the selected input while no source is detected; composite and S-video list the three standards |
| Hardware start | `VIDIOC_STREAMON`; firmware is loaded and the input configured at this point |
| Source changes | `V4L2_EVENT_SOURCE_CHANGE` is queued when the signal appears or is lost, and after a composite or S-video source changes between 480i and 240p or between 576i and 288p |
| Analog mode change | 480i ↔ 240p: the encoder is stopped and restarted for the new mode (`composite.md` §4); the stream resumes with a new SPS after about 1.5 s. 576i ↔ 288p: only the decoder is reconfigured (`composite.md` §8); the stream continues |

The interface is experimental and expected to change.
