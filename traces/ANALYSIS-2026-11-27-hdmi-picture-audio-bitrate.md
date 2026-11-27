# Analysis 2026-11-27: HDMI modes, colour range, picture and audio settings, bitrate slider

Recordings (uploaded 2026-11-26/27, decoded `.ops.txt` next to them):

- HDMI source in all HDMI recordings: a laptop's HDMI output (maintainer).
- `traces/hdmi/gchd_hdmi_{480p60,720p60,1080p60}_input_small`: one source
  mode each, every output size, "Allow 50/60fps" on/off, "HDMI Color Range"
  Standard/Expanded on some outputs (maintainer).
- `traces/hdmi/gchd_hdmi_input_switching{,_extra}_small`: source mode changes.
- `traces/gchd_{brightness,contrast,saturation,hue}_small`: each slider
  0 → −100 → 0 → +100 → 0 (hue −180° … +180°), component 576p (detection
  `9C:68/67 = A6B7`), while capturing.
- `traces/gchd_analog_audio_gain_small`: 0 dB, −1 dB steps to −12 dB, back
  to 0 dB, then +1 dB steps to +12 dB (maintainer), component.
- `traces/component/gchd_{1920x1080i,1280x720p60,1280x720p30,640x480}_output_bitrate_change_small`:
  bitrate slider moved from low to high (R13).

## 1. Device 0x9C values on the wire

Writes and reads of device `0x9C` carry the value XOR `0xCC` (bank select
`CC/CE/CD/4C` = banks 0/2/1/0x80; input prefix `CC/33` = `00/FF`).

## 2. Colour conversion block `0x9C:92`–`0x9C:AA` (25 bytes)

Decoded (XOR `CC`): byte 0 header; then twelve 16-bit big-endian words:
nine matrix coefficients and three offsets.

- Coefficients: 15-bit two's complement (bits 14..0 of the word), 1.0 =
  4096. Word `i*3 + o` = coefficient of input channel `i` for output channel
  `o`; outputs in the order Cr, Y, Cb (inputs for YCbCr sources in the same
  order; for RGB sources R, G, B: the RGB tables give Y = 0.257 R + 0.504 G
  + 0.098 B at words 1, 4, 7).
- Offsets (words 9, 10, 11 = Cr, Y, Cb): value × 64; Cr/Cb `0x2000` (128),
  Y `0x0401` (16, bit 0 set).
- YCbCr sources (component, analog): header `66`, identity matrix.
- HDMI RGB: header `40` (Standard), `55` (Expanded: every coefficient ×
  255/219 = 1.164). 480p: BT.601 matrix; 720p: BT.709 matrix (1080p: no
  block written in the recording; the driver's present RGB table is the
  720p Standard one). Written alone while capturing when the option changes.

| Table | Header | Words 0–8 | Offsets |
|---|---|---|---|
| YCbCr | 66 | 4096 0 0 / 0 4096 0 / 0 0 4096 | 2000 0401 2000 |
| RGB 601 Standard | 40 | 1798 1053 −606 / −1507 2064 −1192 / −291 401 1798 | same |
| RGB 601 Expanded | 55 | 2093 1225 −705 / −1753 2404 −1389 / −340 467 2093 | same |
| RGB 709 Standard | 40 | 1798 750 −414 / −1634 2515 −1384 / −164 254 1798 | same |
| RGB 709 Expanded | 55 | 2093 872 −479 / −1900 2928 −1613 / −192 294 2093 | same |

## 3. Picture settings

Each change writes only the block of §2, while capturing (no encoder
restart, nothing else). From the YCbCr identity:

- Contrast: word 4 (Y → Y) = v × 32, v 0 … 255 (128 = 4096 neutral; −100 →
  0, +100 → 8159; values v·32 or v·32 − 1).
- Saturation: words 0 and 8 (Cr → Cr, Cb → Cb), same scale (−100 → 31,
  +100 → 8159).
- Hue: rotation of Cr/Cb: words 0, 8 = cos θ · 4096, word 2 = −sin θ ·
  4096, word 6 = +sin θ · 4096; θ in 0.5° steps, ±61.5° at the slider's
  ±180° (all 27 recorded angles are multiples of 0.5°).
