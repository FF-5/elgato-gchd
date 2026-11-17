# Hardware, transport and device lifecycle

Notation and status labels are defined in [README.md](README.md).

## 1. Device models

| Model | USB product IDs | Firmware files (idle / encoder) | Encoder |
|---|---|---|---|
| Game Capture HD | `0044`, `004e`, `0051` | `gchd/MB86H57_H58_IDLE` / `gchd/MB86H57_H58_ENC_H` | Fujitsu MB86H57/H58 |
| Game Capture HD (HD-New) | `005d` | `gchd/MB86M01_ASSP_NSEC_IDLE` / `gchd/MB86M01_ASSP_NSEC_ENC_H` | Fujitsu MB86M01 |

The observed sequences in this documentation were recorded on the `005d`
model. The other models differ in the SCMD payload length and the mailbox
transport (see §2); these paths are **Driver** only.

## 2. USB transport

| Operation | USB transfer | Payload |
|---|---|---|
| Register read | Control IN, `bmRequestType 0xC0`, `bRequest 0xBC`, `wValue = bank`, `wIndex = register` | 2 or 4 bytes, big-endian |
| Register write | Control OUT, `0x40`, `0xBC`, `wValue = bank`, `wIndex = register` | 2 bytes, big-endian |
| Encoder word write | Control OUT `0x40`, `0xBC`, `wValue = 0x0000`, `wIndex = address` | 16-bit big-endian value |
| Encoder field write | Control OUT `0x40`, `0xBC`, `wValue = 0x0001`, `wIndex = address & ~3` | `VALUE32_BE MASK32_BE` (8 bytes) |
| SCMD | Control OUT `0x40`, `bRequest 0xB8`, `wValue = 0`, `wIndex = 0` | `005d`: `CMD MODE DATA_H DATA_L`; other models: `00 00 CMD MODE DATA_H DATA_L` |
| Mailbox data (`005d`) | Control OUT `0xBC`, `0800:00C0` | Payload padded to an even length with `00` |
| Mailbox command (`005d`) | Control OUT `bRequest 0xB9`, `wValue = 0`, `wIndex = 0` | Write `09 00 PORT<<1 LEN`; read request `09 01 PORT<<1 LEN` |
| Mailbox read data (`005d`) | Control IN `0xBC`, `0800:23BE` | `LEN + 2 + (LEN & 1)` bytes: 2 header bytes, data, pad byte for odd lengths |
| Mailbox (other models) | Control OUT/IN, `bRequest 0xBD`, `wValue = 0`, `wIndex = PORT << 8` | Raw bytes, no padding |
| Interrupt | Interrupt IN `0x83`, 3 bytes (`10 00 00` observed) | Notification only |
| Firmware download | Bulk OUT `0x02` | 32 768-byte chunks (`005d`), 16 384-byte chunks (other models) |
| Video stream | Bulk IN `0x81` | MPEG transport stream, see [video-output.md](video-output.md) |

Bulk IN endpoint `0x84` is present on the `005d` model. Receive transfers are
queued on it during capture start (**Observed**); its payload is not
documented.

### 2.1 Mailbox handshake (`005d`, Observed)

Write:

1. `0800:00C0` ← payload, padded to an even length
2. `0xB9` ← `09 00 PORT<<1 LEN` (`LEN` = unpadded length)
3. Interrupt IN `0x83`
4. Read `0800:0016` (`09 00`)
5. Poll `0900:001C` until bit 0 is set
6. Read `0800:23BE`, 2 bytes, value not used
7. Write `0900:0014` ← current enable shadow
8. Poll `0900:001C` until bit 0 is set

Read:

1. `0xB9` ← `09 01 PORT<<1 LEN`
2. Interrupt IN `0x83`
3. Read `0800:0016`
4. Poll `0900:001C` until bit 0 is set
5. Read `0800:23BE`, `LEN + 2 + (LEN & 1)` bytes; drop the 2 header bytes and
   the pad byte

The 3-byte auxiliary write `94 RR VV` on port `0x33` uses the write handshake
only.

### 2.2 Mailbox handshake (other models, Driver)

Write: `0xBD` OUT with `wIndex = PORT << 8`, then write the enable shadow to
`0900:0014` and poll `0900:001C` bit 0. Read: a single `0xBD` IN transfer.

### 2.3 Cable detection bits in `0900:001C`

| Bits | Meaning |
|---|---|
| 0 | Mailbox ready |
| 8–9 | Cable type: `00` HDMI, `10` component, `11` composite |
| 10–11 | Cable presence (non-zero if a cable is detected) |

The driver accumulates these bits during initialization
(`GCHD_MAIL_READY_DETECT`). S-video is not distinguished from composite.

## 3. Enable register and shadow

| Register | Driver name | Role |
|---|---|---|
| `0900:0018` | `GCHD_REG_ENABLE` | Enable bits. |
| `0900:0014` | `GCHD_REG_ENABLE_SHADOW` | Shadow of the enable state; written after every mailbox write. Session-specific value. |

Enable bits (provisional names):

