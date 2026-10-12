# HDMI input reference

## Common commands and configuration

| Target / operation | Bytes written or expected read | Notes |
|---:|---|---|---|
| `0x9C:51` | Write `45` | HDMI mux selector; full standalone recipe remains incomplete. |
| `0x9C:B7`; mailbox read | Write `CC`; read `9D CD 88` | Initial read/follow-up write; allow about 330 ms in this observed path. |
| `0900:0018` | Write/read `02 0A` | HDMI routing state word. |
| HDMI front-end registers | See ordered sequence below | Input setup; not verified across all HDMI modes. |

## Front-end register sequence observed in trace prefix

| Step | Command / bytes written or read | Notes |
|---:|---|---|
| 1 | Read `9D CD 3F \u2192 B2`; write `00=CE` | Select page `CE`. |
| 2 | Read `9D CD 3E \u2192 D3`; write `01=AD` | Front-end setup; meaning unknown. |
| 3 | Read `9D CD 3B \u2192 B3`; write `04=CD`, `06=C4` | Front-end setup; meaning unknown. |
| 4 | Read `9D CD 36 \u2192 BA`; write `09=E4` | Front-end setup; meaning unknown. |
| 5 | Read `9D CD 3F \u2192 B0`; write `00=CC` | Return to default page. |
| 6 | Read `9D CD 6B \u2192 82`; write `54=EC` | Front-end setup. |
| 7 | Read `9D CD 93 \u2192 B2`; write `AC=4C` | Front-end setup. |
| 8 | Read `9D CD 3F \u2192 B2`; write `00=4C`; read `3F \u2192 32`; write `00=CC` | Page/bank sequence. |
| 9 | Read `9D CD F1 \u2192 B2`; write `CE=4C`; read `F0 \u2192 B2`; write `CF=CE`; read `3F \u2192 B2`; write `00=CC` | Front-end setup. |
| 10 | Read `9D CD 6A \u2192 B1`; write `B0=E8` | Open parameter-table window. |
| 11 | Write `B1=0C, 92=8C, 93=CB, 94=CA, 95=C8, 96=D1, 97=B1, 98=6E, 99=B6, 9A=D1, 9B=C4, 9C=DC, 9D=B7, 9E=94, 9F=B2, A0=11, A1=CD, A2=5D, A3=CB, A4=CA, A5=EC, A6=CC, A7=C8, A8=CC, A9=EC, AA=CC, AD=C9` | Parameter-table bytes in captured order. Individual meanings unknown. |
| 12 | Read `9D CD 8F \u2192 96`; write `B0=E9` | Close/commit parameter table. |
| 13 | Recurring `AB 4C` / `9D CD 6A` / `99 89 F5` / `9D CD 94` | Idle/keep-alive after front-end setup (see below). |

## Idle / keep-alive after HDMI front-end setup (no signal)

**Observed:** no-signal HDMI configuration.

After the HDMI selector (`51 45`) and the front-end / parameter-table sequence, the device enters a long-running repeating loop:

```
AB 4C
9D CD 6A 00
99 89 F5 00
9D CD 94 00
```

(each followed by the normal HD-New mailbox handshake). The loop continues while the device stays configured for HDMI with no signal present.

This is the same pattern previously noted as \u201cIdle/keep-alive behavior in analog trace prefix\u201d. The present capture shows it is not merely a short prefix; it is the steady-state behaviour once HDMI setup has finished and no further input change or signal lock has occurred.

**Authoritative description, open questions (generic vs input-specific), and evidence status** are kept in [hardware.md](hardware.md) so that later captures on other inputs can be compared in one place.

## HDMI detection and format setup

| Command / data | Description | Status |
|---|---|---|
| Reads involving `0x66/0x65`, `0x64/0x63` | Legacy implementation averages timing/status values. | Heuristic thresholds only. |
| Read `0x34` after selecting page `CE` | Bit 2 is used by source as an RGB indicator. | Source-derived interpretation; verify on hardware. |
| Timing candidates | `0x6463\u22480xB6D7` 1080p; `\u22480xB081` 1080i; `\u22480xB05C` 720p; `\u22480xB0C1` midpoint for 480p/576p | Approximate source heuristics, not a complete format table. |

Complete HDMI input setup, per-format programming, device configuration before stream start, and HDMI-specific lock confirmation are **NOT CAPTURED**. The shared firmware lifecycle and SCMD/start confirmation are in [hardware.md](hardware.md); generic unknowns are in [commands.md](commands.md).

## Sequences

### 240p / 288p


#### NTSC-family / 60 Hz

| Step | Command / bytes written or read | Notes |
|---:|---|---|
| 1 | Complete NTSC-family 240p sequence **NOT CAPTURED**. | Complete ordered bring-up **NOT CAPTURED**. |

#### PAL-family / 50 Hz

| Step | Command / bytes written or read | Notes |
|---:|---|---|
| 1 | Complete PAL-family 288p sequence **NOT CAPTURED**. | Complete ordered bring-up **NOT CAPTURED**. |

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
