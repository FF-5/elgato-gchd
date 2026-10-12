# HDMI input reference

## Known HDMI commands

| Order | Target / bytes | Description | Confirmation / status |
|---:|---|---|---|
| 1 | `0x9C:51=45` | HDMI mux selector observed in traces. | Observed; full standalone HDMI recipe is not captured. |
| 2 | Read `9D CD 88`; write `0x9C:B7=CC` | Initial read and follow-up write after selector. | Wait ~330 ms in the analog-trace prefix; timing is trace-specific. |
| 3 | State word `0900:0018 = 02 0A` | Set HDMI routing state. | Trace writes and reads back the state word. |
| 4 | Bank/page and front-end setup below | Configure HDMI front end. | Exact values observed in analog trace prefix, but not verified across HDMI modes. |

## Front-end register sequence observed in trace prefix

| Order | Target write/read | Description / note |
|---:|---|---|
| 1 | Read `9D CD 3F → B2`; write `00=CE` | Select page `CE). |
| 2 | Read `9D CD 3E → D3`; write `01=AD` | Front-end setup; meaning unknown. |
| 3 | Read `9D CD 3B → B3`; write `04=CD`, `06=C4` | Front-end setup; meaning unknown. |
| 4 | Read `9D CD 36 → BA`; write `09=E4` | Front-end setup; meaning unknown. |
| 5 | Read `9D CD 3F → B0`; write `00=CC` | Return to default page. |
| 6 | Read `9D CD 6B → 82`; write `54=EC` | Front-end setup. |
| 7 | Read `9D CD 93 → B2`; write `AC=4C` | Front-end setup. |
| 8 | Read `9D CD 3F → B2`; write `00=4C`; read `3F → 32`; write `00=CC` | Page/bank sequence. |
| 9 | Read `9D CD F1 → B2`; write `CE=4C`; read `F0 → B2`; write `CF=CE`; read `3F → B2`; write `00=CC` | Front-end setup. |
| 10 | Read `9D CD 6A → B1`; write `B0=E8` | Open parameter-table window. |
| 11 | Write `B1=0C, 92=8C, 93=CB, 94=CA, 95=C8, 96=D1, 97=B1, 98=6E, 99=B6, 9A=D1, 9B=C4, 9C=DC, 9D=B7, 9E=94, 9F=B2, A0=11, A1=CD, A2=5D, A3=CB, A4=CA, A5=EC, A6=CC, A7=C8, A8=CC, A9=EC, AA=CC, AD=C9` | Parameter-table bytes in captured order. Individual meanings unknown. |
| 12 | Read `9D CD 8F → 96`; write `B0=E9` | Close/commit parameter table. |
| 13 | Poll `F5) as shown in trace; recurring reads `94`, `6A` and write `AB=4C` | Idle/keep-alive behavior in analog trace prefix; whether needed for active HDMI source is not established. |

## HDMI detection and format setup

| Command / data | Description | Status |
|---|---|---|
| Reads involving `0x66/0x65`, `0x64/0x63` | Legacy implementation averages timing/status values. | Heuristic thresholds only. |
| Read `0x34` after selecting page `CE` | Bit 2 is used by source as an RGB indicator. | Source-derived interpretation; verify on hardware. |
| Timing candidates | `0x6463≈0xB6D7` 1080p; `≈0xB081` 1080i; `≈0xB05C` 720p; `≈0xB0C1` midpoint for 480p/576p | Approximate source heuristics, not a complete format table. |

Complete HDMI input setup, per-format programming, device configuration before stream start, and HDMI-specific lock confirmation are **NOT CAPTURED**. The shared firmware lifecycle and SCMD/start confirmation are in [hardware.md](hardware.md); generic unknowns are in [commands.md](commands.md).

## Format coverage

Complete ordered HDMI sequences for 240p, 288p, 480i, 576i, 480p, 576p, 720i, 720p, 1080i, and 1080p: **NOT CAPTURED**.
