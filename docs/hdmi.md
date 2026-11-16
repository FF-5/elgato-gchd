# HDMI input

Notation and confidence markers: [README.md](README.md).

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

Complete HDMI programming is **not in any Windows trace**, but it is implemented in the C++ reference (next section). The shared firmware lifecycle and SCMD/start confirmation are in [hardware.md](hardware.md); generic unknowns are in [commands.md](commands.md).

## C++ reference path (`configure_hdmi.cpp`) — known-good baseline

The Windows traces cover HDMI selection, front-end setup and the no-signal
loop. Everything after that comes from the C++, which captured HDMI 1080p,
1080i, 720p and 480p/576p.

1. `94 41 37`, `94 4A AF`, `94 4B AF`; front-end sequence identical to the
   table above (steps 1–9), including selector `0x9C:51 = 45`; a
   `doEnable(0,0)` "no-op" in the middle (**C++**).
2. Mode detection (`readHdmiSignalInformation`): bank `CC`, read
   `9D CD 66/65` and `64/63`; bank `CE`, read `34` (bit 2 = RGB). Ten samples
   per iteration, 200 ms between iterations, at least 3 iterations, at most 10.
   `66/65 ≈ AD4D` means no signal.

   | `64/63` (±10) | Mode | Notes |
   |---|---|---|
   | `B6D7` | 1080p60 | |
   | `B081` | 1080i60 | |
   | `B05C` | 720p60 | |
   | `B0C1` (±12) | 480p (`B0BF`) / 576p (`B0C3`) | 576p if `66/65 ≈ BB75`, NTSC otherwise (`BA95`) |

   HDMI 480i/576i are not detected by the C++.
   **Possible C++ bug:** the 576p branch sets the refresh rate to 60 Hz, while
   `InputSettings` only accepts 50 Hz for PAL.
3. Per-mode `0x9C:B2/B5` (1080: `C4/D0`, 720: `CC/CC`, SD: `CF/CC`), then
   `1B=30`, `1F=DC/CC`, `04=CF/CD`, `40=CC`, parameter window `B0=E8 … E9`,
   `AB=CC`; back-end retrigger `0x98:0C=89`, `0E=65`, `0E=64`.
4. `configureCommonBlockA`, `configureSetupSubblock`, `0x98:C0=80, A2=77, C0=77`,
   `configureCommonBlockB1(mystery)` (`mystery` = non-1080 progressive),
   `B2`, `B3`, `C`.
5. Re-read the RGB bit and pick the colour space; `configureColorSpace`.
6. `transcoderFinalConfigure`, `transcoderSetup`, capture start.

HDMI colour primaries: `colour_primaries = matrix_coefficients = 6` when the
HDMI colour range is "full" (and always for analog), otherwise `1`
(`transcoderSetup`).
