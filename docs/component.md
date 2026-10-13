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

## Allowable input formats

The component input format families documented here are **240p, 288p, 480i, 576i, 480p, 576p, 720p, 1080i, and 1080p**. The tables distinguish the format families from the portions of their register sequences that are established. A listed format does not imply that every format-specific register value or lock criterion is known.

## Sequences

### 240p / 288p

#### NTSC-family 240p vs PAL-family 288p

| NTSC step | NTSC-family 240p sequence | PAL step | PAL-family 288p sequence |
|---:|---|---:|---|
| 1 | Component prefix: `06 86`; read helper `89 89 F8`; `03 2F`; `B3 CC` | 1 | Component prefix: `06 86`; read helper `89 89 F8`; `03 2F`; `B3 CC` |
| 2 | Page setup: page `CE`, `0x9C:27=CC`, return to page `CC` | 2 | Page setup: page `CE`, `0x9C:27=CC`, return to page `CC` |
| 3 | Route `0900:0018=02 0E`; shared path; selector `0x9C:51=ED` | 3 | Route `0900:0018=02 0E`; shared path; selector `0x9C:51=ED` |
| 4 | Per-format decoder/transcoder programming and lock confirmation: **not established** | 4 | Per-format decoder/transcoder programming and lock confirmation: **not established** |
| 5 | Pipeline configuration and stream start: **not established** | 5 | Pipeline configuration and stream start: **not established** |

#### Differences: 240p vs 288p

| Order | NTSC step(s) | NTSC sequence difference | PAL step(s) | PAL sequence difference |
|---:|---:|---|---:|---|
| 1 | 1–3 | Common component-selection path is documented; mode-specific setup is not established. | 1–3 | Same documented common path; mode-specific setup is not established. |
| 2 | 4–5 | Format-specific programming, lock criteria, and stream-start commands are not established. | 4–5 | Format-specific programming, lock criteria, and stream-start commands are not established. |

### 480i / 576i

| NTSC step | NTSC-family 480i sequence | PAL step | PAL-family 576i sequence |
|---:|---|---:|---|
| 1 | Component prefix and page setup as described above | 1 | Component prefix and page setup as described above |
| 2 | Route `0900:0018=02 0E`; shared path; selector `0x9C:51=ED` | 2 | Route `0900:0018=02 0E`; shared path; selector `0x9C:51=ED` |
| 3 | 480i-specific decoder/transcoder programming and lock confirmation: **not established** | 3 | 576i-specific decoder/transcoder programming and lock confirmation: **not established** |
| 4 | Pipeline configuration and stream start: **not established** | 4 | Pipeline configuration and stream start: **not established** |

#### Differences: 480i vs 576i

| Order | NTSC step(s) | NTSC sequence difference | PAL step(s) | PAL sequence difference |
|---:|---:|---|---:|---|
| 1 | 1–2 | Common input-selection path only; no mode-specific sequence is established. | 1–2 | Common input-selection path only; no mode-specific sequence is established. |
| 2 | 3–4 | 480i-specific programming, lock criteria, and stream-start commands are not established. | 3–4 | 576i-specific programming, lock criteria, and stream-start commands are not established. |

### 480p / 576p

| NTSC step | NTSC-family 480p sequence | PAL step | PAL-family 576p sequence |
|---:|---|---:|---|
| 1 | Component prefix and page setup as described above | 1 | Component prefix and page setup as described above |
| 2 | Route `0900:0018=02 0E`; shared path; selector `0x9C:51=ED` | 2 | Route `0900:0018=02 0E`; shared path; selector `0x9C:51=ED` |
| 3 | 480p-specific decoder/transcoder programming and lock confirmation: **not established** | 3 | 576p-specific decoder/transcoder programming and lock confirmation: **not established** |
| 4 | Pipeline configuration and stream start: **not established** | 4 | Pipeline configuration and stream start: **not established** |

