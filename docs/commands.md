# Command catalog: auxiliary, unknown, and non-input operations

This document collects commands that do not belong exclusively to one input path, including unresolved commands. Byte sequences are hexadecimal. **Observed** means present in source or a trace; **inferred** means interpretation is not proven.

## Mailbox transport

| Operation | Bytes / USB transaction | Sequence and result |
|---|---|---|
| Original-HD mailbox write | Vendor OUT request `0xBD`; `wIndex = PORT << 8`; raw command bytes, no padding | Send command → restore saved enable-state shadow at `0900:0014` → poll `0900:001C` until bit 0 is set. |
| HD-New mailbox write | Write payload to `0800:00C0` using request `0xBC`; pad odd payload length with `00`; request `0xB9` payload `09 00 (PORT<<1) LENGTH` | Wait interrupt `0x83` → read `0800:0016` → poll `0900:001C` → read dummy/status bytes at `0800:23BE` → restore `0900:0014` → poll ready before next command. |
| HD-New mailbox read | Request `0xB9`, payload `09 01 (PORT<<1) LENGTH`; read `LENGTH+2+(LENGTH&1)` bytes from `0800:23BE` | Remove two leading status bytes and odd-length pad to obtain reply. |
| Interrupt wait | Interrupt IN endpoint `0x83`, 3-byte buffer | Source checks transfer status; payload is not generally validated by `interruptPend()`. |
| Register read helper | Mailbox bytes `9D CD INDEX`, port `0x33` | Returns one byte. Helper name is `readDevice0x9DCD`; target meaning unknown. |

## Generic mailbox command inventory

| Bytes | Port / reply | Meaning / confidence |
|---|---|---|
| `99 89 ED` | Port `0x33`; repeat read until reply bit 6 is set | Poll-complete helper according to source. |
| `99 89 EC` | Port `0x33`; repeated read, count passed by caller | Sequence-read helper; source discards returned values. |
| `89 89 FA` | Port `0x33`; one-byte reply | Composite NTSC/PAL-family detect: low nibble `6` = NTSC/60-Hz family, `7` = PAL/50-Hz family. Not a 240p/480i detector. ||
| `AB A9 0F A4 55` | Port `0x33`; 24-bit reply | Phase probe. Replies `33 44 55` and `27 F9 7B` observed; full meaning unknown. |
| `29 89 5B` | Port `0x33`; expected reply `91` | Source-annotated handshake; semantic meaning unknown. | Source-annotated handshake; semantic meaning unknown. |
| `DD CE 3F B2` | Port `0x33`; expected reply `DA 67` | Source-annotated handshake; semantic meaning unknown. | Source-annotated handshake; semantic meaning unknown. |
| `43 23 84` | Port `0x33`; poll until `F7` | Source-annotated handshake; semantic meaning unknown. | Source-annotated handshake; semantic meaning unknown. |
| `89 89 FB` | Port `0x33`; expected reply `6E` | Source-annotated handshake; semantic meaning unknown. | Source-annotated handshake; semantic meaning unknown. |
| `AB 92 3E B4 ...` | Port `0x33`; followed by 56-byte reads | Not understood in source; preserve exact call-site order and length. |
| `CMD 94 41 37` | HD-New device `0x94`, commit via `09 00 66 03` + handshake; no fetch stage | Observed auxiliary command in S-video trace. |
| `CMD 94 4A B7/BB` | Device `0x94`; commit + handshake | Value varies by trace: S-video uses `B7`, another trace uses `BB`. |
| `CMD 94 4B B7/BB` | Device `0x94`, commit + handshake | Value varies by trace, as above. |

## SCMD commands

