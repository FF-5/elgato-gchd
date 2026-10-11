# GCHD Command Reference

This document is the working inventory of commands and register operations identified so far for the Elgato Game Capture HD (GCHD). It is intentionally conservative: a value seen on the USB wire is **observed**, but its hardware meaning is not considered proven unless noted.

**Evidence currently catalogued:** `drivers/elgato_gchd/trace_analysis.txt` (composite 480i/240p format transitions), `drivers/elgato_gchd/hdmi2composite_trace_analysis.txt` (HDMI-to-composite input switch and SoC pipeline configuration), and `drivers/elgato_gchd/input_switch_trace_analysis.txt` (input switches with no source attached plus comparisons against the earlier trace). These are trace-derived observations, not a complete device protocol specification. Treat interpretations as provisional until controlled hardware tests confirm them.

## Status legend

- **Observed** — present in the capture; this does not by itself prove the semantic meaning.
- **Inferred** — plausible interpretation from context, not confirmed by controlled hardware testing.
- **Untested in driver** — newly catalogued operation not yet validated as a driver implementation step. Keep it out of the production path until tested.
- **Unknown** — purpose or protocol semantics are not established.

## 1. USB/protocol command forms

| Command / wire form | Values / fields observed | Description | Status / notes |
|---|---|---|---|
| Mailbox/register read: `89 89 RR 00` | `RR` is the register; response is one byte | Reads status/registers associated with the `0x88` device block. Trace notation: `READ 89 89 reg RR -> VV`. | Observed. Exact transport framing beyond the trace notation should be checked against the USB transfer implementation. |
| Mailbox/register read: `99 89 RR 00` | `RR` is the register; response is one byte | Reads status/registers associated with the `0x98` device block. | Observed. |
| Register write transaction | 2-byte write on `0x0800:0x00C0`, followed by `B9 09 00 ADDR 02`; trace labels the target block as `[ADDR]` | Writes a single register/value pair to the selected device block. | Observed trace notation. Confirm the precise low-level framing in the transport code before reimplementing. |
| SCMD control transfer: `B8 xx xx xx xx` | `B8 04 A0 00 00`; `B8 05 00 00 02`; `B8 05 00 00 04`; `B8 05 00 00 01` | Vendor-specific control transfer, shown in the trace as `SCMD (40 B8 ... wLength 4)`. | Values observed; semantic names and expected acknowledgements are not yet proven. See untested commands below. |

## 2. Format/status polling

All values below are response bytes to reads, not commands sent to the device.

| Read register (wire prefix) | Observed values | Current interpretation | Status / notes |
|---|---|---|---|
| `0xCA` (`89 89 CA`) | `0xFE`, `0x7E`, `0x66`, transient `0xEE` | `0xFE` appears to mean unlocked/no stable input; `0x7E` is associated with locked 480i; `0x66` is associated with locked 240p. `0xEE` appeared briefly during a 240p transition. | Inferred from one composite trace; validate on repeated captures and other input types. Do **not** describe `0xFE` as 480i. |
| `0xFA` (`89 89 FA`) | `0x3F`, `0x06`, `0x16` | Polled alongside format status; `0x06/0x16` flicker in the trace. | Meaning unknown. |
| `0xE7` (`89 89 E7`) | `0x69`, `0x09`, `0x89` | A transient value appears around format changes. | Meaning unknown; do not use as a format enum yet. |
| `0xF1` (`89 89 F1`) | `0x7E`, `0x69` | Polled alongside other status registers. | Meaning unknown. |
| `0xCB` (`89 89 CB`) | `0x6E`, `0x44` | Polled during input/format detection. | Meaning unknown. |
| `0xFD` (`89 89 FD` or `99 89 FD`) | `0x6E` | Read around configuration sequences. | Meaning unknown; same register number is read through different prefixes. |
| `0xFC` (`99 89 FC`) | `0x6E` | Read with `0xFD` and `0xF3`. | Meaning unknown. |
| `0xF3` (`99 89 F3`) | `0x7E` | Read with `0xFD` and `0xFC`. | Meaning unknown. |
| `0xF8` (`89 89 F8`) | `0xC9` | Read early in initial setup. | Meaning unknown. |
| `0x8B` (`99 89 8B`) | `0x2E`, `0x36` | Read around decoder setup; differs between initial/full setup and intermediate transition. | Meaning unknown. |
| `0x89` (`99 89 89`) | `0x6E` | Read during configuration. | Meaning unknown. |
| `0xFF` (`99 89 FF`) | `0x73` | Read during configuration. | Meaning unknown. |
| `0x6B` (`99 89 6B`) | `0x6E` | Read during intermediate 480i reconfiguration. | Meaning unknown. |
| `0x58` (`99 89 58`) | `0x91` | Read during intermediate 480i reconfiguration. | Meaning unknown. |

