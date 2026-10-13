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
| Read `0900:0014` | Enable-state shadow used during handshake; values vary by session. `D3 9E` has been observed in the late part of no-signal initialization. |

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
| 7 | USB control OUT, request `0xBC`, `wValue=0x0900`, `wIndex=0x01B0` | Write `00 00` | Cleanup write after completion. Its purpose remains unknown. |
| 8 | USB control OUT, request `0xBC`, `wValue=0x0900`, `wIndex=0x0074` | Write `00 04` | Clears/acknowledges the sticky completion flag; the exact hardware semantics are not fully established. |

**Expected state after confirmation:** IDLE → `0x11`; RESET with mode `00` → `0x10`; START (`05 00 00 02`) → `0x02`. INIT is issued through `scmd()` and followed by the firmware/version steps; it is not confirmed through the same `stateConfirmedScmd()` helper as IDLE, RESET and START.


### Initialization command comparison: observed transfers vs. userspace

The observed sequence begins at USB connect on HD-New with no signal present. The table below covers only the **common boot / init path** that finishes before any input selector is written. HDMI selection (`51 45`), the post-setup idle/keep-alive loop, and the later component switch (`51 ED`) are input-path material and are documented on the input pages, not here.

**Userspace column** follows the call order in `src/gchd/configure.cpp` / `commands.cpp` for the common initialization path (up to the branch into the selected input configuration). Unknown command meanings are left unresolved.

| Observed step | Hardware transfers | Userspace step | Userspace implementation |
|---:|---|---:|---|
| 1 | Vendor control IN: read `0800:0094` (4 bytes) → `56 31 33 30` (`"V130"`); read `0800:0098` (repeated) | 1 | Read version: HD-New probes `0800:0094` / `0800:0098`; fallback to Original-HD `0800:0010/14/18` if both zero |
| 2 | Vendor control OUT: write `0900:0000 = 00 00` | 2 | Write `0900:0000 = 00 00` |
| 3 | Vendor control IN: read `0900:0014` (enable-state shadow) | 3 | Read `0900:0014` (enable-state shadow) |
| 4 | Vendor control IN: read `0800:2008` → state `0x00` | 4 | Read `0800:2008` & `0x1F`. On HD-New, if state `0x00` also wait interrupt endpoint `0x83` and clear sticky bit on `0900:0074` |
| 5 | Bulk OUT endpoint `0x02`: idle firmware image – 10 × 32 768 + 4 016 = **331 696 B** | 5A | **Only if state was `0x00`:** bulk OUT endpoint `0x02`, load idle firmware (chunk size 32 768 on HD-New) |
| 6 | Vendor control OUT: write `0900:0070 = 00 04` | 6A | Write `0900:0070 = 00 04` |
| 7 | Vendor control IN: re-read `0900:0014` and `0900:0018`; series of 2-byte reads from `0000:0010` … `001E` | 7A | Re-read `0900:0014` / `0900:0018`; read `0000:0010` … `001E` (two banks appear identical) |
| 8 | SCMD via request `0xB8`: payload `01 00 00 00`; interrupt IN `0x83`; receipt/ready path on `0800:0014` / `0900:0074` / `0800:2008` | 8 | SCMD `01 00 00 00` (IDLE); state-confirmed, expected readback `0x11` |
| 9 | Mailbox port `0x33`: write `AB A9 0F A4 55`, read 3 bytes (reply `33 44 55`) | 9 | Port `0x33` write `AB A9 0F A4 55`, read 3 bytes (expects `33 44 55`) |
| 10 | Repeat mailbox `AB A9 0F A4 55` + 3-byte read while reply is `33 44 55`; restore enable-state shadow; write analog-input enable bit; write enable word that turns on the firmware processor | 10 | Loop on phase `33 44 55`: restore enable-state shadow, configure analog-input bit, enable firmware processor until phase becomes `27 F9 7B` |
| 11 | On first reply `27 F9 7B`: long series of transcoder / register writes (vendor OUT request `0xBC`, `wValue = 0x0001`, many addresses) | 11 | Transcoder defaults / ordered register table writes |
| 12 | SCMD via request `0xB8`: payload `04 00 00 00`; interrupt / receipt path | 12 | SCMD `04 00 00 00` (INIT) – issued **before** encode-firmware download |
| 13 | Bulk OUT endpoint `0x02`: encode firmware image – 45 × 32 768 + 26 224 = **1 500 784 B** | 13 | Bulk OUT endpoint `0x02`, load encode firmware |
| 14 | Vendor control IN: read `0000:0010` … `0016`; phase may briefly return to `33 44 55` | 14 | Read `0000:0010/12/14/16`; allow temporary return to `33 44 55` |
| 15 | Up to ~5× mailbox port `0x33`: write `AB A9 0F A4 5B`, read 3 bytes, refresh analog-input enable; stop when `(reply & 0xF8F0F0) == 0x78E040` | 15 | Same loop (mask `0xF8F0F0 == 0x78E040`); also observed `0x7BEC47` on some hardware |
| 16 | Write enable word that turns on the encoder; poll mailbox `AB A9 0F A4 55` until reply is again `27 F9 7B` | 16 | Enable encoder; poll until phase `27 F9 7B` |
| 17 | Mailbox port `0x33`: write `28 28`; write `29 89 5B` → read 1 byte (`91`); write `DD CE 3F B2` → read 2 bytes (`DA 67`) | 17 | Identical mailbox sequence; expects `91` / `DA 67` |
| 18 | Write enable word that asserts the encoder trigger; poll mailbox `43 23 84` until reply `F7`; clear the trigger bit | 18 | Identical; trigger only held for the poll |
| 19 | Mailbox port `0x33`: write `89 89 FB` → read 1 byte (`6E`) | 19 | Identical; expects `6E` |
| 20 | Long ordered mailbox block on ports `0x44`, `0x33`, `0x4E`, `0x4C` (generation-specific payloads) | 20 | Ordered common mailbox blocks on ports `0x44` / `0x33` / `0x4E` / `0x4C`; several meanings still UNKNOWN |
| 21 | Mailbox port `0x33` block-read series: write `AB 92 3E B4 …`, then 56-byte reads | 21 | Same block-read series (purpose unknown; do not omit) |
| 22 | Common setup complete; next observed traffic is the HDMI selector and input path (see [hdmi.md](hdmi.md) / [component.md](component.md)) | 22 | Complete common setup and branch to the selected input configuration |

