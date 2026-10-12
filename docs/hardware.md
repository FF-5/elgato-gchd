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

### Observed HD-New init (no signal)

**Observed:** cold initialization ending in HDMI setup, followed by a component switch. Chunk sizes and SCMD order match the lifecycle above.

| Phase | Observed traffic | Notes |
|---|---|---|
| Version / status reads | Vendor reads at `0800:0094`, `0800:0098`, `0800:009C`, `0800:2008`, `0900:0014`, `0900:0074` | Device-generation and current-state probes. |
| Idle firmware download | Bulk OUT `0x02`: ten \u00d7 32 768 bytes + final 4 016 bytes (**331 696 bytes total**) | HD-New chunk size. |
| Post-idle setup | Write `0900:0070 = 00 04` | Purpose still unknown (already listed in commands.md). |
| IDLE SCMD | `0xB8` payload `01 00 00 00` | Followed by standard confirmation (poll `0900:0074`, read `0800:2008`, ack `0900:0074 = 00 04`, write `0900:01B0 = 00 00`). |
| Phase probe | Mailbox `AB A9 0F A4 55` (twice in early region) | Matches source probe. |
| Enable-state progression | `0900:0018` written through `00 00` \u2192 `00 02` \u2192 later `00 0A` / `00 1A` | Progressive enable bits before input selection. |
| Encode firmware download | Bulk OUT `0x02`: 45 \u00d7 32 768 bytes + final 26 224 bytes (**1 500 784 bytes total**) | Second bulk image; occurs after IDLE and before INIT. |
| INIT SCMD (with encode load) | `0xB8` payload `04 00 00 00` | After encode firmware. |
| Later INIT (no reload) | `0xB8` payload `04 A0 00 00` | Observed after input-path programming had begun (around the HDMI setup region). |
| Input path | HDMI selected (`51 45`), front-end programmed, then idle/keep-alive loop; later component switch begins | See [hdmi.md](hdmi.md) and [component.md](component.md). |

This no-signal switch sequence does **not** contain a START SCMD or bulk video IN; it ends at component selector `51 ED`.

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
