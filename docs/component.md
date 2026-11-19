# Component input

Notation and status labels are defined in [README.md](README.md).

## 1. Selecting the component input (Observed, no source present)

Starting from HDMI with no source, the switch to component is:

1. Input prefix: `0x88:06 = 86`; read `0x88:F8`; `0x88:03 = 2F`;
   `0x9C:B3 = CC`; read `0x9C:3F`; bank `CE`; `0x9C:27 = CC`; read
   `0x9C:3F`; bank `CC`.
2. `0900:0018` written `02 0A`, then `02 0E` (before the selector).
3. Read `0x9C:94`; `0x9C:AB = 4C`; read `0x9C:3F`; bank `CE`;
   `0x9C:1B = 33`; read `0x9C:3F`; bank `CC`; read `0x9C:88`;
   `0x9C:B7 = CE`, `B8 = DC`, `B8 = CC`; read `0x9C:3F`; bank `CE`;
   `0x9C:07 = 38`, `07 = C8`; read `0x9C:3F`; bank `CC`.
4. `0x9C:51 = ED` (input selector).
5. `0x9C:00 = 4C`, `24 = 0C`, `21 = CD`, `91 = C8`, `0E = 8C`, `11 = EC`,
   `54 = FC`, `71 = 4C`, `04 = CC`, interleaved with bank selects and reads
   of `0x9C:3F`.

Every mailbox write uses the handshake in
[hardware.md §2.1](hardware.md#21-mailbox-handshake-005d-observed).

## 2. No-signal polling (Observed)

| Order | Operation |
|---:|---|
| 1 | `0x9C:AB = 4C` |
| 2 | Read `0x9C:2B` |
| 3 | Read `0x9C:2A` |
| 4 | Read `0x9C:60` |
| 5 | Read `0x98:F5` |
| 6 | Read `0x9C:94` |

## 3. Mode detection and configuration (Driver)

The driver configuration is implemented in `gchd_configure_component_exact()`
(`src/driver/input.c`).

1. `0x94:41 = 37`, `0x94:4A = AF`, `0x94:4B = AF`; front-end setup;
   `0x9C:51 = ED`.
2. Mode detection: the 16-bit value of `0x9C:68/67` is averaged over repeated
   samples; `0x9C:66/65 ≈ AD4D` indicates no signal. Modes (±10):

   | `0x9C:68/67` | Mode | `v_format` |
   |---|---|---|
   | `BBF4` | 1080p30 | 32 |
   | `A03D` | 1080i60 | 0 |
   | `BF59` | 720p60 | 2 |
   | `A6B7` | 576p50 | 9 |
   | `9AB9` | 576i50 | 5 |
   | `A150` | 480p60 | 8 |
   | `9576` | 480i60 | 4 |

3. Per-mode front-end programming, colour space tables (`0x9C:92`–`AA`, with
   the component-specific `0B`–`0D`, `08`–`0A`, `1E`, `1F` and per-mode
   `B2`/`B5`/`03` writes), per-mode back-end table and the common back-end
   blocks.
4. Encoder setup and capture start ([hardware.md §6](hardware.md#6-capture-start-observed)).

Component capture with a source present has not yet been confirmed by a
trace.