## 3. Observed writes to device block `0x9C`

These writes occur early in the initial composite setup, before the `0x88` and `0x98` configuration groups. They are included because they are part of the observed command inventory, although their role is unknown.

| Register | Observed value(s) | Context | Description / status |
|---|---|---|---|
| `0xB3` | `0x33` | Initial setup, 8.835 s | Observed write to block `0x9C`; purpose unknown. |
| `0x00` | `0xCE`, then `0xCC` | Initial setup, after reads of block `0x9D` | Two values observed; possible staged setup, but unconfirmed. |
| `0x27` | `0x33` | Initial setup, 8.843 s | Observed write; purpose unknown. |
| `0x51` | `0x44` | Initial setup, 8.854 s | Observed write; purpose unknown. |

## 4. Observed writes to device block `0x88`

The trace shows these writes during initial composite lock and format changes. Their role is not proven solely by observation.

| Register | Observed values | Context | Description / status |
|---|---|---|---|
| `0x03` | `0x28` | Initial setup | Setup write; purpose unknown. |
| `0x06` | `0x08` | Initial setup and after format-dependent writes | Repeated write; possibly a latch/reapply operation, but unconfirmed. |
| `0x07` | `0x8A` | Initial 480i, 480i→240p, and 240p→480i | Reapplied on both transitions; purpose unknown. |
| `0x08` | `0x9B` | Initial 480i and both transitions | Reapplied on both transitions; purpose unknown. |
| `0x09` | `0x7A` | Initial 480i and both transitions | Reapplied on both transitions; purpose unknown. |
| `0x0A` | `0x8F`, `0x98` | During 480i→240p transition | Value changes from `0x8F` to `0x98` while 240p remains detected. Format-related role is plausible, not proven. |
| `0x10` | `0x6F` | Initial setup | Written three times as part of a group. |
| `0x11` | `0xD4`, `0xEA` | Initial setup | Changed during the initial setup sequence. |
| `0x12` | `0xD0` | Initial setup | Written with registers `0x10–0x15`. |
| `0x13` | `0x08`, `0x1B` | Initial setup | Changes from `0x08` to `0x1B`. |
| `0x14` | `0x08`, `0x1B` | Initial setup | Changes from `0x08` to `0x1B`. |
| `0x15` | `0x88` | Initial setup | Written with registers `0x10–0x14`. |
| `0x28` | `0x88`, `0x8A` | `0x88` at initial 480i and return to 480i; `0x8A` at 240p | Strong format correlation in this trace; exact bit semantics remain unknown. |

## 5. Observed writes to device block `0x98`

### 5.1 Common setup writes

| Register | Observed values | Context | Description / status |
|---|---|---|---|
| `0x04` | `0x95`, then `0x8D` | Initial/full setup and return to 480i | Setup-stage change; purpose unknown. |
| `0x0D` | `0xC8` | Initial setup, format transitions, full reconfiguration | Repeated before writes to `0x0E`; purpose unknown. |
| `0x0E` | `0x05` then `0x04`; later `0x15` then `0x14` | Initial setup and format changes | Ordered write pairs are observed; possible command/latch semantics are unconfirmed. |
| `0x0F` | `0x88` | Initial/full setup | Not present in the intermediate flat-table phase. |
| `0x61` | `0x98` | Initial/full setup and reconfiguration | Purpose unknown. |
| `0x62` | `0x91` | Initial/full setup and reconfiguration | Purpose unknown. |
| `0x63` | `0x89` | Initial/full setup and reconfiguration | Purpose unknown. |
| `0x64` | `0x96` | Initial/full setup and reconfiguration | Purpose unknown. |
| `0x65` | `0x88` | Initial/full setup and reconfiguration | Purpose unknown. |
| `0x70` | `0xD0`, `0xC8` | `0xD0` in full setup; `0xC8` in intermediate phase | Correlates with configuration phase; exact meaning unknown. |
| `0x72` | `0x88` | Initial/full setup and reconfiguration | Purpose unknown. |
| `0xC0` | `0x88`, `0x80`, `0x77` | Normal setup and intermediate flat-table phase | Intermediate sequence writes `0x80`, later `0x77`, then returns to `0x88`. Meaning unknown. |