**Observed firmware sizes**
- Idle: 331 696 B = 10 × 32 768 + 4 016
- Encode: 1 500 784 B = 45 × 32 768 + 26 224

**SCMDs observed during common init (request `0xB8`)**
- `01 00 00 00` – after idle firmware
- `04 00 00 00` – before encode firmware

(`04 A0 00 00` appears later in the capture during/after transcoder activity and on the capture-start path; it is not part of the common boot sequence above.)

**Notes on alignment**
- Steps 1–21 are the shared boot path. The observed transfers and userspace follow essentially the same order for the firmware loads, SCMD `01` / `04`, phase probes, encoder enable/trigger, and the early mailbox blocks.
- **Already-initialized branch:** when the device state is not `0x00`, userspace skips the idle-firmware download and sends SCMD RESET, mode `00`, data `0000` — Original HD `00 00 02 00 00 00`; HD-New `02 00 00 00`. The state-confirmation path expects `0x10`. This branch is not represented in the observed sequence.
- When the device state is `0x00`, userspace takes the first-time path: download idle firmware, write `0900:0070 = 00 04`, perform the post-download register reads, then continue through SCMD IDLE and the shared initialization steps.
- Input selection (`0x9C:51 = 45` for HDMI, `0x9C:51 = ED` for component, etc.), the post-setup idle/keep-alive loops, and any subsequent input switch belong on the input-specific pages, not in this common init comparison.
- Enable-state shadow restored after mailbox operations in the late portion of the common path is `D3 9E`.

See [hdmi.md](hdmi.md), [component.md](component.md), [composite.md](composite.md), and [s-video.md](s-video.md) for selectors and per-input sequences. The “Idle / keep-alive loops” section below documents the no-signal loops that run *after* an input has been selected.


### Capture-start sequence

