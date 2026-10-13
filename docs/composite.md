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

Rows are aligned by logical operation so the two columns can be compared directly. A dash means the corresponding step was not observed (or not required) in that family’s capture. Step numbers are sequential within each column and reflect the order seen in the respective traces.

Observed NTSC order is taken from the HDMI→composite 240p NTSC signal-present capture (`filtered_240p_composite.pcapng`). The previously listed `0x88:02=C9` does not appear in that switch window and is omitted from the ordered NTSC steps (it remains in the common-commands table).

| NTSC step | NTSC-family 240p | PAL step | PAL-family 288p-equivalent |
|---:|---|---:|---|
| 1 | Read `89 89 FD` | 1 | Read `89 89 FD` |
| 2 | Decoder `0x88:06=08` | 2 | Decoder `0x88:06=08` |
| 3 | Read `89 89 F8` | 3 | Read `89 89 F8` |
| 4 | Decoder `0x88:03=28` | 4 | Decoder `0x88:03=28` |
| 5 | Front-end `0x9C:B3=33` | 5 | Front-end `0x9C:B3=33` |
| 6 | Page `CE`; `0x9C:27=33`; page `CC` (mailbox reads between writes) | 6 | Page `CE`; `0x9C:27=33`; page `CC` (mailbox reads between writes) |
| 7 | Selector `0x9C:51=44` | 7 | Selector `0x9C:51=44` |
| 8 | Routing `02 0E → 03 0E → 03 0E` | 8 | Routing `02 0E → 03 0E → 03 0E` |
| — | — | 9 | Decoder wake / status prime; status loop (`CA=66`, `E7=69`, `F1=7E`, `CB=6E`) |
| 9 | Tuning pass 1: `6F D4 D0 08 08 88` to `0x88:10–15` | 10 | Tuning pass 1: `6F D4 D0 08 08 88` |
| 10 | Tuning pass 2: `6F EA D0 08 08 88` | 11 | Tuning pass 2: `6F EA D0 08 08 88` |
| 11 | Tuning pass 3: `6F EA D0 1B 1B 88` | 12 | Tuning pass 3: `6F EA D0 1B 1B 88` |
| 12 | Device `0x94`: `41 37`, `4A B7`, `4B B7` | 13 | Device `0x94`: `41 37`, `4A B7`, `4B B7` (commit `09 00 66 03` + handshake after each) |
| 13 | Backend `04=95`, `70=D0`, `0F=88` | 14 | Backend `04=95`, `70=D0`, `0F=88` |
| 14 | Coefficient table `0x98:90–A3`; then `72=88`, `C0=88`, `61=98`, `63=89`, `64=96`, `65=88`, `04=8D`, `62=91` | 15 | Coefficient table `0x98:90–A3`; then `72=88`, `C0=88`, `61=98`, `63=89`, `64=96`, `65=88`, `04=8D`, `62=91` |
| 15 | Retrigger `0x98:0D=C8`, `0x98:0E=05 → 04`; later `15 → 14` | 16 | Retrigger `0x98:0D=C8`, `0x98:0E=05 → 04`; poll status and allow settle |
| 16 | Pipeline setup; SCMD `04 A0 00 00`; read `0800:2008 → 00 01`; SCMD `05 00 00 02` | 17 | Configure SoC pipeline; queue bulk IN on `0x81` and `0x84`; SCMD `04 A0 00 00`; read `0800:2008 → 00 01`; SCMD `05 00 00 02`. Capture starts and frames were received. |

#### No-input then source-found: NTSC and PAL

| General order | NTSC step | NTSC no-input then source-found | PAL step | PAL no-input then source-found |
|---|---:|---|---:|---|
| 1 | 9–11 | Tuning passes 1–3: `6F D4 D0 08 08 88`, `6F EA D0 08 08 88`, `6F EA D0 1B 1B 88`. Runs before decoder/front-end/selector in this path. | 10–12 | Tuning passes are not run early; they occur later or are absent from the visible switch sequence. |
| 2 | 1–8 | Decoder `0x88:06=08`, decoder `0x88:03=28`, front-end `0x9C:B3=33`, selector `0x9C:51=44`, routing `02 0E → 03 0E`. | 1–8 | Decoder `0x88:06=08`, decoder `0x88:03=28`, front-end `0x9C:B3=33`, selector `0x9C:51=44`, routing `02 0E → 03 0E`. This setup precedes tuning in the PAL path. |
| 3 | 12 | Device `0x94`: `41 37`, `4A B7`, `4B B7`. | 13 | Device `0x94`: `41 37`, `4A B7`, `4B B7`. |
| 4 | — | No-input idle loop: `89 89 FA` / `99 89 F5` (about 50 ms). | — | No-input idle loop: `89 89 FA` / `99 89 F5` (about 50 ms). |
| 5 | — | Source recognition: status changes to `89 89 CA`, followed by `E7` / `F1` / further status reads; exits the idle loop. | — | Source recognition: status changes to `89 89 CA`; exits the idle loop. |
| 6 | — | Apply block: `07=8A`, `08=9B`, `09=7A`, `28=88` / `8A`, `06=08` (the `06=08` write appears twice). | — | Apply block: `07=8A`, `08=9B`, `09=7A`, `28=88` / `8A`, `06=08`. |
| 7 | 13–16 | Backend coefficients `0x98:90–A3` and tail writes; SCMD `04 A0 00 00` → `05 00 00 02` (later `05 00 00 04`), after source recognition. | 14–17 | Backend coefficients `0x98:90–A3` and tail writes; SCMD `04 A0 00 00` → `05 00 00 02` (later `05 00 00 04`), after source recognition. |

#### Other formats

##### 480i / 576i

| NTSC step | NTSC-family / 60 Hz | PAL step | PAL-family / 50 Hz |
|---:|---|---:|---|
| 1 | `0x88:28=88` seen in setup observations; full 480i sequence **NOT CAPTURED**. | 1 | PAL-family 576i sequence **NOT CAPTURED**. |

##### 480p / 576p

| NTSC step | NTSC-family / 60 Hz | PAL step | PAL-family / 50 Hz |
|---:|---|---:|---|
| 1 | 480p sequence **NOT CAPTURED**. | 1 | 576p sequence **NOT CAPTURED**. |
