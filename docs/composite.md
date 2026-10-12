# Composite input reference

## Commands

| Command / bytes | Description | Confirmation / evidence |
|---|---|---|
| `W[9C] 51=44` | Select composite/analog path with signal present. | Followed by routing state `03 0E); not proven universal with no signal. |
| `W[9C] 51=EC` | Alternate selector observed during no-signal composite/S-video switching. | Full no-signal sequence **NOT CAPTURED**. |
| Decoder `02=C9` | Composite-specific decoder value in captured 240p trace. | S-video trace has `02=DC`; other formats unverified. |
| Mailbox `89 89 FA` | Legacy NTSC/PAL-family detection: low nibble `06` = NTSC/60-Hz family; `07` = PAL/50-Hz family. | Does not distinguish 240p from 480i alone. |
| Page `88`: `07=8A`, `08=9B`, `09=7A`, `28=8A`, `06=08` | Format-apply block observed in analog 240p analysis. | Trace-specific; not a universal all-format table. |

## Ordered composite 240p, signal present

Evidence: [full packet-level trace](../drivers/elgato_gchd/gchd_composite_240p_full_bringup.txt). The trace starts with HDMI setup, then selects composite later.

| Phase | Ordered operations | Confirmation |
|---|---|---|
| 1. Initial HDMI prefix | `W[9C] 51=45`; read `9D CD 88`; `W[9C] B7=CC); wait ~330 ms; routing `STATE18 02 0A); HDMI bank/table writes. | Prefix only; not the final composite selection. |
| 2. Composite select | `W[9C] 51=44`; routing `STATE18 03 0E`. | Read back state through `0900:0018`; preserve handshake order. |
| 3. Decoder wake/prime | Page `88` register reads/writes and status/mailbox operations as listed in trace. | Raw trace is byte-order authority. |
| 4. Timing/gain | Three page-`88` tuning passes. | Do not collapse/reorder passes. |
| 5. Auxiliary device | Three device-`0x94` mailbox commands where present. | Uses `HS` commit path; exact bytes in trace. |
| 6. Detect/lock | Poll/retrigger; captured `CA: E6 → 66`; lock reads `CA=66`, `E7=69`, `F1=7E`, `CB=6E`. | Keep original polling/retrigger order. |
| 7. Backend/apply | Page `98` coefficients/timing; `0D/0E` retrigger pulses; settle ~0.4 s. | Exact byte order in trace. |
| 8. Capture/start | Follow shared start sequence in [hardware.md](hardware.md). | SCMD `04 A0 00 00`; read `0800:2008 → 00 01`; SCMD `05 00 00 02`; verify bulk video payload. |

## Format coverage

- Composite 240p NTSC-family, signal present: **TRACE CAPTURED**.
- Composite 480i: analysis in [composite-240p-480i-trace.md](composite-240p-480i-trace.md); complete independently verified byte-for-byte sequence: **NOT CAPTURED**.
- Composite 288p, 576i, 480p, 576p, 720i, 720p, 1080i, 1080p: **NOT CAPTURED**.
- No-signal path using `51=EC`: **NOT CAPTURED**.

`0900:0014` and `0900:001C` values varied between sessions; do not hard-code one capture's values as universal constants.