### 5.2 Coefficient/table writes

The following table is populated during initial setup and again during the final full configuration when returning to 480i.

| Register | Observed value | Status |
|---|---:|---|
| `0x90` | `0x01` | Observed; semantic purpose unknown. |
| `0x91` | `0xBD` | Observed; semantic purpose unknown. |
| `0x92` | `0x9A` | Observed; semantic purpose unknown. |
| `0x93` | `0x3A` | Observed; semantic purpose unknown. |
| `0x94` | `0xE9` | Observed; semantic purpose unknown. |
| `0x95` | `0xAB` | Observed; semantic purpose unknown. |
| `0x96` | `0x17` | Observed; semantic purpose unknown. |
| `0x97` | `0x88` | Observed; semantic purpose unknown. |
| `0x98` | `0x84` | Observed; semantic purpose unknown. |
| `0x99` | `0x8A` | Observed; semantic purpose unknown. |
| `0x9A` | `0x9C` | Observed; semantic purpose unknown. |
| `0x9B` | `0x8C` | Observed; semantic purpose unknown. |
| `0x9C` | `0x98` | Observed; semantic purpose unknown. |
| `0x9D` | `0x93` | Observed; semantic purpose unknown. |
| `0x9E` | `0x83` | Observed; semantic purpose unknown. |
| `0x9F` | `0xA9` | Observed; semantic purpose unknown. |
| `0xA0` | `0x8B` | Observed; semantic purpose unknown. |
| `0xA1` | `0xE8` | Observed; semantic purpose unknown. |
| `0xA2` | `0x81` | Observed; semantic purpose unknown. |
| `0xA3` | `0x49` | Observed; semantic purpose unknown. |

### 5.3 Intermediate table during 240p→480i

This phase appears only in the captured return-to-480i sequence. The driver must **not** assume its purpose or necessity until tested.

| Register range / register | Observed value(s) | Description / status |
|---|---|---|
| `0x70` | `0xC8` | Precedes intermediate table writes; purpose unknown. |
| `0x90` | `0x88` | First entry differs from the final full table. |
| `0x91–0xA3` | Mostly `0x77`; `0xA2=0x77` is explicitly written again later | Flat/intermediate table phase; exact purpose unknown. |
| `0xC0` | `0x80`, then `0x77`, then `0x88` | Staged writes around the intermediate table; purpose unknown. |

## 6. SCMD commands seen in the trace — untested semantics

These exact 4-byte payloads are observed on the wire. The names below are provisional and are not assertions about what the device does.

| Payload after `B8` | Context | Provisional interpretation | Status / expected confirmation |
|---|---|---|---|
| `04 A0 00 00` | Initial capture setup at 10.315 s; again after full 480i reconfiguration at 59.853 s | Possible capture/setup or start-related command | **Untested.** No confirmed acknowledgement mapping yet. |
| `05 00 00 02` | Immediately after `04 A0 00 00` at initial setup and final reconfiguration | Possible capture-start/enable command | **Untested.** Sequence correlation only; command meaning and confirmation response unknown. |
| `05 00 00 04` | During 240p→480i reconfiguration at 57.985 s | Possible stop/disable or transition-stage command | **Untested.** Do not call it “stop” as a fact. |
| `05 00 00 01` | At 58.646 s, after `05 00 00 04` and before final full setup | Possible intermediate lifecycle command | **Untested.** Meaning and acknowledgement unknown. |

## 7. Newly catalogued / untested driver operations

This section intentionally separates **seen in a reference trace** from **tested as a standalone operation**. Treat these as experimental until reproduced with controlled tests and capture/video health checks.

