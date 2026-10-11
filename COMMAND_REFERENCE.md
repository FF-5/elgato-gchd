# GCHD command reference

Compact byte-level reference for commands found in the original userspace implementation and uploaded USB traces. Register values are hexadecimal. “Source” means implemented/commented in C++; “Trace” means observed in capture; “Inferred” means correlation only.

## 1. USB transport and generic command forms

| Operation | Generic bytes / USB fields | Meaning |
|---|---|---|
| Vendor control read | USB request type `0xC0`; request/value/index/length depend on operation | Reads a device register/status buffer. Source requires exact requested length. |
| Vendor control write | USB request type `0x40`; request/value/index/payload depend on operation | Writes a device register/configuration buffer. Source requires exact byte count. |
| SCMD | Vendor OUT request `0xB8`; original HD payload `00 00 CMD MODE DATA_H DATA_L` (6 bytes); HD New payload `CMD MODE DATA_H DATA_L` (4 bytes) | Firmware/encoder state command. See SCMD table and required follow-up sequence. |
| Original-HD mailbox write | Vendor OUT request `0xBD`; `wIndex = PORT << 8`; payload is command bytes, unpadded | Sends a command to mailbox port `PORT`. After writing, source restores enable shadow and polls mailbox-ready. |
| HD-New mailbox write | Write payload to `0x0800:0x00C0` via request `0xBC`; odd byte counts padded with `00`. Then request `0xB9`, `wIndex=0`, payload `09 00 (PORT<<1) LENGTH` | Asynchronous mailbox write. Requires interrupt/status/readiness sequence below. |
| HD-New mailbox read | Request `0xB9`, `wIndex=0`, payload `09 01 (PORT<<1) LENGTH`; then read `LENGTH+2+(LENGTH&1)` bytes from `0x0800:0x23BE` | Drops two leading bytes and any odd-length pad; remaining bytes are mailbox reply. |
| Firmware download | Bulk OUT endpoint `0x02`; chunks 16,384 bytes (original HD), 32,768 bytes (HD New) | Loads idle or encode firmware. |
| Interrupt wait | Interrupt endpoint `0x83`, 3-byte buffer | HD-New mailbox/SCMD synchronization. Source checks transfer status; payload is not generally validated by `interruptPend()`. |
| Transcoder full-word write (`slsi`) | Vendor OUT `0xBC`; `wIndex=ADDRESS`; 2-byte big-endian DATA | Writes a 16-bit transcoder register; address must be 2-byte aligned. |
| Transcoder masked write (`sparam`) | Vendor OUT `0xBC`; `wValue=1`; `wIndex=ADDRESS & ~3`; payload `VALUE32_BE MASK32_BE` (8 bytes) | Updates only mask-selected bits in a 32-bit transcoder register. |
| Transcoder table write | Sequential 16-bit words using transcoder writes; odd trailing byte occupies high byte of final word | Loads table/coefficient blocks; writable range boundaries are not known in source. |

## 2. Mailbox generic commands

| Bytes | Port / reply | Meaning |
|---|---|---|
| `9D CD INDEX` | Write to port `0x33`, then read 1 byte | Helper named `readDevice0x9DCD`; target/meaning unknown to original author. |
| `99 89 ED` | Port `0x33`; repeat/read until response bit 6 is set | Poll helper; source identifies bit 6 as poll-complete. |
| `99 89 EC` | Port `0x33`; repeated read, count supplied by caller | Reads a sequence of bytes; helper currently discards returned values. |
| `89 89 FA` | Port `0x33`; one-byte reply | Composite video mode detection; low nibble 6 = NTSC/60.0, 7 = PAL/50.0. |
| `AB A9 0F A4 55` | Port `0x33`; 24-bit magic reply | Device/firmware phase probe. Source observes `33 44 55` and `27 F9 7B`; full meaning unknown. |
| `29 89 5B` | Port `0x33`; expected reply `91` | Source-annotated handshake; semantic meaning unknown. |
| `DD CE 3F B2` | Port `0x33`; expected reply `DA 67` | Source-annotated handshake; semantic meaning unknown. |
| `43 23 84` | Port `0x33`; poll until `F7` | Source-annotated handshake; semantic meaning unknown. |
| `89 89 FB` | Port `0x33`; expected reply `6E` | Source-annotated handshake; semantic meaning unknown. |
| `AB 92 3E B4 ...` | Port `0x33`; followed by 56-byte reads | Explicitly not understood in source; retain order and length from call site. |

