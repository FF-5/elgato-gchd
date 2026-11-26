# Component TS samples (T-051)

Captured by `gchd-test.sh T-051` (driver 7fcad33), PAL PS3 on component,
4 s per mode (about 1 s of video after the capture setup), MPEG transport
stream (H.264 + MP2).

- `component_<mode>.ts`: as captured; the capture was stopped in the middle
  of the last frame, so ffmpeg reports one decode error for it
  (`*.ffmpeg.txt`).
- `component_<mode>_frames.ts`: the same bytes up to the start of the last
  video frame; decode without errors.
- `component_1080i_1280x720.ts`: taken again (driver 33eea68, up to 20 s /
  20 MB, cut at the start of the last video frame; `.raw` as read): 12.5 s,
  decodes without errors.

| Mode | Size | Field order | Level | Colour |
|---|---|---|---|---|
| 1080i | 1920 × 1080 | tt | 40 | BT.709 |
| 720p | 1280 × 720 | progressive | 40 | BT.709 |
| 576p | 720 × 576 | progressive | 30 | SMPTE 170M |
| 576i | 720 × 576 | tt | 31 | SMPTE 170M |
| 1080i → 1280 × 720 | 1280 × 720 | progressive, 30000/1001 | 31 | BT.709 |