| Operation to test | Exact known values | Why it is listed | Safe test / acceptance evidence |
|---|---|---|---|
| Change `0x88:0x28` for detected format | 480i: `0x88`; 240p: `0x8A` | Strong correlation with the `0xCA` status change. | Change only this register while streaming; verify stable image and format, and record whether any other writes are required. |
| Reapply `0x88:0x07–0x09` | `07=8A, 08=9B, 09=7A` | Repeated at both transitions and initial setup. | Compare transition success with/without reapplying this group; restore known-good configuration on failure. |
| Apply `0x88:0x0A` updates | `0x8F`, later `0x98` on 480i→240p | Appears only during the captured 240p transition. | Test each write separately and record timing relative to lock status and valid frames. |
| Run the `0x98` intermediate table | `0x70=C8`, `0x90=88`, `0x91–0xA3=77`, staged `0xC0` values | Seen only in the captured 240p→480i transition. | Compare return-to-480i with and without this phase; verify decoder lock, frame integrity, and whether it is actually required. |
| Replay the final `0x98` table | `0x70=D0`, `0x0F=88`, `0x90–0xA3` values in §5.2, common setup values in §5.1 | Reapplied after the intermediate phase. | First reproduce the full trace sequence; then remove one group at a time while capturing results. |
| Send SCMD `04 A0 00 00` | Exact payload shown above | Repeated at initial setup and after return-to-480i setup. | Capture the control transfer status, any subsequent device state changes, and whether streaming starts; do not infer acknowledgement from timing alone. |
| Send SCMD `05 00 00 02` | Exact payload shown above | Paired with `04 A0 00 00` in the trace. | Determine required ordering and observable confirmation; test only with recovery path available. |
| Send SCMD `05 00 00 04` | Exact payload shown above | Appears during 240p→480i transition. | Establish whether it halts capture, resets a block, or does something else by observing stream continuity and control-transfer results. |
| Send SCMD `05 00 00 01` | Exact payload shown above | Appears after `05 00 00 04`. | Establish purpose and ordering independently; record the device response and subsequent stream state. |
| Use `0xCA` as a format-state input | `FE`, `7E`, `66`, transient `EE` observed | Most useful current candidate for format/lock detection. | Capture multiple transitions per input type; require stable repeated samples and validate against actual decoded frames before updating V4L2 format. |

## 8. Observed sequences

### Initial composite lock / 480i setup

1. Write `0x88:0x06=08`, `0x88:0x03=28`; perform early `0x9C` writes and status reads.
2. Program `0x88:0x10–0x15` in several passes.
3. Read `0xCA=FE` (unlocked in the current interpretation), then write `0x88:0x07=8A`, `0x08=9B`, `0x09=7A`, `0x28=88`, read `0xFD`, and write `0x06=08`.
4. Program the full `0x98` table and common registers.
5. Send `B8 04 A0 00 00`, then `B8 05 00 00 02`.

### 480i→240p

1. `0xCA` changes from `0x7E` to `0x66`.
2. Reapply `0x88:0x07–0x09`, change `0x28=8A`, and write `0x06=08`.
3. Write `0x0A=8F`, later `0x0A=98`; write `0x98:0x0D=C8`, `0x0E=15`, `0x0E=14`.
4. The trace shows no SCMD and no SoC writes during this transition, and capture continues. This is an observation from this one capture, not a universal rule.

### 240p→480i

1. Before the status transition, write `0x98:0x0D=C8`, `0x0E=15`, `0x0E=14` twice.
2. `0xCA` changes from `0x66` to `0x7E`; reapply `0x88:0x07–0x09`, `0x28=88`, and `0x06=08`.
3. Apply the intermediate `0x98` table and common setup.
4. Send `B8 05 00 00 04`, then `B8 05 00 00 01`.
5. Apply the full `0x98` table and common setup; send `B8 04 A0 00 00`, then `B8 05 00 00 02`.

## 9. Input selection and HDMI/composite differences (second trace)

**Evidence:** `drivers/elgato_gchd/hdmi2composite_trace_analysis.txt`, trace identified there as `gchd_240p_switch_to_composite.pcapng`. Device reported by the analysis as VID `0x0FD9`, PID `0x005D`. Times below are relative to that capture.

### 9.1 Input-select candidate

