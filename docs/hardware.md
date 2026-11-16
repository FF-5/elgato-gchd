# Hardware, transport and shared lifecycle

Notation and confidence markers are defined in [README.md](README.md).
Function names in **C++** references are from `../src/gchd/*.cpp` and
`../src/gchd_hardware.hpp`.

## 1. Device families

| Family | USB PIDs | Firmware (idle / encode) | Encoder SoC |
|---|---|---|---|
| Original GCHD ("HD") | `0044`, `004e`, `0051` | `mb86h57_h58_idle.bin` / `mb86h57_h58_enc_h.bin` | Fujitsu MB86H57/H58 |
| HD-New | `005d` | `mb86m01_assp_nsec_idle.bin` / `mb86m01_assp_nsec_enc_h.bin` | Fujitsu MB86M01 |
| HD60 / HD60 S | `005c` / `004f` | — | Not supported (different hardware) |

All evidence in this repository (Windows traces, usbmon logs, kernel logs) was
captured on an **HD-New** (`005d`, version string `V130…`). Original-HD
behaviour is known only from the C++ code.

## 2. USB transport primitives

| Operation | USB transfer | Payload | Source |
|---|---|---|---|
| Register read | Control IN, `bmRequestType 0xC0`, `bRequest 0xBC`, `wValue=bank`, `wIndex=reg` | 2 or 4 bytes, big-endian | C++ `read_config` |
| Register write | Control OUT, `0x40`, `0xBC`, `wValue=bank`, `wIndex=reg` | 2 bytes, big-endian | C++ `write_config` |
| Transcoder word write (`slsi`) | Control OUT `0x40`, `0xBC`, `wValue=0x0000`, `wIndex=addr` | 16-bit BE value; `addr` 2-byte aligned | C++ `slsi` |
| Transcoder bit-field write (`sparam`) | Control OUT `0x40`, `0xBC`, `wValue=0x0001`, `wIndex=addr & ~3` | `VALUE32_BE MASK32_BE` (8 bytes) | C++ `sparam` |
| SCMD | Control OUT `0x40`, `bRequest 0xB8`, `wValue=0`, `wIndex=0` | HD: `00 00 CMD MODE DH DL`; HD-New: `CMD MODE DH DL` | C++ `scmd` |
| Original-HD mailbox write/read | Control OUT/IN, `bRequest 0xBD`, `wValue=0`, `wIndex=PORT<<8` | Raw bytes, no padding | C++ `mailWrite`/`mailRead` |
| HD-New mailbox data write | Control OUT `0xBC`, `0800:00C0` | Payload padded to even length with `00` | C++ `mailWrite` |
| HD-New mailbox commit | Control OUT `bRequest 0xB9`, `wValue=0`, `wIndex=0` | Write `09 00 PORT<<1 LEN`; read request `09 01 PORT<<1 LEN` | C++ `mailWrite`/`mailRead` |
| HD-New mailbox read data | Control IN `0xBC`, `0800:23BE` | `LEN + 2 + (LEN & 1)` bytes; drop 2 leading bytes and the odd pad byte | C++ `mailRead` |
| Interrupt wait | Interrupt IN `0x83`, 3-byte buffer (`10 00 00` observed) | Status only checked for transfer success | C++ `interruptPend` |
| Firmware download | Bulk OUT `0x02` | Chunks of 16 384 B (HD) or 32 768 B (HD-New) | C++ `dlfirm` |
| Video stream | Bulk IN `0x81` | MPEG transport stream, see [video-output.md](video-output.md) | C++ `GCHD::stream` |

Bulk IN `0x84` exists on HD-New and Windows queues 8 URBs on it during capture
start (**WIN**). The C++ driver never touches `0x84` and still streams
(**C++**), so queueing `0x84` is **not required** for video.

### 2.1 Mailbox handshake (HD-New), exact C++ order

Write (`mailWrite`):