SCMD uses vendor OUT request `0xB8`. Original HD payload is six bytes `00 00 CMD MODE DATA_H DATA_L`; HD-New payload is four bytes `CMD MODE DATA_H DATA_L`.`

| Payload (HD-New) | Source interpretation | Confirmation / caveat |
|---|---|---|
| `01 MODE ...` | IDLE | Expected state readback `0x11`; verify via common SCMD completion sequence. |
| `02 00 ...` or `02 01 ...` | RESET | Modes expect different readbacks; deeper meaning partly unconfirmed. |
| `04 00 00 00` | INIT with firmware load | Firmware-load/init path; check firmware version/words afterward. |
| `04 A0 00 00` | INIT without reloading encode firmware | Seen before stream start after input/transcoder setup. |
| `05 00 00 01` | State-change command; likely stop/transition phase | Observed in transition trace; exact meaning is untested. |
| `05 00 00 02` | START according to source; stream-start payload in traces | Expected state `0x02` when using source's state-confirmation routine. |
| `05 00 00 04` | NULL/empty stream according to source; transition interpretation remains cautious | Expected state `0x04` in source routine; trace use during format transition needs separate validation. |

### SCMD confirmation sequence

1. Send the generation-specific `0xB8` payload.
2. On HD-New when required, wait for interrupt endpoint `0x83`, then read `0800:0014` until its high byte matches CMD. This confirms command receipt, not state transition.
3. Poll `0900:0074` until bit `0x04` is set.
4. Read `0800:2008` twice; compare `value & 0x1F` with expected state.
5. Write `00 04` to `0900:0074` to acknowledge completion (source's interpretation); write `00 00` to `0900:01B0` (purpose unknown).

State values used by source: `00` uninitialized, `10` idle firmware loaded before IDLE, `11` idle, `01` initialized/stopped, `02` started, `04` null/empty stream, `12` special shutdown/reset state (interpretation partly theoretical).

## Unknown and readiness registers

| Address / bit | Observed behavior | Meaning / status |
|---|---|---|
| `0900:0018` | Written and read back | Enable register / input routing state word depending on operation; do not confuse enable values with input-state words. |
| `0900:0014` | Shadow updated after mailbox operation; trace writes session-dependent bytes | Enable-state shadow in source; values like `D3 9E` or `03 1E` vary by session. |
| `0900:001C bit 0` | Poll until set | Mailbox ready/busy flag according to source; trace values vary. |
| `0900:0074 bit 2` | Poll until set, then write `0004` | SCMD completion flag. |
| `0800:2008 low 5 bits` | Readback after SCMD | Firmware/encoder state. |
| `0800:0014 high byte` | Read after interrupt | SCMD command receipt on HD-New. |
| `0800:0016` | Often returns `09 00` in HD-New handshake | Interrupt/status role uncertain. |
| `0900:0070` | Write `00 04` during initial firmware setup | Purpose unknown. |
| `0900:01B0` | Write `00 00` after SCMD completion; trace also reads this after start | Purpose unknown; observed values vary. |
### Enable-register bits

These are **bit fields in the 16-bit register at `0x0900:0x0018`**. They are not separate register addresses or standalone commands. The source identifiers in the second column are names used by the code. A bit mask is `1 << bit_index`; setting/clearing a bit changes the full register word, which is written as two bytes. The exact complete word depends on the other bits already set.

| Register address | Bit index | Mask | Source identifier | Observed/source-documented use |
|---|---:|---:|---|---|
| `0x0900:0x0018` | 1 | `0x0002` | `EB_FIRMWARE_PROCESSOR` | Named by source; likely processor enable. Reset/CPU interpretation is conjecture. |
| `0x0900:0x0018` | 2 | `0x0004` | `EB_ANALOG_INPUT` | Source comments: set for composite/component, cleared for HDMI. |
| `0x0900:0x0018` | 3 | `0x0008` | `EB_ENCODER_ENABLE` | Source comments associate it with enabling the encoder after encode-firmware load. |
| `0x0900:0x0018` | 4 | `0x0010` | `EB_ENCODER_TRIGGER` | Purpose unknown; trigger/reset is only a guess. |
| `0x0900:0x0018` | 8 | `0x0100` | `EB_COMPOSITE_MUX` | Source comment says composite only. |
| `0x0900:0x0018` | 9 | `0x0200` | `EB_ANALOG_MUX` | Source comment says composite/component. |

**What value is written?** The driver writes the combined 16-bit enable/state word to `0x0900:0x0018`; the rows above identify masks, not complete write values. For example, setting only bit 2 would produce word `0x0004` (bytes `00 04`), but that is **not** evidence that `00 04` is the correct full value for an input switch. Preserve unrelated bits and use the exact per-input words recorded in §10.2 of `COMMAND_REFERENCE.md` only as trace observations: HDMI `02 0A`, component `02 0E`, composite/S-video `03 0E`. Those observed words include other state bits and must not be derived by OR-ing only the masks in this table.
| `BANKSEL` | Write `0000` at setup | Meaning unknown. |
| `0xCA` via `89 89 CA` | `FE`, `7E`, `66`, transient `EE` | Inferred: `66` associated with 240p lock and `7E` with 480i lock in one trace; validate across captures. |
| `0xFA` via `89 89 FA` | `3F`, `06`, `16` | Polled during setup/format transitions; exact meaning unknown. |
| `0xE7` via `89 89 E7` | `69`, `09`, `89` | Meaning unknown. |
| `0xF1` via `89 89 F1` | `7E`, `69` | Meaning unknown. |
| `0xCB` via `89 89 CB` | `6E`, `44` | Meaning unknown. |
| `0xFD`, `0xFC`, `0xF3` | Common reads `6E`, `6E`, `7E` in one sequence | Meaning unknown; prefix/address differs by read helper. |
| `0xF8` via `89 89 F8` | `C9` | Meaning unknown. |
| `0x8B` via `99 89 8B` | `2E`, `36` | Meaning unknown. |
| `0x89` via `99 89 89` | `6E` | Meaning unknown. |
| `0xFF` via `99 89 FF` | `73` | Meaning unknown. |
| `0x6B` via `99 89 6B` | `6E` | Meaning unknown. |
| `0x58` via `99 89 58` | `91` | Meaning unknown. |

## Transcoder and pipeline commands

| Operation | USB payload | Meaning |
|---|---|---|
| Full-word write (`slsi`) | Vendor OUT `0xBC`; `wIndex=ADDRESS`; 16-bit big-endian DATA | Writes one aligned 16-bit transcoder register. |
| Masked write (`sparam`) | Vendor OUT `0xBC`; `wValue=1`; `wIndex=ADDRESS & ~3`; 8-byte payload `VALUE32_BE MASK32_BE` | Updates only mask-selected bits. |
| Table write | Sequential 16-bit words, big-endian; odd trailing byte occupies high byte of last word | Coefficient/table load. Writable range boundaries not known. |
| Pipeline dimension candidates | `0x152C`, `0x162C`: value/mask writes encode `0x02D0` (720) and `0x01E0` (480) | Consistent with 720×480-related pipeline settings; exact field semantics not established. Other pipeline addresses observed: `1124`, `1264`, `1128`, `1268`, `1530`, `1534`, `1538`, `1630`, `1634`, `1638`. |

## Input-switch / format-transition commands not yet fully classified

| Target / bytes | Context | Status |
|---|---|---|
| `0x9C:00=CE/CC`, `B3=33`, `27=33`, `51=44/EC` | Analog path switch | Selector discrepancy is unresolved; both values are retained in the input-specific documents. |
| `0x88:0A=8F`, later `0A=98` | 480i→240p transition | Format correlation plausible; semantics unknown. |
| `0x98:70=C8`, `0x98:90=88`, `0x98:91–A3=77`, staged `0x98:C0=80/77/88` | Intermediate 240p→480i table | Preserve order; necessity and meaning untested. |
| `0x98:90–A3` | Full coefficient table: `01 BD 9A 3A E9 AB 17 88 84 8A 9C 8C 98 93 83 A9 8B E8 81 49` | Ordered observed table; individual coefficient meanings unknown. |
| `0x88:28=88/8A` | Observed in 480i/240p changes | Strong format correlation in one trace, exact bits unknown. |
| `0x98:0D=C8`; `0x98:0E=05,04` and `0x98:0E=15,14` | Setup/retrigger pairs | Preserve order; latch/command semantics unconfirmed. |

## Unknowns policy

Keep a byte sequence even when its meaning is unknown if it appears in source or a trace. Mark semantic meaning **UNKNOWN**; do not omit it, silently normalize conflicting values, or turn a one-trace correlation into a constant. Record exact order, target block, transfer type, expected response, retries, and evidence status whenever available.
