# Component input reference

## Common commands and configuration

| Target / operation | Bytes written or expected read | Notes |
|---|---|---|
| `0x88:06` | Write `86` | Digital/component prefix. |
| Port `0x33` read helper | `89 89 F8` | Read before `0x88:03`; reply meaning is not established. |
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

   Every mailbox write above is followed by the normal HD-New handshake (`0xB9` commit, interrupt, restore `0900:0014` shadow `D3 9E`, poll `0900:001C`).

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

   (Interleaved with `9D CD` helpers / page selects.) Signal detection, per-format decoder configuration, final transcoder setup, and lock criteria with a real signal remain **NOT CAPTURED**.

8. Shared SCMD / sequence-start sequence (see [hardware.md](hardware.md)) only after the above is validated with a real signal.

### Notes on earlier occurrences of the same prefix bytes

The bytes `06 86`, `03 2F` and `B3 CC` also occur during earlier setup. The sequence ending with `51 ED` is accompanied by the routing-state change to `02 0E` and is the documented component-switch path.

## No-signal idle / keep-alive



Once the component selector `51 ED` and the component tail have run with no source present, the device enters a six-command keep-alive that differs from the HDMI loop:

```
AB 4C
9D CD 2B 00
9D CD 2A 00
9D CD 60 00
99 89 F5 00
9D CD 94 00
```

Full description and comparison with HDMI / composite / S-video idles are in [hardware.md](hardware.md).

## Sequences

The tables cover the standard SD formats listed for component input. “Not established” means the format-specific sequence is not documented; it does not imply that the format is unsupported.

### 240p / 288p

| Order | NTSC-family / 60 Hz (240p) | PAL-family / 50 Hz (288p) |
|---:|---|---|
| 1 | Digital/component prefix, routing `02 0E`, shared path, selector `51 ED` | Digital/component prefix, routing `02 0E`, shared path, selector `51 ED` |
| 2 | Per-format decoder/transcoder programming: **not established** | Per-format decoder/transcoder programming: **not established** |
| 3 | Pipeline setup and stream start: **not established** | Pipeline setup and stream start: **not established** |

### 480i / 576i

| Order | NTSC-family / 60 Hz (480i) | PAL-family / 50 Hz (576i) |
|---:|---|---|
| 1 | Common component selection path described above | Common component selection path described above |
| 2 | Per-format programming and lock criteria: **not established** | Per-format programming and lock criteria: **not established** |
| 3 | Pipeline setup and stream start: **not established** | Pipeline setup and stream start: **not established** |

### 480p / 576p

| Order | NTSC-family / 60 Hz (480p) | PAL-family / 50 Hz (576p) |
|---:|---|---|
| 1 | Common component selection path described above | Common component selection path described above |
| 2 | Per-format programming and lock criteria: **not established** | Per-format programming and lock criteria: **not established** |
| 3 | Pipeline setup and stream start: **not established** | Pipeline setup and stream start: **not established** |