| Register | HDMI observed value | Composite observed value | Current interpretation | Status |
|---|---:|---:|---|---|
| `0x51` | `0x45` at 5.691 s | `0x44` at 9.608 s | Strong candidate for input selection: `0x45` correlates with HDMI and `0x44` with composite. | **Inferred**, not proven from this trace alone. The later no-source trace reports component `0xED`; this is observed in that trace, but still needs controlled reproduction. |

Do not assume `0x51` is the only input-selection control. Both input transitions are accompanied by other register writes.

### 9.2 Input-specific register writes

These are the values reported around the input-selection events. They appear to be part of a larger setup sequence, not necessarily a complete selector table.

| Register | HDMI value(s) | Composite value(s) | Description / status |
|---|---|---|---|
| `0x06` | `0x86` | `0x08` | Changes with the selected input; purpose unknown. |
| `0x03` | `0x2F` | `0x28` | Changes with the selected input; purpose unknown. |
| `0xB3` | `0xCC` | `0x33` | Changes with the selected input; exact bank/block interpretation requires confirmation. |
| `0x27` | `0xCC` | `0x33` | Written after `0x00=0xCE`; likely part of a banked sequence, but semantics unknown. |
| `0x51` | `0x45` | `0x44` in the earlier trace; `0xEC` in the no-source trace | Input-select candidate. The composite values conflict across captures and must not be silently reconciled. |
| `0x1B` | `0x33` | Not observed in the composite-switch event list | HDMI-side write in a sequence bracketed by writes to register `0x00`; not enough evidence to infer its purpose. |
| `0x07` | `0x38`, then `0xC8` | Not observed in the composite-switch event list | HDMI-side writes; meaning unknown. |
| `0xB7` | `0xCE`, then `0xCC` | Not observed in the composite-switch event list | HDMI-side writes around the selection sequence. |
| `0xB8` | `0xDC`, then `0xCC` | Not observed in the composite-switch event list | HDMI-side writes; do not confuse these register writes with the USB vendor request opcode `B8`. |

The analysis notes repeated writes of `0x00=0xCE` and `0x00=0xCC` around some HDMI writes. This is consistent with a bank/page-selection mechanism, but the trace alone does not establish the mechanism. Keep the exact write order in the raw analysis file.

### 9.3 Transaction/framing words observed around register writes

| USB control request / payload | Observed use | Current interpretation / caveat |
|---|---|---|
| `40 BC 00 08 C0 00 02 00 <reg> <val>` | Two-byte writes such as `51 45` and `51 44` | Reported as a register/value write through port `0x0800:0x00C0`. The I²C-like bridge interpretation is plausible, not confirmed. |
| `40 BC 00 08 C0 00 04 00 <p0 p1 reg 00>` | Four-byte read requests; response reported at `C0 BC 00 08 BE 23` | Reported read framing. Prefixes `99 89`, `89 89`, and `9D CD` appear in requests. |
| `40 B9 00 00 00 00 04 00 09 00 88 02` | Appears around writes targeting the `0x88` block | Transaction/framing word associated with the following register access. Exact sequencing semantics remain unproven. |
| `40 B9 00 00 00 00 04 00 09 00 98 02` | Appears around writes targeting the `0x98` block | Same caveat; do not model as an acknowledgement unless the transport implementation or further captures prove that. |

### 9.4 SoC/pipeline register writes

The second trace reports many writes using request `40 BC 01 00 <addr16> 08 00 <8 bytes>`, where the 8 data bytes are interpreted as a big-endian value and mask pair. This interpretation is trace-derived and should be verified against the implementation.