## 3. SCMD commands and confirmation

| CMD | MODE | DATA | Meaning | Confirmation / notes |
|---:|---:|---:|---|---|
| `0x01` IDLE | mode as call site | — | Enter idle; expected state readback `0x11` | State confirmation routine checks readback and completion. |
| `0x02` RESET | `0x00` or `0x01` | — | Reset firmware state | Modes expect different state readbacks; source says deeper reset interpretation is probable, not fully proven. |
| `0x04` INIT | `0x00` | `0x0000` | Load/initialize encode firmware; generates interrupt | Firmware-loading path then reads expected version/firmware words. |
| `0x04` INIT | `0xA0` | `0x0000` | Initialize without reloading encode firmware or generating that interrupt | Used before stream start after input/transcoder setup. |
| `0x05` STATE_CHANGE | `0x00` | `0x0001` | STOP | Expected state `0x01`; wait for completion bit `0x04`. |
| `0x05` STATE_CHANGE | `0x00` | `0x0002` | START | Expected state `0x02`; wait for completion bit `0x04`. |
| `0x05` STATE_CHANGE | `0x00` | `0x0004` | NULL / empty output stream | Expected state `0x04`; wait for completion bit `0x04`. |

State values used by source: `0x00` uninitialized; `0x10` idle firmware loaded before IDLE; `0x11` idle; `0x01` initialized/stopped; `0x02` started; `0x04` null/empty stream; `0x12` special shutdown/reset state (deeper meaning is a theory).

### Required sequence after SCMD

1. Send `0xB8` SCMD payload in the device-generation-specific length.
2. On HD New where requested by the source, wait for interrupt endpoint `0x83`; read `0x0800:0x0014` until its high byte equals CMD. This is command-receipt/readback, not proof that the state transition completed.
3. For a state transition, poll `0x0900:0x0074` until bit `0x04` is set.
4. Read `0x0800:0x2008`, mask with `0x1F`, and check the expected state. Source reads it twice.
5. Write `0x0004` to `0x0900:0x0074` (believed to clear/acknowledge sticky completion; exact effect unconfirmed), then write `0x0000` to `0x0900:0x01B0` (purpose unknown).

## 4. Enable and readiness registers

| Address | Bytes / bit | Meaning |
|---|---|---|
| `0x0900:0x0018` | `ENABLE_REGISTER` | Actual enable register. Source writes and reads back it. |
| `0x0900:0x0014` | Enable-state shadow | Written after mailbox operations; shadow bits may be marked as touched even when actual bit is cleared. |
| `0x0900:0x001C` | Bit 0 | Mailbox request ready/busy poll; source waits for bit 0 to be set. |
| `0x0900:0x0074` | Bit `0x04` | SCMD state-change completion; write `0x0004` after checking state. |
| `0x0800:0x2008` | Low 5 bits | SCMD state readback. |
| `0x0800:0x0014` | High byte = CMD (HD New) | SCMD command-receipt/readback after interrupt. |
| `0x0800:0x0016` | Expected `09 00` (HD New) | Read after mailbox interrupt; source speculates about interrupt status/clear-on-read, meaning not proven. |
| `0x0900:0x0070` | Write `00 04` during initial firmware setup | Purpose unknown in source. |
| `0x0900:0x01B0` | Write `00 00` during SCMD completion | Purpose unknown in source. |
| Enable bit 1 | `EB_FIRMWARE_PROCESSOR` | Believed to enable firmware processor; CPU/reset theory is conjecture. |
| Enable bit 2 | `EB_ANALOG_INPUT` | Set for composite/component, clear for HDMI. |
| Enable bit 3 | `EB_ENCODER_ENABLE` | Enables encoder after encode firmware load. |
| Enable bit 4 | `EB_ENCODER_TRIGGER` | Purpose unknown; source guesses momentary trigger/reset. |
| Enable bit 8 | `EB_COMPOSITE_MUX` | Source comment says set only for composite. |
| Enable bit 9 | `EB_ANALOG_MUX` | Source comment says set for composite/component. |
| `BANKSEL` | Write `0x0000` at setup | Meaning unknown. |

