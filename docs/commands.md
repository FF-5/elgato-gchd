# Register and command reference

Notation, device addresses and status labels are defined in
[README.md](README.md). Transport and SCMD are described in
[hardware.md](hardware.md). Driver names refer to `src/gchd_protocol.h`.

## 1. Control registers

| Register | Driver name | Access | Use |
|---|---|---|---|
| `0800:0094`, `0098`, `009C` | `GCHD_REG_HDNEW_VERSION0..2` | R, 4 bytes | Version text on `005d` (`"V130"`…); zero on other models. |
| `0800:0010`, `0014`, `0018` | `GCHD_REG_HD_VERSION0..2` | R, 4 bytes | Version text on other models. |
| `0800:0014` | `GCHD_REG_HDNEW_SCMD_RECEIPT` | R | High byte = last SCMD received (`005d`). |
| `0800:0016` | `GCHD_REG_HDNEW_INT_STATUS` | R | Read after every interrupt; `09 00` observed. |
| `0800:00C0` | `GCHD_REG_HDNEW_MAIL_WRITE` | W | Mailbox payload (`005d`). |
| `0800:23BE` | `GCHD_REG_HDNEW_MAIL_READ` | R | Mailbox read data (`005d`). |
| `0800:2008` | `GCHD_REG_SCMD_STATE` | R | SCMD state, low 5 bits. |
| `0900:0000` | `GCHD_REG_BANKSEL` | W | Written `0000` before encoder programming and at shutdown. Unknown. |
| `0900:0014` | `GCHD_REG_ENABLE_SHADOW` | R/W | Enable shadow; session-specific. |
| `0900:0018` | `GCHD_REG_ENABLE` | R/W | Enable bits. |
| `0900:001C` | `GCHD_REG_MAIL_READY` | R | Bit 0 mailbox ready; bits 8–11 cable type and presence. |
| `0900:0070` | `GCHD_REG_POST_IDLE_FW` | W | Written `0004` after the idle firmware download. Unknown. |
| `0900:0074` | `GCHD_REG_SCMD_COMPLETE` | R/W | Bit 2: state change complete (sticky); acknowledged by writing `0004`. Other bits unknown (`011E`, `011A` observed). |
| `0900:01B0` | `GCHD_REG_SCMD_COMPLETE_AUX` | R/W | Read in every completion round, written `0000` after the acknowledge. Unknown (`0150`, `0010`, `0000` observed). |
| `0000:0010` … `001E` | `GCHD_REG_TC_PROBE_FIRST/LAST` | R | Read after the firmware downloads; values not used. |

## 2. Bridge commands (port `0x33`)

| Payload | Reply | Use |
|---|---|---|
| `AB A9 0F A4 55` | 3 bytes: `33 44 55` (processor not running) or `27 F9 7B` (running) | Processor status. |
| `AB A9 0F A4 5B` | 3 bytes; ready when `(reply & F8F0F0) == 78E040` (`78 E0 45`, `7B EC 47` observed) | Encoder status. |
| `28 28` | — | Unknown. |
| `29 89 5B` | 1 byte, `91` | Unknown. |
| `DD CE 3F B2` | 2 bytes, `DA 67` | Unknown. |
| `43 23 84` | 1 byte; polled until `F7` | Polled while `GCHD_EB_ENCODER_TRIGGER` is set. |
| `21 01 72`; `20 0N VV` | `F4`; — | `005d` only; `N = 2…F`. Unknown. |
| `AA 8F 3B`, `AA 8D 35`, `AA B8 29 F6`, `AA B8 29 E7` | — | Unknown; surround the block reads. |
| `AB 92 3E B4 XX YY` + 56-byte read (70 times); `AB A2 3E B4 FB 79` + 8-byte read | data | Unknown block read. Order and lengths are fixed. |
| `A1 08 XX` + 8-byte read (32 times) | data | Unknown block read. |
| `10 00 33`, `10 01 VV`, `10 02 71`, `10 03 72`, `10 01 VV&7F` | — | `VV` is derived from the value read from `0x98:B8`. Unknown. |
| `94 RR VV` | — | Write register `RR` of device `0x94`. |
| `89 89 RR`, `99 89 RR`, `9D CD RR` + 1-byte read | value | Register reads of devices `0x88`, `0x98`, `0x9C`. |
| `99 8A RR` + 2-byte read | 2 values | 2-byte read of device `0x98`. |

Repeated reads used by the driver:

- `gchd_be_poll_ed()`: read `0x98:ED` until bit 6 (`0x40`) is set.
- `gchd_be_read_ec(n)`: read `0x98:EC` `n` times; values not used.

## 3. Device registers

### Device `0x9C` — input front end, HDMI receiver

| Register | Values | Use |
|---|---|---|
| `00` | `CC`, `CE`, `CD`, `4C` | Register bank select. Writes are usually preceded by a read of `3F`. |
| `51` | `45` HDMI, `ED` component, `44`/`EC` composite and S-video | **Input selector** (`GCHD_FE_REG_INPUT_SEL`). |
| `B3`, `27` | `CC` digital inputs / `33` analog inputs | Input prefix. |
| `66/65`, `64/63` | 16-bit values | HDMI timing measurements; `66/65 ≈ AD4D` without a signal. |
| `68/67` | 16-bit value | Component timing measurement. |
| `34` (bank `CE`) | bit 2 | HDMI RGB indication. |
| `92`–`AA` (bank `CC`) | tables | Colour space (YUV or RGB). |
| `B0` | `E8` / `E9` | Opens and closes a parameter-table write window. |

