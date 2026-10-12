# Hardware and shared lifecycle reference

## USB operations

| Operation | USB fields / payload | Sequence and expected response |
|---|---|---|
| Vendor control read | Request type `0xC0`; request/value/index/length depend on operation | Read exactly the length requested by the source; short reads are errors. |
| Vendor control write | Request type `0x40); request/value/index/payload depend on operation | Write exactly the payload length expected by the source. |
| Firmware download | Bulk OUT endpoint `0x02`; chunks of 16,384 bytes (Original HD) or 32,768 bytes (HD New) | Firmware image and generation-specific startup order must match the device path. |
| SCMD | Vendor OUT request `0xB8); Original HD payload `00 00 CMD MODE DATA_H DATA_L` (6 bytes), HD New payload `CMD MODE DATA_H DATA_L` (4 bytes) | Follow command-receipt, completion, state readback, and acknowledgement steps below. |
| Original-HD mailbox write | Vendor OUT request `0xBD`; `wIndex=PORT<<8`; unpadded command bytes | Write command → restore enable shadow → poll mailbox ready. |
| HD-New mailbox write | Write payload to `0800:00C0` via request `0xBC` (odd length padded with `00`); then request `0xB9` payload `09 00 (PORT<<1) LENGTH` | Interrupt `0x83` → status/readiness reads → restore enable shadow → poll ready before next command. |
| HD-New mailbox read | Request `0xB9` payload `09 01 (PORT<<1) LENGTH`; read `LENGTH+2+(LENGTH&1)` bytes from `0800:23BE` | Strip two leading status bytes and odd-length pad. |
| Interrupt wait | Interrupt IN endpoint `0x83`, 3-byte buffer | Source checks transfer status; interrupt payload is not generally validated. |
| Transcoder full-word write | Vendor OUT request `0xBC`; `wIndex=ADDRESS`; 16-bit big-endian data | Writes one aligned 16-bit transcoder register. |
| Transcoder masked write | Vendor OUT `0xBC`; `wValue=1`; `wIndex=ADDRESS & ~3`; payload `VALUE32_BE MASK32_BE` (8 bytes) | Masked 32-bit update; see [commands.md](commands.md). |

## Firmware and device initialization sequence

This is the lifecycle sequence recorded in the userspace command reference. Some conditional branches depend on current firmware/state; do not blindly run every step on every device.

| Order | Operation | Expected check / notes |
|---:|---|---|
| 1 | Read device version and current state | Select Original HD vs HD New transport and determine whether firmware/state is already initialized. Exact read payloads are implementation-specific. |
| 2A | If uninitialized: load idle firmware using bulk OUT endpoint `0x02` in device-generation-specific chunks | Firmware/version words are read back after download. Image names and exact version values are implementation-specific and must be taken from driver/source. |
| 2B | If already initialized: issue generation-appropriate RESET SCMD | Reset modes differ; verify state using SCMD confirmation sequence. |
| 3 | Issue IDLE SCMD | Source expects state readback `0x11`. |
| 4 | Probe device/firmware phase and cable/status bits | Source uses mailbox probes including `AB A9 0F A4 55`; full semantic meaning remains unknown. |
| 5 | Initialize transcoder defaults | Use the ordered register/table initialization in the implementation; table boundaries and some fields are not fully understood. |
| 6 | Issue INIT mode `00`, data `0000` (`04 00 00 00` on HD New) | Loads/initializes encode firmware and generates an interrupt in the source path; verify firmware/version readback. |
| 7 | Configure common mailbox/decoder hardware and required enable bits | Exact writes depend on device generation and selected input. See input-specific document. |
| 8 | Enable encoder and perform required handshake | Preserve enable shadow, ready polls, interrupt/status reads. |
| 9 | Configure input, detect signal/format, and program signal-dependent tables | Input selection is not the same as format lock. See HDMI/component/composite/S-video references. |
| 10 | Configure final transcoder output parameters | Dimensions, video/audio settings, and output bitfields depend on runtime configuration. |
| 11 | Issue INIT mode `A0`, data `0000` (`04 A0 00 00` on HD New) | Used without reloading encode firmware before capture start. |
| 12 | Queue bulk IN URBs and issue START SCMD (`05 00 00 02` on HD New) in the required order | Check device state/completion and verify actual bulk video payload; USB transfer success alone is insufficient. |

## SCMD confirmation sequence

1. Send the generation-specific `0xB8` payload.
2. On HD New where required, wait for interrupt endpoint `0x83`; read `0800:0014` until its high byte equals CMD. This is command receipt, not proof that a state transition completed.
3. For state-changing commands, poll `0900:0074` until bit `0x04` is set.
4. Read `0800:2008` twice; compare `value & 0x1F` with the expected state.
5. Write `00 04` to `0900:0074` (source believes this acknowledges completion; exact effect not proven), then write `00 00` to `0900:01B0` (purpose unknown).

| SCMD payload | Source interpretation | Expected state / note |
|---|---|---|
| `01 MODE ...` | IDLE | `0x11) |
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

## Shared state words observed

| Target / bytes | Meaning |
|---|---|
| `0900:0018 = 02 0A` | HDMI route/state |
| `0900:0018 = 02 0E` | Component route/state |
| `0900:0018 = 03 0E` | Composite/S-video route/state |
| `0900:0014` | Enable-state shadow / handshake bytes; trace values vary by session |

See [commands.md](commands.md) for all generic mailbox commands, unknown registers, transcoder/pipeline operations, and unresolved status values.