1. `0800:00C0` ← payload (padded to even length)
2. `0xB9` ← `09 00 PORT<<1 LEN` (`LEN` is the unpadded length)
3. Interrupt IN `0x83`
4. Read `0800:0016` (2 bytes; always `09 00` so far)
5. Poll `0900:001C` until bit 0 is set (`mailReadyWait`)
6. Read `0800:23BE` (2 bytes, discarded)
7. Write `0900:0014` ← saved enable-state shadow
8. Poll `0900:001C` bit 0 again

Read (`mailRead`):

1. `0xB9` ← `09 01 PORT<<1 LEN`
2. Interrupt IN `0x83`
3. Read `0800:0016`
4. Poll `0900:001C` bit 0
5. Read `0800:23BE`, `LEN+2+(LEN&1)` bytes, then strip

Windows performs the same steps (**WIN**, "HS"/"HSR" primitives in
`evidence/windows-traces/gchd_composite_240p_full_bringup.txt`). The 3-byte
"CMD" form on port `0x33` (`94 RR VV`) gets only the write handshake.

### 2.2 Mailbox handshake (Original HD)

`0xBD` write to `wIndex=PORT<<8`, then write the shadow to `0900:0014` and poll
`0900:001C` bit 0 (`sendEnableState`). Reads are a single `0xBD` IN transfer
that blocks until data is ready (**C++** only).

### 2.3 Side information in `0900:001C`

Every poll of `0900:001C` (`MAIL_REQUEST_READY`) also carries cable detection
(**C++** `mailReadyWait`, `sendEnableState`):

| Bits | Meaning (C++) |
|---|---|
| 0 | Mailbox ready |
| 8–9 | Cable type: `00` HDMI, `10` component, `11` composite |
| 10–11 | Cable presence; either bit set means something is connected |

The C++ AND-accumulates these bits into `specialDetectMask_` during early init
and uses them for input autodetection (see §5 step 11). S-video is not
distinguished.

## 3. Enable register and its shadow

| Register | Name (C++) | Role |
|---|---|---|
| `0900:0018` | `ENABLE_REGISTER` | GPIO-like enable bits. |
| `0900:0014` | `MAIL_SEND_ENABLE_REGISTER_STATE` | Shadow / "configured" mask. Rewritten after every mailbox write. Value is session-specific (`D3 9E`, `03 1E` seen on HD-New). |

Enable bits (masks into both registers; **C++** names, meanings are educated
guesses from the original author):

| Bit | Mask | Name | Interpretation |
|---|---|---|---|
| 1 | `0x0002` | `EB_FIRMWARE_PROCESSOR` | Powers the processor the firmware is loaded into. |
| 2 | `0x0004` | `EB_ANALOG_INPUT` | Set for analog inputs, clear for HDMI. |
| 3 | `0x0008` | `EB_ENCODER_ENABLE` | Set after the encode firmware is running. |
| 4 | `0x0010` | `EB_ENCODER_TRIGGER` | Pulsed once during init (see §5 step 18). |
| 8 | `0x0100` | `EB_COMPOSITE_MUX` | Composite (and, in Windows, S-video). |
| 9 | `0x0200` | `EB_ANALOG_MUX` | C++: set for composite and component. |

`doEnable(setMask, valueMask)` writes the shadow first (with `setMask` OR-ed
in), then `ENABLE_REGISTER`, then reads both back.

Observed full words in `0900:0018` (**WIN**): HDMI `02 0A`, component `02 0E`,
composite and S-video `03 0E`. **DISCREPANCY:** the C++ clears
`EB_ANALOG_MUX` (bit 9) for HDMI, which gives `00 0A` (`configure_hdmi.cpp`
comments "enable 000a"), but Windows writes `02 0A` for HDMI. Bit 9 is
therefore not simply "analog mux".

## 4. SCMD state machine

### 4.1 Commands (**C++** `scmd`, `stateConfirmedScmd`)

