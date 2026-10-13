# HDMI input reference

## Common commands and configuration

| Target / operation | Bytes written or expected read | Notes |
|---|---|---|
| `0x9C:51` | Write `45` | HDMI input selector. |
| `0x9C:B7`; mailbox read | Write `CC`; read `9D CD 88` | Initial front-end read/follow-up write sequence. |
| `0900:0018` | Write/read `02 0A` | HDMI routing state word. |
| HDMI front-end registers | See ordered sequence below | Front-end and parameter-table setup; per-format coverage is incomplete. |

## Front-end register sequence

| Step | Command / bytes written or read | Notes |
|---:|---|---|
| 1 | Read `9D CD 3F → B2`; write `00=CE` | Select page `CE`. |
| 2 | Read `9D CD 3E → D3`; write `01=AD` | Front-end setup; meaning unknown. |
| 3 | Read `9D CD 3B → B3`; write `04=CD`, `06=C4` | Front-end setup; meaning unknown. |
| 4 | Read `9D CD 36 → BA`; write `09=E4` | Front-end setup; meaning unknown. |
| 5 | Read `9D CD 3F → B0`; write `00=CC` | Return to default page. |
| 6 | Read `9D CD 6B → 82`; write `54=EC` | Front-end setup. |
| 7 | Read `9D CD 93 → B2`; write `AC=4C` | Front-end setup. |
| 8 | Read `9D CD 3F → B2`; write `00=4C`; read `3F → 32`; write `00=CC` | Page/bank sequence. |
| 9 | Read `9D CD F1 → B2`; write `CE=4C`; read `F0 → B2`; write `CF=CE`; read `3F → B2`; write `00=CC` | Front-end setup. |
| 10 | Read `9D CD 6A → B1`; write `B0=E8` | Open parameter-table window. |
| 11 | Write `B1=0C, 92=8C, 93=CB, 94=CA, 95=C8, 96=D1, 97=B1, 98=6E, 99=B6, 9A=D1, 9B=C4, 9C=DC, 9D=B7, 9E=94, 9F=B2, A0=11, A1=CD, A2=5D, A3=CB, A4=CA, A5=EC, A6=CC, A7=C8, A8=CC, A9=EC, AA=CC, AD=C9` | Parameter-table bytes in sequenced order. Individual meanings unknown. |
| 12 | Read `9D CD 8F → 96`; write `B0=E9` | Close/commit parameter table. |
| 13 | Recurring `AB 4C` / `9D CD 6A` / `99 89 F5` / `9D CD 94` | Idle/keep-alive after front-end setup (see below). |

### Idle / keep-alive after HDMI front-end setup (no signal)

**Observed:** no-signal HDMI configuration.

After the HDMI selector (`51 45`) and the front-end / parameter-table sequence, the device enters a long-running repeating loop:

```
AB 4C
9D CD 6A 00
99 89 F5 00
9D CD 94 00
```

(each followed by the normal HD-New mailbox handshake). The loop continues while the device stays configured for HDMI with no signal present.

This is the same pattern previously noted as “Idle/keep-alive behavior in analog initialization sequence”. The present sequence shows it is not merely a short prefix; it is the steady-state behaviour once HDMI setup has finished and no further input change or signal lock has occurred.

**Authoritative description of all input-specific no-signal keep-alives (HDMI, component, composite/S-video), the source-detect candidate, and multi-switch evidence from `no-input input-switch sequence`** are kept in [hardware.md](hardware.md).

### HDMI detection and format setup

| Command / data | Description | Status |
|---|---|---|
| Reads involving `0x66/0x65`, `0x64/0x63` | Legacy implementation averages timing/status values. | Heuristic thresholds only. |
| Read `0x34` after selecting page `CE` | Bit 2 is used by source as an RGB indicator. | Source-derived interpretation; verify on hardware. |
| Timing candidates | `0x6463≈0xB6D7` 1080p; `≈0xB081` 1080i; `≈0xB05C` 720p; `≈0xB0C1` midpoint for 480p/576p | Approximate source heuristics, not a complete format table. |

Complete HDMI input setup, per-format programming, device configuration before stream start, and HDMI-specific lock confirmation are **NOT CAPTURED**. The shared firmware lifecycle and SCMD/start confirmation are in [hardware.md](hardware.md); generic unknowns are in [commands.md](commands.md).

## Sequences

These tables cover the standard SD modes and HD timing families identified by the current format-detection logic. Where a complete mode-specific setup is not established, that limitation is stated explicitly.

### 240p / 288p

| Order | NTSC-family / 60 Hz (240p) | PAL-family / 50 Hz (288p) |
|---:|---|---|
| 1 | HDMI selector `51 45`, routing `02 0A`, common front-end setup | HDMI selector `51 45`, routing `02 0A`, common front-end setup |
| 2 | Per-format HDMI programming and lock criteria: **not established** | Per-format HDMI programming and lock criteria: **not established** |
| 3 | Stream-start sequence for this mode: **not established** | Stream-start sequence for this mode: **not established** |

### 480i / 576i

| Order | NTSC-family / 60 Hz (480i) | PAL-family / 50 Hz (576i) |
|---:|---|---|
| 1 | HDMI selector and common front-end setup | HDMI selector and common front-end setup |
| 2 | Timing candidate and complete mode programming: **not established** | Timing candidate and complete mode programming: **not established** |
| 3 | Lock confirmation and stream start: **not established** | Lock confirmation and stream start: **not established** |

### 480p / 576p

| Order | NTSC-family / 60 Hz (480p) | PAL-family / 50 Hz (576p) |
|---:|---|---|
| 1 | HDMI selector and common front-end setup | HDMI selector and common front-end setup |
| 2 | Timing heuristic near `0xB0C1`, shared with 576p; full mode programming: **not established** | Timing heuristic near `0xB0C1`, shared with 480p; full mode programming: **not established** |
| 3 | Lock confirmation and stream start: **not established** | Lock confirmation and stream start: **not established** |

### 720p

| Order | HDMI 720p |
|---:|---|
| 1 | HDMI selector and common front-end setup |
| 2 | Timing candidate near `0xB05C`; per-mode programming and lock criteria: **not established** |
| 3 | Stream-start sequence: **not established** |

### 1080i

| Order | HDMI 1080i |
|---:|---|
| 1 | HDMI selector and common front-end setup |
| 2 | Timing candidate near `0xB081`; per-mode programming and lock criteria: **not established** |
| 3 | Stream-start sequence: **not established** |

### 1080p

| Order | HDMI 1080p |
|---:|---|
| 1 | HDMI selector and common front-end setup |
| 2 | Timing candidate near `0xB6D7`; per-mode programming and lock criteria: **not established** |
| 3 | Stream-start sequence: **not established** |