| Order | Command / bytes | Notes / description |
|---:|---|---|
| 1 | Finish selected input setup and signal/format detection; apply transcoder setup/final configuration | Input selector, decoder tables, and output parameters depend on input and detected format. |
| 2 | Queue bulk IN URBs on endpoints `0x81` and `0x84` where used by the capture path | Queue before START as required by the implementation; queueing alone does not prove capture is active. |
| 3 | SCMD INIT, mode `A0`, data `0000` — Original HD `00 00 04 A0 00 00`; HD-New `04 A0 00 00` | Re-enters initialized/stopped state without reloading encode firmware. |
| 4 | Read `0800:2008` | For analog 240p, the readback has been observed as `00 01`; this is an observation, not a universal expected value. |
| 5 | SCMD START, mode `00`, data `0002` — Original HD `00 00 05 00 00 02`; HD-New `05 00 00 02` | State-change request; source expects state `0x02`. Follow completion confirmation. |
| 6 | Observe bulk IN completions on `0x81` / `0x84` | Observed bulk IN transfers were 65,536 bytes. Validate payload contents; USB completion alone is not proof of valid video. |

### Shared capture-start sequence observed on analog 240p

| Order | Command / operation | Confirmation |
|---:|---|---|
| 1 | Apply the ordered SoC/transcoder register burst after input lock; queue bulk IN URBs on endpoints `0x81` and `0x84` | Queueing is preparation, not proof of active capture. |
| 2 | SCMD `04 A0 00 00` | Follow command-receipt and ready/readback path. |
| 3 | Read `0800:2008` | Observed ready value `00 01` in analog 240p observations. |
| 4 | SCMD `05 00 00 02` | Confirm state/completion and data flow. |
| 5 | Observe bulk IN completions on `0x81` / `0x84` | Captured transfers were 65,536 bytes; verify payload is actual video. |

### Idle / keep-alive loops (post-input-setup, no signal)

Observed on HD-New after input selection with no source present. Each input has its own loop body; every mailbox command uses the normal HD-New mailbox handshake (commit / interrupt / restore-shadow / poll-ready).

#### HDMI no-signal idle

| Order | Mailbox payload | Notes |
|---:|---|---|
| 1 | `AB 4C` | `0x9C:AB = 4C` |
| 2 | `9D CD 6A 00` | Read helper index `0x6A` |
| 3 | `99 89 F5 00` | Sequence-read / poll helper |
| 4 | `9D CD 94 00` | Read helper index `0x94` |

Observed after HDMI selector and front-end setup, during HDMI no-signal idle, and after returning to HDMI from component.

#### Component no-signal idle

| Order | Mailbox payload | Notes |
|---:|---|---|
| 1 | `AB 4C` | `0x9C:AB = 4C` |
| 2 | `9D CD 2B 00` | Read helper index `0x2B` |
| 3 | `9D CD 2A 00` | Read helper index `0x2A` |
| 4 | `9D CD 60 00` | Read helper index `0x60` |
| 5 | `99 89 F5 00` | Sequence-read / poll helper |
| 6 | `9D CD 94 00` | Read helper index `0x94` |

Observed after component selection from both HDMI and composite. The three extra `9D CD` helpers (`2B` / `2A` / `60`) replace the single HDMI `6A` helper.

#### Composite / S-video no-signal idle

| Order | Mailbox payload | Notes |
|---:|---|---|
| 1 | `89 89 FA 00` | Status / source-detect poll (`0x9C` mailbox read helper) |
| 2 | `99 89 F5 00` | Sequence-read / poll helper |

This two-command cycle (~50 ms) is shared by composite and S-video once the analog path is selected and no source is locked. It is the same loop previously documented on the HDMI→composite and HDMI→S-video no-input captures. While the device is still on HDMI the HDMI four-command loop runs instead; the FA/F5 loop only appears after the composite or S-video selector has been written.

#### Source-detection candidate

The analog no-input idle centres on the repeated `89 89 FA` read. With a signal present or when transitioning from no input to source found, the same helper has returned a different value (`89 89 CA`) and the FA/F5 loop is exited; that transition is the best current candidate for “source found at the analog input”. The common `99 89 F5` helper appears in every input’s keep-alive and is therefore unlikely to be the sole source detector.

**Still open**
- Exact semantics of the component-specific helpers `9D CD 2B` / `2A` / `60`.
- Whether any of the loops change once a signal is locked (vs. merely present).
- Whether the read helpers can be elided without breaking the keep-alive.

**State-word timing note:** when switching from HDMI to component with no signal, the write `0900:0018 = 02 0E` was observed before the final mailbox selector `51 ED`. A transient re-write of the previous HDMI value `02 0A` immediately preceded the component value. When switching from HDMI to composite or S-video with no signal the routing change (`02 0E → 03 0E`) occurs after the selector.

See [commands.md](commands.md) for generic mailbox commands, unknown registers, transcoder/pipeline operations, and unresolved status values.