| CMD | Name | MODE / DATA | Expected state afterwards |
|---|---|---|---|
| `01` | `SCMD_IDLE` | `00` / `0000` | `0x11` |
| `02` | `SCMD_RESET` | mode `00` (warm start, firmware already loaded) | `0x10` |
| `02` | `SCMD_RESET` | mode `01` (final step of shutdown) | `0x12` |
| `04` | `SCMD_INIT` | mode `00`: leave idle **and** start encode-firmware load (raises interrupt) | `0x01`, not state-confirmed in C++ |
| `04` | `SCMD_INIT` | mode `A0`: leave idle without firmware load (no interrupt) | `0x01`, not state-confirmed in C++ |
| `05` | `SCMD_STATE_CHANGE` | data `0001` = STOP | `0x01` |
| `05` | `SCMD_STATE_CHANGE` | data `0002` = START (encoding, TS output) | `0x02` |
| `05` | `SCMD_STATE_CHANGE` | data `0004` = NULL (device outputs an empty/null stream) | `0x04` |

On HD-New, after writing SCMD the C++ waits for interrupt `0x83` and polls
`0800:0014` until its high byte equals `CMD`. It does this **only** for CMD
`01`, `04` and `05`, and only when `(MODE & 0xA0) != 0xA0`. This confirms
receipt, not completion.

### 4.2 State readback `0800:2008` (low 5 bits)

| Value | Meaning (**C++** `gchd_hardware.hpp`) |
|---|---|
| `0x00` | Power-on, nothing loaded |
| `0x10` | Idle firmware loaded, IDLE not yet sent |
| `0x11` | IDLE (bit 4 = idling, low nibble = STOP) |
| `0x01` | Not idling, stopped (after `SCMD_INIT`) |
| `0x02` | Encoding / streaming |
| `0x04` | Null stream |
| `0x12` | After `SCMD_RESET` mode 1 (shutdown) |

### 4.3 Completion handshake (**C++** `completeStateChange`)

```
loop {
    loop {
        state = read 0800:2008 & 0x1F
        if state not in {current, next}:
            if first iteration: return state      # e.g. reset already happened
            else: error "unexpected state"
        (stopStream only) drain bulk IN 0x81
        done = read 0900:0074 & 0x0004
        read 0900:01B0                            # value unused, purpose UNKNOWN
    } until done
    read 0800:2008; state = read 0800:2008 & 0x1F   # double read, as in C++
    write 0900:0074 = 00 04                         # ack sticky completion bit
    write 0900:01B0 = 00 00                         # purpose UNKNOWN
} until state == next
```

Windows performs the same per-round order (**WIN**: read `2008`, `0074`,
`01B0`, `2008`, `2008`, write `0074=0004`, `01B0=0000`). On analog 240p START
it took two rounds: `2008 = 01`, then `2008 = 02`.

**Kernel lesson (KERNEL, `evidence/kernel-logs/2da03ea-480i-data-flowing.txt`).**
The completion bit was already set (`0074 = 011E`) before START was sent. The
driver acknowledged it, then timed out waiting for it again, and declared START
failed while the device was in fact at state `0x02` and delivering about
2.9 MB/s. A port must follow the C++ loop exactly: the loop exit condition is
`state == next`, and a stale sticky bit only costs one extra round.

## 5. Common initialization (**C++** `configureDevice`, verified against WIN)

The device starts on HDMI. Input-specific configuration follows at step 25.

