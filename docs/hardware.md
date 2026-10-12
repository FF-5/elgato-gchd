# Hardware and shared lifecycle reference

## USB operations

| Operation | USB fields / payload | Sequence and expected response |
|---|---|---|
| Vendor control read | Request type `0xC0`; request/value/index/length depend on operation | Read exactly the length requested by the source; short reads are errors. |
| Vendor control write | Request type `0x40`; request/value/index/payload depend on operation | Write exactly the payload length expected by the source. |
| Firmware download | Bulk OUT endpoint `0x02`; chunks of 16,384 bytes (Original HD) or 32,768 bytes (HD New) | Firmware image and generation-specific startup order must match the device path. |
| SCMD | Vendor OUT request `0xB8`; Original HD payload `00 00 CMD MODE DATA_H DATA_L` (6 bytes), HD New payload `CMD MODE DATA_H DATA_L` (4 bytes) | Follow command-receipt, completion, state readback, and acknowledgement steps below. |
| Original-HD mailbox write | Vendor OUT request `0xBD`; `wIndex=PORT<<8`; unpadded command bytes | Write command \u2192 restore enable shadow \u2192 poll mailbox ready. |
| HD-New mailbox write | Write payload to `0800:00C0` via request `0xBC` (odd length padded with `00`); then request `0xB9` payload `09 00 (PORT<<1) LENGTH` | Interrupt `0x83` \u2192 status/readiness reads \u2192 restore enable shadow \u2192 poll ready before next command. |
| HD-New mailbox read | Request `0xB9` payload `09 01 (PORT<<1) LENGTH`; read `LENGTH+2+(LENGTH&1)` bytes from `0800:23BE` | Strip two leading status bytes and odd-length pad. |
| Interrupt wait | Interrupt IN endpoint `0x83`, 3-byte buffer | Source checks transfer status; interrupt payload is not generally validated. |
| Transcoder full-word write | Vendor OUT request `0xBC`; `wIndex=ADDRESS`; 16-bit big-endian data | Writes one aligned 16-bit transcoder register. |
| Transcoder masked write | Vendor OUT `0xBC`; `wValue=1`; `wIndex=ADDRESS & ~3`; payload `VALUE32_BE MASK32_BE` (8 bytes) | Masked 32-bit update; see [commands.md](commands.md). |

## Sequences

The tables below follow the call order in the userspace implementation, principally `src/gchd/configure.cpp`, `src/gchd/commands.cpp`, and `src/gchd/transcoder.cpp`. They describe the implementation's order, not a claim that every operation is fully understood. Conditional branches are called out explicitly; do not flatten them into one unconditional script.

### Common initialization sequence

| Order | Command / bytes | Notes / description |
|---:|---|---|
| 1 | Read version registers: HD-New probes `0800:0094` and `0800:0098`; if both are zero, read Original-HD version registers `0800:0010`, `0014`, and `0018` | `readVersion()` selects the device-generation-specific version locations. |
| 2 | Write `0900:0000 = 00 00` | `BANKSEL`; purpose unknown. |
| 3 | Read `0900:0014` | Save the current enable-state shadow before changing hardware state. |
| 4 | Read `0800:2008` and mask to `0x1F` | Determine the current SCMD state. Original HD uses `completeStateChange(0, 0)`; HD-New handles the interrupt path separately if the state is `0x00`. |
| 5A | **Only if state is `0x00`:** bulk OUT endpoint `0x02`, load idle firmware | Chunk size is 16,384 bytes on Original HD and 32,768 bytes on HD-New. The source does not add a separate state-confirmation step immediately after the transfer. |
| 6A | Write `0900:0070 = 00 04` | Purpose unknown; executed after idle-firmware download. |
| 7A | Read `0900:0014`, then `0900:0018`; read `0000:0010`, `0012`, `0014`, `0016`, `0018`, `001A`, `001C`, `001E` (2 bytes each) | Reads after idle-firmware download. The source comments say the returned values appear to repeat across two banks; their purpose is unknown. |
| 5B | **Otherwise, if already initialized:** SCMD RESET, mode `00`, data `0000` — Original HD payload `00 00 02 00 00 00`; HD-New payload `02 00 00 00` | `stateConfirmedScmd()` expects state `0x10`. Firmware is not reloaded in this branch. |
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

## SCMD confirmation sequence

