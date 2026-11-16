# Component input

Notation and confidence markers: [README.md](README.md).

## Common commands and configuration

| Target / operation | Bytes written or expected read | Notes |
|---|---|---|
| `0x88:06` | Write `86` | Digital/component prefix. |
| Read `89 89 F8` | Device `0x88` register `F8` | Read before `0x88:03`; reply meaning UNKNOWN. |
| `0x88:03` | Write `2F` | Digital/component prefix. |
| `0x9C:B3` | Write `CC` | Digital/component prefix. |
| Page selection | Page `CE` → `0x9C:27=CC` → page `CC` | Page-select sequence around `27=CC`. |
| `0900:0018` | Write `02 0E` | Component routing state; set before the final selector. |
| `0x9C` shared path | `AB=4C`, `1B=33`, `B7=CE`, `B8=DC`, `B8=CC`, `07=38`, `07=C8` | Shared writes before the selector; preserve order. |
| `0x9C:51` | Write `ED` | Component input selector. |
| Component-specific tail | `24 0C`, `21 CD`, `91 C8`, `0E 8C`, `11 EC`, `54 FC`, `71 4C`, `04 CC` | Post-selector component setup. |

### Ordered component selection (HD-New)

**Observed sequence:** initialization starts configured for HDMI (no signal), then switches to component. Confirmed on both the the component-selection sequence and repeated input switching.

1. Run common device initialization in [hardware.md](hardware.md) (firmware loads, IDLE, INIT, etc.).
2. (In this trace) HDMI path and front-end were already programmed and the device was sitting in the idle/keep-alive loop described in [hardware.md](hardware.md).
3. Digital/component prefix – exact observed order:

   ```
   06 86              # 0x88:06 = 86
   89 89 F8 00        # read helper
   03 2F              # 0x88:03 = 2F
   B3 CC              # 0x9C:B3 = CC
   9D CD 3F 00
   00 CE              # select page CE
   27 CC              # 0x9C:27 = CC
   9D CD 3F 00
   00 CC              # return to default page
   ```

4. Write routing state `0900:0018`:
   - first a transient `02 0A` (HDMI value),
   - then `02 0E` (component value).  
   The change to `02 0E` occurs **before** the final selector write.

5. Shared path – exact observed order:

   ```
   9D CD 94 00
   AB 4C              # 0x9C:AB = 4C
   9D CD 3F 00
   00 CE
   1B 33              # 0x9C:1B = 33
   9D CD 3F 00
   00 CC
   9D CD 88 00
   B7 CE              # 0x9C:B7 = CE
   B8 DC              # 0x9C:B8 = DC
   B8 CC              # 0x9C:B8 = CC
   9D CD 3F 00
   00 CE
   07 38              # 0x9C:07 = 38
   07 C8              # 0x9C:07 = C8
   9D CD 3F 00
   00 CC
   ```

6. Component selector:

   ```
   51 ED              # 0x9C:51 = ED
   ```

   Every mailbox write above is followed by the normal HD-New handshake (`0xB9` commit, interrupt, restore the session-specific `0900:0014` shadow, poll `0900:001C`).

7. Component-specific tail (observed on no-source path in `no-input input-switch sequence`):

   ```
   00 4C
   24 0C
   21 CD
   91 C8
   0E 8C
   11 EC
   54 FC
   71 4C
   04 CC
   ```

   (Interleaved with `9D CD` helpers / page selects.) Signal detection, per-format decoder configuration, final transcoder setup, and lock criteria with a real signal are not in any Windows trace; the C++ starting point below fills the gap until we trace it.

8. Shared SCMD / sequence-start sequence (see [hardware.md](hardware.md)) only after the above is validated with a real signal.

### Notes on earlier occurrences of the same prefix bytes

The bytes `06 86`, `03 2F` and `B3 CC` also occur during earlier setup. The sequence ending with `51 ED` is accompanied by the routing-state change to `02 0E` and is the documented component-switch path.

## No-signal idle / keep-alive

Once the component selector `51 ED` and component tail have run with no source present, the device enters this repeating six-command loop. Each mailbox command uses the normal HD-New mailbox handshake.

| Order | Mailbox payload | Notes |
|---:|---|---|
| 1 | `AB 4C` | `0x9C:AB = 4C` |
| 2 | `9D CD 2B 00` | Read helper index `0x2B` |
| 3 | `9D CD 2A 00` | Read helper index `0x2A` |
| 4 | `9D CD 60 00` | Read helper index `0x60` |
| 5 | `99 89 F5 00` | Sequence-read / poll helper |
| 6 | `9D CD 94 00` | Read helper index `0x94` |

See [hardware.md](hardware.md) for the cross-input comparison and unresolved helper semantics.

## C++ starting point (`configure_component.cpp`)

The Windows traces above only cover switching to component **without** a
source. The C++ implements full component capture and is the starting point
for everything after the selector until a Windows trace with a component
signal is recorded.

1. `94 41 37`, `94 4A AF`, `94 4B AF`; the `0x9C` shared path; selector
   `0x9C:51 = ED` (same bytes as the Windows switch trace).
2. Mode detection, at least 3 iterations, 10 samples each, 200 ms apart.
   "No signal" is `9D CD 66/65 ≈ AD4D`. The mode is chosen from the averaged
   16-bit value of `9D CD 68/67` (±10):

   | `68/67` | Mode | `v_format` |
   |---|---|---|
   | `BBF4` | 1080p30 | 32 |
   | `A03D` | 1080i60 | 0 |
   | `BF59` | 720p60 | 2 |
   | `A6B7` | 576p50 | 9 |
   | `9AB9` | 576i50 | 5 |
   | `A150` | 480p60 | 8 |
   | `9576` | 480i60 | 4 |

3. Per-mode programming of device `0x9C`, followed by `configureColorSpace`
   (YUV/RGB tables `0x9C:92–AA`, plus component-only `0B–0D`, `08–0A`, `1E`,
   `1F` and the per-mode `B2`/`B5`/`03` writes), `configureSetupSubblock`,
   `configureCommonBlockB1/B2/B3`, `configureCommonBlockA`, `configureCommonBlockC`.
4. `transcoderFinalConfigure`, `transcoderSetup`, capture start
   ([hardware.md §6](hardware.md#6-capture-start)).

240p/288p over component is a valid console signal but is **not** handled by
the C++ and has never been traced. Earlier documentation listed it as
"allowable"; that was not backed by any evidence.

The no-signal loop in Windows reads `9D CD 2B/2A/60`, while the C++ reads
`66/65` and `68/67`. Both are expected to work; the Windows registers may be
cheaper status flags.
