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

## Maintainer comments

Before starting this task I want you to annotate somethings that have occurred to me that might make the whole development process somewhat faster:
- I already saw that you wrote on TODO.md traces that you want from me to verify somethings. Please write there all traces that we need to verify things. Mark any that we might not need because the feature we already have it working.
- We need a better way to test changes. I was already thinking that before doing the first merge to master (after HDMI), I want to make sure everything works and thought that this process can be easily automated. Instead of me inputting commands every time and copying log outputs all of this can be done with a script. So for the last test we are going to need a way to test everything that we have planned for before the master merge. I was thinking you can just write a script with all the commands needed and log the precise info you need to verify if the element we're testing is wrong. I'll do the visual testing and on every step that you need me to confirm output or switch physical connections, you prompt me with what to do and to confirm when I'm ready. I also thought that maybe we can already start doing this for all the small tests we are running now, only the last test is going to be longer but maybe not as detailed as these smaller tests.
