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

## 3. Mode detection and configuration

Values of device `0x9C` are written and read XOR `0xCC` (bank select
`CC`/`CE`/`CD`/`4C` = banks 0, 2, 1, 0x80). HDMI does not use the video
back end (device `0x98`). `gchd_configure_hdmi_exact()` (`src/driver/input.c`):

1. Receiver setup, once after HDMI was selected (`gchd_hdmi_rx_setup()`):
   the front-end writes of §1 including `0x9C:51 = 45`, then read `6A` and
   the BT.601 conversion table in the parameter window (§5).
2. `9C:AB = 4C` (after reading `94`); timing measurement, repeated until a
   mode is recognized: bank `CC`, `0x9C:66/65` (field rate), `0x9C:64/63`
   (lines); bank `CE`, `0x9C:34` (bit 2: RGB source).

   | `0x9C:64/63` | Lines |
   |---|---|
   | `B6D7` ± 10 | 1080, progressive |
   | `B081` ± 10 | 1080, interlaced |
   | `B05C` ± 10 | 720 |
   | `B0B5`–`B0CD` | 480 or 576 (576 at 50 Hz) |

   | `0x9C:66/65` | Field rate |
   |---|---|
   | `BA95` ± 10 | 60 Hz |
   | `BB75` ± 10 | 50 Hz |
   | `A2F9` ± 10 | 30 Hz |
   | `A13C` ± 10 | 25 Hz |
   | `AD4D` ± 10 | no signal |

3. Per mode `0x9C:B2`/`B5`: 1080 at 50/60 Hz `C4`/`D0`, other HD `CC`/`CC`,
   standard definition `CF`/`CC`.
4. Status reads (bank `CD` `3E`, `0B`; bank `CE` `34`, `33`, `31` twice,
   `1B = 30`, `2B`, `77`, `74`, `76`). On the first configuration after the
   receiver setup, below 1080: read `20`, `1F = DC`, read `29`, `1F = CC`,
   read `3B`, `04 = CF`, `04 = CD`. Bank `CC`, `40 = CC`.
5. Bank `CD` reads `3E`, `0B`; bank `CC`: the conversion of the source's
   standard (BT.601 for standard definition, BT.709 for HD) in the
   parameter window; `AB = CC`.
6. Status reads (bank `CE` `6A`, `A7`, `69`; bank `CC` `66/65`, `64/63`), the
   status reads of step 4 again, bank `CC`; the conversion of the source
   (§5) without the window; read `8D`, `8A`; `B2 = CC` (standard
   definition `CF`), `B5 = C4`.
7. Encoder setup and capture start ([hardware.md §6](hardware.md#6-capture-start-observed)).

While capturing, the mode worker reads `0x9C:64/63` and `66/65` every
0.75 s; another mode on two reads in a row stops the capture and the
configuration (steps 2–7) runs again.

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

The driver encodes HDMI with these output sizes and transport outputs, as
component ([component.md §6](component.md)).

## 5. Colour conversion

`0x9C:92`–`0x9C:AA` (bank `CC`, 25 bytes): a header byte, a 3 × 3 matrix of
16-bit big-endian words (15-bit two's complement coefficients, 4096 = 1.0;
word `in × 3 + out`, output channels Cr, Y, Cb) and three offset words
(value × 64; Cr, Y, Cb). Parameter window: `B0 = E8`, `B1 = 0C`, the table
with Y offset `0400`, `AD = C9`, read `8F`, `B0 = E9`. Outside the window
(also while capturing, after read `3F` and bank `CC`) the Y offset is
`0401`.

| Source | Header | Coefficients (words 0–8) |
|---|---|---|
| YCbCr (component, composite, S-video, HDMI YCbCr) | `66` | identity |
| HDMI RGB, BT.601, Standard | `40` | 1798 1053 −606 / −1507 2064 −1192 / −291 401 1798 |
| HDMI RGB, BT.601, Expanded | `55` | 2093 1225 −705 / −1753 2404 −1389 / −340 467 2093 |
| HDMI RGB, BT.709, Standard | `40` | 1798 750 −414 / −1634 2515 −1384 / −164 254 1798 |
| HDMI RGB, BT.709, Expanded | `55` | 2093 872 −479 / −1900 2928 −1613 / −192 294 2093 |

Offsets: Cr and Cb `2000` (128), Y `0401` (16). "Expanded" scales the
coefficients by 255/219. The control "HDMI Color Range" (`hdmi_range`)
selects Standard or Expanded; a change while capturing rewrites the table
without an encoder restart.
