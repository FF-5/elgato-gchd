# S-video input reference

## Common commands and configuration

| Target / operation | Bytes written or expected read | Notes |
|---|---|---|
| `0x88:02` | Write `DC` | S-video Y/C decoder path. Composite uses `C9`. |
| `0x88:14` | Write `D2` | S-video path parameter. |
| `0x88:28` | Write `88`; some 240p paths later write `8A` | Mode-correlated value; format semantics are not fully decoded. |
| `0x88:06`, `0x88:03` | Write `08`, `28` | Analog decoder setup. |
| `0x9C:B3`, `0x9C:00`, `0x9C:27` | Write `33`, `CE`, `33`, `CC` in order | Analog front-end bank/page setup. |
| `0x9C:51` | Write `45`, then later `44` | Initial HDMI selector followed by S-video selector during the switch. |
| `0x88:10–15` | `6F D4 D0 08 08 88`; then `6F EA D0 08 08 88`; then `6F EA D0 1B 1B 88` | Three distinct decoder-tuning passes. |
| `0x98:0D`, `0x98:0E` | `C8`, `05`, `04`; later `C8`, `15`, `14` | Backend retrigger/apply pulses; preserve order and timing. |
| Mailbox status helpers | `89 89 FA`, `89 89 CA`, `89 89 E7`, `89 89 F1`, `99 89 F5` | Polled during decoder setup and lock settling; several meanings remain UNKNOWN. |


## Sequences

### 240p / 288p


#### NTSC-family 240p — working full bring-up

| Step | Command / bytes written or read | Notes |
|---:|---|---|
| 1 | Write `0x88:02=DC`, `0x88:14=D2`; wait about 206 ms | S-video decoder setup. |
| 2 | Read `89 89 FA`; write `0x88:28=88` | Initial mode-style value. |
| 3 | Read `89 89 FD`; write `0x88:06=08`; read `89 89 F8`; write `0x88:03=28` | Decoder setup. |
| 4 | Write `0x9C:B3=33`; page `CE`; write `0x9C:27=33`; return page `CC`; read `9D CD 6E` | Analog front-end setup. |
| 5 | Write `0x9C:51=44`; routing state `02 0E`, `03 0E`, `03 0E` | Selector and routing state. |
| 6 | Three tuning passes: `6F D4 D0 08 08 88`; `6F EA D0 08 08 88`; `6F EA D0 1B 1B 88` to `0x88:10–15` | Preserve all passes. |
| 7 | Apply block: `0x88:07=8A`, `08=9B`, `09=7A`, `28=8A`, `06=08` | Observed in the NTSC-family 240p path; not observed in the PAL sequence above. |
| 8 | Program backend and retrigger `0x98:0D=C8`, `0E=05`, `0E=04` | See [commands.md](commands.md) for the separate coefficient reference. |
| 9 | SCMD `04 A0 00 00`; state readback; SCMD `05 00 00 02` | Start capture after pipeline setup. |



#### PAL-family 288p-equivalent — working, frames received

