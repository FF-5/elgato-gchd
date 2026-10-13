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

After the HDMI selector (`51 45`) and front-end / parameter-table sequence, the device repeats this loop while no source is present. Each mailbox command is followed by the normal HD-New mailbox handshake.

| Order | Mailbox payload | Notes |
|---:|---|---|
| 1 | `AB 4C` | `0x9C:AB = 4C` |
| 2 | `9D CD 6A 00` | Read helper index `0x6A` |
| 3 | `99 89 F5 00` | Sequence-read / poll helper |
| 4 | `9D CD 94 00` | Read helper index `0x94` |

This is the steady-state no-signal loop after HDMI setup. See [hardware.md](hardware.md) for comparison with component, composite, and S-video.

### HDMI detection and format setup

| Command / data | Description | Status |
|---|---|---|
| Reads involving `0x66/0x65`, `0x64/0x63` | Legacy implementation averages timing/status values. | Heuristic thresholds only. |
| Read `0x34` after selecting page `CE` | Bit 2 is used by source as an RGB indicator. | Source-derived interpretation; verify on hardware. |
| Timing candidates | `0x6463≈0xB6D7` 1080p; `≈0xB081` 1080i; `≈0xB05C` 720p; `≈0xB0C1` midpoint for 480p/576p | Approximate source heuristics, not a complete format table. |

Complete HDMI input setup, per-format programming, device configuration before stream start, and HDMI-specific lock confirmation are **NOT CAPTURED**. The shared firmware lifecycle and SCMD/start confirmation are in [hardware.md](hardware.md); generic unknowns are in [commands.md](commands.md).

## Sequences

These tables use the same sequence-comparison layout as the analog input references: four columns for the ordered NTSC/PAL sequences, followed by five columns that summarize differences while retaining each sequence's own step numbers. For HD timing families without an NTSC/PAL pair, the tables compare HDMI with component input.

### 480i / 576i

| NTSC step | NTSC-family 480i sequence | PAL step | PAL-family 576i sequence |
|---:|---|---:|---|
| 1 | HDMI selector `0x9C:51=45`; route `0900:0018=02 0A` | 1 | HDMI selector `0x9C:51=45`; route `0900:0018=02 0A` |
| 2 | Common HDMI front-end and parameter-table setup | 2 | Common HDMI front-end and parameter-table setup |
| 3 | Timing candidate and complete mode programming: **not established** | 3 | Timing candidate and complete mode programming: **not established** |
| 4 | Lock confirmation and stream start: **not established** | 4 | Lock confirmation and stream start: **not established** |

#### Differences: 480i vs 576i

| Order | NTSC step(s) | NTSC sequence difference | PAL step(s) | PAL sequence difference |
|---:|---:|---|---:|---|
| 1 | 1–2 | HDMI selection and common front-end setup are documented. | 1–2 | Same common HDMI selection and front-end setup. |
| 2 | 3–4 | 480i-specific timing, mode programming, lock confirmation, and stream start are not established. | 3–4 | 576i-specific timing, mode programming, lock confirmation, and stream start are not established. |

### 480p / 576p

| NTSC step | NTSC-family 480p sequence | PAL step | PAL-family 576p sequence |
|---:|---|---:|---|
| 1 | HDMI selector `0x9C:51=45`; route `0900:0018=02 0A` | 1 | HDMI selector `0x9C:51=45`; route `0900:0018=02 0A` |
| 2 | Common HDMI front-end and parameter-table setup | 2 | Common HDMI front-end and parameter-table setup |
| 3 | Timing heuristic near `0xB0C1`; complete per-mode programming: **not established** | 3 | Timing heuristic near `0xB0C1`; complete per-mode programming: **not established** |
| 4 | Lock confirmation and stream start: **not established** | 4 | Lock confirmation and stream start: **not established** |

#### Differences: 480p vs 576p

