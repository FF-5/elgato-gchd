# Component recording, 2026-11-25

Recording: `traces/component/gchd_component_modes_output-sizes_5060fps_small_part1`
and `_part2` (Windows software; the source switched between 576i, 576p,
720p, 1080i and 1080p30, output sizes and the "Allow 50/60fps" option
changed; order not noted). Decoded listings `.ops.txt` next to them.

## Change sequences

Each sequence runs from `STATE_CHANGE NULL` to `START`. "Detection" is the
last `0x9C:68/67` and `0x9C:66/65` read before START. "Writes" counts the
register writes (front end `9C`, back end `98`, `88`, bridge) in the
sequence; a source mode change reprograms the front end (120–195 writes),
an encoder-only change writes 4–16.

| # | Part, s | Detection 68/67, 66/65 | Mode | Output | Writes | Reading |
|---|---|---|---|---|---|---|
| 0 | 1, 13.2–19.3 | 9ABE, BB74 | 576i | 720×576 | 195 | capture start |
| 1 | 1, 21.1–22.8 | 9AB9, BB75 | 576i | 720×576 | 6 | restart, no encoder field changed (`9C:04 = 30`, `9C:E0 = CC`) |
| 2 | 1, 34.0–37.2 | A6B4, BB75 | 576p | 720×576 | 123 | source → 576p |
| 3 | 1, 47.3–50.4 | 9ABE, BB75 | 576i | 720×576 | 130 | source → 576i |
| 4 | 1, 69.9–74.0 | BF59, BA95 | 720p | 640×480 | 133 | source → 720p (640×480 chosen) |
| 5 | 1, 84.7–86.5 | BF59, BA95 | 720p | 1280×720 | 4 | output size → 1280×720 (60 fps) |
| 6 | 1, 97.2–100.7 | 9AB9, BB74 | 576i | 720×576 | 133 | source → 576i |
| 7 | 1, 128.2–132.2 | A03D, BA95 | 1080i | 1920×1080 | 126 | source → 1080i |
| 8 | 2, 4.0–7.3 | 9AB9, BB74 | 576i | 720×576 | 134 | source → 576i |
| 9 | 2, 28.6–31.8 | BBF5, BA95 | 1080p30 | 1920×1080 | 122 | source → 1080p30 |
| 10 | 2, 45.8–48.0 | BBF5, BA95 | 1080p30 | 1280×720 | 4 | output size |
| 11 | 2, 52.5–54.6 | BBF5, BA95 | 1080p30 | 640×480 | 4 | output size |
| 12 | 2, 59.0–62.3 | 9AB9, BB75 | 576i | 720×576 | 135 | source → 576i |
| 13 | 2, 82.0–85.2 | BF59, BA95 | 720p | 640×480 | 126 | source → 720p |
| 14 | 2, 90.0–92.2 | BF58, BA95 | 720p | 1280×720 | 16 | output size → 1280×720 (60 fps; `9C:04`/`9C:E0` writes) |
| 15 | 2, 104.5–106.4 | BF58, BA95 | 720p | 1280×720 | 4 | "Allow 50/60fps" off: 30 fps |
| 16 | 2, 111.5–115.5 | 9ABE, BB75 | 576i | 720×576 | 167 | source → 576i |
| 17 | 2, 120.8–123.4 | BF59, BA95 | 720p | 1280×720 | 123 | source → 720p, first START with the 576i encoder values |
| 18 | 2, 123.8–126.1 | BF59, BA95 | 720p | 1280×720 | 6 | 0.5 s later: restart with the 720p values (60 fps) |

The detection values match the driver's table (`docs/component.md §3`):
576i `9AB9`, 576p `A6B7`, 720p `BF59`, 1080i `A03D`, 1080p30 `BBF4`
(±10); no signal `0x9C:66/65 ≈ AD4D`; with a signal BB74/BB75 (SD) or
BA95 (HD).

## Encoder fields

Fields that differ between the sequences (all others equal). `ts` is
`time_scale_l_bl` / `num_units_in_tick_bl` (0/0 or 60000/1001); `out` the
transport output (1: `v_ts_out_mode = 1`, 2: `v_ts_out2_mode = 1`, audio
on the same output).

