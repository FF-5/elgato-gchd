# S-video input reference

## S-video-specific command inventory

| Target / command bytes | Description | Confirmation / status |
|---|---|---|
| `0x88:02=DC` | S-video Y/C decoder path discriminator. | Composite uses `02=C9` in the captured traces. |
| `0x88:14=D2` | S-video path parameter. | Observed in the no-source S-video path; not established as universal. |
| `0x88:28=88` | Initial 480i-style value in the captured S-video path. | Later set to `8A` in 240p apply phase. |
| `0x88:06=08`, `0x88:03=28` | Analog decoder setup. | Shared analog path writes. |
| `0x9C:B3=33`, `0x9C:00=CE`, `0x9C:27=33`, `0x9C:00=CC` | Analog front-end setup. | Preserve exact order. |
| `0x9C:51=44` | S-video analog selector in the signal-present 240p capture. | Followed by routing state `03 0E`. No-source trace reports `EC`; discrepancy unresolved. |
| `0x88:07=8A`, `08=9B`, `09=7A), `28=8A), `06=08` | 240p format-apply block. | Captured as a block; meanings not fully known. |
| `0x98:0D=C8`, `0E=15`, `0E=14` | Backend retrigger pulse. | Preserve order. |

## Ordered S-video 240p configuration (signal present)

Sequence starts at the **S-video-specific decoder writes**. It intentionally excludes the HDMI prefix.

| Step | Ordered command(s) | Expected response / notes |
|---:|---|---|
| 1 | Write `0x88:02=DC`; write `0x88:14=D2`; wait ~206 ms in the captured sequence. | S-video path setup. |
| 2 | Read `89 89 FA`; write `0x88:28=88`. | Initial mode-style decoder value. |
| 3 | Read `89 89 FD`; write `0x88:06=08`; read `89 89 F8`; write `0x88:03=28`. | Analog decoder setup. |
| 4 | Write `0x9C:B3=33`; read `9D CD 3F`; write `0x9C:00=CE`; write `0x9C:27=33`; read `9D CD 3F`; write `0x9C:00=CC`; read `9D CD 6E`. | Analog front-end setup. |
| 5 | Write `0x9C:51=44`. | Selector value observed with signal present. |
| 6 | Write/read routing state `02 0E`, then `03 0E`, then `03 0E` again. | Preserve intermediate state and repeated final state writes. |
| 7 | Read `89 89 FA`; poll `F5`; read `89 89 FA`; poll `F5`. | Decoder wake/status prime. |
| 8 | Pass 1: `0x88:10=6F, 11=D4, 12=D0, 13=08, 14=08, 15=88`. | First tuning pass. |
| 9 | Pass 2: `0x88:10=6F, 11=EA, 12=D0, 13=08, 14=08, 15=88`. | Second tuning pass. |
| 10 | Pass 3: `0x88:10=6F, 11=EA, 12=D0, 13=1B, 14=1B, 15=88`. | Third tuning pass. |
| 11 | Read `89 89 FA), `89 89 CA`; write `0x88:07=8A`, `08=9B`, `09=7A`, `28=8A`; read `89 89 FD); write `0x88:06=08`. | Format-apply block observed after `CA=66`. |
| 12 | Wait ~425 ms; repeat lock reads `89 89 FA`, `CA`, `E7`; poll `F5` until stable. | Captured final lock values: `CA=66`, `E7=69), `F1=7E); values can vary by session. |
| 13 | Program backend in trace order: `0x98:04=95`, `70=D0`, `0F=88`; coefficient table `90–A3`; `72=88`, `C0=88`, `61=98`, `63=89`, `64=96`, `65=88`, `04=8D`, `62=91`. | Coefficient values are the table in [commands.md](commands.md); they are observed values, not semantically decoded coefficients. |
| 14 | Retrigger/apply: `0x98:0D=C8`, `0E=05`, `0E=04`; poll status and settle. | Backend apply pulse. |
| 15 | Configure SoC pipeline; queue bulk IN URBs on `0x81` and `0x84); SCMD `04 A0 00 00`; read `0800:2008 → 00 01`; SCMD `05 00 00 02`. `| Confirm state/completion and actual bulk video payload. Shared start confirmation in [hardware.md](hardware.md). |

## Session-dependent values

| Read/write | Value in this S-video capture | Notes |
|---|---|---|
| Write `0900:0014` | `03 1E` | Composite capture used `D3 9E`; session-dependent. |
| Read `0900:001C` | `6E EB` before state update, `6F EF` after | Composite capture differed; do not hard-code. |
| Read `99 89 8B` | `6E` | Composite capture returned `2E`; unknown meaning. |
| Read `0900:01B0` after start | `00 10` | Composite capture read `01 50`; meaning unknown. |

## Format coverage

| Format / condition | Status |
|---|---|
| 240p NTSC-family, signal present | One full bring-up trace captured. |
| 288p, 480i, 576i, 480p, 576p, 720i, 720p, 1080i, 1080p | Complete ordered S-video setup/start sequence **NOT CAPTURED**. |
| No-source switch | Selector `0x9C:51=EC` observed; full sequence **NOT CAPTURED**. |