| Order | NTSC step(s) | NTSC sequence difference | PAL step(s) | PAL sequence difference |
|---:|---:|---|---:|---|
| 1 | 1–2 | HDMI selection and common front-end setup are documented. | 1–2 | Same common HDMI selection and front-end setup. |
| 2 | 3–4 | 480p shares the current `0xB0C1` timing heuristic; mode-specific programming and stream start are not established. | 3–4 | 576p shares the current `0xB0C1` timing heuristic; mode-specific programming and stream start are not established. |

### 720p

| HDMI step | HDMI 720p sequence | Component step | Component 720p sequence |
|---:|---|---:|---|
| 1 | HDMI selector `51 45`, route `02 0A`, common front-end setup | 1 | Component selector `51 ED`, route `02 0E`, shared path |
| 2 | Timing candidate near `0xB05C`; per-mode programming and lock criteria: **not established** | 2 | Component-specific 720p programming and lock criteria: **not established** |
| 3 | Stream-start sequence: **not established** | 3 | Pipeline configuration and stream start: **not established** |

#### Differences: HDMI 720p vs component 720p

| Order | HDMI step(s) | HDMI sequence difference | Component step(s) | Component sequence difference |
|---:|---:|---|---:|---|
| 1 | 1 | HDMI uses selector `51 45` and route `02 0A`, then configures the HDMI front end. | 1 | Component uses selector `51 ED` and route `02 0E`, with a component-specific shared path. |
| 2 | 2–3 | HDMI timing candidate is near `0xB05C`; full mode setup and stream start are not established. | 2–3 | Component-specific timing, lock criteria, and stream start are not established. |

### 1080i

| HDMI step | HDMI 1080i sequence | Component step | Component 1080i sequence |
|---:|---|---:|---|
| 1 | HDMI selector `51 45`, route `02 0A`, common front-end setup | 1 | Component selector `51 ED`, route `02 0E`, shared path |
| 2 | Timing candidate near `0xB081`; per-mode programming and lock criteria: **not established** | 2 | Component-specific 1080i programming and lock criteria: **not established** |
| 3 | Stream-start sequence: **not established** | 3 | Pipeline configuration and stream start: **not established** |

#### Differences: HDMI 1080i vs component 1080i

| Order | HDMI step(s) | HDMI sequence difference | Component step(s) | Component sequence difference |
|---:|---:|---|---:|---|
| 1 | 1 | HDMI uses selector `51 45` and route `02 0A`, then configures the HDMI front end. | 1 | Component uses selector `51 ED` and route `02 0E`, with a component-specific shared path. |
| 2 | 2–3 | HDMI timing candidate is near `0xB081`; full mode setup and stream start are not established. | 2–3 | Component-specific timing, lock criteria, and stream start are not established. |

### 1080p

| HDMI step | HDMI 1080p sequence | Component step | Component 1080p sequence |
|---:|---|---:|---|
| 1 | HDMI selector `51 45`, route `02 0A`, common front-end setup | 1 | Component selector `51 ED`, route `02 0E`, shared path |
| 2 | Timing candidate near `0xB6D7`; per-mode programming and lock criteria: **not established** | 2 | Component-specific 1080p programming and lock criteria: **not established** |
| 3 | Stream-start sequence: **not established** | 3 | Pipeline configuration and stream start: **not established** |

#### Differences: HDMI 1080p vs component 1080p

| Order | HDMI step(s) | HDMI sequence difference | Component step(s) | Component sequence difference |
|---:|---:|---|---:|---|
| 1 | 1 | HDMI uses selector `51 45` and route `02 0A`, then configures the HDMI front end. | 1 | Component uses selector `51 ED` and route `02 0E`, with a component-specific shared path. |
| 2 | 2–3 | HDMI timing candidate is near `0xB6D7`; full mode setup and stream start are not established. | 2–3 | Component-specific timing, lock criteria, and stream start are not established. |