#### Differences: 480p vs 576p

| Order | NTSC step(s) | NTSC sequence difference | PAL step(s) | PAL sequence difference |
|---:|---:|---|---:|---|
| 1 | 1–2 | Common input-selection path only; no mode-specific sequence is established. | 1–2 | Common input-selection path only; no mode-specific sequence is established. |
| 2 | 3–4 | 480p-specific programming, lock criteria, and stream-start commands are not established. | 3–4 | 576p-specific programming, lock criteria, and stream-start commands are not established. |

### 720p

| Component step | Component 720p sequence | HDMI step | HDMI 720p sequence |
|---:|---|---:|---|
| 1 | Component prefix, route `02 0E`, shared path, selector `51 ED` | 1 | HDMI selector `51 45`, route `0900:0018=02 0A`, common front-end setup |
| 2 | 720p-specific programming and lock confirmation: **not established** | 2 | Timing candidate near `0xB05C`; complete per-mode programming and lock criteria: **not established** |
| 3 | Pipeline configuration and stream start: **not established** | 3 | Stream-start sequence: **not established** |

#### Differences: component 720p vs HDMI 720p

| Order | Component step(s) | Component sequence difference | HDMI step(s) | HDMI sequence difference |
|---:|---:|---|---:|---|
| 1 | 1 | Component selects with `51 ED` and route `02 0E`. | 1 | HDMI selects with `51 45` and route `02 0A`; HDMI front-end setup follows. |
| 2 | 2–3 | Component-specific 720p programming and stream start are not established. | 2–3 | HDMI has a timing candidate near `0xB05C`; full programming and stream start are not established. |

### 1080i

| Component step | Component 1080i sequence | HDMI step | HDMI 1080i sequence |
|---:|---|---:|---|
| 1 | Component prefix, route `02 0E`, shared path, selector `51 ED` | 1 | HDMI selector `51 45`, route `0900:0018=02 0A`, common front-end setup |
| 2 | 1080i-specific programming and lock confirmation: **not established** | 2 | Timing candidate near `0xB081`; complete per-mode programming and lock criteria: **not established** |
| 3 | Pipeline configuration and stream start: **not established** | 3 | Stream-start sequence: **not established** |

#### Differences: component 1080i vs HDMI 1080i

| Order | Component step(s) | Component sequence difference | HDMI step(s) | HDMI sequence difference |
|---:|---:|---|---:|---|
| 1 | 1 | Component selects with `51 ED` and route `02 0E`. | 1 | HDMI selects with `51 45` and route `02 0A`; HDMI front-end setup follows. |
| 2 | 2–3 | Component-specific 1080i programming and stream start are not established. | 2–3 | HDMI has a timing candidate near `0xB081`; full programming and stream start are not established. |

### 1080p

| Component step | Component 1080p sequence | HDMI step | HDMI 1080p sequence |
|---:|---|---:|---|
| 1 | Component prefix, route `02 0E`, shared path, selector `51 ED` | 1 | HDMI selector `51 45`, route `0900:0018=02 0A`, common front-end setup |
| 2 | 1080p-specific programming and lock confirmation: **not established** | 2 | Timing candidate near `0xB6D7`; complete per-mode programming and lock criteria: **not established** |
| 3 | Pipeline configuration and stream start: **not established** | 3 | Stream-start sequence: **not established** |

#### Differences: component 1080p vs HDMI 1080p

| Order | Component step(s) | Component sequence difference | HDMI step(s) | HDMI sequence difference |
|---:|---:|---|---:|---|
| 1 | 1 | Component selects with `51 ED` and route `02 0E`. | 1 | HDMI selects with `51 45` and route `02 0A`; HDMI front-end setup follows. |
| 2 | 2–3 | Component-specific 1080p programming and stream start are not established. | 2–3 | HDMI has a timing candidate near `0xB6D7`; full programming and stream start are not established. |
