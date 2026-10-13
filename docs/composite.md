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

#### No-input / no-signal NTSC path (HDMI → composite, source appears later)

Source: `filtered_240p_noinput_composite.pcapng`. Capture starts on HDMI, switches to composite with **no source attached**, idles, then the source is recognized and the host proceeds to apply / coefficients / SCMD.

Step numbers below refer to the **NTSC column of the signal-present table above**. Identical ranges are collapsed; only differences are expanded.

| Signal-present steps | No-input then source-found | Difference |
|---:|---|---|
| 9–11 (tuning) | Run **first**, before decoder/front-end/selector | **Order difference** — signal-present does steps 1–8 then 9–11; this path does 9–11 then 1–8. |
| 1–8 | Decoder `06=08` / `03=28`, front-end, selector `51=44`, routing `02 0E → 03 0E` | Same content; now after tuning. |
| 12 | Device `0x94`: `41 37`, `4A B7`, `4B B7` | Identical. |
| — | **No-input idle loop** `89 89 FA` / `99 89 F5` (~50 ms) | **Inserted** — not present on signal-present path; same loop as S-video no-input (not the HDMI `AB 4C` keep-alive). |
| — | Status becomes `89 89 CA` (then `E7` / `F1` / …) | **Source-found trigger** that exits the FA/F5 idle. |
| — | Apply block `07=8A`, `08=9B`, `09=7A`, `28=88`/`8A`, `06=08` (appears twice) | **Inserted** — same values as S-video apply block; absent from signal-present ordered table. |
| 13–16 | Backend coeffs `90=01…A3=49` + tail; SCMD `04 A0 00 00` → `05 00 00 02` (later `05 00 00 04`) | Same NTSC table and SCMD; run after source recognition. |

#### No-input / no-signal PAL path (HDMI → composite 288p, source appears later)

Source: `filtered_288p_pal_noinput_composite.pcapng`. Capture starts on HDMI (selector `51 45`), switches to composite with **no source attached**, idles in the FA/F5 loop, then the PAL source is recognized; the host proceeds to apply / coefficients / SCMD. Trace ends when the first bulk stream packets arrive on `0x81`.

Step numbers below refer to the **PAL column of the signal-present table above**. Identical ranges are collapsed; only differences are expanded.

| Signal-present steps | No-input then source-found | Difference |
|---:|---|---|
| 1–8 | Decoder `06=08` / `03=28`, front-end, selector `51=44`, routing `02 0E → 03 0E` | Same content and order (decoder/front-end first). |
| 9 (status prime) | Present after selector in signal-present; not required to re-run here | — |
| 10–12 (tuning) | **Not run early**; tuning occurs later or is omitted from the visible switch window | **Order difference vs NTSC no-input** — PAL no-input keeps decoder-first order (no early tuning), matching the signal-present PAL path. |
| 13 | Device `0x94`: `41 37`, `4A B7`, `4B B7` | Identical. |
| — | **No-input idle loop** `89 89 FA` / `99 89 F5` (~50 ms) | **Inserted** — same loop as NTSC / S-video no-input. |
| — | Status becomes `89 89 CA` | **Source-found trigger** that exits the FA/F5 idle (identical to NTSC composite no-input). |
| — | Apply block `07=8A`, `08=9B`, `09=7A`, `28=88`/`8A`, `06=08` | **Inserted** — same values as NTSC / S-video apply block. |
| 14–17 | Backend coeffs `90=01…A3=49` + tail; SCMD `04 A0 00 00` → `05 00 00 02` (later `05 00 00 04`) | Same coefficient table and SCMD; run after source recognition. Capture starts. |

#### Other formats

##### 480i / 576i

| NTSC step | NTSC-family / 60 Hz | PAL step | PAL-family / 50 Hz |
|---:|---|---:|---|
| 1 | `0x88:28=88` seen in setup observations; full 480i sequence **NOT CAPTURED**. | 1 | PAL-family 576i sequence **NOT CAPTURED**. |

##### 480p / 576p

| NTSC step | NTSC-family / 60 Hz | PAL step | PAL-family / 50 Hz |
|---:|---|---:|---|
| 1 | 480p sequence **NOT CAPTURED**. | 1 | 576p sequence **NOT CAPTURED**. |