| Mode | Output | fps | `v_format` | `ip` | src type | vinpelclk | GOP | B (kbit/s) max/ave/min | level / `_bl` | `system_rate` | ts | out | colour |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 576i | 720×576 | 50i | 5 | 0 | 3 | 1 | 12 | 11250 / 10125 / 5625 / 2812 | 31 / 31 | 12531 | 0/0 | 1 | 6 |
| 576p | 720×576 | 50 | 9 | 2 | 3 | 1 | 6 | 15000 / 13500 / 7500 / 3750 | 30 / 30 | 16597 | 0/0 | 1 | 6 |
| 720p | 1280×720 | 60 | 2 | 2 | 0 | 0 | 14 | 26250 / 23625 / 13125 / 9187 | 40 / 31 | 28683 | 0/0 | 1 | 1 |
| 720p | 1280×720 | 30 (option off) | 2 | 2 | 0 | 0 | 8 | 15000 / 13500 / 7500 / 3750 | 31 / 31 | 16562 | 60000/1001 | 1 | 1 |
| 720p | 640×480 | 30 | 2 | 2 | 0 | 0 | 8 | 11250 / 10125 / 5625 / 2812 | 30 / 30 | 12539 | 60000/1001 | 2 | 1 |
| 1080i | 1920×1080 | 60i | 0 | 0 | 0 | 0 | 14 | 30000 / 27000 / 15000 / 10500 | 40 / 31 | 32655 | 0/0 | 1 | 1 |
| 1080p30 | 1920×1080 | 30 | 32 | 2 | 0 | 0 | 8 | 30000 / 27000 / 15000 / 10500 | 40 / 31 | 32655 | 60000/1001 | 1 | 1 |
| 1080p30 | 1280×720 | 30 | 32 | 2 | 0 | 0 | 8 | 15000 / 13500 / 7500 / 3750 | 31 / 31 | 16562 | 60000/1001 | 1 | 1 |
| 1080p30 | 640×480 | 30 | 32 | 2 | 0 | 0 | 8 | 11250 / 10125 / 5625 / 2812 | 30 / 30 | 12539 | 60000/1001 | 2 | 1 |

`colour`: `colour_primaries` and `matrix_coefficients` (6 SMPTE 170M, 1
BT.709). The `_bl` fields carry the same size, GOP, bitrates and IP
format as the main fields; only the level differs (31 where the main
level is 40). All sequences CBR (`v_rate_mode 0`, `v_max_cpb_delay 3`),
profile 1, `v_gop_struct 3`.

Readings:

- "Allow 50/60fps" off halves the frame rate of 60 Hz progressive modes:
  the stream is signalled 30000/1001 (60000/1001 per two ticks), GOP 8,
  bitrate 15000 at 1280×720. 1080p30 and the 640×480 output always use
  the 30 fps values.
- The bitrate depends on the output size and frame rate: 11250 (720×576i,
  640×480), 15000 (576p, 1280×720 at 30), 26250 (1280×720 at 60), 30000
  (1920×1080). Maximum 0.9·B, average B/2, minimum B/4 below 26250 and
  0.35·B from 26250 (9187 / 10500).
- `system_rate` = ⌊1.07285·B + 461.7⌋ (50 Hz source) / ⌊1.07285·B +
  469.7⌋ (60 Hz), plus 43 (576p) or 51 (720p60) for progressive 50/60
  frames per second: reproduces all seven recorded values.
- 640×480 uses transport output 2 (audio too), as composite's scaled
  sizes.
- An output-size change is NULL, STOP, encoder fields, START without
  front-end writes, as on composite.
