# Component input

Notation and status labels are defined in [README.md](README.md).

## 1. Selecting the component input (Observed, no source present)

Starting from HDMI with no source, the switch to component is:

1. Input prefix: `0x88:06 = 86`; read `0x88:F8`; `0x88:03 = 2F`;
   `0x9C:B3 = CC`; read `0x9C:3F`; bank `CE`; `0x9C:27 = CC`; read
   `0x9C:3F`; bank `CC`.
2. `0900:0018` written `02 0A`, then `02 0E` (before the selector).
3. Read `0x9C:94`; `0x9C:AB = 4C`; read `0x9C:3F`; bank `CE`;
   `0x9C:1B = 33`; read `0x9C:3F`; bank `CC`; read `0x9C:88`;
   `0x9C:B7 = CE`, `B8 = DC`, `B8 = CC`; read `0x9C:3F`; bank `CE`;
   `0x9C:07 = 38`, `07 = C8`; read `0x9C:3F`; bank `CC`.
4. `0x9C:51 = ED` (input selector).
5. `0x9C:00 = 4C`, `24 = 0C`, `21 = CD`, `91 = C8`, `0E = 8C`, `11 = EC`,
   `54 = FC`, `71 = 4C`, `04 = CC`, interleaved with bank selects and reads
   of `0x9C:3F`.