- Brightness: Y offset (word 10 bits 14..6, 9-bit two's complement) = 16 +
  ⌊(v − 128) · 1.25⌋, v 0 … 255: −100 → −144, +100 → 174.
- Combined settings and RGB sources: not recorded; natural composition
  (adjustment × conversion matrix).

## 4. Analog audio gain

`0x94:41`, `0x94:4A`, `0x94:4B` (left, right; same value), written alone
while capturing. Wire value of `4A` = code XOR `0x08`; one dB = 2 codes.

- 0 to −12 dB: `41 = 37`, code = `A7` + 2·attenuation (0 dB wire `AF`,
  −1 `A1`, −4 `A7`, −5 `B9`, −12 `B7`): 12 steps recorded in order.
- +1 to +12 dB: `41 = 57` (boost, +20 dB by the values), code = `A7` +
  2·(20 − gain): +1 `C5`, +3 `C1`, +7 `C9`, +11 `B1`, +12 `BF` (all values
  written with `57` here and in the composite recordings fit whole dB).
  The positive sweep wrote only +1, +11, +3, +1, 0 (to be verified by a
  level measurement).
- Older recordings: `B7` (−12 dB) at input setup for analog inputs, `AF`
  (0 dB) for HDMI.

## 5. Bitrate slider (R13)

One slider: lower part VBR (`v_rate_mode 1`, cpb delay 10, shown as the
average B/2), upper part CBR (`0`, delay 3, shown as B).

| Output | VBR B (shown average) | CBR B |
|---|---|---|
| 640 × 480 (max 11250) | 6000–19500 (3.00–9.75) | 10440–11250 |
| 720p at 30 (max 15000) | 8000–18800 (4.00–9.40) | 10440–15000 |
| 1080i (max 30000) | 16000 (8.00) | 18800–30000 |
| 720p at 60 (max 26250) | not recorded | 20300–26250 |

Composite (older recordings, 720 × 480/576, same output mode as 640 × 480):
VBR 6000–14460, CBR 11250. So for the 11250 and 15000 outputs CBR starts at
10440 and VBR reaches 19500/18800 (average 9.75/9.40); the VBR top for
1080 and 720p at 60 is not recorded.

## 6. HDMI modes (encoder fields per source and output)

Same values per output mode as component: 480p 720 × 480 GOP 8 (option on)
/ 14 (off), 15000, level 30/31, BT.601; 576p GOP 12 (off; 6 on by the
component values); 720p60 GOP 14, 26250, level 40 / at 30 GOP 8, 15000,
`ts 60/1`; 720p50 GOP 12, 26250; 1080p60 1920 × 1080 GOP 8, 30000, `ts
60/1` (30 frames per second); 1080p50 GOP 6, `ts 50/1` (25); scaled outputs
640 × 480 (11250), 480 × 360 (VBR 15000), 1280 × 720 (15000) with `ts 60/1`.
`v_format`: 480p 8, 576p 9, 720p60 2, 720p50 3, 1080p60 32, 1080p50 33.
`v_vinpelclk = 1` for every HDMI mode. Source mode changes: STOP, INIT A0,
encoder setup, START, as for component (`input_switching`). Detection on
`9C:66/65` and `9C:64/63`.

## 7. Later recordings (2026-11-27)

### Bitrate slider, 1080i and 720p at 60 (`*_output_bitrate_change_2_small`)

Slider moved from CBR down into VBR: 1080i CBR 17440 … 12320, then VBR
19840 (average 9.92), 16000; 720p at 60 CBR 15470 … 10640, then VBR 19600
(9.80), 16800, 14000. Every output: VBR up to about 10 (average), CBR from
about 10.4–12.3. Maintainer decision: VBR below a shown 10.00, CBR from
10.00, for every output (480 × 360 VBR only).

### First HDMI start (`gchd_hdmi_{720p60,1080p60}_input_start_small`)

HDMI selected with a source connected, up to the first START. No back-end
(`0x98`) writes at all, no `0x94` writes. Sequence:

1. Before the selection: polls `9C:94` (read), `9C:AB = 4C`, reads `2B`,
   `2A`, `60` (about 0.5 s apart).
2. Digital-input prefix: `0x88:06 = 86`, read `0x88:F8`, `0x88:03 = 2F`,
   `9C:B3 = CC`, bank `CE` `9C:27 = CC`; `0900:0018 = 02 0E`, then `02 0A`
   (twice, each read back).
3. Receiver setup as the driver's (bank `CC`: read `94`, `AB = 4C`; bank `CE`
   `1B = 33`; bank `CC` read `88`, `B7 = CE`, `B8 = DC`, `B8 = CC`; bank `CE`
   `07 = 38`, `07 = C8`; bank `CC` `51 = 45`, read `88`, `B7 = CC`;
   `0900:0018 = 02 0A`; bank `CE` `01 = AD` (after read `3E`), `04 = CD`,
   `06 = C4` (after read `3B`), `09 = E4` (after read `36`); bank `CC`
   `54 = EC` (read `6B`), `AC = 4C` (read `93`); bank `4C` and back;
   `CE = 4C` (read `F1`), `CF = CE` (read `F0`)).
4. Read `6A`; window `B0 = E8`, `B1 = 0C`, the RGB 601 Standard table with
   Y offset `0x0400`, `AD = C9`, read `8F`, `B0 = E9`.
5. Timing polls (`AB = 4C`; bank `CC` `6A`, `64/63`; bank `CE` `16/17`;
   bank `CC` `66/65`) about 20 times until stable.
6. Per mode `B2`/`B5`: 1080 `C4`/`D0`, 720 `CC`/`CC`, SD `CF`/`CC`.
7. Status block (bank `CD` `3E`, `0B`; bank `CE` `34`, `33`, `31` ×2;
   `1B = 30`; `2B`, `77`, `74`, `76`); read `20`; `1F = DC`, read `29`,
   `1F = CC`; read `3B`, `04 = CF`, `04 = CD`; bank `CC` `40 = CC`.
8. Window `B0 = E8`, `B1 = 0C`, RGB 709 Standard table (Y offset `0400`),
   `AD = C9`, read `8F`, `B0 = E9`, `AB = CC`.
9. Status reads (bank `CE` `6A`, `A7`, `69`; bank `CC` `66/65`; bank `CE`
   `78`, `7F`–`7A`, `76`), the same status block again, then the table of
   the "HDMI Color Range" setting (here Expanded, Y offset `0401`) without
   the window; reads `8D`, `8A`; `B2 = CC` (SD `CF`), `B5 = C4` (sometimes
   `CC` and `C4` only after START).
10. INIT A0, START.

Source mode changes (`input_switching`) repeat steps 5–10 after STOP/NULL.
The driver's HDMI path matches steps 2–3 and parts of 6–8, and in addition
runs the back-end blocks (retrigger, calibration, per-mode table): not
needed for HDMI.
