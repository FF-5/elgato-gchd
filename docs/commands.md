# Command catalog

Hex bytes are shown in transfer order. **UNKNOWN** means the command is observed but its effect is not established.

## USB and mailbox transport

| Target / operation | Bytes written or expected read | Notes |
|---|---|---|
| Original-HD mailbox write | Vendor OUT `0xBD`; `wIndex=PORT<<8`; raw command bytes | Send command, restore `0900:0014` shadow, poll `0900:001C` bit 0. No odd-byte padding. |
| HD-New mailbox write | Write to `0800:00C0` via OUT `0xBC`; commit `09 00 (PORT<<1) LENGTH` via `0xB9` | Pad odd payload with `00`; wait interrupt `0x83`, read `0800:0016`, poll ready, restore shadow. |
| HD-New mailbox read | `09 01 (PORT<<1) LENGTH` via `0xB9`; read from `0800:23BE` | Read `LENGTH+2+(LENGTH&1)` bytes; strip two status bytes and odd-byte pad. |
| Interrupt IN | Endpoint `0x83`, 3-byte buffer | Source checks transfer status; payload is not generally validated. |
| Transcoder register read | `9D CD INDEX` on port `0x33` | Reads one byte from the indexed transcoder register. |
| Transcoder full-word write | Vendor OUT `0xBC`; `wIndex=ADDRESS`; 16-bit big-endian data | Writes one aligned 16-bit register. |
| Transcoder masked write | Vendor OUT `0xBC`; `wValue=1`; `wIndex=ADDRESS & ~3`; 8-byte `VALUE32_BE MASK32_BE` | Updates mask-selected bits. |
| Transcoder table write | Sequential big-endian 16-bit words | Odd trailing byte occupies high byte of final word; writable boundaries UNKNOWN. |

## Generic mailbox commands

| Target / operation | Bytes written or expected read | Notes |
|---|---|---|
| Port `0x33` poll-complete | `99 89 ED`; repeat read until bit 6 set | Poll-complete helper. |
| Port `0x33` sequence read | `99 89 EC` | Caller supplies repeat count; values discarded by source. |
| Analog video-standard family detection | `89 89 FA`; one-byte reply | Low nibble `6` = NTSC/60-Hz family; `7` = PAL/50-Hz family. Does not distinguish progressive from interlaced modes. |
| Port `0x33` phase probe | `AB A9 0F A4 55`; 24-bit reply | Replies `33 44 55` and `27 F9 7B` observed; meaning UNKNOWN. |
| Port `0x33` handshake | `29 89 5B`; expect `91` | Meaning UNKNOWN. |
| Port `0x33` handshake | `DD CE 3F B2`; expect `DA 67` | Meaning UNKNOWN. |
| Port `0x33` handshake | `43 23 84`; poll for `F7` | Meaning UNKNOWN. |
| Port `0x33` handshake | `89 89 FB`; expect `6E` | Meaning UNKNOWN. |
| Port `0x33` block read | `AB 92 3E B4 ...`; then 56-byte reads | Meaning UNKNOWN; preserve exact call-site order and length. |
| Device `0x94` auxiliary command | `41 37`; commit `09 00 66 03` + handshake | Observed in analog input setup. |
| Device `0x94` auxiliary command | `4A B7` or `4A BB`; commit + handshake | Value varies by sequence. |
| Device `0x94` auxiliary command | `4B B7` or `4B BB`; commit + handshake | Value varies by sequence. |

## SCMD

SCMD uses vendor OUT request `0xB8`. Original HD uses six bytes `00 00 CMD MODE DATA_H DATA_L`; HD-New uses four bytes `CMD MODE DATA_H DATA_L`.

