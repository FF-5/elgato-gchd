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

The columns are aligned by operation, not by row count. A dash means that no corresponding step is documented in that trace; each sequence's step numbers retain their original order. HDMI selector/keep-alive traffic is deliberately excluded because it is not part of the S-video decoder sequence.

Observed NTSC order is taken from the HDMI→S-video 240p NTSC signal-present capture (`filtered_240p_svideo.pcapng`). The previously documented NTSC steps already matched the packet order (no reordering required).

| NTSC step | NTSC-family 240p — working full bring-up | PAL step | PAL-family 288p-equivalent — working, frames received |
|---:|---|---:|---|
| 1 | Write `0x88:02=DC`, `0x88:14=D2`; wait about 206 ms | 3 | Write `0x88:02=DC`, `0x88:14=D2`; read `89 89 FA`; write `0x88:28=88` |
| 2 | Read `89 89 FA`; write `0x88:28=88` | 4 | Read `89 89 FD`; write `0x88:06=08`; read `89 89 F8`; write `0x88:03=28` |
| 3 | Read `89 89 FD`; write `0x88:06=08`; read `89 89 F8`; write `0x88:03=28` | 5 | Write `0x9C:B3=33`; page `CE`; write `0x9C:27=33`; return page `CC`; mailbox reads during setup |
| 4 | Write `0x9C:B3=33`; page `CE`; write `0x9C:27=33`; return page `CC`; read `9D CD 6E` | 6–7 | Write `0x9C:51=44`; routing `02 0E → 03 0E → 03 0E` |
| 5 | Write `0x9C:51=44`; routing `02 0E → 03 0E → 03 0E` | 8–10 | Read `89 89 FA`; poll `99 89 F5`; read status `FD`, `FC`, `F3`; first backend retrigger `0x98:0D=C8`, `0x98:0E=05 → 04`; further status checks |
| 6 | Tuning pass 1: `6F D4 D0 08 08 88` to `0x88:10–15` | 11 | Tuning pass 1: `6F D4 D0 08 08 88` |
| 7 | Tuning pass 2: `6F EA D0 08 08 88` | 12 | Tuning pass 2: `6F EA D0 08 08 88` |
| 8 | Tuning pass 3: `6F EA D0 1B 1B 88` | 13 | Tuning pass 3: `6F EA D0 1B 1B 88` |
| 9 | Apply block: `0x88:07=8A`, `08=9B`, `09=7A`, `28=8A`, `06=08` | — | — |
| — | — | 14 | Read `89 89 FA`, `CA`, `E7`, `F1`; read `99 89 8B`; status values are observations, not universal lock constants |
| — | — | 15–16 | Backend writes: `04=95`, `70=D0`, `0F=88`, `90=D8`, `91=BF`, `95=9D`, `96=1B`, `97=88`, `A0=8A`, `A1=D8`, `A2=B2`, `A3=59`; then `72=88`, `C0=88`, `61=98`, `63=89`, `64=96`, `65=88`, `04=8D`, `62=91` |
| — | — | 17–18 | Poll status; second retrigger `0x98:0D=C8`, `0x98:0E=15 → 14` |
| — | — | 19–21 | Configure pipeline and queue bulk IN on `0x81`, `0x84`; SCMD `04 A0 00 00`; read `0800:2008 → 00 01`; SCMD `05 00 00 02`; initial stream packets appear on `0x81` |

#### Other formats

##### 480i / 576i

| NTSC step | NTSC-family / 60 Hz | PAL step | PAL-family / 50 Hz |
|---:|---|---:|---|
| 1 | NTSC-family 480i sequence **NOT CAPTURED**. | 1 | PAL-family 576i sequence **NOT CAPTURED**. |

##### 480p / 576p

| NTSC step | NTSC-family / 60 Hz | PAL step | PAL-family / 50 Hz |
|---:|---|---:|---|
| 1 | NTSC-family 480p sequence **NOT CAPTURED**. | 1 | PAL-family 576p sequence **NOT CAPTURED**. |