1. Send the generation-specific `0xB8` payload.
2. On HD New where required, wait for interrupt endpoint `0x83`; read `0800:0014` until its high byte equals CMD. This is command receipt, not proof that a state transition completed.
3. For state-changing commands, poll `0900:0074` until bit `0x04` is set.
4. Read `0800:2008` twice; compare `value & 0x1F` with the expected state.
5. Write `00 04` to `0900:0074` (source believes this acknowledges completion; exact effect not proven), then write `00 00` to `0900:01B0` (purpose unknown).

| SCMD payload | Source interpretation | Expected state / note |
|---|---|---|
| `01 MODE ...` | IDLE | `0x11` |
| `02 00 ...` / `02 01 ...` | RESET | Mode-dependent readback; deeper interpretation partly unconfirmed. |
| `04 00 00 00` | INIT and encode firmware load | Verify firmware/version words. |
| `04 A0 00 00` | INIT without encode firmware reload | Used before capture start. |
| `05 00 00 01` | State change | Seen during transition; exact standalone meaning needs validation. |
| `05 00 00 02` | START | Source expects state `0x02`; captured with bulk video output. |
| `05 00 00 04` | NULL/empty stream | Source expects state `0x04`; transition use needs validation. |

## Shared capture-start sequence observed on analog 240p

| Order | Command / operation | Confirmation |
|---:|---|---|
| 1 | Apply the ordered SoC/transcoder register burst after input lock; queue bulk IN URBs on endpoints `0x81` and `0x84` | Queueing is preparation, not proof of active capture. |
| 2 | SCMD `04 A0 00 00` | Follow command-receipt and ready/readback path. |
| 3 | Read `0800:2008` | Observed ready value `00 01` in these traces. |
| 4 | SCMD `05 00 00 02` | Confirm state/completion and data flow. |
| 5 | Observe bulk IN completions on `0x81` / `0x84` | Captured transfers were 65,536 bytes; verify payload is actual video. |

## Idle / keep-alive loop (post-input-setup, no signal)

**Observed:** HD-New no-signal HDMI setup followed by a component switch.  
**When observed:** After HDMI selector + HDMI front-end / parameter-table programming have completed, while the device remains configured for HDMI and no signal is present. The loop continues until the host begins the component-switch prefix.

**Status:** Observed on a no-signal HDMI configuration. It is **not yet known** whether the same loop appears when the device is configured for component, composite or S-video, nor whether it appears when a signal is locked. Treat as shared/generic only after confirmation on other inputs; until then document it as \u201cobserved after HDMI input setup with no signal\u201d.

### Loop body (repeats)

Each iteration is a normal HD-New mailbox write to `0800:00C0` followed by the standard commit / interrupt / restore-shadow / poll-ready handshake.

| Order | Mailbox payload | Notes |
|---:|---|---|
| 1 | `AB 4C` | Write `0x9C:AB = 4C` |
| 2 | `9D CD 6A 00` | Read helper (`9D CD` index `6A`); result discarded |
| 3 | `99 89 F5 00` | Sequence-read / poll helper; result discarded |
| 4 | `9D CD 94 00` | Read helper (`9D CD` index `94`); result discarded |

The four-command cycle repeats many times (dozens of iterations in this trace). Enable-state shadow remains `D3 9E` for the duration of the observed loop.

### Open questions

- Does the identical loop run when the current input is component / composite / S-video?
- Does the loop stop, change, or continue once a signal is detected / locked?
- Are the read helpers strictly required, or can they be elided?

Cross-reference from input-specific documents; keep the authoritative description here so later captures can be compared in one place.

## Shared state words observed

| Target / bytes | Meaning |
|---|---|
| `0900:0018 = 02 0A` | HDMI route/state |
| `0900:0018 = 02 0E` | Component route/state |
| `0900:0018 = 03 0E` | Composite/S-video route/state |
| `0900:0014` | Enable-state shadow / handshake bytes; trace values vary by session (`D3 9E` in the late portion of the no-signal init trace) |

### State-word timing note from the no-signal switch trace

When switching from an HDMI configuration to component, the write of `0900:0018 = 02 0E` was observed **before** the final mailbox selector `51 ED`. A transient re-write of the previous HDMI value `02 0A` immediately preceded the component value.

See [commands.md](commands.md) for all generic mailbox commands, unknown registers, transcoder/pipeline operations, and unresolved status values.