| Target / operation | Bytes written or expected read | Notes |
|---|---|---|
| SCMD IDLE | `01 MODE ...` | Expected state `0x11`. |
| SCMD RESET | `02 00 ...` or `02 01 ...` | Mode-dependent readback; deeper meaning partly unconfirmed. |
| SCMD INIT with encode firmware | `04 00 00 00` | Firmware-load/init path; verify firmware/version words. |
| SCMD INIT without reload | `04 A0 00 00` | Used before capture start after input/transcoder setup. |
| SCMD state change | `05 00 00 01` | Seen during transition; exact meaning UNKNOWN. |
| SCMD START | `05 00 00 02`; expect state `02` | Starts stream; confirm state and actual video payload. |
| SCMD NULL/empty stream | `05 00 00 04`; expect state `04` | Interpretation during transitions needs validation. |
| SCMD receipt | Read `0800:0014` high byte after interrupt | On HD-New, matching CMD confirms receipt, not state transition. |
| SCMD completion | Poll `0900:0074` bit 2; read `0800:2008` twice | Compare low 5 bits with expected state; acknowledge by writing `00 04` to `0900:0074`. Then write `00 00` to `0900:01B0` (purpose UNKNOWN). |

State values used by the source: `00` uninitialized, `10` idle firmware loaded, `11` idle, `01` initialized/stopped, `02` started, `04` null/empty stream, `12` special shutdown/reset state (interpretation partly theoretical).

## Registers and status values

| Target / operation | Bytes written or expected read | Notes |
|---|---|---|
| `0900:0018` routing/enable word | HDMI `02 0A`; component `02 0E`; composite/S-video `03 0E` | Full 16-bit state words; preserve other bits. |
| `0900:0014` shadow | Session-dependent bytes such as `D3 9E` or `03 1E` | Restored after mailbox operation; not a constant. |
| `0900:001C` bit 0 | Poll until set | Mailbox ready/busy flag according to source. |
| `0900:0074` bit 2 | Poll until set; acknowledge with `00 04` | SCMD completion flag. |
| `0800:2008` low 5 bits | Read after SCMD | Firmware/encoder state. |
| `0800:0016` | Often `09 00` during HD-New handshake | Interrupt/status role uncertain. |
| `0900:0070` | Write `00 04` during firmware setup | Purpose UNKNOWN. |
| `0900:01B0` | Usually write `00 00` after SCMD completion | Purpose UNKNOWN; readback varies. |
| Analog decoder status / source-found candidate (`89 89 CA`) | Replies include `FE`, `7E`, `66`, transient `EE` | `66` correlates with 240p lock and `7E` with 480i in an observed sequence; these are not universal constants. |
| Analog family/status poll (`89 89 FA`) | Replies include `3F`, `06`, `16` | Used during analog setup and source detection; low nibble `6`/`7` identifies NTSC/PAL family in the family-detection path. Other reply semantics remain unconfirmed. |
| Analog decoder status poll (`89 89 E7`) | Replies `69`, `09`, `89` | Polled during decoder setup and lock settling; exact bit meanings are unconfirmed. |
| Analog decoder status poll (`89 89 F1`) | Replies `7E`, `69` | Polled during decoder setup and lock settling; exact bit meanings are unconfirmed. |
| Analog decoder status poll (`89 89 CB`) | Replies `6E`, `44` | Polled during decoder setup; exact bit meanings are unconfirmed. |
| `0xFD`, `0xFC`, `0xF3` | Common replies `6E`, `6E`, `7E` | Meaning UNKNOWN; helper prefixes differ. |
| `0xF8` via `89 89 F8` | Reply `C9` | Meaning UNKNOWN. |
| `0x8B` via `99 89 8B` | Replies `2E`, `36` | Meaning UNKNOWN. |
| `0x89` via `99 89 89` | Reply `6E` | Meaning UNKNOWN. |
| `0xFF` via `99 89 FF` | Reply `73` | Meaning UNKNOWN. |
| `0x6B` via `99 89 6B` | Reply `6E` | Meaning UNKNOWN. |
| `0x58` via `99 89 58` | Reply `91` | Meaning UNKNOWN. |
| `BANKSEL` | Write `00 00` during setup | Meaning UNKNOWN. |