| Step | Command / bytes written or read | Notes |
|---:|---|---|
| 1 | Write `0x9C:51=45`; write routing word `0900:0018 = 02 0A` | Trace begins at the HDMI selector; routing state is initially HDMI. |
| 2 | HDMI no-signal keep-alive loop: `0x9C:AB=4C`; read `9D CD 6A 00`; read `99 89 F5 00`; read `9D CD 94 00` | Repeats before the input change. |
| 3 | Write `0x88:02=DC`, `0x88:14=D2`; read `89 89 FA`; write `0x88:28=88` | Select S-video decoder path and initialize it. |
| 4 | Read `89 89 FD`; write `0x88:06=08`; read `89 89 F8`; write `0x88:03=28` | Decoder setup. |
| 5 | Write `0x9C:B3=33`; read `9D CD 3F 00`; write `0x9C:00=CE`, `0x9C:27=33`; read `9D CD 3F 00`; write `0x9C:00=CC`; read `9D CD 6E 00` | Analog front-end bank/page setup. |
| 6 | Write `0x9C:51=44` | S-video selector. |
| 7 | Write routing word `0900:0018 = 02 0E`, then `03 0E`, then `03 0E` again | Preserve intermediate state and repeated final write. |
| 8 | Read `89 89 FA`; poll `99 89 F5`; read `99 89 FD`, `99 89 FC`, `99 89 F3`; poll `99 89 F5` | Decoder status/wake sequence. |
| 9 | Write `0x98:0D=C8`; poll `99 89 F5`; write `0x98:0E=05`, then `0x98:0E=04` | First backend retrigger pulse. |
| 10 | Read `89 89 FA`, `89 89 CA`, `89 89 E7`; poll `99 89 F5` | Status checks before decoder tuning. |
| 11 | Pass 1: `0x88:10–15 = 6F D4 D0 08 08 88` | First tuning pass. |
| 12 | Pass 2: `0x88:10–15 = 6F EA D0 08 08 88` | Second tuning pass. |
| 13 | Pass 3: `0x88:10–15 = 6F EA D0 1B 1B 88` | Third tuning pass. |
| 14 | Read `89 89 FA`, `89 89 CA`, `89 89 E7`, `89 89 F1`; read `99 89 8B` | Status checks after tuning. Values are not treated as universal lock constants. |
| 15 | Write backend values in order: `0x98:04=95`, `70=D0`, `0F=88`, `90=D8`, `91=BF`, `95=9D`, `96=1B`, `97=88`, `A0=8A`, `A1=D8`, `A2=B2`, `A3=59` | PAL S-video backend writes observed in this path. This is a sparse set of writes, not a complete `90–A3` coefficient table. |
| 16 | Read `99 89 89`; write `0x98:72=88`, `C0=88`, `61=98`, `63=89`, `64=96`, `65=88`; read `99 89 FF`; write `0x98:04=8D`, `62=91` | Continue backend configuration in observed order. |
| 17 | Poll `99 89 F5`, `99 89 FD`, `99 89 FC`, `99 89 F3`; poll `99 89 F5` repeatedly | Backend/status settling. |
| 18 | Write `0x98:0D=C8`; poll `99 89 F5`; write `0x98:0E=15`, then `0x98:0E=14` | Second backend retrigger pulse. |
| 19 | Continue status polling; configure pipeline and queue bulk IN transfers on endpoints `0x81` and `0x84` | Pipeline setup precedes stream start. |
| 20 | SCMD `04 A0 00 00`; read `0800:2008 → 00 01`; SCMD `05 00 00 02`; confirm state/completion | INIT without firmware reload, then START. Video frames were received. |
| 21 | Bulk IN traffic on `0x81` | Initial stream packets appear before the trace ends. |


### 480i / 576i

#### NTSC-family / 60 Hz

| Step | Command / bytes written or read | Notes |
|---:|---|---|
| 1 | NTSC-family 480i sequence **NOT CAPTURED**. | Complete ordered bring-up **NOT CAPTURED**. |

#### PAL-family / 50 Hz

| Step | Command / bytes written or read | Notes |
|---:|---|---|
| 1 | PAL-family 576i sequence **NOT CAPTURED**. | Complete ordered bring-up **NOT CAPTURED**. |

### 480p / 576p

#### NTSC-family / 60 Hz

| Step | Command / bytes written or read | Notes |
|---:|---|---|
| 1 | NTSC-family 480p sequence **NOT CAPTURED**. | Complete ordered bring-up **NOT CAPTURED**. |

#### PAL-family / 50 Hz

| Step | Command / bytes written or read | Notes |
|---:|---|---|
| 1 | PAL-family 576p sequence **NOT CAPTURED**. | Complete ordered bring-up **NOT CAPTURED**. |



## Session-dependent values

| Target / operation | Bytes written or expected read | Notes |
|---|---|---|
| `0900:0014` | Common shadow bytes include `D3 9E` and `03 1E` | Session-dependent; restore observed shadow, do not hard-code. |
| `0900:001C` | Values vary by session | Mailbox readiness/status; do not hard-code a single reply. |
| `99 89 8B` | Replies include `6E` and `2E` | Meaning UNKNOWN. |
| `0900:01B0` after start | Replies vary, including `00 10` and `01 50` | Meaning UNKNOWN. |
