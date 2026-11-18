# Video output

## 1. Stream produced by the device

The device contains a hardware H.264 encoder (Fujitsu MB86H57/H58 or
MB86M01). In the encoding state (SCMD state `0x02`) it sends an MPEG-2
transport stream on bulk IN endpoint `0x81`. Encoding is done entirely on the
device.

Stream parameters as programmed by the driver (`src/transcoder.c`):

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

The driver registers these V4L2 devices:

| Device (card name) | Direction | Registered | Content |
|---|---|---|---|
| "Elgato Game Capture HD (H.264)" | capture | per connected Game Capture HD | The compressed stream of the device (this section) |
| "Elgato Game Capture HD" | capture | while the module is loaded | Uncompressed frames for applications (§4) |
| "Elgato Game Capture HD (frame input)" | output | while the module is loaded | Frames for the application device, written by a userspace program (§4) |

The H.264 device has the inputs HDMI (0), Component (1), Composite (2) and
S-Video (3).

| Item | Current behaviour |
|---|---|
| Pixel formats | `V4L2_PIX_FMT_H264` (default) and `V4L2_PIX_FMT_MPEG`; the selected format is kept while the device stays connected |
| Buffer content, H.264 | Payload of one video PES packet (PID `0x1011`) per buffer, starting with the first packet that contains an SPS; no audio |
| Buffer content, MPEG | 348 transport-stream packets of 188 bytes (65 424 bytes) per buffer, as sent by the device without null packets, starting with a PAT packet after each stream start; contains video, audio (PID `0x010F`) and PSI |
| I/O | Streaming (mmap) and `read()`; with `read()` the MPEG format is read like a transport-stream file |
| Timestamps | H.264: decoding timestamp of the PES packet (DTS, or PTS if no DTS), converted to the monotonic clock and anchored at the first buffer of a stream. MPEG: time the buffer was completed (timing is carried in the stream) |
| Output size of composite and S-video | `VIDIOC_S_FMT` (while no buffers are allocated) selects the source size (720 × 480 / 576, any other requested size), 640 × 480 or 480 × 360 (mobile); kept while the device stays connected. A capture with a scaled output starts with the standard output and switches to the scaled one right after START (NULL, STOP, encoder settings, START, as for a size change while capturing); the standard picture of this first second is not delivered. The two small sizes are encoded progressive (`V4L2_FIELD_NONE`) at 25 or 30000/1001 frames per second on transport output 2 (`composite.md` §10). `VIDIOC_ENUM_FRAMESIZES` lists the three |
| Format of composite and S-video | With the source size: 720 × 480 or 720 × 576 as the source delivers it (`VIDIOC_G_FMT` reports the current source, also while capturing); `V4L2_FIELD_INTERLACED_TB` (also for 240p/288p sources); frame rate (`VIDIOC_G_PARM`, `VIDIOC_ENUM_FRAMEINTERVALS`) 30000/1001 or 25/1. Before a source has been detected: the values of the standard last detected on that input (remembered across module loads, parameters `std_composite` and `std_svideo`), NTSC before the first detection |
| Analog standard | `VIDIOC_QUERYSTD`: `V4L2_STD_NTSC_M` (60 Hz sources) or `V4L2_STD_PAL` (50 Hz sources) of the detected source, `V4L2_STD_UNKNOWN` while no source is detected. `VIDIOC_G_STD`: the last detected standard. `VIDIOC_S_STD` accepts these standards and changes nothing (the source decides). HDMI and component: `ENODATA` |
| HDMI and component timings | `VIDIOC_G/S_DV_TIMINGS`, `VIDIOC_ENUM_DV_TIMINGS`, `VIDIOC_DV_TIMINGS_CAP` with the CEA-861 timings of the supported modes (1080p60, 1080i60, 720p60, 576p50/576i50, 480p/480i 59.94); `VIDIOC_QUERY_DV_TIMINGS` reports the detected mode or `ENOLINK` without a signal. Before a detection or `S_DV_TIMINGS`: 720p60. `V4L2_CID_DV_RX_POWER_PRESENT` (read-only): 1 while a source is detected on the selected HDMI/component input |
| Input status | `VIDIOC_ENUMINPUT`: `V4L2_IN_ST_NO_SIGNAL` for the selected input while no source is detected; composite and S-video list the two standards. `VIDIOC_S_INPUT` returns `EBUSY` for another input while a different file handle owns the capture buffers |
| Encoder controls | `V4L2_CID_MPEG_VIDEO_BITRATE` (composite and S-video: 2–15 Mbit/s; default 11.25 Mbit/s, 15 Mbit/s for 480 × 360), `V4L2_CID_MPEG_VIDEO_BITRATE_MODE` (CBR default, VBR; 480 × 360 is always VBR), "Stretch SD Input" (composite and S-video), `V4L2_CID_MPEG_VIDEO_H264_PROFILE`, `_LEVEL`. On composite and S-video a change while capturing restarts the encoder (about 2 s); on HDMI and component the controls can be changed only while not capturing |
| Hardware start | `VIDIOC_STREAMON`; firmware is loaded and the input configured at this point |
| Source changes | `V4L2_EVENT_SOURCE_CHANGE` is queued when the signal appears or is lost, and after a composite or S-video source changes between 480i and 240p or between 576i and 288p |
| Analog mode change | 480i ↔ 240p: the encoder is stopped and restarted for the new mode (`composite.md` §4); the stream resumes with a new SPS after about 1.5 s. 576i ↔ 288p: only the decoder is reconfigured (`composite.md` §8); the stream continues |