| SoC address | Observed value | Mask | Description / status |
|---|---:|---:|---|
| `0x1124` | `0x00001011` | `0x00001FFF` | Pipeline configuration write; field semantics unknown. |
| `0x1264` | `0x00001011` | `0x00001FFF` | Paired with `0x1124`; likely related pipeline block, exact relationship unknown. |
| `0x1128` | `0x010F0000` | `0x1FFF0000` | Pipeline configuration write; field semantics unknown. |
| `0x1268` | `0x010F0000` | `0x1FFF0000` | Paired with `0x1128`; field semantics unknown. |
| `0x152C` | `0x02D00000`, then `0x000001E0` | `0xFFFF0000`, then `0x0000FFFF` | Two half-word writes encode `0x02D0` (720) and `0x01E0` (480) in the trace's value/mask interpretation. Strong evidence of a 720×480-related pipeline setting, but the precise register semantics are not established. |
| `0x162C` | `0x02D00000`, then `0x000001E0` | `0xFFFF0000`, then `0x0000FFFF` | Same values as `0x152C` in a second block; exact function unknown. |
| `0x1530` | `0x00002BF2` | `0x0000FFFF` | Pipeline configuration; meaning unknown. |
| `0x1534` | `0x278D0000`, then `0x000015F9` | `0xFFFF0000`, then `0x0000FFFF` | Pipeline configuration; meaning unknown. |
| `0x1538` | `0x0AFC0000` | `0xFFFF0000` | Pipeline configuration; meaning unknown. |
| `0x1630` | `0x00002BF2` | `0x0000FFFF` | Mirrors `0x1530` in the second block. |
| `0x1634` | `0x278D0000`, then `0x000015F9` | `0xFFFF0000`, then `0x0000FFFF` | Mirrors `0x1534` in the second block. |
| `0x1638` | `0x0AFC0000`, then `0x00000000` | `0xFFFF0000`, then `0x0000FFFF` | Pipeline configuration; meaning unknown. |

The trace contains many additional SoC writes. This table is a selected index, not a complete register dump; the full sequence remains in `hdmi2composite_trace_analysis.txt`. Do not infer that the source is necessarily 240p solely from a 720×480-related output setting. The capture records an input switch and the source timing is not independently established by this register sequence.

### 9.5 SCMD at pipeline start

| Payload after USB request `B8` | Observed time | Context | Status |
|---|---:|---|---|
| `04 A0 00 00` | 11.159 s | At the end of the SoC/pipeline register burst | Observed; semantics unknown. |
| `05 00 00 02` | 11.159 s | Immediately after the previous payload | Observed; semantics and expected confirmation unknown. |

This independently corroborates the sequence seen in the composite format trace, but it still does not establish the commands' meanings or acknowledgement requirements.

### 9.6 New untested experiments

| Experiment | Hypothesis to test | Evidence needed |
|---|---|---|
| Switch HDMI↔composite while streaming | `0x51=0x45/0x44` selects the input as part of a larger register sequence. | Repeat both directions, record every write and read, and verify the actual resulting input and stable frames. |
| Find component selector | Component may use the same `0x51` register, but the value cannot be safely predicted from `0x44` and `0x45`. | Capture a deliberate switch to component; do not guess the value. |
| Minimize input-switch sequence | Some input-specific writes may be required in addition to `0x51`. | Remove one write group at a time with a known-good recovery path; record lock and frame outcome. |
| Validate SoC width/height fields | `0x152C` and `0x162C` appear to receive `720` and `480` as upper/lower halves. | Correlate with supported output modes and a known source format; confirm by comparing other traces and driver/decompiled code. |
| Validate read-modify-write framing | The 8-byte payload appears to be a value/mask pair. | Compare the bytes with the transport function implementation and observe readback/state changes. |

## 10. Input-switch transport, target devices, and no-source recipes

**Evidence:** `drivers/elgato_gchd/input_switch_trace_analysis.txt`, added in commit `ffc080b2a129fe4220de3d8abb3f3fbafd902fa2`. It summarizes two USBPcap traces and gives ordered writes for HDMI, component, composite, and S-video transitions. The new capture explicitly includes switches with no source attached; the earlier trace includes a 240p source and subsequent signal-dependent configuration.

### 10.1 Register-write framing and target addresses

The analysis reports this write pair for a single register/value operation:

```
40 BC 00 08 C0 00 02 00 <reg> <val>
40 B9 00 00 00 00 04 00 09 00 <ADDR> 02
```

The first transfer carries the two bytes; the following B9 word is described by the trace analysis as a commit naming the target address. The observed target addresses are:

| Target write address | Observed register groups | Interpretation / caveat |
|---|---|---|
| `0x9C` | `0x51`, `0x00`, `0xB3`, `0x27`, HDMI-side registers and table | Carries the selector candidate and many input-dependent writes. Target-device role beyond observed traffic is unknown. |
| `0x88` | `0x02`, `0x03`, `0x06`, `0x10–0x15`, `0x28`, `0x3C` | Carries composite/S-video-side configuration in these traces. |
| `0x98` | `0x0D`, `0x0E`, plus signal-dependent table and configuration writes | Carries the signal-lock/format-dependent writes in the earlier trace. The no-source trace does not show the full signal-dependent block. |

