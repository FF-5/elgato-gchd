# TASK T-061 — Bitrate and stretch for analog sources

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** analysis
- **Type:** driver feature
- **Branch:** `feat/bitrate` (from `dev` 0.5.3)
- **TODO items:** T-061 (stage 1)

## Why

The capture application sets the video bitrate (a slider per output size)
and offers "stretch" for standard-definition input; the driver always
encodes the default bitrate and never stretches. Recorded
(`docs/composite.md §10`, `traces/composite/`):
`gchd_288p_standard_bitrate_change_small`,
`gchd_288p_mobile_bitrate_change_small`, `gchd_288p_stretch_input_small`.

| Setting | Encoder fields |
|---|---|
| Standard, maximum (default) | CBR (`v_rate_mode 0`), `v_max_cpb_delay 3`, 11250 / 10125 / 5625 / 2812 |
| Standard, lower | VBR (`v_rate_mode 1`), `v_max_cpb_delay 10`, e.g. 6000 / 5400 / 3000 / 600 |
| Mobile | VBR, 4000 … 15000 |
| Stretch | `v_in_source_type 2` instead of 3 |

## Implementation (plan)

1. Analysis of the three recordings: the bitrate fields as a function of
   the slider value (bitrate, maximum, average, minimum, `system_rate`),
   the switch between CBR and VBR, the ranges per output size, and the
   restart sequence.
2. V4L2 controls: `V4L2_CID_MPEG_VIDEO_BITRATE` (and `_BITRATE_MODE`
   CBR/VBR if the recordings tie it to the value), ranges per output size;
   a private control (or `V4L2_CID_...` if one fits) for stretch. Changing
   them while capturing restarts the encoder like the recordings (NULL,
   STOP, encoder block, START).
3. The existing driver controls (bitrate, H.264 level/profile) checked
   against the recordings; the menu-control failure of v4l2-compliance
   (T-066) looked at along the way.
4. Docs: `video-output.md`, `composite.md §10`.

## Hardware test (maintainer)

Follows with the implementation.

## Findings
- Maintainer requests (2026-11-18, before T-061): all wanted Windows
  recordings listed in `TODO.md` "Evidence wanted" (R1-R10, marked
  needed / optional / not needed); hardware tests through a script
  (`TODO.md` "Testing", `tools/gchd-test.sh` with `tools/tests/smoke.sh`
  on the `tools` branch, CLAUDE.md "Hardware testing"); pre-release test
  T-090.

## Maintainer comments

