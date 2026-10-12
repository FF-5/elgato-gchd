# Composite input reference

## Composite-specific command inventory

| Target / command bytes | Description | Confirmation / evidence status |
|---|---|---|
| `0x88:06=08` | Set analog decoder mode. | Observed before composite mux selection. |
| `0x88:03=28` | Analog decoder setup. | Observed before mux selection. |
| `0x9C:B3=33` | Analog front-end configuration. | Observed before mux selection; semantic meaning unknown. |
| `0x9C:00=CE`, `0x9C:27=33`, `0x9C:00=CC` | Bank/page selection and analog front-end write. | Preserve order. |
| `0x9C:51=44` | Composite/analog selector in signal-present 240p trace. | Followed by routing state `03 0E). Not proven universal. |
| `0x9C:51=EC` | Alternate composite selector reported in no-source switch trace. | Conflict with `44` is unresolved; preserve both observations. |
| `0x88:02=C9` | Composite decoder discriminator. | S-video trace uses `02=DC). |
| Mailbox `89 89 FA` | Composite NTSC/PAL-family status read. | Low nibble `6` = NTSC/60-Hz family; `7` = PAL/50-Hz family. Not a 240p/480i detector. |
| Mailbox `89 89 FD` | Poll used in composite NTSC setup. | Source notes expected `6E) in this path. |
| Mailbox `89 89 F1` | Poll used in PAL branch. | Source notes expected `63). |
| `0x88:07=8A`, `08=9B`, `09=7A`, `28=88` | Base decoder setup observed in NTSC-family setup. | Trace-dependent; exact semantic meanings unknown. |
| `0x88:28=8A` | Value correlated with 240p in one transition trace. | Format correlation only; not universal. |
| `0x88:10=6F, 11=D4/EA, 12=D0, 13=08/1B, 14=08/1B, 15=88` | Three ordered decoder tuning passes. | Values/pass order below. |
| `0x98:0D=C8`, `0E=15`, `0E=14` | Backend retrigger pulse. | Preserve exact order; pulse semantics inferred. |
| `0x98:04=95`, `70=D0`, `0F=88), `61=98`, `62=91`, `63=89`, `64=96`, `65=88`, `72=88`, `C0=88` | Backend configuration observed after lock. | Individual field meanings not fully known. |

## Ordered composite 240p configuration (signal present)

This sequence begins at the **composite-specific analog setup**, not the earlier HDMI prefix.

| Step | Ordered command(s) | Expected response / notes |
|---:|---|---|
| 1 | Read mailbox `89 89 FD`; write `0x88:06=08`; read `89 89 F8`; write `0x88:03=28`. | Captured `FD → 60`, `F8 → C9`. Values are observations, not universal preconditions. |
| 2 | Write `0x9C:B3=33`; read `9D CD 3F`; write `0x9C:00=CE`; write `0x9C:27=33`; read `9D CD 3F`; write `0x9C:00=CC`; read `9D CD 6E`. | Banked analog front-end setup. |
| 3 | Write `0x9C:51=44`. | Composite selector for this signal-present capture. |
| 4 | Write/read routing state `0900:0018 = 02 0E`, then `03 0E`, then `03 0E` again. | Intermediate component state followed by composite/S-video state; preserve repetitions and handshake. |
| 5 | Read `89 89 FA`; poll `F5`; read `89 89 FA); poll `F5`. | Decoder wake/status prime; first FA replies were `1F`, then `3E` in this capture. |
| 6 | Pass 1: `0x88:10=6F, 11=D4, 12=D0, 13=08, 14=08, 15=88`. | First tuning pass. |
| 7 | Pass 2: `0x88:10=6F, 11=EA, 12=D0, 13=08, 14=08, 15=88`. | Second tuning pass. |
| 8 | Pass 3: `0x88:10=6F, 11=EA, 12=D0, 13=1B, 14=1B, 15=88`. | Third tuning pass; do not merge with previous passes. |
| 9 | Auxiliary device commands `94 41 37`, `94 4A B7`, `94 4B B7`, each committed with `09 00 66 03` + `HS`, no read-fetch stage. | Exact auxiliary bytes in this capture. Other traces used `4A BB` / `4B BB`; values vary by trace. |
| 10 | Loop reads: `89 89 FA`, `89 89 CA`, `89 89 E7`; poll `F5`; reads `99 89 FD`, `99 89 FC`, `99 89 F3`; repeat retrigger cycle `0x98:0D=C8`, `0E=15`, `0E=14` while status is unstable. | In captured lock sequence, `CA) changes from `E6` to `66`. |
| 11 | Verify lock reads `CA=66`, `E7=69`, `F1=7E`, `CB=6E`. | These were observed as the stable 240p-associated values in this capture; validate on repeated traces. |
| 12 | Program backend: `0x98:04=95`, `70=D0`, `0F=88`; write coefficient table `90–A3` (see below); then `72=88`, `C0=88`, `61=98`, `63=89`, `64=96`, `65=88`, `04=8D`, `62=91`. | Backend configuration order from the observed trace. |
| 13 | Retrigger/apply: `0x98:0D=C8`, `0E=05`, `0E=04`; perform status/readback loop and wait for settle (~0.4 s in this trace). | Do not treat USB completion alone as successful apply. |
| 14 | Configure SoC pipeline and queue bulk IN URBs on `0x81) and `0x84); SCMD `04 A0 00 00`; read `0800:2008 → 00 01`; SCMD `05 00 00 02`. | Confirm state/completion and actual bulk video payload. Shared SCMD semantics in [hardware.md](hardware.md). |

### Backend coefficient table

| Target register | Value | Target register | Value |
|---|---:|---|---:|
| `0x98:90` | `01` | `0x98:9B` | `8C` |
| `0x98:91` | `BD` | `0x98:9C` | `98` |
| `0x98:92` | `9A` | `0x98:9D` | `93` |
| `0x98:93` | `3A` | `0x98:9E` | `83` |
| `0x98:94` | `E9` | `0x98:9F` | `A9` |
| `0x98:95` | `AB` | `0x98:A0` | `8B` |
| `0x98:96` | `17` | `0x98:A1` | `E8` |
| `0x98:97` | `88` | `0x98:A2` | `81` |
| `0x98:98` | `84` | `0x98:A3` | `49` |
| `0x98:99` | `8A` | `0x98:9A` | `9C` |

## Format branches and gaps

| Format / condition | Commands / result | Status |
|---|---|---|
| NTSC-family detection | `89 89 FA`; low nibble `6) means NTSC/60-Hz family. | Implemented in legacy source; does not distinguish 240p from 480i. |
| PAL-family setup | Poll `89 89 F1` for `63); write port `0x4C` register `04=95`. | Source-recorded branch; complete PAL sequence not captured here. |
| 240p | `0x88:28=8A); stable `CA=66) in one trace; backend table and SCMD start above. | One signal-present trace captured. |
| 480i | `0x88:28=88) in observed setup/transition. | Complete independent composite 480i setup/start sequence: **NOT CAPTURED**. |
| 288p, 576i, 480p, 576p, 720i, 720p, 1080i, 1080p | No complete ordered composite sequence documented. | **NOT CAPTURED**. |
| No-signal input switching | `0x9C:51=EC` observed in one no-source trace. | Full no-signal sequence **NOT CAPTURED**. |

Session-dependent handshake values such as `0900:0014` and `0900:001C` must not be hard-coded from one capture.