- The Windows software starts a new mode once with the previous mode's
  encoder values and restarts 0.5 s later with the right ones (#17/#18).

## Open (single-option recordings would settle these)

1. 480i and 480p (no recording yet).
2. 576i and 576p with 1280×720 / 640×480 output, and whether "Allow
   50/60fps" changes them (50 Hz: 25 fps?).
3. 1080i with 1280×720 / 640×480 output and with the option off.
4. 720p 640×480 with the option on: the same 30 fps values?
5. `9C:04` / `9C:E0` writes in #1, #14 and #18 (`9C:04` stepping EC, E0,
   F4, 88 in #14): a picture setting or a front-end adjustment?

# Single-option recordings, 2026-11-25 (second upload)

Recordings in `traces/component/` (decoded `.ops.txt` next to them):
`gchd_component_576p_input_bitrate`, `_720p_input_bitrate`,
`_1080i_input_bitrate`, `_1080p_input_bitrate` (one source mode, bitrate
slider from maximum to minimum and back, the 50/60 fps option, output
sizes), `_720_output`, `_480_output`, `_480_output_SD_option`,
`_50-60fps_option`. Maintainer's notes: on 576p the 50/60 fps option had a
visible effect only with the 720 or 1080 output; on 1080i the 720 output
looked like 720p60 and the option did nothing; each output size has its
own bitrate.

Encoder settings per sequence (B / max / min in kbit/s; L level / `_bl`;
ts `time_scale_l_bl` / `num_units_in_tick_bl`; out transport output):

| Recording # | Source | Output | IP | GOP | B / max / min | L | system_rate | ts | out |
|---|---|---|---|---|---|---|---|---|---|
| 576p_bitrate 0 | 576p | 720×576 | 2 | 12 | 8000 / 7200 / 800 | 31/31 | 9087 | 0/0 | 1 |
| 576p_bitrate 1 | 576p | 720×576 | 2 | 6 | 8000 / 7200 / 800 | 30/30 | 9087 | 0/0 | 1 |
| 576p_bitrate 3, 5, 7 | 576p | 720×576 | 2 | 12 | 15000 / 13500 / 3750 | 31/31 | 16597 | 0/0 | 1 |
| 576p_bitrate 4, 6 | 576p | 720×576 | 2 | 12 | 11250 / 10125 / 2812 | 31/31 | 12574 | 0/0 | 1 |
| 720p_bitrate 0 | 720p | 1280×720 | 2 | 14 | 14000 / 12600 / 3500 | 40/31 | 15541 | 0/0 | 1 |
| 720p_bitrate 1 | 720p | 1280×720 | 2 | 8 | 8000 / 7200 / 800 | 31/31 | 9053 | 60000/1001 | 1 |
| 720p_bitrate 2 | 720p | 1280×720 | 2 | 8 | 15000 / 13500 / 3750 | 31/31 | 16562 | 60000/1001 | 1 |
| 720p_bitrate 3 | 720p | 1280×720 | 2 | 14 | 26250 / 23625 / 9187 | 40/31 | 28683 | 0/0 | 1 |
| 1080i_bitrate 1 | 1080i | 1920×1080 | 0 | 14 | 16000 / 14400 / 4000 | 40/31 | 17635 | 0/0 | 1 |
| 1080i_bitrate 3; 720_output 4; 50-60 12 | 1080i | 1280×720 | 2 | 8 | 15000 / 13500 / 3750 | 31/31 | 16562 | 0/0 | 1 |
| 1080i_bitrate 4; 480_output 3; 50-60 8 | 1080i | 640×480 | 2 | 8 | 11250 / 10125 / 2812 | 30/30 | 12539 | 0/0 | 2 |
| 1080i_bitrate 5 | 1080i | 480×360 | 2 | 8 | 15000 / 13500 / 3750 | 22/22 | 16562 | 0/0 | 2 |
| 1080p_bitrate 0 | 1080p30 | 1920×1080 | 2 | 8 | 16000 / 14400 / 4000 | 40/31 | 17635 | 60000/1001 | 1 |
| 1080p_bitrate 2 | 1080p30 | 1280×720 | 2 | 8 | 15000 / 13500 / 3750 | 31/31 | 16562 | 60000/1001 | 1 |
| 1080p_bitrate 3 | 1080p30 | 640×480 | 2 | 8 | 11250 / 10125 / 2812 | 30/30 | 12539 | 60000/1001 | 2 |
| 1080p_bitrate 4 | 1080p30 | 480×360 | 2 | 8 | 15000 / 13500 / 3750 | 22/22 | 16562 | 60000/1001 | 2 |
| 480_output_SD_option 0 | 576i | 640×480 | 2 | 6 | 11250 / 10125 / 2812 | 30/30 | 12531 | 0/0 | 2 |
| 50-60 0, 2 / 1, 3 | 576p | 720×576 | 2 | 6 / 12 | 15000 / 13500 / 3750 | 30 / 31 | 16597 | 0/0 | 1 |
| 50-60 5 | 720p | 640×480 | 2 | 8 | 11250 / 10125 / 2812 | 30/30 | 12539 | 60000/1001 | 2 |
| 50-60 9 | 1080i | 1920×1080 | 0 | 14 | 30000 / 27000 / 10500 | 40/31 | 32655 | 0/0 | 1 |

Readings:

- Minimum bitrate as on composite (0 up to 4000, 10 % below 10000, 25 %
  above) and 35 % from 26250: 800, 3500, 4000, 9187, 10500.
- `system_rate` = ⌊1.07285·B + 461.7⌋ (50 Hz source) / ⌊1.07285·B +
  470.2⌋ (60 Hz), plus 43 (576p) / 51 (720p at 60 frames per second) for
  progressive 50/60 frames per second: all 14 distinct recorded values.
  (The earlier +469.7 fitted the first recording only.)
- 576p: the 50/60 fps option switches GOP 6 (on, level 30) / 12 (off,
  level 31); the stream stays at 50 frames per second (`system_rate` with
  the 576p addition in both). Its visible effect at the 720/1080 output
  sizes is the Windows software's own scaling (the encoder stays at
  720×576, as for composite's HD sizes).
- 720p: option on 60 frames per second (GOP 14, level 40); off 30
  (GOP 8, timing 60000/1001, level 31). Slider ranges seen: 14000–26250 at
  60, 8000–15000 at 30.
- 1080i: the option has no effect. Output 1280×720: GOP 8, 15000, no
  timing fields (the 1080i field pairs give 60 pictures per second); 640×480
  and 480×360 on transport output 2 (480×360 level 22, 15000 as
  composite's mobile size). Slider range at 1920×1080: 16000–30000.
- 1080p30: every output size signals 60000/1001; slider 16000–30000.
- Sequences that start with the previous mode's values and are restarted
  0.5–3 s later (e.g. `720_output` #3/#4, `480_output` #1/#2,
  `50-60fps_option` #7/#8, #11/#12): the Windows software's first START
  after a source change.