## 5. Input-selection and decoder setup bytes

| Input / stage | Bytes (mailbox port or target block noted) | Meaning / required follow-up |
|---|---|---|
| HDMI selector | Port `0x4E`: `51 45`; trace sees target `0x9C:0x51=45` | Select HDMI path. Follow shared HDMI/component setup, detect timing/color space, then mode-specific setup and transcoder configuration. |
| Component selector | Port `0x4E`: `51 ED`; trace sees target `0x9C:0x51=ED` | Select component path. Follow shared HDMI/component setup, detect timing/scan mode, apply component mode table and transcoder configuration. |
| HDMI/component shared prefix | Register-style writes include `00 CC`, `AB 4C`, `00 CE`, `1B 33`, `00 CC`, `B7 CE`, `B8 DC`, `B8 CC`, `00 CE`, `07 38`, `07 C8`, `00 CC`, selector `51 45` or `51 ED`, `B7 CC` | These are ordered with reads from `0x9DCD`; do not extract the selector and omit the surrounding setup. |
| HDMI detect | Read/average `0x66/0x65`, `0x64/0x63`; read `0x34` after `00 CE` | Bit 2 of `0x34` is used as RGB flag. Timing thresholds are heuristics: `0x6463≈0xB6D7` 1080p; `≈0xB081` 1080i; `≈0xB05C` 720p; `≈0xB0C1` 480p/576p midpoint. |
| Component-specific writes | `24 0C`, `21 CD`, `91 C8`, `0E 8C`, `11 EC`, `71 4C`, `04 CC` | Apply after shared prefix; source notes `0x04` may need another value for non-1080 modes but does not detect it. |
| Composite mode detect | Port `0x33`: `89 89 FA`, then read one byte | Low nibble 6 → NTSC/60.0; 7 → PAL/50.0. Default scan mode interlaced. |
| Composite NTSC setup | Target block `0x88`: `07=8A`, `08=9B`, `09=7A`, `28=88`; poll `89 89 FD` for `6E`; then `06=08` | Must follow composite mode detection. |
| Composite PAL setup | Poll `89 89 F1` for `63`; write port `0x4C` register `04=95` | PAL-specific branch. |
| Composite/S-video distinction | Target block `0x88`: `02=C9` composite; `02=DC` S-video. Composite return may include `3C=6B`. | Keep the source-specific analog setup sequence. Trace shows composite selector `0x9C:0x51` as `0x44` in one capture and `0xEC` in another; discrepancy unresolved. |
| Shared analog prefix | Target `0x88`: `06=08`, `03=28`; target `0x9C`: `B3=33`, `27=33` | Seen in analog setup; exact meanings of individual values unknown. |
| HDMI/component shared path prefix | Target `0x88`: `06=86`, `03=2F`; target `0x9C`: `B3=CC`, `27=CC` | Shared digital/component path setup. |
| 240p signal-dependent setup | Target `0x98`: `04=95`, `70=D0`, `0F=88`; then coefficient table `90–A3`; then `72=88,C0=88,61=98,63=89,64=96,65=88,04=8D,62=91`; then `0D=C8,0E=05,0E=04`; later `0D=C8,0E=15,0E=14` | Trace-observed after signal lock. Do not treat as a universal input-select command; compare with 480i and preserve order. |
| Coefficient table | Target `0x98`: `90=01,91=BD,92=9A,93=3A,94=E9,95=AB,96=17,97=88,98=84,99=8A,9A=9C,9B=8C,9C=98,9D=93,9E=83,9F=A9,A0=8B,A1=E8,A2=81,A3=49` | Ordered signal/format table observed in 240p-related trace; meaning unknown. |
| Intermediate flat table | Target `0x98`: `70=C8`, `90–A3` mostly `77`, `C0=80` then `C0=77` then `C0=88` | Intermediate transition phase; do not substitute for the final coefficient table. |

