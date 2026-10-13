# S-video input reference

## Common commands and configuration

| Target / operation | Bytes written or expected read | Notes |
|---|---|---|
| `0x88:02` | Write `DC` | S-video Y/C decoder path. Composite uses `C9`. |
| `0x88:14` | Write `D2` | S-video path parameter. |
| `0x88:28` | Write `88`; some 240p paths later write `8A` | Mode-correlated value; format semantics are not fully decoded. |
| `0x88:06`, `0x88:03` | Write `08`, `28` | Analog decoder setup. |
| `0x9C:B3`, `0x9C:00`, `0x9C:27` | Write `33`, `CE`, `33`, `CC` in order | Analog front-end bank/page setup. |
| `0x9C:51` | Write `44` | S-video selector in the S-video configuration sequence. HDMI switching traffic is outside this input-specific reference. |
| `0x88:10–15` | `6F D4 D0 08 08 88`; then `6F EA D0 08 08 88`; then `6F EA D0 1B 1B 88` | Three distinct decoder-tuning passes. |
| `0x98:0D`, `0x98:0E` | `C8`, `05`, `04`; later `C8`, `15`, `14` | Backend retrigger/apply pulses; preserve order and timing. |
| Mailbox status helpers | `89 89 FA`, `89 89 CA`, `89 89 E7`, `89 89 F1`, `99 89 F5` | Polled during decoder setup and lock settling; several meanings remain UNKNOWN. |



## Session-dependent values

| Target / operation | Bytes written or expected read | Notes |
|---|---|---|
| `0900:0014` | Common shadow bytes include `D3 9E` and `03 1E` | Session-dependent; restore observed shadow, do not hard-code. |
| `0900:001C` | Values vary by session | Mailbox readiness/status; do not hard-code a single reply. |
| `99 89 8B` | Replies include `6E` and `2E` | Meaning UNKNOWN. |
| `0900:01B0` after start | Replies vary, including `00 10` and `01 50` | Meaning UNKNOWN. |

## Sequences

### 240p / 288p

#### NTSC-family 240p vs PAL-family 288p-equivalent

Rows are aligned by logical operation so the two columns can be compared directly. A dash means the corresponding step was not observed (or not required) in that family’s capture. Step numbers are sequential within each column and reflect the order seen in the respective traces. HDMI selector/keep-alive traffic is deliberately excluded.

Observed NTSC order is taken from the HDMI→S-video 240p NTSC signal-present capture (`filtered_240p_svideo.pcapng`). Backend coefficient table, retriggers, and SCMD are present after the apply block; the coefficient values match the composite NTSC set (`90=01 … A3=49`), not the different PAL set.

| NTSC step | NTSC-family 240p | PAL step | PAL-family 288p-equivalent |
|---:|---|---:|---|
| 1 | Write `0x88:02=DC`, `0x88:14=D2`; wait ~206 ms | 1 | Write `0x88:02=DC`, `0x88:14=D2` |
| 2 | Read `89 89 FA`; write `0x88:28=88` | 2 | Read `89 89 FA`; write `0x88:28=88` |
| 3 | Read `89 89 FD` | 3 | Read `89 89 FD` |
| 4 | Decoder `0x88:06=08` | 4 | Decoder `0x88:06=08` |
| 5 | Read `89 89 F8` | 5 | Read `89 89 F8` |
| 6 | Decoder `0x88:03=28` | 6 | Decoder `0x88:03=28` |
| 7 | Front-end `0x9C:B3=33` | 7 | Front-end `0x9C:B3=33` |
| 8 | Page `CE`; `0x9C:27=33`; page `CC`; read `9D CD 6E` | 8 | Page `CE`; `0x9C:27=33`; page `CC` (mailbox reads during setup) |
| 9 | Selector `0x9C:51=44` | 9 | Selector `0x9C:51=44` |
| 10 | Routing `02 0E → 03 0E → 03 0E` | 10 | Routing `02 0E → 03 0E → 03 0E` |
| — | — | 11 | Read `89 89 FA`; poll `99 89 F5`; status reads `FD`, `FC`, `F3`; first retrigger `0x98:0D=C8`, `0x98:0E=05 → 04` |
| 11 | Tuning pass 1: `6F D4 D0 08 08 88` to `0x88:10–15` | 12 | Tuning pass 1: `6F D4 D0 08 08 88` |
| 12 | Tuning pass 2: `6F EA D0 08 08 88` | 13 | Tuning pass 2: `6F EA D0 08 08 88` |
| 13 | Tuning pass 3: `6F EA D0 1B 1B 88` | 14 | Tuning pass 3: `6F EA D0 1B 1B 88` |
| 14 | Apply block: `0x88:07=8A`, `08=9B`, `09=7A`, `28=8A`, `06=08` | — | — |
| 15 | Status polls (`FA`, `CA`, `E7`, `F1`); read `99 89 8B` | 15 | Status loop: read `89 89 FA`, `CA`, `E7`, `F1`; read `99 89 8B` (values are observations, not universal lock constants) |
| 16 | Backend `04=95`, `70=D0`, `0F=88`; coefficient table `90=01 … A3=49`; then `72=88`, `C0=88`, `61=98`, `63=89`, `64=96`, `65=88`, `04=8D`, `62=91` | 16 | Backend writes: `04=95`, `70=D0`, `0F=88`, `90=D8`, `91=BF`, `95=9D`, `96=1B`, `97=88`, `A0=8A`, `A1=D8`, `A2=B2`, `A3=59`; then `72=88`, `C0=88`, `61=98`, `63=89`, `64=96`, `65=88`, `04=8D`, `62=91` |
| 17 | Retrigger `0x98:0D=C8`, `0x98:0E=05 → 04`; later `15 → 14` | 17 | Second retrigger `0x98:0D=C8`, `0x98:0E=15 → 14`; poll status |
| 18 | Pipeline setup; SCMD `04 A0 00 00`; read `0800:2008 → 00 01`; SCMD `05 00 00 02` | 18 | Configure pipeline; queue bulk IN on `0x81`, `0x84`; SCMD `04 A0 00 00`; read `0800:2008 → 00 01`; SCMD `05 00 00 02`; initial stream packets appear on `0x81` |

#### Other formats

##### 480i / 576i

| NTSC step | NTSC-family / 60 Hz | PAL step | PAL-family / 50 Hz |
|---:|---|---:|---|
| 1 | NTSC-family 480i sequence **NOT CAPTURED**. | 1 | PAL-family 576i sequence **NOT CAPTURED**. |

##### 480p / 576p

| NTSC step | NTSC-family / 60 Hz | PAL step | PAL-family / 50 Hz |
|---:|---|---:|---|
| 1 | NTSC-family 480p sequence **NOT CAPTURED**. | 1 | PAL-family 576p sequence **NOT CAPTURED**. |
