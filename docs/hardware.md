# Hardware and shared lifecycle reference

## Common operations

These are transport and register-access primitives used by the sequences below. Addresses are shown as `bank:register` where the userspace code uses that notation.

### USB and mailbox operations

| Operation | Where / direction | Bytes or fields | Notes / description |
|---|---|---|---|
| Vendor control read | USB control IN, request type `0xC0` | Request, value, index and length are operation-specific | Reads the requested number of bytes from the device. The target register/address is encoded in the request fields; short reads are errors. |
| Vendor control write | USB control OUT, request type `0x40` | Request, value, index and payload are operation-specific | Writes the supplied bytes to the device. The target is encoded in the request fields. |
| Firmware download | Bulk OUT endpoint `0x02` | Firmware image in chunks: 16,384 bytes on Original HD; 32,768 bytes on HD-New | Transfers firmware to the device. The image and timing depend on the initialization branch. |
| SCMD | USB control OUT, request `0xB8` | Original HD: `00 00 CMD MODE DATA_H DATA_L` (6 bytes); HD-New: `CMD MODE DATA_H DATA_L` (4 bytes) | Requests a device command/state transition. Concrete SCMD command payloads and their state-confirmation transfers are listed in the sequence table. |
| Original-HD mailbox write | USB control OUT, request `0xBD`; `wIndex = PORT << 8` | Unpadded mailbox command bytes | Writes a command to the selected mailbox port. Userspace restores the enable-state shadow and polls mailbox readiness before continuing. |
| HD-New mailbox write | First request `0xBC` writes payload to `0800:00C0`; then request `0xB9` | Commit payload: `09 00 (PORT << 1) LENGTH`; odd-length data is padded with `00` | Writes the mailbox payload, then commits it to the selected port. The implementation waits for interrupt endpoint `0x83`, checks readiness, restores the enable-state shadow and polls before the next command. |
| HD-New mailbox read | USB control OUT request `0xB9`, then control IN from `0800:23BE` | Request payload `09 01 (PORT << 1) LENGTH`; read `LENGTH + 2 + (LENGTH & 1)` bytes | Reads from a mailbox port. The response has two leading status bytes and may include a pad byte for odd-length reads; the helper strips these. |
| Interrupt wait | Interrupt IN endpoint `0x83` | 3-byte buffer | Waits for the device notification. The source checks transfer status; it does not generally validate the interrupt payload. |
| Transcoder full-word write | USB control OUT request `0xBC`; `wIndex = ADDRESS` | One 16-bit big-endian value | Writes a 16-bit value to an aligned transcoder register address. |
| Transcoder masked write | USB control OUT request `0xBC`; `wValue = 1`; `wIndex = ADDRESS & ~3` | `VALUE32_BE MASK32_BE` (8 bytes) | Updates selected bits in a 32-bit transcoder register. See [commands.md](commands.md). |

### SCMD commands

| SCMD payload (Original HD / HD-New) | Command / source interpretation | Expected state / notes |
|---|---|---|
| Original HD `00 00 01 MODE DATA_H DATA_L`; HD-New `01 MODE DATA_H DATA_L` | IDLE | Expected state `0x11`. |
| Original HD `00 00 02 MODE DATA_H DATA_L`; HD-New `02 MODE DATA_H DATA_L` | RESET | Used when the device is already initialized; the mode-dependent readback is only partly understood. |
| Original HD `00 00 04 00 00 00`; HD-New `04 00 00 00` | INIT before encode-firmware download | Verify firmware/version words afterward. |
| Original HD `00 00 04 A0 00 00`; HD-New `04 A0 00 00` | INIT mode `A0` | Re-enters initialized/stopped state before capture start, without reloading encode firmware. |
| Original HD `00 00 05 00 00 01`; HD-New `05 00 00 01` | State-change command | Seen during transition; exact standalone meaning needs validation. |
| Original HD `00 00 05 00 00 02`; HD-New `05 00 00 02` | START | Source expects state `0x02`; observed with bulk video output. |
| Original HD `00 00 05 00 00 04`; HD-New `05 00 00 04` | NULL/empty stream | Source expects state `0x04`; exact transition use needs validation. |

### Shared state words observed