## 6. Transcoder parameters and setup

| Form / register family | Bytes / values | Meaning / follow-up |
|---|---|---|
| `slsi` | `BC`, `wIndex=ADDRESS`, two-byte big-endian value | Write 16-bit transcoder register. |
| `sparam` | `BC`, `wValue=1`, `wIndex=ADDRESS&~3`, `VALUE32_BE MASK32_BE` | Masked 32-bit field update; preserves bits outside mask. |
| Transcoder table | Sequential 16-bit words, big-endian; odd final byte in high halfword byte | Used for table/coefficient programming. |
| Pipeline dimensions | Trace contains `0x02D0=720` and `0x01E0=480` among writes to `0x1124`, `0x1264`, `0x1128`, `0x1268`, `0x152C`, `0x162C`, `0x1530`, `0x1534`, `0x1538`, `0x1630`, `0x1634`, `0x1638` | Consistent with 720×480-related pipeline values; exact field semantics are not all established. |
| Output configuration | Video format/size/bitrate/H.264, transport-stream/PIDs, audio/AAC, color-description and mirrored `_bl` fields | Configured separately from decoder/mailbox registers. Actual values depend on runtime settings; source bitfield map was recovered from scripts, not a data sheet. |

## 7. Required command sequences

| When | Required order | Why / completion condition |
|---|---|---|
| Any mailbox write on original HD | Send `BD` payload → write saved enable shadow to `0900:0014` → poll `0900:001C` bit 0 set | Source implements this readiness handshake; whether every poll is strictly necessary is unknown. |
| Any mailbox write on HD New | Write/pad payload at `0800:00C0` → `B9` config `09 00 (PORT<<1) LEN` → interrupt `83` → read `0800:0016` → poll `0900:001C` → read 2 dummy bytes at `0800:23BE` → restore `0900:0014` → poll ready | Do not send the next mailbox transaction before the previous asynchronous operation is completed. |
| Mailbox read on HD New | `B9` config `09 01 (PORT<<1) LEN` → interrupt/status/readiness → read `LEN+2+(LEN&1)` from `0800:23BE` → strip 2 status bytes and odd pad | Reply bytes follow transport header/padding. |
| Fresh device initialization | Read version/state → if uninitialized load idle firmware and read expected firmware words; otherwise RESET → IDLE → probe magic/cable bits → initialize transcoder defaults → INIT mode 0 → load encode firmware → common mailbox/decoder setup → enable encoder and handshake → input-specific configure/detect → final transcoder configure → INIT mode A0 → START and confirm | Order is significant; input setup alone is not a complete startup sequence. |
| Input switch | Set required analog/digital enable bits and run that input's full selector + shared + input-specific sequence → wait/poll for signal lock → detect format and apply signal-dependent tables → update transcoder output settings → request/reconfirm START if path was stopped or source does so | Avoid applying the 240p table before the signal-dependent stage; composite uses NTSC/PAL detection. |
| SCMD START/STOP/NULL | Send SCMD → on HD New command-receipt interrupt/readback when required → poll `0900:0074` bit 2 → check `0800:2008 & 0x1F` against expected state → acknowledge completion `0004` → write `0000` to `0900:01B0` | Completion is the state/readback handshake, not merely successful USB transfer. |
| Composite autodetection throws | Run source's `0x9DCD` read / `0x4E` write recovery sequence before continuing or aborting | Developer says this avoids a lockup; exact reason unknown. Preserve sequence in a faithful port. |
| 240p ↔ 480i transition | Poll status; apply the observed intermediate `0x98` table/phase writes; reapply required `0x88` writes; wait for stable status before final format configuration | Trace shows transition writes differ from initial setup; exact semantics remain partly unknown. |

## 8. Status values and other observed reads



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
