# HDMI input

Notation and status labels are defined in [README.md](README.md).

## 1. Selecting the HDMI input (Observed)

1. `0x9C:51 = 45` (input selector); read `0x9C:88`; `0x9C:B7 = CC`; about
   330 ms without traffic.
2. `0900:0018` written `02 0A`.
3. Front-end registers on device `0x9C`:

   | Step | Operation |
   |---:|---|
   | 1 | Read `3F` (`B2`); `00 = CE` (bank `CE`) |
   | 2 | Read `3E` (`D3`); `01 = AD` |
   | 3 | Read `3B` (`B3`); `04 = CD`, `06 = C4` |
   | 4 | Read `36` (`BA`); `09 = E4` |
   | 5 | Read `3F` (`B0`); `00 = CC` (bank `CC`) |
   | 6 | Read `6B` (`82`); `54 = EC` |
   | 7 | Read `93` (`B2`); `AC = 4C` |
   | 8 | Read `3F` (`B2`); `00 = 4C`; read `3F` (`32`); `00 = CC` |
   | 9 | Read `F1` (`B2`); `CE = 4C`; read `F0` (`B2`); `CF = CE`; read `3F` (`B2`); `00 = CC` |
   | 10 | Read `6A` (`B1`); `B0 = E8` (open parameter table) |
   | 11 | `B1 = 0C`, `92 = 8C`, `93 = CB`, `94 = CA`, `95 = C8`, `96 = D1`, `97 = B1`, `98 = 6E`, `99 = B6`, `9A = D1`, `9B = C4`, `9C = DC`, `9D = B7`, `9E = 94`, `9F = B2`, `A0 = 11`, `A1 = CD`, `A2 = 5D`, `A3 = CB`, `A4 = CA`, `A5 = EC`, `A6 = CC`, `A7 = C8`, `A8 = CC`, `A9 = EC`, `AA = CC`, `AD = C9` |
   | 12 | Read `8F` (`96`); `B0 = E9` (close parameter table) |

## 2. No-signal polling (Observed)

| Order | Operation |
|---:|---|
| 1 | `0x9C:AB = 4C` |
| 2 | Read `0x9C:6A` |
| 3 | Read `0x98:F5` |
| 4 | Read `0x9C:94` |

## 3. Mode detection and configuration (Driver)

The driver configuration is implemented in `gchd_configure_hdmi_exact()`
(`src/driver/input.c`).

1. `0x94:41 = 37`, `0x94:4A = AF`, `0x94:4B = AF`; front-end setup including
   `0x9C:51 = 45`.
2. Mode detection: bank `CC`, read `0x9C:66/65` and `0x9C:64/63`; bank `CE`,
   read `0x9C:34` (bit 2 = RGB input). Repeated with a pause between
   batches; `0x9C:66/65 ≈ AD4D` indicates no signal. Modes:

   | `0x9C:64/63` | Mode |
   |---|---|
   | `B6D7` ± 10 | 1080p60 |
   | `B081` ± 10 | 1080i60 |
   | `B05C` ± 10 | 720p60 |
   | `B0B5`–`B0CD` | 480p, or 576p if `0x9C:66/65` is `BB75` ± 10 |

3. Per-mode `0x9C:B2`/`B5` (1080: `C4`/`D0`; 720: `CC`/`CC`; 480p/576p:
   `CF`/`CC`), further front-end writes, parameter table `B0 = E8` … `E9`,
   back-end retrigger, common back-end blocks, per-mode back-end table.
4. Colour space from the RGB bit: YUV or RGB table on `0x9C:92`–`AA`.
   Encoder colour description (`colour_primaries`, `matrix_coefficients`):
   `1` for HDMI, `6` for analog inputs.
5. Encoder setup and capture start ([hardware.md §6](hardware.md#6-capture-start-observed)).

## 4. Capture with a 1080p60 source; output size changes (Observed)

While capturing, the receiver is polled about once per second: bank `CC`,
read `0x9C:6A`, `64`, `63` (`B6 D7` for 1080p60); bank `CE`, read `16`,
`17`; bank `CC`, read `66`, `65`; bank `CD`, read `3E`, `0B`; bank `CE`,
read `34`, `33`, `31`; `0x9C:1B = 30`; read `2B`, `77`, `74`, `76`; then
read `6A`, `A7`, `69`.

The encoder input stays `v_format = 32` (1080p60), `v_ip_format = 2`,
`v_in_source_type = 0`; the encoder output size can differ from the input
size. A change of output size stops and restarts the encoder without any
receiver or back-end writes:

1. SCMD STATE_CHANGE NULL (`05 00 00 04`); completes after 0.7–1.4 s.
2. SCMD STATE_CHANGE STOP (`05 00 00 01`).
3. Encoder setup for the new output size, SCMD INIT mode `A0`, SCMD
   STATE_CHANGE START (`05 00 00 02`).

Encoder settings per output size:

| Output | `v_bitrate` / `v_max_bitrate` / `v_ave_bitrate` / `v_min_bitrate` | `system_rate` | `v_h264_level` | Transport output |
|---|---|---|---|---|
| 1920 × 1080 | 30000 / 27000 / 15000 / 10500 | 32655 | 40 | 1 |
| 1280 × 720 | 15000 / 13500 / 7500 / 3750 | 16562 | 31 | 1 |
| 640 × 480 | 11250 / 10125 / 5625 / 2812 | 12539 | 30 | 2 |
| 480 × 360 | 15000 / 13500 / 7500 / 3750 | 16562 | 22 | 2 |

Transport output 1: `v_ts_out_mode = 1`, `a_ts_out_mode = 2`, `a_mode = 3`,
`v_ts_out2_mode = 0`, `a_ts_out2_mode = 0`, `a2_mode = 0`. Transport
output 2: `v_ts_out_mode = 0`, `a_ts_out_mode = 0`, `a_mode = 0`,
`v_ts_out2_mode = 1`, `a_ts_out2_mode = 2`, `a2_mode = 3`. For 480 × 360
additionally `v_rate_mode = 1` and `v_max_cpb_delay = 10`.

The driver encodes HDMI at the input size and always uses transport
output 1 (Driver).
