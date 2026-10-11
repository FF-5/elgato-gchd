# Original userspace source audit

**Branch:** `diagnostic-input-baseline`. This audit compares the original userspace source with the trace analyses on this branch. It distinguishes source-confirmed behavior from explicit developer guesses and from trace-only observations.

## Scope

Audited the main configuration/protocol paths:
- [`src/gchd/commands.cpp`](https://github.com/FF-5/elgato-gchd/blob/diagnostic-input-baseline/src/gchd/commands.cpp): USB transfer wrappers, SCMD, state completion, mailbox transport, firmware download, helper commands, shutdown.
- [`src/gchd/configure.cpp`](https://github.com/FF-5/elgato-gchd/blob/diagnostic-input-baseline/src/gchd/configure.cpp): firmware bring-up, shared device configuration, mailbox handshakes, common decoder/transcoder blocks.
- [`src/gchd/configure_hdmi.cpp`](https://github.com/FF-5/elgato-gchd/blob/diagnostic-input-baseline/src/gchd/configure_hdmi.cpp): HDMI input, timing/color-space detection, mode-dependent setup.
- [`src/gchd/configure_component.cpp`](https://github.com/FF-5/elgato-gchd/blob/diagnostic-input-baseline/src/gchd/configure_component.cpp): component input, timing/interlace inference and mode tables.
- [`src/gchd/configure_composite.cpp`](https://github.com/FF-5/elgato-gchd/blob/diagnostic-input-baseline/src/gchd/configure_composite.cpp): composite NTSC/PAL detection and decoder setup.
- [`src/gchd_hardware.hpp`](https://github.com/FF-5/elgato-gchd/blob/diagnostic-input-baseline/src/gchd_hardware.hpp): request/register constants, SCMD values, enable bits and transcoder bitfields.

This is not yet a repository-wide audit of every command-producing file, firmware bytes, all runtime paths, or all device generations. The source itself remains canonical for the full ordered sequences and inline expected-response comments. Do not call the overall catalogue complete until the remaining-work section is done.

## 1. USB command primitives

### Control transfers and bulk/interrupt I/O

- `read_config_buffer` and `read_config`: vendor IN request type `0xC0`; exact returned length is required or an exception is thrown. The second helper discards the data.
- `write_config_buffer`: vendor OUT request type `0x40`; exact written length is required.
- `interruptPend()`: waits on interrupt endpoint `0x83` with a 3-byte buffer; throws on nonzero libusb status. It does not inspect the payload.
- Firmware is bulk-written to endpoint `0x02`. Chunk size is 16,384 bytes for Game Capture HD and 32,768 bytes for Game Capture HD New. The source says buffers must be divisible by 16 and HD New needs larger chunks.

### Mailbox transport (do not confuse port IDs with target register addresses)

**Original Game Capture HD**
- Mailbox request `0xBD`; `wIndex = port << 8`; actual payload length, no padding.
- After a mailbox write, write saved enable-state shadow to `0x0900:0x0014` and poll `MAIL_REQUEST_READY = 0x0900:0x001C` until bit 0 is set.
- Header comment says it is unknown whether both ready polls are necessary; implementation performs them.

**Game Capture HD New**
- Mailbox write data goes to `0x0800:0x00C0` with request `0xBC`; odd payload lengths are padded with one trailing zero for this transfer.
- Then write four-byte mailbox request configuration via `0xB9`, index zero: `09 00 (port<<1) original_length`.
- Wait on interrupt endpoint `0x83`; read `0x0800:0x0016` (source says expected bytes `09 00`); poll `0x0900:0x001C`; read two dummy bytes from `0x0800:0x23BE`; write saved enable state to `0x0900:0x0014`; poll ready again.
- Read config is `09 01 (port<<1) requested_length`. After interrupt/status/ready, read `requested_length + 2 + (requested_length & 1)` bytes from `0x0800:0x23BE), remove the two leading bytes and any odd-length pad.
- The source says the two leading read bytes may be status and the dummy write-read bytes may contain error status, but their meanings are unknown. The interpretation of interrupt status `09 00` as an interrupt-status/clear-on-read register is speculation.
- `MAIL_REQUEST_READY` bit 0 is treated as busy/ready. Header says bits 8–9 indicate cable type (`00=HDMI`, `10=Component`, `11=Composite`); bits 10–11 are used as cable-presence bits.

### Helper commands on mailbox ports

- `readDevice0x9DCD(index)`: writes `9D CD index` to port `0x33`, reads one byte. Source says target/meaning is unknown.
- `pollOn0x9989ED()`: repeatedly writes `99 89 ED` to port `0x33` and reads one byte until bit 6 is set; source identifies bit 6 as poll-complete.
- `readFrom0x9989EC(count)`: repeats `99 89 EC` writes and one-byte reads; helper discards results, though some call sites comment expected byte arrays.
- These raw mailbox commands are not interchangeable with the trace's register writes to targets `0x88`, `0x98`, or `0x9C`.

## 2. SCMD: command meaning and required confirmation

IDs documented in source:

| Command | ID | Documented meaning |
|---|---:|---|
| `SCMD_IDLE` | `0x01` | Enter idle; readback becomes idle + STOP (`0x11`). |
| `SCMD_RESET` | `0x02` | Reset firmware state; mode 0 and mode 1 have different expected states. The header still describes the underlying reset interpretation as probable. |
| `SCMD_INIT` | `0x04` | Initialize/leave idle. Mode `0x00` loads encode firmware and generates an interrupt; mode `0xA0` skips that firmware load and interrupt. |
| `SCMD_STATE_CHANGE` | `0x05` | Set stream state: data 1=STOP, 2=START, 4=NULL. NULL makes the encoded output an empty stream. |

For `scmd(command, mode, data)`, Game Capture HD sends six bytes `00 00 command mode data_hi data_lo` using vendor OUT request `0xB8`; HD New sends four bytes `command mode data_hi data_lo`.

On HD New, certain SCMDs additionally wait for interrupt endpoint `0x83` and poll `0x0800:0x0014` until the high byte matches the command ID. This is command-receipt/readback confirmation, not necessarily completed state transition. Source has a TODO to add a timeout.

State transition completion:
- Read `SCMD_STATE_READBACK_REGISTER = 0x0800:0x2008`, mask to `0x1F`.
- Poll `SCMD_STATE_CHANGE_COMPLETE = 0x0900:0x0074` bit `0x04`.
- Read state twice; write `0x0004` to the completion register (sticky-bit reset/acknowledgement is believed but unconfirmed); write `0x0000` to `0x0900:0x01B0`.
- Source says the purpose of the `0x01B0` read/write and second state read is unknown. The completion bit write's exact effect is not confirmed.

Source-documented readback states: `0x00` uninitialized; `0x10` idle firmware loaded before IDLE; `0x11` idle; `0x01` initialized/stopped; `0x02` started; `0x04` null/empty stream; `0x12` special shutdown/reset state (deeper meaning is theory).

This resolves the command-ID interpretation of the trace payloads:
- `B8 04 A0 00 00` → INIT with mode `A0` (skip encode firmware load/interrupt).
- `B8 05 00 00 02` → STATE_CHANGE to START.
- `B8 05 00 00 04` → STATE_CHANGE to NULL.
- `B8 05 00 00 01` → STATE_CHANGE to STOP.

**Confirmation answer:** for state changes, expected confirmation is not merely an ACK to the `0xB8` transfer or matching SCMD byte. The source waits for completion bit 2 and checks the state readback; HD New also has its interrupt/readback command-receipt step.

## 3. Enable/state registers and uncertain meanings

- `ENABLE_REGISTER = 0x0900:0x0018`; `MAIL_SEND_ENABLE_REGISTER_STATE = 0x0900:0x0014`. `doEnable(setMask,valueMask)` validates the mask, updates shadow/actual values, writes both, then reads both back.
- Source header bit map: bit 1 `EB_FIRMWARE_PROCESSOR` (believed to enable the firmware processor); bit 2 `EB_ANALOG_INPUT` (set for composite/component, clear for HDMI); bit 3 `EB_ENCODER_ENABLE`; bit 4 `EB_ENCODER_TRIGGER` (purpose unknown; likely momentary trigger/reset is only a guess); bit 8 `EB_COMPOSITE_MUX` (comment says only composite); bit 9 `EB_ANALOG_MUX` (comment says composite/component).
- Header hypothesizes clearing the processor bit shuts down an internal CPU and resets shadow bits. HD New has extra shadow bits `0xD080` whose meaning is unknown. These are source hypotheses, not confirmed hardware facts.
- `enableAnalogInput()` changes only the analog-vs-HDMI enable bit; it is not the complete input recipe.
- `clearEnableState()` clears encoder-enable, encoder-trigger, composite-mux and analog-mux bits; sends `AB A9 0F A4 55` on port `0x33`; shuts off the processor; then repeats the mailbox probe until the magic changes from `0x27F97B` to `0x334455`. Source says the processor-state meaning is not understood.

## 4. Bring-up path and source-annotated expected replies

`configureDevice()` does considerably more than select an input:
1. Read/report hardware version; write `BANKSEL=0` (meaning unknown).
2. Read saved enable-state and current SCMD state. HD New handles the state-read interrupt side-effect separately.
3. If uninitialized, bulk-load idle firmware, write `0x0004` to `0x0900:0x0070` (purpose unknown), and read two repeated groups at `0x0000:0x0010–0x001E`. Otherwise issue confirmed RESET mode 0. Then issue confirmed IDLE.
4. Probe port `0x33` with `AB A9 0F A4 55`; use returned 24-bit magic and cable status to choose input. Source reports `0x334455` and `0x27F97B`; HDMI is fallback if no signal is found.
5. Initialize transcoder defaults; send INIT mode 0; bulk-load encode firmware; re-read firmware words.
6. Perform enable-bit changes, port `0x33`/`0x44`/`0x4C`/`0x4E` mailbox setup and readback probes; pulse encoder trigger; run shared decoder/transcoder blocks.
7. Enable encoder, poll `0x27F97B`, perform more handshakes, pulse trigger and wait for `F7`.
8. Run input-specific configuration, merge detected and explicit settings, configure transcoder, send INIT mode `A0`, request START and wait for completion.

Source comments annotate these expected firmware words:
- `0x0000:0x0010` → `20 13`
- `0x0000:0x0012` → `12 10`
- `0x0000:0x0014` → `18 80`
- `0x0000:0x0016` → `20 30`
- repeated at `0x0018`, `0x001A`, `0x001C`, `0x001E` with the same pairs. After encode firmware load the first four are annotated with those same pairs.

Other explicit source expectations include:
- port `0x33`, `AB A9 0F A4 55`: `33 44 55` in one phase; `27 F9 7B` in another.
- `29 89 5B` → `91`; `DD CE 3F B2` → `DA 67`; `43 23 84` is polled until `F7`; `89 89 FB` → `6E`.
- `0xB9` mailbox configure writes on HD New are always four bytes; status read expectation is `09 00).
- Numerous further expected replies (including the unexplained 56-byte read sequence) are written inline at the call sites in the source links above. They are source expectations, not necessarily independently verified trace results.

## 5. Per-input source facts

### HDMI
- Input-selector candidate is port `0x4E`, bytes `51 45`, corroborating trace `0x51=45`.
- Shared HDMI/component prefix includes `00 CC`, `AB 4C`, `00 CE`, `1B 33`, `00 CC`, `B7 CE`, `B8 DC`, `B8 CC`, `00 CE`, `07 38`, `07 C8`, `00 CC`, `51 45`, `B7 CC`, interspersed with reads from `0x9DCD`.
- Detection averages reads of `0x66/0x65` and `0x64/0x63`; reads `0x34` after bank write `00 CE` and uses bit 2 as RGB flag.
- Source timing heuristics: `0x6463≈0xB6D7` → 1080p; `≈0xB081` → 1080i; `≈0xB05C` → 720p; `≈0xB0C1` → midpoint for 480p/576p with tolerance. The source notes 480p/576p reference values and a slight NTSC-region bias. These are heuristic thresholds.
- Source calls a resolution-dependent setting a “mystery setup difference” and suggests reading `0x95` instead of `0x97`; unresolved.

### Component
- Selector candidate is port `0x4E`, bytes `51 ED`; source explicitly comments “Changed from HDMI.”
- After the shared prefix, component-specific writes include `24 0C`, `21 CD`, `91 C8`, `0E 8C`, `11 EC`, `71 4C`, `04 CC`.
- Source says `0x04` should differ for non-1080 captures, but the code does not know where/how to detect that and leaves `0xCC` for all modes.
- Signal detection averages `0x66/0x65` and `0x68/0x67`. Source only speculates register `0x60` may encode interlace; it lists observed values for some modes. Separate register tables are chosen by detected/passed mode; comments describe some as assumptions for 1080i60/1080p30.

### Composite
- Source writes `89 89 FA` to port `0x33`, reads one byte, and uses the low nibble: 6 → NTSC/60.0; 7 → PAL/50.0. Default scan mode is interlaced.
- If autodetection throws a runtime error, source runs a particular series of `0x9DCD` reads and `0x4E` writes; comment says it prevents lockup but admits the reason is unknown. Preserve it for a faithful initial port.
- NTSC path: `0x07=8A`, `0x08=9B`, `0x09=7A`, `0x28=88`, poll `89 89 FD` expecting `6E`, then `0x06=08`. PAL path: poll `89 89 F1` expecting `63`, write `0x4C:0x04=95`.
- Runs `configureSetupSubblock` and `configureCommonBlockB1(false)`, then status/bit-setting around `0x4C:0x0C/0x0D/0x0E). Source says `0x0D=C8` is missing in some captures but usually appears later and may be a bank switch.
- Runs common blocks A, setup, B1/B2/B3, C, repeats B3 (comment says probably unnecessary), configures transcoder and starts stream.

## 6. Separate transcoder protocol

- `slsi(address,data)`: vendor OUT `0xBC`, `wIndex=address`, two-byte big-endian data; address must be 2-byte aligned.
- `sparam(address,lsb,bits,data)`: vendor OUT `0xBC`, `wValue=1`, `wIndex=address & ~3`, eight-byte payload: big-endian 32-bit value then 32-bit mask. Only mask-selected bits change. The source describes alignment and how address bit 1 chooses upper/lower 16-bit half.
- `transcoderTableWrite` writes sequential 16-bit words and places an odd final byte in the high byte of the last word. It has no range validation because the source says writable block limits are unknown.
- Header contains named bitfields for video format/size/bitrate/H.264 metadata, transport stream output, audio/AAC fields and mirrored `_bl` fields. Source says these were taken from script files because no data sheet was available.
- The SoC/transcoder register bursts seen in traces are only one part of configuration; the input-decoder/mailbox configuration and transcoder settings are separate paths.

## 7. Original developer uncertainty — preserve as uncertainty

- `BANKSEL` meaning is unknown.
- `0x0900:0x0070=0x0004` during bring-up has “no idea” comment.
- Duplicated early reads at `0x0000:0x0010–0x001E` are not understood; the author suspects two identical banks.
- `0x334455`, `0x27F97B`, `0x78E045`, and reported alternate `0x7BEC47` are operational magic values; full meanings are not known. The code masks late values because user reports differed.
- Two `0x44` write/read sequences are labelled “Potential subroutine”; this is not proof they can be omitted or reordered.
- The `0x08/0x09` setting choice is explicitly an “educated guess”; old configure scripts did not always match captures.
- Long port `0x33` sequence `AB 92 3E B4 ...` with 56-byte reads is explicitly “not understood at all.”
- Purpose of no-op `doEnable(0,0)` in HDMI/component paths is unknown.
- HDMI's resolution-dependent setting is a mystery; component's non-1080 setting cannot currently be detected; component register `0x60` interlace interpretation is speculative.
- Composite exception recovery is said to prevent lockup, but the source author does not know why it works.
- Repeating `configureCommonBlockB3()` is suspected unnecessary, not proven.
- Common-block `mysteryParameter` differences are not understood; the author is unsure whether component and HDMI should share them.
- In `commands.cpp`, purpose of `0x0900:0x01B0`, second state read, and exact sticky-bit acknowledgement semantics are unknown; the HD New SCMD readback loop has a TODO for timeout.
- Enable-bit roles and the shadow-register/internal-CPU theory are partly conjectural. HD New shadow bits `0xD080` are unexplained. Interrupt-status register interpretation is speculative.
- Firmware version text/terminator/device-generation interpretation is phrased as “appears” / “believed.”
- Game Capture HD60 and HD60S are explicitly marked unsupported.

## 8. Reconciliation with trace findings

1. **SCMD semantics are documented in source**, not unknown at the command-ID level. The trace by itself did not prove the meaning, but source comments/code define command IDs, mode behavior, data values, expected states and completion polling.
2. HDMI `0x51=45` and component `0x51=ED` are corroborated by source routines using mailbox port `0x4E`. Do not confuse the mailbox port with trace target address `0x9C`: they are different abstraction layers.
3. Composite `0x44` vs trace `0x9C:0x51=EC` remains unresolved, and may not be the same address space/register. Preserve both raw observations.
4. HDMI/component average signal measurements and classify timing before applying mode-specific settings; composite detects NTSC/PAL from a status nibble. Keep the trace's `0x98` sequence categorized as signal/format-dependent, not a universal input-selection recipe.
5. Source is the authority for USB setup fields and mailbox padding; traces show actual ordering/timing and state-dependent traffic. Expected-response comments in source should not be misreported as measured trace results.

## 9. Remaining work before calling the command catalogue complete

- Generate a literal call-site ledger of every `mailWrite`, `mailRead`, `read_config`, `write_config`, `slsi`, `sparam`, `transcoderTableWrite`, and `scmd` across every runtime path, preserving order, conditionals, loops and expected-response comments.
- Audit the rest of `src/` for other command-producing functions and compare against this branch.
- Cross-check every literal command with raw USB captures; label expected bytes from source separately from observed bytes in traces.
- Keep original HD and HD New transport paths separate.
- Do not call the overall command catalogue complete until this remaining work and the source/trace comparison are committed.