| Bit | Mask | Driver name | Use |
|---|---|---|---|
| 1 | `0x0002` | `GCHD_EB_FIRMWARE_PROCESSOR` | Set while the bridge processor runs. |
| 2 | `0x0004` | `GCHD_EB_ANALOG_INPUT` | Set for analog inputs. |
| 3 | `0x0008` | `GCHD_EB_ENCODER_ENABLE` | Set once the encoder firmware runs. |
| 4 | `0x0010` | `GCHD_EB_ENCODER_TRIGGER` | Pulsed once during initialization. |
| 8 | `0x0100` | `GCHD_EB_COMPOSITE_MUX` | Set for composite and S-video. |
| 9 | `0x0200` | `GCHD_EB_ANALOG_MUX` | See observed values below. |

Observed full values of `0900:0018` after input selection: HDMI `02 0A`,
component `02 0E`, composite and S-video `03 0E`.

To change bits, the shadow is written first (with the changed bits added),
then the enable register; both are read back.

## 4. System commands (SCMD)

### 4.1 Commands

| CMD | Name | MODE / DATA | State afterwards |
|---|---|---|---|
| `01` | IDLE | `00` / `0000` | `0x11` |
| `02` | RESET | mode `00`: firmware already loaded | `0x10` |
| `02` | RESET | mode `01`: last step of shutdown | `0x12` |
| `04` | INIT | mode `00`: leave idle and accept the encoder firmware download | `0x01` |
| `04` | INIT | mode `A0`: leave idle without firmware download | `0x01` |
| `05` | STATE_CHANGE | data `0001` = STOP | `0x01` |
| `05` | STATE_CHANGE | data `0002` = START (encoding, stream output) | `0x02` |
| `05` | STATE_CHANGE | data `0004` = NULL (empty stream) | `0x04` |

On the `005d` model, IDLE, INIT and STATE_CHANGE with a mode other than `A0`
are followed by an interrupt on `0x83` and a read of `0800:0014` until its
high byte equals `CMD`. This confirms receipt of the command; completion is
confirmed separately (§4.3). INIT mode `A0` is followed by neither
(**Observed**).

### 4.2 State register `0800:2008` (low 5 bits)

| Value | State |
|---|---|
| `0x00` | Power-on, no firmware |
| `0x10` | Idle firmware loaded, IDLE not yet sent |
| `0x11` | Idle |
| `0x01` | Initialized, stopped |
| `0x02` | Encoding |
| `0x04` | Null stream |
| `0x12` | After RESET mode `01` |

### 4.3 Completion handshake (Observed)

Each round:

1. Read `0800:2008`
2. Read `0900:0074`; bit 2 indicates a completed state change
3. Read `0900:01B0` (value not used)
4. Repeat 1–3 until bit 2 of `0900:0074` is set
5. Read `0800:2008` twice; the second value (low 5 bits) is the new state
6. Write `0900:0074 = 00 04` (acknowledge)
7. Write `0900:01B0 = 00 00`

Rounds repeat until the state equals the target state. START from the stopped
state was observed to take two rounds: `0x01`, then `0x02`.

The completion bit can already be set before a command is sent. It then
completes a round immediately with the previous state, and the next round
reports the new state. The loop condition is therefore the target state, not
the completion bit alone.

## 5. Initialization

The device starts with the HDMI input. The input-specific sequence follows
step 24.

