# S-video input

Notation and status labels are defined in [README.md](README.md). The
S-video input shares the analog path with composite; see
[composite.md](composite.md) for the status registers, mode transitions and
encoder programming. The driver exposes S-video as V4L2 input 3.

## 1. Differences from composite

| Register | S-video | Composite |
|---|---|---|
| `0x88:02` | `DC` | `C9` |
| `0x88:14` | `D2` | — |

Routing (`0900:0018 = 03 0E`), input selector (`0x9C:51 = 44` or `EC`),
back-end tables, encoder programming and capture start are the same as for
composite. A 240p source is encoded as 720 × 480 interlaced, as on composite.

## 2. Switching from HDMI to S-video with a 240p source (Observed)

| Step | Operation |
|---:|---|
| 1 | `0x88:02 = DC`, `0x88:14 = D2`; about 206 ms pause |
| 2 | Read `0x88:FA`; `0x88:28 = 88` |
| 3 | Read `0x88:FD`; `0x88:06 = 08`; read `0x88:F8`; `0x88:03 = 28` |
| 4 | `0x9C:B3 = 33`; bank `CE`, `0x9C:27 = 33`; bank `CC`; read `0x9C:6E` |
| 5 | `0x9C:51 = 44`; `0900:0018` written `02 0E`, then `03 0E` twice |
| 6 | Decoder tuning passes on `0x88:10`–`15`: `6F D4 D0 08 08 88`, `6F EA D0 08 08 88`, `6F EA D0 1B 1B 88` |
| 7 | `0x88:07 = 8A`, `08 = 9B`, `09 = 7A`, `28 = 8A`, `06 = 08` |
| 8 | Read `0x88:FA`, `CA`, `E7`, `F1`; read `0x98:8B` |
| 9 | Back end: `0x98:04 = 95`, `70 = D0`, `0F = 88`, `90`–`A3` as for composite 480i ([composite.md §3](composite.md#3-480i-lock-and-capture-start-observed)); `72 = 88`, `C0 = 88`, `61 = 98`, `63 = 89`, `64 = 96`, `65 = 88`, `04 = 8D`, `62 = 91` |
| 10 | Retrigger `0x98:0D = C8`, `0E = 05`, `04`; later `0E = 15`, `14` |
| 11 | Encoder setup; SCMD INIT mode `A0`; read `0800:2008` (`0x01`); SCMD STATE_CHANGE START |

## 3. No source present (Observed)

With no source connected, step 2 is preceded by additional reads of
`0x88:FA`, `CA`, `E7`, `F1`, `CB`, `CA`, and `0x88:28` is written `8A`. The
device is then polled with `0x88:FA` / `0x98:F5` until `0x88:CA` indicates a
source, after which steps 7–11 follow.

When the previous input was component or composite, the selector value is
`EC` instead of `44`.

## 4. Other formats

A PAL 288p S-video source uses the PAL back-end table and encoder programming
described in [composite.md §8](composite.md#8-pal-observed). 480i and 576i
S-video sequences have not been characterized.