| Where / bytes | Notes / description |
|---|---|
| Read `0900:0018` → `02 0A` | Observed HDMI route/state value. |
| Read `0900:0018` → `02 0E` | Observed component route/state value. |
| Read `0900:0018` → `03 0E` | Observed composite/S-video route/state value. |
| Read `0900:0014` | Enable-state shadow used during handshake; values vary by session. `D3 9E` was seen in the late part of the no-signal initialization trace. |

## Sequences

The tables below follow the call order in the userspace implementation, principally `src/gchd/configure.cpp`, `src/gchd/commands.cpp`, and `src/gchd/transcoder.cpp`. They describe implementation order; unknown command meanings are explicitly left unresolved.

### SCMD state-transition sequence

This table spells out the transfers and register accesses performed when userspace confirms an SCMD state transition. The command payloads below are concrete examples from the initialization and capture paths; all register addresses and byte values needed to follow the handshake are included here.

| Order | Where / direction | Command / bytes / condition | Notes / description |
|---:|---|---|---|
| 1 | USB control OUT, request `0xB8` | **IDLE:** Original HD `00 00 01 00 00 00`; HD-New `01 00 00 00` | Sent during common initialization. The four leading/trailing fields are command `01`, mode `00`, data `00 00`; Original HD prepends two zero bytes. |
| 1a | USB control OUT, request `0xB8` | **RESET (already initialized):** Original HD `00 00 02 00 00 00`; HD-New `02 00 00 00` | Alternative to the idle-firmware download branch. Mode `00`, data `00 00`; expected state is `0x10`. |
| 1b | USB control OUT, request `0xB8` | **INIT before encode-firmware download:** Original HD `00 00 04 00 00 00`; HD-New `04 00 00 00` | Requests INIT mode `00`; userspace then downloads encode firmware. |
| 1c | USB control OUT, request `0xB8` | **INIT before capture:** Original HD `00 00 04 A0 00 00`; HD-New `04 A0 00 00` | Requests INIT mode `A0`; used before START without reloading encode firmware. |
| 1d | USB control OUT, request `0xB8` | **START:** Original HD `00 00 05 00 00 02`; HD-New `05 00 00 02` | Requests state-change command `05`, mode `00`, data `00 02`; expected state is `0x02`. |
| 2 | HD-New only, interrupt IN endpoint `0x83` | Wait for one interrupt transfer into a 3-byte buffer | Used by the HD-New SCMD receipt path for IDLE, INIT and state-change commands when mode bits do not disable the interrupt. |
| 3 | HD-New only, USB control IN from `0800:0014` | Read 2 bytes repeatedly until the high byte equals the command byte (`01`, `04` or `05`) | Confirms that the command was received. It does not by itself confirm that the state transition completed. |
| 4 | USB control IN from `0900:0074` | Read 2 bytes repeatedly until `value & 0x0004 != 0` | Polls the state-change-complete flag. Original HD uses this in `completeStateChange()`; HD-New uses the interrupt/readback path where applicable. |
| 5 | USB control IN from `0800:2008` | Read 2 bytes; compute `value & 0x001F` | Reads the current SCMD state. During completion, userspace reads this register twice and uses the second value masked with `0x1F`. |
| 6 | USB control IN, request `0xBC`, `wValue=0x0900`, `wIndex=0x01B0` | Read 2 bytes; discard the value | An additional read performed inside the Original-HD completion loop. Its purpose is unknown. |
| 7 | USB control OUT, request `0xBC`, `wValue=0x0900`, `wIndex=0x0074` | Write `00 04` | Clears/acknowledges the sticky completion flag; the exact hardware semantics are not fully established. |
| 8 | USB control OUT, request `0xBC`, `wValue=0x0900`, `wIndex=0x01B0` | Write `00 00` | Cleanup write after completion. Its purpose remains unknown. |

**Expected state after confirmation:** IDLE → `0x11`; RESET with mode `00` → `0x10`; START (`05 00 00 02`) → `0x02`. INIT is issued through `scmd()` and followed by the firmware/version steps; it is not confirmed through the same `stateConfirmedScmd()` helper as IDLE, RESET and START.


### Common initialization sequence

