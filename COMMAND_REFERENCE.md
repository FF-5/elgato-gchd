# GCHD Command Reference

This document is the working inventory of commands and register operations identified so far for the Elgato Game Capture HD (GCHD). It is intentionally conservative: a value seen on the USB wire is **observed**, but its hardware meaning is not considered proven unless noted.

**Evidence currently catalogued:** `drivers/elgato_gchd/trace_analysis.txt`, trace `gchd_240p-480i_switch_to_composite.pcapng` on branch `diagnostic-input-baseline`. The trace is specifically for **composite input** and format changes between 480i and 240p; do not assume HDMI behaviour is identical.

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

## 3. Observed writes to device block `0x88`

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

## 4. Observed writes to device block `0x98`

### 4.1 Common setup writes

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

### 4.2 Coefficient/table writes

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

### 4.3 Intermediate table during 240p→480i

This phase appears only in the captured return-to-480i sequence. The driver must **not** assume its purpose or necessity until tested.

| Register range / register | Observed value(s) | Description / status |
|---|---|---|
| `0x70` | `0xC8` | Precedes intermediate table writes; purpose unknown. |
| `0x90` | `0x88` | First entry differs from the final full table. |
| `0x91–0xA3` | Mostly `0x77`; `0xA2=0x77` is explicitly written again later | Flat/intermediate table phase; exact purpose unknown. |
| `0xC0` | `0x80`, then `0x77`, then `0x88` | Staged writes around the intermediate table; purpose unknown. |

## 5. SCMD commands seen in the trace — untested semantics

These exact 4-byte payloads are observed on the wire. The names below are provisional and are not assertions about what the device does.

| Payload after `B8` | Context | Provisional interpretation | Status / expected confirmation |
|---|---|---|---|
| `04 A0 00 00` | Initial capture setup at 10.315 s; again after full 480i reconfiguration at 59.853 s | Possible capture/setup or start-related command | **Untested.** No confirmed acknowledgement mapping yet. |
| `05 00 00 02` | Immediately after `04 A0 00 00` at initial setup and final reconfiguration | Possible capture-start/enable command | **Untested.** Sequence correlation only; command meaning and confirmation response unknown. |
| `05 00 00 04` | During 240p→480i reconfiguration at 57.985 s | Possible stop/disable or transition-stage command | **Untested.** Do not call it “stop” as a fact. |
| `05 00 00 01` | At 58.646 s, after `05 00 00 04` and before final full setup | Possible intermediate lifecycle command | **Untested.** Meaning and acknowledgement unknown. |

## 6. Newly catalogued / untested driver operations

This section intentionally separates **seen in a reference trace** from **tested as a standalone operation**. Treat these as experimental until reproduced with controlled tests and capture/video health checks.

| Operation to test | Exact known values | Why it is listed | Safe test / acceptance evidence |
|---|---|---|---|
| Change `0x88:0x28` for detected format | 480i: `0x88`; 240p: `0x8A` | Strong correlation with the `0xCA` status change. | Change only this register while streaming; verify stable image and format, and record whether any other writes are required. |
| Reapply `0x88:0x07–0x09` | `07=8A, 08=9B, 09=7A` | Repeated at both transitions and initial setup. | Compare transition success with/without reapplying this group; restore known-good configuration on failure. |
| Apply `0x88:0x0A` updates | `0x8F`, later `0x98` on 480i→240p | Appears only during the captured 240p transition. | Test each write separately and record timing relative to lock status and valid frames. |
| Run the `0x98` intermediate table | `0x70=C8`, `0x90=88`, `0x91–0xA3=77`, staged `0xC0` values | Seen only in the captured 240p→480i transition. | Compare return-to-480i with and without this phase; verify decoder lock, frame integrity, and whether it is actually required. |
| Replay the final `0x98` table | `0x70=D0`, `0x0F=88`, `0x90–0xA3` values in §4.2, common setup values in §4.1 | Reapplied after the intermediate phase. | First reproduce the full trace sequence; then remove one group at a time while capturing results. |
| Send SCMD `04 A0 00 00` | Exact payload shown above | Repeated at initial setup and after return-to-480i setup. | Capture the control transfer status, any subsequent device state changes, and whether streaming starts; do not infer acknowledgement from timing alone. |
| Send SCMD `05 00 00 02` | Exact payload shown above | Paired with `04 A0 00 00` in the trace. | Determine required ordering and observable confirmation; test only with recovery path available. |
| Send SCMD `05 00 00 04` | Exact payload shown above | Appears during 240p→480i transition. | Establish whether it halts capture, resets a block, or does something else by observing stream continuity and control-transfer results. |
| Send SCMD `05 00 00 01` | Exact payload shown above | Appears after `05 00 00 04`. | Establish purpose and ordering independently; record the device response and subsequent stream state. |
| Use `0xCA` as a format-state input | `FE`, `7E`, `66`, transient `EE` observed | Most useful current candidate for format/lock detection. | Capture multiple transitions per input type; require stable repeated samples and validate against actual decoded frames before updating V4L2 format. |

## 7. Observed sequences

### Initial composite lock / 480i setup

1. Write `0x88:0x06=08`, `0x88:0x03=28`; perform early `0x9C` writes and status reads.
2. Program `0x88:0x10–0x15` in several passes.
3. Read `0xCA=FE` (unlocked in the current interpretation), then write `0x88:0x07=8A`, `0x08=9B`, `0x09=7A`, `0x28=88`, read `0xFD`, and write `0x06=08`.
4. Program the full `0x98` table and common registers.
5. Send `B8 04 A0 00 00`, then `B8 05 00 00 02`.

### 480i→240p

1. `0xCA` changes from `0x7E` to `0x66`.
2. Reapply `0x88:0x07–0x09`, change `0x28=8A`, and write `0x06=08`.
3. Write `0x0A=8F`, later `0x0A=98`; write `0x98:0x0D= C8`, `0x0E=15`, `0x0E=14`.
4. The trace shows no SCMD and no SoC writes during this transition, and capture continues. This is an observation from this one capture, not a universal rule.

### 240p→480i

1. Before the status transition, write `0x98:0x0D=C8`, `0x0E=15`, `0x0E=14` twice.
2. `0xCA` changes from `0x66` to `0x7E`; reapply `0x88:0x07–0x09`, `0x28=88`, and `0x06=08`.
3. Apply the intermediate `0x98` table and common setup.
4. Send `B8 05 00 00 04`, then `B8 05 00 00 01`.
5. Apply the full `0x98` table and common setup; send `B8 04 A0 00 00`, then `B8 05 00 00 02`.

## 8. Evidence gaps and rules for updating this reference

- This catalogue is based on one composite-input format-switch trace. It is not a complete inventory of every device command.
- No semantic meaning or expected confirmation response should be assigned to a command just because it occurs before/after a state change.
- Record the capture filename, device/input type, source format, destination format, USB transfer direction/type, exact bytes, response/status, timing, and video outcome for each new finding.
- Promote an item from **Untested** only after a controlled reproduction. Preserve both the raw trace and the interpretation.
- Keep observations, hypotheses, and tested conclusions separate. When evidence contradicts a hypothesis, update the interpretation rather than silently rewriting the observed bytes.