The interface is experimental and expected to change.

## 4. Application device (Driver)

The application device delivers uncompressed frames. It is registered when
the module is loaded and stays registered while the module is loaded,
whether a Game Capture HD is connected or not; the first connected Game
Capture HD is attached to it. The driver does not decode the stream: a
userspace program reads the transport stream of the attached device,
decodes it and writes the frames into the frame input device; the driver copies each written
frame into the next queued buffer of the application device. Frames
written while no buffer is queued are dropped.

| Item | Behaviour |
|---|---|
| Format | `V4L2_PIX_FMT_YUYV`, `V4L2_FIELD_NONE`. Sizes (`VIDIOC_ENUM_FRAMESIZES`): the encoder's output sizes of the current input of the attached device; composite and S-video: the source size (720 × 480 or 720 × 576, also for 240p/288p) at 59.94 or 50 frames per second (one frame per field), 640 × 480 and 480 × 360 at 29.97 or 25 (progressive); HDMI and component: the source size at its rate. One frame rate per size (`VIDIOC_ENUM_FRAMEINTERVALS`, `VIDIOC_G/S_PARM`). `VIDIOC_S_FMT` selects a size (nearest offered); `EBUSY` for another size while buffers are allocated. The frames are not scaled: the userspace program sets the encoder to the selected size |
| Format changes | While no buffers are allocated, the list and the format follow the source of the attached device (a selected 640 × 480 or 480 × 360 is kept); before the first detection the remembered standard of the input applies (`std_composite`, `std_svideo`), NTSC when none. While an application streams, the format does not change; when the encoder delivers another size than the selected one, `V4L2_EVENT_SOURCE_CHANGE` is queued and no frames are delivered until the application selects the new format |
| Inputs | HDMI (0), Component (1), Composite (2), S-Video (3) of the attached device; `VIDIOC_S_INPUT` selects the input (also while capturing). Without an attached device the inputs report `V4L2_IN_ST_NO_POWER` |
| I/O | Streaming (mmap, userptr) and `read()`; timestamps: monotonic clock at the copy |
| Frame input device | Output device, `V4L2_PIX_FMT_YUYV` of the application format (`VIDIOC_S_FMT` sets it; `EBUSY` for another size while buffers exist), streaming and `write()`; one buffer or `write()` of one frame size per frame (a buffer queued with 0 bytes counts as a whole frame) |
| State | sysfs attribute `gchd_state` of the application device (`/sys/class/video4linux/videoN/gchd_state`): `STATE WIDTH HEIGHT RATE-NUM RATE-DEN FEED-NODE STREAM-NODE OUT-W OUT-H`; `STATE` is `streaming` while an application streams from the application device, else `idle`; `STREAM-NODE` is the H.264 device of the attached Game Capture HD, `-` when none is attached; `OUT-W OUT-H` is the size the encoder delivers now, `0 0` while not known. Each change is notified on the attribute (`poll()`) and sent as a `change` uevent of the application device |