Enable-bit masks for `0900:0018`: bit 1 `0x0002` (`EB_FIRMWARE_PROCESSOR`); bit 2 `0x0004` (`EB_ANALOG_INPUT`); bit 3 `0x0008` (`EB_ENCODER_ENABLE`); bit 4 `0x0010` (`EB_ENCODER_TRIGGER`); bit 8 `0x0100` (`EB_COMPOSITE_MUX`); bit 9 `0x0200` (`EB_ANALOG_MUX`). These are masks, not full state words.

## Transcoder and format-transition commands

| Target / operation | Bytes written or expected read | Notes |
|---|---|---|
| Pipeline dimension candidates `0x152C`, `0x162C` | Masked values encode `0x02D0` (720) and `0x01E0` (480) | Consistent with 720×480-related settings; field semantics not established. Other observed addresses: `1124`, `1264`, `1128`, `1268`, `1530`, `1534`, `1538`, `1630`, `1634`, `1638`. |
| Analog front end | `0x9C:00=CE/CC`, `B3=33`, `27=33`, `51=44/EC` | Selector varies by input/condition; see composite and S-video sequence. |
| Decoder transition | `0x88:0A=8F`, later `0A=98` | Seen around 480i→240p transition; meaning UNKNOWN. |
| Backend intermediate table | `0x98:70=C8`, `90=88`, `91–A3=77`, staged `C0=80/77/88` | Preserve order; necessity and meaning untested. |
| Backend coefficient table | `0x98:90–A3`: `01 BD 9A 3A E9 AB 17 88 84 8A 9C 8C 98 93 83 A9 8B E8 81 49` | Ordered values; individual coefficient meanings UNKNOWN. |
| PAL S-video 288p-equivalent backend writes | `0x98:04=95`, `70=D0`, `0F=88`, `90=D8`, `91=BF`, `95=9D`, `96=1B`, `97=88`, `A0=8A`, `A1=D8`, `A2=B2`, `A3=59`; later `72=88`, `C0=88`, `61=98`, `63=89`, `64=96`, `65=88`, `04=8D`, `62=91` | Sparse, ordered writes from the working PAL S-video path; not a complete `90–A3` table. Keep separate from the composite coefficient sequence. |
| Decoder format value | `0x88:28=88/8A` | Correlates with 480i/240p in observed transitions; exact bits UNKNOWN. |
| Backend retrigger | `0x98:0D=C8`; `0x98:0E=05,04` or `15,14` | Preserve exact order; latch/command semantics not confirmed. |

## Idle / keep-alive loop

The HDMI no-signal keep-alive loop repeats after front-end setup while no signal is present.

| Target / operation | Bytes written or expected read | Notes |
|---|---|---|
| Write `0x9C:AB` | `4C` | Keep-alive command. |
| Read helper, port `0x33` | `9D CD 6A 00` | Read index `6A`; result discarded. |
| Decoder status/keep-alive poll, port `0x33` | `99 89 F5 00` | Repeated during no-signal idle loops; result is not used by the documented caller. |
| Read helper, port `0x33` | `9D CD 94 00` | Read index `94`; result discarded. |

The normal HD-New mailbox handshake follows each mailbox command. Composite/S-video use a different two-command idle loop, and component uses a six-command loop. Behaviour after signal lock is not established.

## Component-switch helper

| Target / operation | Bytes written or expected read | Notes |
|---|---|---|
| Port `0x33` read helper | `89 89 F8 00` | Appears between `06 86` and `03 2F` in the component prefix; meaning UNKNOWN. |
| Routing state `0900:0018` | Transient `02 0A`, then `02 0E` before selector `51 ED` | Observed HDMI-to-component ordering. |

Keep unknown commands when observed. Record target, transfer bytes, expected reply, retries and order; do not infer semantics from one correlation.