The trace also reports read requests using a four-byte payload after the port fields, followed by B9 control words and a result transfer reported as `C0 BC 00 08 BE 23 04 00`. The example result `00 00 B2 59` is interpreted as returning `0xB2) in byte 2 by the analysis. Treat the exact bridge protocol and commit/read sequencing as trace-derived until cross-checked against the transport implementation.

Additional recurring traffic reported by the analysis:
- Polls of `0x0800:0x0016`, `0x0900:0x001C`, and `0x0800:0x23BE`.
- A write of `D3 9E` to `0x0900:0x0014) between operations.
- Writes to `0x0900:0x0018) carrying per-input state bytes (below).

These may be handshake/status requirements. The minimum subset required for a successful input switch is **not established**; do not omit them from an initial driver reproduction just because their purpose is unknown.

### 10.2 Input selector and separate state word

| Input | `[0x9C] 0x51` observed | `0x0900:0x0018` bytes observed | Additional observed notes |
|---|---:|---|---|
| HDMI | `0x45` | `02 0A` | HDMI-side writes include `0x01`, `0x04`, `0x06`, `0x09`, `0x54`, `0xAC`, `0xCE`, `0xCF`, and a table spanning `0x92–0xAA). |
| Component | `0xED` | `02 0E` | Observed writes include `0x24=0C`, `0x21=CD`, `0x91=C8`, `0x0E=8C`, `0x11=EC`, `0x54=FC`, `0x71=4C`, and `0x04=CC`. |
| Composite | `0xEC` in the no-source trace; `0x44` in the earlier trace | `03 0E` | Shared analog-input prefix: `[0x88] 0x06=08`, `[0x88] 0x03=28`, `[0x9C] 0xB3=33`, `[0x9C] 0x27=33`. |
| S-video | `0xEC` | `03 0E` | Shares the composite selector/state bytes in this trace; separated by `[0x88] 0x02=DC` rather than composite's `0x02=C9`. |

The `0x0900:0x0018` word is not the same thing as register `0x51`: it is a separate control/state write, encoded as `40 BC 00 09 18 00 02 00 <b0> <b1>`. The trace reports three writes per input state in some transitions, including intermediate old-state bytes during a switch. That repetition and the other recurring handshake traffic may matter.

**Important unresolved discrepancy:** composite is `0x44` in `hdmi2composite_trace_analysis.txt` but `0xEC` in `input_switch_trace_analysis.txt`. These differ by more than the one-bit change discussed in the earlier hypothesis, so the theory that all selectors are formed by simply clearing bit 0 is not supported as a general rule. Possible causes include a different initial register/bank state, a difference in how the traces were decoded, or a context-dependent value; none is confirmed. Preserve both observations until a controlled read/write test determines why they differ.

### 10.3 Composite versus S-video

The new trace shows the following difference at the `0x88` target:

| Transition | Ordered writes shown in the trace | Status |
|---|---|---|
| Composite → S-video | `[0x88] 0x02=DC`, then `0x14=D2`; later `0x28=88`, `0x03=28`, `[0x9C] 0xB3=33`, `0x27=33`, then `0x51=EC`; state word `03 0E` is written repeatedly | Observed in the no-source trace. The exact necessity/order of every write is untested. |
| S-video → composite | `[0x88] 0x02=C9`, then `0x14=D2`, `0x3C=6B`; later `0x28=88`, `0x03=28`, `[0x9C] 0xB3=33`, `0x27=33`, then `0x51=EC` | Observed in the no-source trace. `0x3C=6B` appears on this direction only in the summarized event. |

This supports `[0x88] 0x02` as the clearest observed composite/S-video discriminator in this capture. It does **not** yet prove that `0x02` alone is sufficient, or that `0x3C=6B` is always required when switching back to composite.

### 10.4 Shared prefixes and HDMI/component differences