| Order | Command / bytes | Notes / description |
|---:|---|---|
| 1 | Read version registers: HD-New probes `0800:0094` and `0800:0098`; if both are zero, read Original-HD version registers `0800:0010`, `0014`, and `0018` | `readVersion()` selects the device-generation-specific version locations. |
| 2 | Write `0900:0000 = 00 00` | `BANKSEL`; purpose unknown. |
| 3 | Read `0900:0014` | Save the current enable-state shadow before changing hardware state. |
| 4 | Read `0800:2008` and mask to `0x1F` | Determine the current SCMD state. Original HD uses `completeStateChange(0, 0)`; HD-New handles the interrupt path separately if the state is `0x00`. |
| 5A | **Only if state is `0x00`:** bulk OUT endpoint `0x02`, load idle firmware | Chunk size is 16,384 bytes on Original HD and 32,768 bytes on HD-New. The source does not add a separate state-confirmation step immediately after the transfer. |
| 5B | **Otherwise, if already initialized:** SCMD RESET, mode `00`, data `0000` — Original HD payload `00 00 02 00 00 00`; HD-New payload `02 00 00 00` | `stateConfirmedScmd()` expects state `0x10`. Firmware is not reloaded in this branch. |
| 6A | Write `0900:0070 = 00 04` | Purpose unknown; executed after idle-firmware download. |
| 7A | Read `0900:0014`, then `0900:0018`; read `0000:0010`, `0012`, `0014`, `0016`, `0018`, `001A`, `001C`, `001E` (2 bytes each) | Reads after idle-firmware download. The source comments say the returned values appear to repeat across two banks; their purpose is unknown. |
| 8 | SCMD IDLE, mode `00`, data `0000` — Original HD `00 00 01 00 00 00`; HD-New `01 00 00 00` | State-confirmed; expected readback `0x11`. |
| 9 | Mailbox port `0x33`: write `AB A9 0F A4 55`, then read 3 bytes | Initial phase probe; source comment expects `33 44 55`. |
| 10 | Repeat port-`0x33` probe `AB A9 0F A4 55` + 3-byte read until phase value changes | While reply is `33 44 55`, source sends the saved enable-state shadow, configures the analog-input bit from selected input, and enables `EB_FIRMWARE_PROCESSOR`. The loop waits for `27 F9 7B`. |
| 11 | Configure analog-input enable bit; call `transcoderDefaultsInitialize()` | Runs once on the first `27 F9 7B` phase. The detailed transcoder writes are implementation-ordered; see the transcoder initialization code and the relevant transcoder tables. |
| 12 | SCMD INIT, mode `00`, data `0000` — Original HD `00 00 04 00 00 00`; HD-New `04 00 00 00` | **The userspace implementation sends INIT before downloading encode firmware.** On HD-New, the normal SCMD receipt/interrupt path is used. |
| 13 | Bulk OUT endpoint `0x02`, load encode firmware | Same generation-specific chunk sizes as the idle image. After download, source reads `0000:0010`, `0012`, `0014`, and `0016` (2 bytes each on the first three and a 2-byte read at `0016`). |
| 14 | Repeat phase probe `AB A9 0F A4 55` until `27 F9 7B` is observed again | The source allows a temporary return to `33 44 55` after encode-firmware load. |
| 15 | Up to five times: port `0x33` write `AB A9 0F A4 5B`, read 3 bytes, and refresh analog-input enable | Stop early when `(reply & 0xF8F0F0) == 0x78E040`; source comments note another observed value also matches this mask. |
| 16 | Enable `EB_ENCODER_ENABLE`; repeat `AB A9 0F A4 55` + 3-byte read until reply is `27 F9 7B` | The source notes that enabling the encoder can temporarily return the phase to `33 44 55`. |
| 17 | Port `0x33`: write `28 28`; write `29 89 5B`, read 1 byte; write `DD CE 3F B2`, read 2 bytes | Expected replies are `91` and `DA 67`. |
| 18 | Enable `EB_ENCODER_TRIGGER`; poll port `0x33` with write `43 23 84` and 1-byte reads until `F7`; clear `EB_ENCODER_TRIGGER` | Trigger is asserted only for the polling phase. |
| 19 | Port `0x33`: write `89 89 FB`, read 1 byte | Source expects `6E`. |
| 20 | Continue ordered mailbox setup on ports `0x44`, `0x33`, `0x4E`, `0x4C` (including the device-generation-specific block) | This is a long, order-sensitive sequence, not a single generic “configure hardware” operation. Preserve the call-site order in `configure.cpp`; several command meanings remain unknown. |
| 21 | Perform the port-`0x33` block-read series: write `AB 92 3E B4 xx xx`, then read 56 bytes for each entry | The source explicitly says this sequence and the returned data are not understood. It is part of userspace initialization and should not be omitted merely because its purpose is unknown. |
| 22 | Complete common setup and branch to the selected input configuration | Input-specific programming and signal/format detection are separate from this common initialization. See [hdmi.md](hdmi.md), [component.md](component.md), [composite.md](composite.md), and [s-video.md](s-video.md). |

