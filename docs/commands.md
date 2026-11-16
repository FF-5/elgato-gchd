# Register and command catalog

Notation, device addresses and confidence markers are defined in
[README.md](README.md). Transport primitives and the SCMD state machine are in
[hardware.md](hardware.md).

## 1. Control registers (bank `0800` / `0900`)

| Register | C++ name | Access | Meaning / use |
|---|---|---|---|
| `0800:0094`, `0098`, `009C` | `HDNEW_VERSION_REGISTER0..2` | R 4 B | HD-New version text (`"V130"…`). Zero on Original HD. |
| `0800:0010`, `0014`, `0018` | `HD_VERSION_REGISTER0..2` | R 4 B | Original-HD version text. |
| `0800:0014` | `HDNEW_SCMD_READBACK_REGISTER` | R 2 B | High byte = last SCMD received (HD-New receipt check). Same address as `HD_VERSION_REGISTER1`; meaning depends on the family. |
| `0800:0016` | `HDNEW_INTERRUPT_STATUS` | R 2 B | Read after every interrupt; always `09 00` so far. |
| `0800:00C0` | `HDNEW_MAIL_WRITE` | W | HD-New mailbox payload. |
| `0800:23BE` | `HDNEW_MAIL_READ` | R | HD-New mailbox read data (2-byte header, odd pad). |
| `0800:2008` | `SCMD_STATE_READBACK_REGISTER` | R 2 B | SCMD state, low 5 bits ([hardware.md §4.2](hardware.md#42-state-readback-08002008-low-5-bits)). |
| `0900:0000` | `BANKSEL` | W | Written `0000` before transcoder programming and at shutdown. Effect UNKNOWN. |
| `0900:0014` | `MAIL_SEND_ENABLE_REGISTER_STATE` | R/W | Enable shadow. Session-specific value; restore after each mailbox write. |
| `0900:0018` | `ENABLE_REGISTER` | R/W | Enable bits ([hardware.md §3](hardware.md#3-enable-register-and-its-shadow)). |
| `0900:001C` | `MAIL_REQUEST_READY` | R | Bit 0 ready; bits 8–11 cable type/presence. |
| `0900:0070` | — | W `0004` | Written once after the idle-firmware load. UNKNOWN. |
| `0900:0074` | `SCMD_STATE_CHANGE_COMPLETE` | R/W | Bit 2 = sticky completion; acknowledge by writing `0004`. Other bits UNKNOWN (`011E`/`011A` seen). |
| `0900:01B0` | — | R/W | Read in every completion round and written `0000` after each acknowledge. UNKNOWN; read values vary (`0150`, `0010`, `0000`). |
| `0000:0010 … 001E` | — | R | Read after the firmware loads; the values are not used. |

## 2. Bridge commands (mailbox port `0x33`)

| Payload | Reply | C++ location | Meaning |
|---|---|---|---|
| `AB A9 0F A4 55` | 3 B: `33 44 55` (processor off / not ready) or `27 F9 7B` (running) | `configureDevice`, `clearEnableState` | Processor/firmware status probe. |
| `AB A9 0F A4 5B` | 3 B: `(r & F8F0F0) == 78E040` (`78 E0 45`, `7B EC 47`) | `configureDevice` | Encoder-ready probe; at most 5 tries. |
| `28 28` | — | `configureDevice` | UNKNOWN. |
| `29 89 5B` | 1 B `91` | `configureDevice` | UNKNOWN handshake. |
| `DD CE 3F B2` | 2 B `DA 67` | `configureDevice` | UNKNOWN handshake. |
| `43 23 84` | 1 B, poll until `F7` | `configureDevice` | Polled while `EB_ENCODER_TRIGGER` is held. |
| `21 01 72` / `20 0N VV` | `F4` / — | `configureDevice` (HD-New only) | UNKNOWN table write (`N = 2…F`). |
| `AA 8F 3B`, `AA 8D 35`, `AA B8 29 F6`, `AA B8 29 E7` | — | `configureDevice`, `configureCommonBlockC` | UNKNOWN; they bracket the block reads below. |
| `AB 92 3E B4 XX YY` + 56-B read (70×); `AB A2 3E B4 FB 79` + 8-B read | data | `configureDevice` | UNKNOWN block read. Keep exact order and length. |
| `A1 08 XX` + 8-B read (32×) | data | `configureCommonBlockC` | UNKNOWN block read. |
| `10 00 33`, `10 01 VV`, `10 02 71`, `10 03 72`, `10 01 VV&~80` | — | `configureCommonBlockA` | `VV = ceil(0.95 × read(99 89 B8))`. UNKNOWN calibration. |
| `94 RR VV` | — (write handshake only) | `configureDevice`, `configure*` | Write register `RR` of device `0x94`. Composite/HDMI start with `94 41 37`, `94 4A AF`, `94 4B AF`. Windows writes `4A/4B = B7` or `BB` (**DISCREPANCY** with C++ `AF`). |
| `89 89 RR` + 1-B read | value | many | Read device `0x88` register `RR`. |
| `99 89 RR` + 1-B read | value | many | Read device `0x98` register `RR`. |
| `99 8A RR` + 2-B read | 2 values | `configureCommonBlockA` (`99 8A BF`) | 2-byte read of device `0x98`. |
| `9D CD RR` + 1-B read | value | `readDevice0x9DCD` | Read device `0x9C` register `RR`. |

Helper loops (C++):

- `pollOn0x9989ED()`: read `99 89 ED` until bit 6 (`0x40`) is set.
- `readFrom0x9989EC(n)`: read `99 89 EC` `n` times and discard the values.

## 3. Device registers with known or suspected roles

### Device `0x9C` (port `0x4E`) — HDMI receiver / input front end

| Register | Values | Notes |
|---|---|---|
| `00` | `CC`, `CE`, `CD`, `4C` | Bank/page select. Almost every write is preceded by a `9D CD 3F` read (current bank). |
| `51` | `45` HDMI, `ED` component, `44` / `EC` composite and S-video (**WIN**); C++ composite writes `CC` | **Input selector.** |
| `B3`, `27` | `CC` (digital) / `33` (analog) | Part of the input prefix ([hardware.md §5 step 24](hardware.md#5-common-initialization-c-configuredevice-verified-against-win)). |
| `66/65`, `64/63` | 16-bit timing values | HDMI mode detection (C++ `readHdmiSignalInformation`); `66/65 ≈ AD4D` means no signal. |
| `68/67` | 16-bit timing value | Component mode detection (C++). |
| `34` (bank `CE`) | bit 2 | HDMI RGB indicator (C++). |
| `92–AA` (bank `CC`) | YUV vs RGB tables | Colour-space programming (`configureColorSpace`). |
| `B0` | `E8` open / `E9` close | Parameter-table window (HDMI, component). |

### Device `0x88` (port `0x44`) — analog video decoder

| Register | Values | Notes |
|---|---|---|
| `02` | `C9` composite, `DC` S-video | Y/C vs CVBS discriminator. C++ always writes `C9`. |
| `03` | `2F` digital, `28` analog | Input prefix. |
| `06` | `86` digital, `08` analog | Input prefix. |
| `07`, `08`, `09` | `8A`, `9B`, `7A` (NTSC) | C++ `configureComposite` NTSC branch; Windows writes the same block on every analog lock and mode change. |
| `0A` | `8F` → `98` | Written during the 480i → 240p change (**WIN**). UNKNOWN. |
| `10–15` | three tuning passes (Windows); C++ writes `88 D4 D0 08 08 88` once | **DISCREPANCY**: Windows pass 1 writes `10 = 6F`, C++ writes `10 = 88`. |
| `28` | `88` (480i / interlaced lock), `8A` (240p / no-input path) | **WIN**. Toggles on 480i ↔ 240p. C++ writes `88` (composite NTSC). |
| `CA` (read) | `FE` unlocked, `7E` 480i locked, `66` 240p locked, `EE`/`E6` transient | **Lock and format status** (WIN). |
| `FA` (read) | low nibble `6` NTSC/60 Hz, `7` PAL/50 Hz | C++ composite family detection; also the no-signal poll. |
| `E7`, `F1`, `CB`, `FD`, `F8` (read) | see [composite.md](composite.md#2-status-registers) | Status; bit meanings UNKNOWN. |

### Device `0x98` (port `0x4C`) — video back end

| Register(s) | C++ helper | Notes |
|---|---|---|
| `70`, `0F`, `90–A3` | `configureSetupSubblock` | Per-mode table (HDMI, 1080i/p, 720p, 576i/p, 480i/p). The "coefficient tables" in the Windows traces are this table. |
| `72`, `C0`, `61`, `63`, `64`, `65`, `04`, `62` | `configureCommonBlockB1(mystery)` | `64 = 96/DE`, `62 = 91/01` depending on `mystery` (true for non-1080 progressive HDMI/component). |
| `58–65`, `5D` | `configureCommonBlockB2` | Per-mode values. |
| `0C`, `0D`, `0E` | `configureComposite`, `configureCommonBlockB2` | Retrigger: `0E = (s & 10) | 65`, then `… | 64`, then `0D = C8`, where `s` = read `99 89 F5`. **DISCREPANCY**: Windows writes `0E = 15/14` or `05/04` (same bit-4 toggle, base differs by `0x60`). |
| `10–15`, `65` | `configureCommonBlockA` | 7 calibration rounds, each followed by `pollOn0x9989ED` and 32 × `99 89 EC` reads. |
| `04` | many | `95`, `8D`, `8C`, `88` written between blocks, after a `99 89 FF` read. |

## 4. Status reply values observed (WIN unless stated)

| Read | Values | Correlation |
|---|---|---|
| `89 89 CA` | `FE`, `7E`, `66`, `EE`, `E6` | unlocked / 480i / 240p / transient. |
| `89 89 FA` | `3F`, `1F`, `3E`, `16`, `06`, `ED`, `FD`, `A4` | Low nibble 6/7 = family once locked. |
| `89 89 E7` | `69`, `09`, `89`, `E9`, `49` | `09` seen right after a 240p change. |
| `89 89 F1` | `7E`, `69`, `63` | `63` = C++ PAL expectation. |
| `89 89 CB` | `6E`, `44` | `44` seen right after a 240p change. |
| `99 89 F5` | `62`, `72`, `F2`, `E2`, `82`, `02` | Bit 4 feeds the `0x98:0E` retrigger; C++ B2 waits for `& 90 ≠ 0`. |
| `99 89 FD/FC/F3` | `6E`, `6E`, `7E` (or `6E`) | Read immediately before every retrigger pulse. |
| `99 89 FF` | `73`, `6B`, `6A` | Read before every `0x98:04` write. |
| `99 89 8B` | `2E`, `36`, `6E` | Session-dependent. |

## 5. Transcoder (encoder) registers

The encoder is programmed with `sparam(addr, lsb, bits, value)` (masked
32-bit write at `addr & ~3`) and `slsi(addr, value16)`. Field names come from
`src/gchd_hardware.hpp` (Fujitsu script names). A Windows trace line such as
`SOC 1500 v=00000004 m=000000ff` is an `sparam` to the 32-bit word at `0x1500`.
When bit 1 of the field address is clear, the field lives in the **upper**
16 bits of that word.

Key fields (main stream; `_bl` / `_ts_out2` copies live at `+0x100` / `+0x140`):

| Field | Address / lsb / bits | Values used |
|---|---|---|
| `v_format` | `1502 / 0 / 8` | `0` 1080i60, `1` 1080i50, `2` 720p60, `3` 720p50, `4` 480i60, `5` 576i50, `8` 480p60, `9` 576p50, `32` 1080p60, `33` 1080p50, `34` 1080p24 |
| `v_ip_format` | `1512 / 8 / 7` | `0` interlaced, `2` progressive |
| `v_in_source_type` | `1504 / 8 / 3` | C++: `0` NTSC 4:3, `1` NTSC 16:9, `2` PAL 16:9, `3` PAL 4:3. **DISCREPANCY**: Windows writes `3` for NTSC 240p composite. |
| `tfrff` | `1504 / 0 / 2` | `2` |
| `v_hsize_out`, `v_vsize_out` | `152C`, `152E` (16 bits each) | Output size, for example `720 × 480`. These are the "dimension candidates `0x152C`" of older notes. |
| `v_bitrate`, `v_max_bitrate`, `v_ave_bitrate`, `v_min_bitrate` | `1532`, `1534`, `1536`, `1538` | kbit/s |
| `v_gop_struct`, `v_gop_size` | `1518 / 0 / 8`, `1518 / 8 / 8` | 3 (B-frames), `(fps+10)/5` |
| `v_h264_profile`, `v_h264_level` | see header | profile 1 = High, level 41 / mode-dependent |
| `v_pid_out`, `a_pid_out`, `pmt_pid_out`, `pcr_pid_out`, `sit_pid_out` | `1126`, `1128`, `112C`, `112E`, `1134` | C++: video `0x1011`, audio `0x010F`, PMT `0x0110`, PCR `0x0100`, SIT `0x001F` |
| `system_rate` | `1104` | `1.075 × (max video + audio) + 256` kbit/s |
| `auto_null` | `1102 / 0 / 2` | `2` |
| `a_mode`, `a_bitrate` | `100C / 12 / 4`, `1A04` | `3` = MPEG-1 Layer II; bitrate in kbit/s |
| `amck_mode` | `1A08 / 3 / 1` | C++ `transcoderOutputEnable(on/off)` |
| PAT / PMT / SIT tables | `slsi` from `1144` / `1174` / `1010` (TS_OUT2: `1284` / `12B4` / `1370`) | Generated by `src/gchd/psi_*.cpp`. |

The full list (about 200 fields) is in `src/gchd_hardware.hpp` lines 460–695.