| # | Operation | Status |
|---:|---|---|
| 1 | Read version: `0800:0094`, `0800:0098` (4 bytes each); if both are zero, read `0800:0010/14/18` (other models) | Observed (`005d` returns `"V130"`) |
| 2 | Write `0900:0000 = 00 00` | Observed; Unknown effect |
| 3 | Read `0900:0014` (enable shadow) | Observed |
| 4 | Read `0800:2008`. If `0x00`: wait for interrupt `0x83`, poll `0900:0074` bit 2, write `0900:0074 = 00 04` | Observed |
| 5a | State `0x00`: download the idle firmware (331 696 bytes = 10 × 32 768 + 4 016 on `005d`); write `0900:0070 = 00 04`; read `0900:0014`, `0900:0018`; read `0000:0010` … `0000:001E` | Observed; `0900:0070` and `0000:00xx` Unknown |
| 5b | State ≠ `0x00`: SCMD RESET mode `00`, wait for `0x10` | Driver |
| 6 | SCMD IDLE, wait for `0x11` | Observed |
| 7 | Bridge status: write `AB A9 0F A4 55` to port `0x33`, read 3 bytes (`33 44 55`) | Observed |
| 8 | Repeat 7. While `33 44 55`: write the enable shadow, set `GCHD_EB_ANALOG_INPUT` as required, set `GCHD_EB_FIRMWARE_PROCESSOR`. On `27 F9 7B`: continue | Observed |
| 9 | Encoder register defaults (about 170 field and word writes, including PAT, PMT and SIT tables) | Observed |
| 10 | SCMD INIT mode `00` | Observed |
| 11 | Download the encoder firmware (1 500 784 bytes = 45 × 32 768 + 26 224 on `005d`) | Observed |
| 12 | Read `0000:0010/12/14/16` (the bridge status can return to `33 44 55` briefly) | Observed |
| 13 | Up to five times: write `AB A9 0F A4 5B`, read 3 bytes; stop when `(reply & F8F0F0) == 78E040` | Observed |
| 14 | Set `GCHD_EB_ENCODER_ENABLE`; repeat step 7 until `27 F9 7B` | Observed |
| 15 | Port `0x33`: `28 28`; `29 89 5B` → 1 byte (`91`); `DD CE 3F B2` → 2 bytes (`DA 67`) | Observed; Unknown |
| 16 | Set `GCHD_EB_ENCODER_TRIGGER`; poll `43 23 84` until the reply is `F7`; clear `GCHD_EB_ENCODER_TRIGGER` | Observed |
| 17 | Read `0x88:FB` | Observed |
| 18 | Register blocks on devices `0x88`, `0x94`, `0x9C`, `0x98`, interleaved with reads; `005d` only: `21 01 72` → 1 byte, then `20 02 …` to `20 0F …` on port `0x33`; then `AA 8F 3B` | Observed; Unknown |
| 19 | 70 block reads: `AB 92 3E B4 XX YY` + 56-byte read; final `AB A2 3E B4 FB 79` + 8-byte read; `AA 8D 35` | Observed; Unknown |
| 20 | Repeat step 13 | Observed |
| 21 | Encoder setup for the selected mode; enable audio clock (`amck_mode = 1`) | Observed |
| 22 | SCMD INIT mode `A0` | Observed |
| 23 | Input prefix (see the input pages): devices `0x88` (registers `06`, `03`) and `0x9C` (registers `B3`, `27`) | Observed |
| 24 | Update `GCHD_EB_COMPOSITE_MUX`, `GCHD_EB_ANALOG_INPUT`, `GCHD_EB_ANALOG_MUX` | Observed |
| 25 | Input-specific configuration: [hdmi.md](hdmi.md), [component.md](component.md), [composite.md](composite.md), [s-video.md](s-video.md) | |

## 6. Capture start (Observed)

1. Encoder setup for the detected mode (about 120 field writes).
2. Receive transfers are queued on bulk IN `0x81` (32–36 × 65 536 bytes)
   and `0x84` (8 transfers) right before or during the encoder writes.
   No data is received on `0x84`.
3. SCMD INIT mode `A0`.
4. Read `0800:2008` (`0x01`).
5. SCMD STATE_CHANGE START (`05 00 00 02`); receipt confirmed by interrupt
   and `0800:0014 = 05 00`.
6. Completion handshake (§4.3) until state `0x02`.
7. MPEG-TS data on `0x81`; the first transfer completed about 0.5 s after
   START.

## 7. Capture stop (Observed)

1. SCMD STATE_CHANGE NULL (`05 00 00 04`). The receive transfers stay
   queued; data keeps arriving at the normal rate for about 0.4 s.
2. Completion handshake until state `0x04` (about 0.7 s after NULL).
3. SCMD STATE_CHANGE STOP (`05 00 00 01`); completion handshake until `0x01`
   (about 0.3 s). The last data (35 328 and 64 000 bytes in two recordings, a
   multiple of 512 bytes, so the transfer is not terminated) stays in a
   pending transfer.
4. Right after the completion is acknowledged, all pending transfers are
   cancelled: `0x81` first, then `0x84`. The partly filled transfer is
   discarded. No further reads take place until the next capture start.

## 8. Shutdown (Driver)

1. If the state is START or NULL: capture stop (§7).
2. If the state is not `0x00`, `0x10` or `0x12`:
   1. `0x88:06 = 86`; read `0x88:F8`; `0x88:03 = 2F`
   2. Write `0900:0000 = 00 00`; read the enable shadow; read `0800:2008`
   3. Disable the audio clock (`amck_mode = 0`)
   4. SCMD INIT mode `A0`
   5. Clear `GCHD_EB_ENCODER_ENABLE`, `GCHD_EB_ENCODER_TRIGGER`,
      `GCHD_EB_COMPOSITE_MUX`, `GCHD_EB_ANALOG_MUX` in the shadow; bridge status
      (step 7 of §5); clear `GCHD_EB_FIRMWARE_PROCESSOR`; repeat the bridge
      status until `33 44 55`
   6. SCMD IDLE (`0x11`), then SCMD RESET mode `01` (`0x12`)

## 9. No-signal loops (Observed)

After an input has been selected and no source is present, the device is
polled with the following sequences:

| Input | Loop |
|---|---|
| HDMI | `0x9C:AB = 4C`; read `0x9C:6A`; read `0x98:F5`; read `0x9C:94` |
| Component | `0x9C:AB = 4C`; read `0x9C:2B`, `2A`, `60`; read `0x98:F5`; read `0x9C:94` |
| Composite, S-video | read `0x88:FA`; read `0x98:F5` (about every 50 ms) |

For analog inputs, register `0x88:CA` reports lock and format; see
[composite.md](composite.md#2-status-registers).