### Device `0x88` — analog video decoder

| Register | Values | Use |
|---|---|---|
| `02` | `C9` composite, `DC` S-video | Composite / S-video path. |
| `03` | `2F` digital inputs, `28` analog inputs | Input prefix. |
| `06` | `86` digital inputs, `08` analog inputs | Input prefix. |
| `07`, `08`, `09` | `8A`, `9B`, `7A` | Written on every NTSC lock and mode change. |
| `0A` | `8F`, then `98` | Written during the 480i → 240p change. Unknown. |
| `10`–`15` | three tuning passes | See [composite.md](composite.md). |
| `14` | `D2` | S-video path. |
| `28` | `88` (480i), `8A` (240p) | Changes with the analog line format. |
| `CA` (read) | `FE` unlocked, `7E` 480i, `66` 240p, `EE`/`E6` transitional | **Lock and format status** (`GCHD_DEC_REG_LOCK`). |
| `FA` (read) | low nibble `6` = 60 Hz family, `7` = 50 Hz family | Signal family; polled without a signal. |
| `E7`, `F1`, `CB`, `FD`, `F8` (read) | see [composite.md](composite.md#2-status-registers) | Status; bit meanings unknown. |

### Device `0x98` — video back end

| Register(s) | Use |
|---|---|
| `70`, `0F`, `90`–`A3` | Per-mode table (`gchd_setup_subblock`). |
| `72`, `C0`, `61`, `63`, `64`, `65`, `04`, `62` | Block written after the per-mode table (`gchd_common_block_b1`). |
| `0D`, `0E` | Retrigger: read `0x98:F5`; read `FD`, `FC`, `F3`; read `F5`; `0D = C8`; read `F5`; then `0E` written twice with bit 0 set and then cleared. Bit 4 of the `0E` value equals bit 4 of the `0x98:F5` read just before it (all 44 recorded pulses). Observed values: `15`/`14`, `05`/`04`. |
| `10`–`15`, `65` | Calibration rounds (`gchd_common_block_a`), each followed by `gchd_be_poll_ed()` and 32 reads of `0x98:EC`. |
| `04` | Written (`95`, `8D`, `8C`, `88`) between blocks, after reading `0x98:FF`. |

## 4. Observed status values

| Read | Values | Notes |
|---|---|---|
| `0x88:CA` | `FE`, `7E`, `66`, `EE`, `E6` | Unlocked / 480i / 240p / transitional. |
| `0x88:FA` | `3F`, `1F`, `3E`, `16`, `06` | `1F`, `3E` right after selecting the input; low nibble `6` once locked (NTSC). |
| `0x88:E7` | `69`, `09`, `E9`, `89` | `09` right after a 240p change. |
| `0x88:F1` | `7E`, `69` | |
| `0x88:CB` | `6E`, `44` | `44` right after a 240p change. |
| `0x98:F5` | `62`, `72`, `F2`, `E2`, `82` | Bit 4 is copied into `0x98:0E`. Before capture start: after a retrigger, polled about every 4 ms; `62` until bit 7 is set (`F2`), then a second retrigger, after which it reads `72`. |
| `0x98:FD`, `FC`, `F3` | `6E`, `6E`, `7E` | Read before every retrigger. |
| `0x98:FF` | `73`, `6B`, `6A` | Read before every `0x98:04` write. |
| `0x98:8B` | `2E`, `6E` | Session-dependent. |

## 5. Encoder registers

The encoder is programmed with field writes (address, least significant bit,
width, value) and 16-bit word writes. Field names are defined in
`src/gchd_transcoder_fields.h` as `GCHD_TC_<FIELD>`. A field whose address
has bit 1 clear lives in the upper 16 bits of the 32-bit word at
`address & ~3`.

| Field | Address / lsb / bits | Values |
|---|---|---|
| `v_format` | `1502 / 0 / 8` | `0` 1080i60, `1` 1080i50, `2` 720p60, `3` 720p50, `4` 480i60, `5` 576i50, `8` 480p60, `9` 576p50, `32` 1080p60, `33` 1080p50 |
| `v_ip_format` | `1512 / 8 / 7` | `0` interlaced, `2` progressive |
| `v_in_source_type` | `1504 / 8 / 3` | Source type; `3` observed for NTSC composite |
| `tfrff` | `1504 / 0 / 2` | `2` |
| `v_hsize_out`, `v_vsize_out` | `152C`, `152E` | Output size, for example 720 × 480 |
| `v_bitrate`, `v_max_bitrate`, `v_ave_bitrate`, `v_min_bitrate` | `1532`, `1534`, `1536`, `1538` | kbit/s |
| `v_gop_struct`, `v_gop_size` | `1518 / 0 / 8`, `1518 / 8 / 8` | Anchor distance, GOP length |
| `v_pid_out`, `a_pid_out`, `pmt_pid_out`, `pcr_pid_out`, `sit_pid_out` | `1126`, `1128`, `112C`, `112E`, `1134` | TS PIDs |
| `system_rate` | `1104` | Transport stream rate, kbit/s |
| `auto_null` | `1102 / 0 / 2` | `2` |
| `a_mode`, `a_bitrate` | `100C / 12 / 4`, `1A04` | `3` = MPEG-1 Layer II; kbit/s |
| `amck_mode` | `1A08 / 3 / 1` | Audio clock enable |
| PAT, PMT, SIT tables | word writes from `1144`, `1174`, `1010` | Second output: `1284`, `12B4`, `1370` |
