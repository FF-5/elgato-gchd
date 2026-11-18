# TASK T-055 — Output sizes for analog sources

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** analysis
- **Type:** driver feature
- **Branch:** `feat/output-size` (from `dev` 0.5.2)
- **TODO items:** T-055 (stage 1)

## Why

The capture application offers three output sizes for composite and
S-video; the driver always encodes the source size (720 × 480 / 576).
Recorded (`docs/composite.md §10`, recordings in `traces/composite/`:
`gchd_288p_output_switch_small`, `gchd_288p_standard_640x480_output_small`,
`gchd_288p_mobile_bitrate_change_small`):

| Output | Encoder fields |
|---|---|
| Standard | source size, `v_ip_format = 0`, GOP 12, transport output 1 |
| Standard 640 × 480 | `v_ip_format = 2`, GOP 6, level 30, transport output 2 |
| Mobile 480 × 360 | `v_ip_format = 2`, GOP 6, VBR, level 22, transport output 2 |

## Implementation (plan)

1. Analysis of the three recordings: complete encoder field sets per size
   (NTSC and PAL where recorded), the restart sequence, and the bulk-IN
   data of transport output 2 (PIDs, endpoint).
2. V4L2: `S_FMT` with 640 × 480 or 480 × 360 on composite/S-video selects
   the output size (encoder restart while capturing, as recorded);
   720 × source lines selects the standard output. `ENUM_FRAMESIZES` lists
   the three. `G_FMT` reports the selected output size. Kept while the
   device stays connected, like the input and the pixel format.
3. Docs: `video-output.md`, `composite.md §10`.

## Hardware test (maintainer)

Follows with the implementation.

## Findings

## Maintainer comments

