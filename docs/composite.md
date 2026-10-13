# Composite input reference

## Common commands and configuration

| Target / operation | Bytes written or expected read | Notes |
|---|---|---|
| `0x88:06` | Write `08` | Analog decoder mode. |
| `0x88:03` | Write `28` | Analog decoder setup. |
| `0x9C:B3` | Write `33` | Analog front-end setup; meaning partly unknown. |
| `0x9C:00`, `0x9C:27` | `CE`, `33`, `CC` | Bank/page setup; preserve order. |
| `0x9C:51` | Write `44` | Composite selector in the working signal-present path. |
| `0x9C:51` | Write `EC` | Also seen during no-source switching; context-specific alternative. |
| `0x88:02` | Write `C9` | Composite discriminator; S-video uses `DC`. |
| Mailbox read `89 89 FA` | Expect low nibble `6` (NTSC/60 Hz) or `7` (PAL/50 Hz) | Detects family, not progressive/interlaced format. |
| Mailbox read `89 89 FD` | Working NTSC-family path: `6E` | Status poll; meaning unknown. |
| Mailbox read `89 89 F1` | PAL branch expects `63` | PAL status poll; exact semantics unknown. |
| `0x88:10–15` | `6F D4 D0 08 08 88` | Decoder tuning pass 1. |
| `0x88:10–15` | `6F EA D0 08 08 88` | Decoder tuning pass 2. |
| `0x88:10–15` | `6F EA D0 1B 1B 88` | Decoder tuning pass 3. |
| `0x98:0D`, `0x98:0E` | `C8`, `15`, `14` | Backend retrigger pulse; preserve order. |
| `0x98:0D`, `0x98:0E` | `C8`, `05`, `04` | Apply pulse after backend programming. |
| `0x98` backend | See ordered sequence and coefficient table | Register meanings are not fully decoded. |



### Backend coefficient table

| Target | Value | Target | Value |
|---|---:|---|---:|
| `0x98:90` | `01` | `0x98:9A` | `9C` |
| `0x98:91` | `BD` | `0x98:9B` | `8C` |
| `0x98:92` | `9A` | `0x98:9C` | `98` |
| `0x98:93` | `3A` | `0x98:9D` | `93` |
| `0x98:94` | `E9` | `0x98:9E` | `83` |
| `0x98:95` | `AB` | `0x98:9F` | `A9` |
| `0x98:96` | `17` | `0x98:A0` | `8B` |
| `0x98:97` | `88` | `0x98:A1` | `E8` |
| `0x98:98` | `84` | `0x98:A2` | `81` |
| `0x98:99` | `8A` | `0x98:A3` | `49` |


## Sequences

### 240p / 288p

#### NTSC-family 240p vs PAL-family 288p-equivalent

The columns are aligned by operation, not by row count. A dash means that no corresponding step is documented in that trace; step numbers within each sequence retain their original order.

Observed NTSC order is taken from the HDMI→composite 240p NTSC signal-present capture (`filtered_240p_composite.pcapng`). Decoder/front-end setup precedes the composite selector and routing-state change. The previously listed `0x88:02=C9` does not appear in that switch window and is omitted from the ordered NTSC steps (it remains in the common-commands table).

| NTSC step | NTSC-family 240p — working full bring-up | PAL step | PAL-family 288p-equivalent — working, frames received |
|---:|---|---:|---|
| 1 | Read `89 89 FD`; decoder `0x88:06=08`, read `89 89 F8`, `0x88:03=28`; front end `0x9C:B3=33`, page `CE`, `0x9C:27=33`, page `CC`, with mailbox reads between writes | 1–2 | Read `89 89 FD`; decoder `0x88:06=08`, read `89 89 F8`, `0x88:03=28`; front end `0x9C:B3=33`, page `CE`, `0x9C:27=33`, page `CC`, with mailbox reads between writes |
| 2 | Switch to composite `0x9C:51=44`; routing `02 0E → 03 0E → 03 0E` | 3–4 | Select composite `0x9C:51=44`; routing `02 0E → 03 0E → 03 0E` |
| 3 | Tuning pass 1: `6F D4 D0 08 08 88` to `0x88:10–15` | 6 | Tuning pass 1: `6F D4 D0 08 08 88` |
| 4 | Tuning pass 2: `6F EA D0 08 08 88` | 7 | Tuning pass 2: `6F EA D0 08 08 88` |
| 5 | Tuning pass 3: `6F EA D0 1B 1B 88` | 8 | Tuning pass 3: `6F EA D0 1B 1B 88` |
| 6 | Auxiliary device `0x94`: `41 37`, `4A B7`, `4B B7`; backend `04=95`, `70=D0`, `0F=88` | 9 | Device `0x94`: `41 37`, `4A B7`, `4B B7`; commit `09 00 66 03` and handshake after each |
| 7 | Program coefficient table `0x98:90–A3`; then `72=88`, `C0=88`, `61=98`, `63=89`, `64=96`, `65=88`, `04=8D`, `62=91` | 12 | Backend `04=95`, `70=D0`, `0F=88`; coefficient table `90–A3`; then `72=88`, `C0=88`, `61=98`, `63=89`, `64=96`, `65=88`, `04=8D`, `62=91` |
| 8 | Retrigger `0x98:0D=C8`, `0x98:0E=05 → 04`; later `15 → 14` | 13 | Retrigger `0x98:0D=C8`, `0x98:0E=05 → 04`; poll status and allow settle |
| 9 | Pipeline setup; SCMD `04 A0 00 00`; read `0800:2008 → 00 01`; SCMD `05 00 00 02` | 5, 10–11 | Decoder wake/status prime; status loop and confirmations `CA=66`, `E7=69`, `F1=7E`, `CB=6E` |
| — | — | 14 | Configure SoC pipeline; queue bulk IN on `0x81` and `0x84`; SCMD `04 A0 00 00`; read `0800:2008 → 00 01`; SCMD `05 00 00 02`. Capture starts and frames were received. |

#### Other formats

##### 480i / 576i

| NTSC step | NTSC-family / 60 Hz | PAL step | PAL-family / 50 Hz |
|---:|---|---:|---|
| 1 | `0x88:28=88` seen in setup observations; full 480i sequence **NOT CAPTURED**. | 1 | PAL-family 576i sequence **NOT CAPTURED**. |

##### 480p / 576p

| NTSC step | NTSC-family / 60 Hz | PAL step | PAL-family / 50 Hz |
|---:|---|---:|---|
| 1 | 480p sequence **NOT CAPTURED**. | 1 | 576p sequence **NOT CAPTURED**. |