**Branching rules:** steps 5A–7A run only when the device reports state `0x00`; step 5B is the already-initialized alternative. Steps 11–15 are part of the first-time initialization path. The input source can be autodetected or forced by userspace settings. These are the userspace call-order steps; values labelled unknown remain unknown.

### Capture-start sequence

| Order | Command / bytes | Notes / description |
|---:|---|---|
| 1 | Finish selected input setup and signal/format detection; apply transcoder setup/final configuration | Input selector, decoder tables, and output parameters depend on input and detected format. |
| 2 | Queue bulk IN URBs on endpoints `0x81` and `0x84` where used by the capture path | Queue before START as required by the implementation; queueing alone does not prove capture is active. |
| 3 | SCMD INIT, mode `A0`, data `0000` — Original HD `00 00 04 A0 00 00`; HD-New `04 A0 00 00` | Re-enters initialized/stopped state without reloading encode firmware. |
| 4 | Read `0800:2008` | In the observed analog 240p trace, the readback was `00 01`; this is an observation, not a universal expected value. |
| 5 | SCMD START, mode `00`, data `0002` — Original HD `00 00 05 00 00 02`; HD-New `05 00 00 02` | State-change request; source expects state `0x02`. Follow completion confirmation. |
| 6 | Observe bulk IN completions on `0x81` / `0x84` | In the referenced trace, transfers were 65,536 bytes. Validate payload contents; USB completion alone is not proof of valid video. |

### Shared capture-start sequence observed on analog 240p

| Order | Command / operation | Confirmation |
|---:|---|---|
| 1 | Apply the ordered SoC/transcoder register burst after input lock; queue bulk IN URBs on endpoints `0x81` and `0x84` | Queueing is preparation, not proof of active capture. |
| 2 | SCMD `04 A0 00 00` | Follow command-receipt and ready/readback path. |
| 3 | Read `0800:2008` | Observed ready value `00 01` in these traces. |
| 4 | SCMD `05 00 00 02` | Confirm state/completion and data flow. |
| 5 | Observe bulk IN completions on `0x81` / `0x84` | Captured transfers were 65,536 bytes; verify payload is actual video. |

### Idle / keep-alive loop (post-input-setup, no signal)

**Observed context:** HD-New after HDMI input setup with no signal, continuing until the host begins the component-switch prefix. It is not yet confirmed whether the same loop runs on component, composite or S-video, or while a signal is locked.

**Repeated loop body:** each iteration uses the HD-New mailbox transport and its standard commit / interrupt / restore-shadow / poll-ready handshake.

| Order | Where / direction | Command / bytes | Notes / description |
|---:|---|---|---|
| 1 | Mailbox write, port `0x9C` | `AB 4C` | Writes value `4C` to register `0x9C:AB`. |
| 2 | Mailbox read helper, index `0x6A` | `9D CD 6A 00` | Reads helper/index `0x6A`; result is discarded. |
| 3 | Mailbox sequence-read/poll helper | `99 89 F5 00` | Performs a sequence-read/poll operation; result is discarded. |
| 4 | Mailbox read helper, index `0x94` | `9D CD 94 00` | Reads helper/index `0x94`; result is discarded. |

The four-command cycle repeats many times (dozens of iterations in the observed trace). The enable-state shadow remained `D3 9E` throughout that loop.

**Open questions**
- Does the identical loop run when the current input is component, composite or S-video?
- Does the loop stop, change, or continue once a signal is detected or locked?
- Are the read helpers strictly required, or can they be omitted?

**State-word timing note:** when switching from HDMI to component in the no-signal trace, the write `0900:0018 = 02 0E` was observed before the final mailbox selector `51 ED`. A transient re-write of the previous HDMI value `02 0A` immediately preceded the component value.

See [commands.md](commands.md) for generic mailbox commands, unknown registers, transcoder/pipeline operations, and unresolved status values.