| # | Operation | Notes |
|---:|---|---|
| 1 | Read version: `0800:0094` and `0800:0098` (4 bytes each); if both zero, use Original-HD registers `0800:0010/14/18` | HD-New reads `"V130"`… |
| 2 | Write `BANKSEL 0900:0000 = 00 00` | Name from scripts; effect UNKNOWN. |
| 3 | Read shadow `0900:0014` | Save as the shadow value. |
| 4 | Read state `0800:2008 & 0x1F`. HD-New with state `00`: wait interrupt `0x83`, poll `0900:0074` bit 2, write `0900:0074 = 00 04`. Original HD: `completeStateChange(0,0)`. | Reading the state at power-on triggers a state change. |
| 5a | **State `00` (cold):** load idle firmware; write `0900:0070 = 00 04` (UNKNOWN); re-read `0900:0014` and `0900:0018`; read `0000:0010 … 001E` (8 × 2 bytes, values unused) | HD-New idle image = 331 696 B (10 × 32 768 + 4 016). |
| 5b | **State ≠ `00` (warm):** `stateConfirmedScmd(RESET, 00)` → expects `0x10` | Not present in any trace. |
| 6 | `stateConfirmedScmd(IDLE)` → expects `0x11` | |
| 7 | Bridge probe: port `0x33` write `AB A9 0F A4 55`, read 3 bytes (`33 44 55`) | |
| 8 | Loop: repeat the probe. On `33 44 55`: `sendEnableState`, `enableAnalogInput`, `doEnable(FIRMWARE_PROCESSOR)`. On `27 F9 7B` (first time): go to step 9. | |
| 9 | Input autodetect from `specialDetectMask_` (§2.3) unless the user forced an input; no signal → HDMI | **C++** only. |
| 10 | `enableAnalogInput()` | |
| 11 | `transcoderDefaultsInitialize()`: about 170 `sparam`/`slsi` writes, including PAT/PMT/SIT tables | See [commands.md §5](commands.md#5-transcoder-encoder-registers). |
| 12 | `SCMD INIT 04 00 00 00` (interrupt + `0800:0014` receipt on HD-New) | |
| 13 | Load encode firmware | HD-New image = 1 500 784 B (45 × 32 768 + 26 224). |
| 14 | Read `0000:0010/12/14/16` | The bridge may briefly fall back to `33 44 55`; the loop in step 8 continues until `27 F9 7B` is seen again. |
| 15 | Up to 5×: write `AB A9 0F A4 5B`, read 3, `enableAnalogInput`; stop when `(reply & 0xF8F0F0) == 0x78E040` | `78 E0 45` seen; `7B EC 47` reported on another unit. |
| 16 | `doEnable(ENCODER_ENABLE)`; poll `AB A9 0F A4 55` until `27 F9 7B` | |
| 17 | `0x33`: write `28 28`; write `29 89 5B` → read 1 (`91`); write `DD CE 3F B2` → read 2 (`DA 67`) | |
| 18 | `doEnable(ENCODER_TRIGGER)`; poll `43 23 84` until reply `F7`; `doEnable(ENCODER_TRIGGER, 0)` | |
| 19 | `0x33`: `89 89 FB` → read 1 (`6E`) | Read of device `0x88` register `FB`. |
| 20 | Common register blocks on devices `0x88`, `0x94`, `0x9C`, `0x98`, interleaved with reads | See `configure.cpp` lines 260–480. Contains the composite discriminator `0x88:02 = C9` (always, regardless of input) and analog/HDMI-dependent `0x88:08/09`. HD-New-only: `0x33` `21 01 72` → read `F4`, then `20 02 … 20 0F`. Then `0x33` `AA 8F 3B`. |
| 21 | 70 × `0x33` `AB 92 3E B4 XX YY` + 56-byte read; final `AB A2 3E B4 FB 79` + 8-byte read; `AA 8D 35` | UNKNOWN; keep exactly. |
| 22 | Repeat step 15 (5× `AB A9 0F A4 5B`) | |
| 23 | `transcoderSetup()`; `transcoderOutputEnable(true)` (`amck_mode = 1`) | |
| 24 | `SCMD INIT 04 A0 00 00`. Non-composite: `0x88:06 = 86`. Composite: read `89 89 FD`. Then read `89 89 F8`. `0x88:03 = 2F` (non-composite) or `28` (composite). `0x9C` bank `CC`, `0x9C:B3 = CC` / `33`; bank `CE`, `0x9C:27 = CC` / `33`; composite only: bank `CC`, read `9D CD 6E`, `0x9C:51 = CC`. Then `doEnable` for `COMPOSITE_MUX`, `ANALOG_INPUT`, `ANALOG_MUX`. | This is the input "prefix" that the Windows traces show before every input switch. |
| 25 | `configureHDMI()` / `configureComponent()` / `configureComposite()` | See input pages. |

**DISCREPANCY (step 24, composite):** the C++ writes selector `0x9C:51 = CC`.
Windows writes `0x9C:51 = 44` (direct switch from HDMI) or `EC` (switch from
another analog input). See [composite.md](composite.md).

## 6. Capture start

Every input path ends the same way (**C++**):

1. `transcoderFinalConfigure()` then `transcoderSetup()` with the detected mode.
2. `SCMD INIT 04 A0 00 00` (no interrupt).
3. Read `0800:2008` (expect `0x01`).
4. `SCMD 05 00 00 02` (START). HD-New: interrupt plus `0800:0014` receipt.
5. `completeStateChange(state, START)`.
6. Read bulk IN `0x81` continuously. The C++ uses synchronous 16 KiB
   (`DATA_BUF = 0x4000`) reads. Windows uses 65 536-byte URBs and queues 36 on
   `0x81` and 8 on `0x84` in the middle of the transcoder burst, before INIT
   (**WIN**).

## 7. Stream stop (**C++** `stopStream`)

1. `SCMD 05 00 00 04` (NULL); keep draining `0x81`.
2. `completeStateChange(START, NULL)`.
3. `SCMD 05 00 00 01` (STOP); `completeStateChange(NULL, STOP)`.

Windows performs the same `05…04` then `05…01` pair before reconfiguring for a
240p → 480i change (**WIN**, `trace_analysis.txt`, t ≈ 57.985 s / 58.646 s).

## 8. Shutdown (**C++** `uninitDevice`)

1. If state is START or NULL: `stopStream`.
2. If state is not `0x00`, `0x10` or `0x12`:
   1. `0x88:06 = 86`; read `89 89 F8`; `0x88:03 = 2F`
   2. `BANKSEL = 0000`; re-read shadow `0900:0014`; read `0800:2008`
   3. `transcoderOutputEnable(false)`
   4. `SCMD INIT 04 A0 00 00`
   5. `clearEnableState()`: clear shadow bits `ENCODER_ENABLE`,
      `ENCODER_TRIGGER`, `COMPOSITE_MUX`, `ANALOG_MUX`; probe `AB A9 0F A4 55`;
      `doEnable(FIRMWARE_PROCESSOR, 0)`; poll the probe until `33 44 55`
   6. `doEnable(FIRMWARE_PROCESSOR, 0)` again
   7. `stateConfirmedScmd(IDLE)` → `0x11`; `stateConfirmedScmd(RESET, 01)` → `0x12`

## 9. Idle / keep-alive loops after input selection (**WIN**, no signal)

These loops are what the Windows driver runs while it waits for a source. The
C++ has no keep-alive loop: it detects once and either starts or fails.

| Input | Loop body (mailbox payloads) |
|---|---|
| HDMI | `0x9C:AB = 4C`; read `9D CD 6A`; read `99 89 F5`; read `9D CD 94` |
| Component | `0x9C:AB = 4C`; read `9D CD 2B`, `2A`, `60`; read `99 89 F5`; read `9D CD 94` |
| Composite / S-video | read `89 89 FA`; read `99 89 F5` (about 50 ms per cycle) |

Component reads `9D CD 2B/2A/60`, while the C++ component detection reads
`9D CD 66/65` and `68/67`. HDMI detection in C++ reads `66/65`, `64/63` and
`34`. For analog inputs, decoder register `0x88:CA` is the lock/format status
(see [composite.md](composite.md#2-status-registers)).