The new analysis identifies a recurring analog-input prefix:
- `[0x88] 0x06=08`
- `[0x88] 0x03=28`
- `[0x9C] 0xB3=33`
- `[0x9C] 0x27=33)

The HDMI/component group instead starts with:
- `[0x88] 0x06=86)
- `[0x88] 0x03=2F`
- `[0x9C] 0xB3=CC)
- `[0x9C] 0x27=CC)

HDMI and component then share a longer prefix of writes including `[0x9C] 0xAB=4C`, `0x1B=33`, `0xB7=CE), `0xB8=DC) followed by `0xB8=CC), and `0x07=38) followed by `0x07=C8), before the `0x51` write. Component then has its own observed tail. These are register writes to target `0x9C`; register `0xB8` here is distinct from the USB request opcode `B8` used for SCMD.

The trace gives **ordered observed recipes**, not a proven minimal sequence. Some writes may be staged bank selection, latching, recovery from the prior input, or redundant state synchronization. Keep the full order in `input_switch_trace_analysis.txt` rather than reducing it to just the selector value.

### 10.5 Signal-dependent writes remain a separate phase

The new capture was made with no source attached and does not show the full `0x98) signal-dependent coefficient/configuration sequence. The earlier trace with a 240p source shows that block being written after the analog-input switch and subsequent lock-related activity. This strengthens the distinction between:
1. selecting/configuring the physical input path, and
2. configuring the decoder after a signal is detected and lock/status changes.

Do not label the `0x98:0x90–0xA3) table “the 240p command” based on the current evidence. A 480i-with-source trace is still needed to determine which writes are format-specific.

### 10.6 New controlled tests

| Test | Question it answers | Evidence to capture |
|---|---|---|
| Repeat composite selection from a known reset/known prior input | Why does `0x51` appear as both `0x44` and `0xEC`? | Initial readback, all writes to target `0x9C`, state-word writes, source connection state, and post-switch readback. |
| Capture component from each analog input | Is `0x51=ED` repeatable and what writes are essential? | Exact ordered transactions in both directions, handshake/status traffic, and final readback. |
| Toggle only `[0x88] 0x02` between `C9` and `DC` | Is it sufficient to distinguish composite and S-video? | Stable video outcome, status, and any required companion writes. |
| Omit/reinstate `[0x88] 0x3C=6B` on return to composite | Is the direction-specific write essential? | Repeated switch outcomes and recovery after a failed attempt. |
| Reduce repeated `0x0900:0x0018` writes and handshake polling | Which repetitions and polls are required? | One change at a time, exact USB status, state readback if available, and whether switching completes. |
| Capture composite with a known 480i source and compare to the 240p trace | Which `0x98` writes are format-dependent? | Full ordered `0x98` write diff, lock/status transitions, and actual output frame format. |

## 11. Evidence gaps and rules for updating this reference

- This catalogue now draws on three trace analyses: composite 480i/240p transitions, HDMI-to-composite switching, and input-switch transitions captured with no source attached. It is still not a complete inventory of every device command.
- No semantic meaning or expected confirmation response should be assigned to a command just because it occurs before/after a state change.
- Record the capture filename, device/input type, source format, destination format, USB transfer direction/type, exact bytes, response/status, timing, and video outcome for each new finding.
- Promote an item from **Untested** only after a controlled reproduction. Preserve both the raw trace and the interpretation.
- Keep observations, hypotheses, and tested conclusions separate. When evidence contradicts a hypothesis, update the interpretation rather than silently rewriting the observed bytes.


## Source-first audit

The trace catalogue is accompanied by a source-first audit of the original userspace protocol, including documented SCMD meanings and confirmation, mailbox transport for both Game Capture HD generations, enable-state semantics, per-input configuration facts, transcoder writes, and the original developers' explicit uncertainty notes:

- [SOURCE_AUDIT.md](https://github.com/FF-5/elgato-gchd/blob/diagnostic-input-baseline/SOURCE_AUDIT.md)

**Correction to earlier trace-only wording:** SCMD meanings and state-confirmation behavior are documented in `src/gchd/commands.cpp` and `src/gchd_hardware.hpp`; those semantics should not be described as unknown at the source level. Individual register-field meanings, some mailbox responses, and the exact effect of the completion acknowledgement remain unresolved. The audit explicitly lists the remaining work before claiming a complete command inventory.
