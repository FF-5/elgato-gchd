# Component input reference

## Common commands and configuration

| Target / operation | Bytes written or expected read | Notes |
|---:|---|---|---|
| `0x88:06` | Write `86` | Digital/component prefix. |
| Port `0x33` read helper | `89 89 F8` | Read before `0x88:03`; meaning UNKNOWN. |
| `0x88:03` | Write `2F` | Digital/component prefix. |
| `0x9C:B3` | Write `CC` | Digital/component prefix. |
| 5 | page `CE` \u2192 `0x9C:27=CC` \u2192 page `CC` | Page select around `27=CC` | **Observed** |
| `0900:0018` | Write `02 0E` | Component routing state; written before final selector. |
| `0x9C` shared path | `AB=4C`, `1B=33`, `B7=CE`, `B8=DC`, `B8=CC`, `07=38`, `07=C8` | Common path writes before selector; preserve order. |
| 8 | `0x9C:51=ED` (mailbox `51 ED`) | Component selector | **Observed** \u2013 trace ends at this write |
| Component-specific tail | `24 0C`, `21 CD`, `91 C8`, `0E 8C`, `11 EC`, `54 FC`, `71 4C`, `04 CC` | Full transaction context **NOT CAPTURED**. |

### Ordered component selection (HD-New)

**Observed sequence:** initialization starts configured for HDMI (no signal), then switches to component. The sequence ends at the selector.

1. Run common device initialization in [hardware.md](hardware.md) (firmware loads, IDLE, INIT, etc.).
2. (In this trace) HDMI path and front-end were already programmed and the device was sitting in the idle/keep-alive loop described in [hardware.md](hardware.md).
3. Digital/component prefix \u2013 exact observed order:

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

5. Shared path \u2013 exact observed order:

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

   Trace ends here. Every mailbox write above is followed by the normal HD-New handshake (`0xB9` commit, interrupt, restore `0900:0014` shadow `D3 9E`, poll `0900:001C`).

7. Component-specific tail, signal detection, per-format decoder configuration, final transcoder setup, and lock criteria: **NOT CAPTURED**.
8. Shared SCMD / capture-start sequence (see [hardware.md](hardware.md)) only after the above is validated with a real signal.

### Notes on earlier occurrences of the same prefix bytes

The bytes `06 86`, `03 2F` and `B3 CC` also occur during earlier setup. The sequence ending with `51 ED` is accompanied by the routing-state change to `02 0E` and is the documented component-switch path.

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
