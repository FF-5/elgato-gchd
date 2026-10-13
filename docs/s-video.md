# S-video input reference

## Common commands and configuration

| Target / operation | Bytes written or expected read | Notes |
|---|---|---|
| `0x88:02` | Write `DC` | S-video Y/C decoder path. Composite uses `C9`. |
| `0x88:14` | Write `D2` | S-video path parameter. |
| `0x88:28` | Write `88` (signal-present path) or `8A` (no-input path, early after switch) | Lock-/path-correlated. On the no-input path the early write after status polls is `8A`; the later apply block may write `88` or `8A`. |
| `0x88:06`, `0x88:03` | Write `08`, `28` | Analog decoder setup. |
| `0x9C:B3`, `0x9C:00`, `0x9C:27` | Write `33`, `CE`, `33`, `CC` in order | Analog front-end bank/page setup. |
| `0x9C:51` | Write `44` | S-video selector on the HDMI→S-video path (signal-present and no-input). |
| `0x9C:51` | Write `EC` | S-video selector when switching from component or another analog input with no source. See composite.md “Selector 51 EC vs 51 44”. |
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

## No-signal idle / keep-alive

After the S-video selector and front-end programming complete with no source present, the device repeats this two-command loop (approximately 50 ms per cycle). Each mailbox command uses the normal HD-New mailbox handshake.

| Order | Mailbox payload | Notes |
|---:|---|---|
| 1 | `89 89 FA 00` | Status / source-detect poll |
| 2 | `99 89 F5 00` | Sequence-read / poll helper |

The repeated `89 89 FA` read is the current source-detection candidate: when a source appears, the reply can change to `CA` and the idle loop is exited. See [hardware.md](hardware.md) for the comparison across all inputs.

## Sequences

### 240p / 288p

#### NTSC-family 240p vs PAL-family 288p-equivalent

Rows are aligned by logical operation so the two columns can be compared directly. A dash means the corresponding step was not observed (or not required) in that family’s sequence. Step numbers are sequential within each column and reflect the order seen in the respective sequences. HDMI selector/keep-alive traffic is deliberately excluded.

Observed NTSC order is taken from the HDMI→S-video 240p NTSC signal-present sequence. Backend coefficient table, retriggers, and SCMD are present after the apply block; the coefficient values match the composite NTSC set (`90=01 … A3=49`), not the different PAL set.

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

#### No-input then source-found: NTSC and PAL

The order column numbers rows only. Read the NTSC and PAL step references independently from top to bottom; blank entries mean that sequence has no corresponding step at that position.

Both NTSC and PAL no-input paths share the same early order (decoder-first) and the same source-found trigger.

| Order | NTSC step(s) | NTSC sequence difference | PAL step(s) | PAL sequence difference |
|---:|---:|---|---:|---|
| 1 | 1 | Write `0x88:02=DC`, `0x88:14=D2`; ~200 ms. | 1 | Write `0x88:02=DC`, `0x88:14=D2`. |
| 2 | 2–3 | Extra status polls `FA`, `CA`, `E7`, `F1`, `CB`, `CA`; then early `0x88:28=8A` (signal-present uses one `FA` then `28=88`). | 2–3 | Extra status polls `FA`, `CA`, `E7`, `F1`, `CB`, `CA`; then early `0x88:28=8A`. |
| 3 | 4–13 | Decoder `06=08` / `03=28`, front-end `B3=33` + page, selector `51=44`, routing `02 0E → 03 0E`, tuning passes 1–3 (decoder-first order). | 4–14 | Decoder `06=08` / `03=28`, front-end `B3=33` + page, selector `51=44`, routing `02 0E → 03 0E`, tuning passes 1–3 (decoder-first order; same as NTSC no-input). |
| 4 | — | No-input idle loop: `89 89 FA` / `99 89 F5` (about 50 ms). | — | No-input idle loop: `89 89 FA` / `99 89 F5` (about 50 ms). |
| 5 | — | Source recognition: status changes to `89 89 CA`, followed by `E7` / `F1` / further status reads; exits the idle loop. | — | Source recognition: status changes to `89 89 CA`, followed by `E7` / `CB` / `F1` / further status reads; exits the idle loop. |
| 6 | — | Apply block: `07=8A`, `08=9B`, `09=7A`, `28=88` / `8A`, `06=08`. | — | Apply block: `07=8A`, `08=9B`, `09=7A`, `28=88` / `8A`, `06=08`. |
| 7 | 14–18 | Backend coefficients `90=01…A3=49` (NTSC table) and tail writes; SCMD `04 A0 00 00` → `05 00 00 02` (later `05 00 00 04`), after source recognition. | 16–18 | Backend coefficients `90=01…A3=49` (NTSC table, not the signal-present PAL set) and tail writes; SCMD `04 A0 00 00` → `05 00 00 02`, after source recognition. Capture ends as first stream packets arrive. |

When the previous input was component (or another analog input), the selector written when no source is present is `51 EC` rather than `51 44`. See composite.md “Selector 51 EC vs 51 44” for multi-switch evidence. After `51 EC` the same FA/F5 idle and source-found path still apply.

### 480i / 576i

| NTSC step | NTSC-family / 60 Hz | PAL step | PAL-family / 50 Hz |
|---:|---|---:|---|
| 1 | NTSC-family 480i sequence **NOT CAPTURED**. | 1 | PAL-family 576i sequence **NOT CAPTURED**. |

### 480p / 576p

| NTSC step | NTSC-family / 60 Hz | PAL step | PAL-family / 50 Hz |
|---:|---|---:|---|
| 1 | NTSC-family 480p sequence **NOT CAPTURED**. | 1 | PAL-family 576p sequence **NOT CAPTURED**. |