Every mailbox write uses the handshake in
[hardware.md §2.1](hardware.md#21-mailbox-handshake-005d-observed).

## 2. No-signal polling (Observed)

| Order | Operation |
|---:|---|
| 1 | `0x9C:AB = 4C` |
| 2 | Read `0x9C:2B` |
| 3 | Read `0x9C:2A` |
| 4 | Read `0x9C:60` |
| 5 | Read `0x98:F5` |
| 6 | Read `0x9C:94` |

## 3. Mode detection and configuration (Driver)

The driver configuration is implemented in `gchd_configure_component_exact()`
(`src/driver/input.c`).

1. `0x94:41 = 37`, `0x94:4A = AF`, `0x94:4B = AF`; front-end setup;
   `0x9C:51 = ED`.
2. Mode detection: the 16-bit value of `0x9C:68/67` is averaged over repeated
   samples; `0x9C:66/65 ≈ AD4D` indicates no signal. Modes (±10):

   | `0x9C:68/67` | Mode | `v_format` |
   |---|---|---|
   | `BBF4` | 1080p30 | 32 |
   | `A03D` | 1080i60 | 0 |
   | `BF59` | 720p60 | 2 |
   | `A6B7` | 576p50 | 9 |
   | `9AB9` | 576i50 | 5 |
   | `A150` | 480p60 | 8 |
   | `9576` | 480i60 | 4 |

3. Per-mode front-end programming, colour space tables (`0x9C:92`–`AA`, with
   the component-specific `0B`–`0D`, `08`–`0A`, `1E`, `1F` and per-mode
   `B2`/`B5`/`03` writes), per-mode back-end table and the common back-end
   blocks.
4. Encoder setup and capture start ([hardware.md §6](hardware.md#6-capture-start-observed)).

Component capture with a source present has not yet been confirmed by a
trace.

## 4. Encoder settings per source mode (Observed)

Recorded with the Windows software (576i, 576p, 720p, 1080i, 1080p30;
output sizes; "Allow 50/60fps"). Detection values as in §3. All
sequences CBR (`v_rate_mode 0`, `v_max_cpb_delay 3`), High profile
(encoder value 1), `v_gop_struct 3`. `ts`: `time_scale_l_bl` /
`num_units_in_tick_bl`; output 2: `v_ts_out2_mode = 1` (audio on output 2
as well).

| Source | Output | Frames/s | `v_format` | `v_ip_format` | `v_in_source_type` | `v_vinpelclk` | GOP | Bitrate B / max / ave / min (kbit/s) | Level (`_bl`) | `system_rate` | `ts` | TS output | Colour |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 576i | 720 × 576 | 25 (50 fields) | 5 | 0 | 3 | 1 | 12 | 11250 / 10125 / 5625 / 2812 | 31 (31) | 12531 | 0 / 0 | 1 | 6 |
| 576i | 640 × 480 | 25 | 5 | 2 | 3 | 1 | 6 | 11250 / 10125 / 5625 / 2812 | 30 (30) | 12531 | 0 / 0 | 2 | 6 |
| 576p | 720 × 576 | 50 | 9 | 2 | 3 | 1 | 6 | 15000 / 13500 / 7500 / 3750 | 30 (30) | 16597 | 0 / 0 | 1 | 6 |
| 576p, 50/60fps off | 720 × 576 | 50 | 9 | 2 | 3 | 1 | 12 | 15000 / 13500 / 7500 / 3750 | 31 (31) | 16597 | 0 / 0 | 1 | 6 |
| 720p | 1280 × 720 | 60 | 2 | 2 | 0 | 0 | 14 | 26250 / 23625 / 13125 / 9187 | 40 (31) | 28683 | 0 / 0 | 1 | 1 |
| 720p, 50/60fps off | 1280 × 720 | 30 | 2 | 2 | 0 | 0 | 8 | 15000 / 13500 / 7500 / 3750 | 31 (31) | 16562 | 60000 / 1001 | 1 | 1 |
| 720p | 640 × 480 | 30 | 2 | 2 | 0 | 0 | 8 | 11250 / 10125 / 5625 / 2812 | 30 (30) | 12539 | 60000 / 1001 | 2 | 1 |
| 1080i | 1920 × 1080 | 30 (60 fields) | 0 | 0 | 0 | 0 | 14 | 30000 / 27000 / 15000 / 10500 | 40 (31) | 32655 | 0 / 0 | 1 | 1 |
| 1080i | 1280 × 720 | 60 | 0 | 2 | 0 | 0 | 8 | 15000 / 13500 / 7500 / 3750 | 31 (31) | 16562 | 0 / 0 | 1 | 1 |
| 1080i | 640 × 480 | — | 0 | 2 | 0 | 0 | 8 | 11250 / 10125 / 5625 / 2812 | 30 (30) | 12539 | 0 / 0 | 2 | 1 |
| 1080i | 480 × 360 | — | 0 | 2 | 0 | 0 | 8 | 15000 / 13500 / 7500 / 3750 | 22 (22) | 16562 | 0 / 0 | 2 | 1 |
| 1080p30 | 1920 × 1080 | 30 | 32 | 2 | 0 | 0 | 8 | 30000 / 27000 / 15000 / 10500 | 40 (31) | 32655 | 60000 / 1001 | 1 | 1 |
| 1080p30 | 1280 × 720 | 30 | 32 | 2 | 0 | 0 | 8 | 15000 / 13500 / 7500 / 3750 | 31 (31) | 16562 | 60000 / 1001 | 1 | 1 |
| 1080p30 | 640 × 480 | 30 | 32 | 2 | 0 | 0 | 8 | 11250 / 10125 / 5625 / 2812 | 30 (30) | 12539 | 60000 / 1001 | 2 | 1 |
| 1080p30 | 480 × 360 | 30 | 32 | 2 | 0 | 0 | 8 | 15000 / 13500 / 7500 / 3750 | 22 (22) | 16562 | 60000 / 1001 | 2 | 1 |

Colour: `colour_primaries` and `matrix_coefficients` (6 SMPTE 170M, 1
BT.709). The values are the defaults of each output size; the bitrate
slider of the Windows software changes B (seen: 576p 8000–15000, 720p
14000–26250 at 60 and 8000–15000 at 30 frames per second, 1080i/1080p
16000–30000). Maximum 0.9·B, average B/2, minimum 0 up to 4000, B/10
below 10000, B/4 above, 0.35·B from 26250. `system_rate` = ⌊1.07285·B +
461.7⌋ with a 50 Hz source, ⌊1.07285·B + 470.2⌋ with 60 Hz, plus 43
(576p) or 51 (720p at 60 frames per second) for progressive 50/60 frames
per second. "Allow 50/60fps": off encodes 720p at 30 frames per second
(GOP 8, signalled 60000/1001); on 576p it switches GOP 6 / 12 only; no
effect on 1080i and 1080p30. The 1280 × 720 and 1920 × 1080 output sizes
of SD sources are scaled by the Windows software (the encoder stays at
720 × 576). An output size change is NULL, STOP, encoder fields, START
without front-end writes. Not recorded: 480i, 480p.

The driver uses these values at the standard output size; 480i and 480p
follow the same rules (11250, GOP 14 / 15000, GOP 8), not yet confirmed.
