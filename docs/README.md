# Elgato Game Capture HD — protocol and driver documentation

This folder documents the USB protocol of the Elgato Game Capture HD (original
`0fd9:0044/004e/0051` and "HD-New" `0fd9:005d`) and the design of the Linux
DKMS/V4L2 driver in [`../dkms`](../dkms).

## Pages

| Page | Contents |
|---|---|
| [hardware.md](hardware.md) | USB transport, mailbox, SCMD state machine, firmware load, common initialization, capture start/stop, shutdown. |
| [commands.md](commands.md) | Register and mailbox catalog: addressing model, device addresses, status registers, transcoder (encoder) register map, unknowns. |
| [composite.md](composite.md) | Composite input: C++ reference path (480i/576i), Windows 240p sequence, 480i↔240p transitions. |
| [s-video.md](s-video.md) | S-video input (Windows traces only; not implemented in the C++ reference). |
| [component.md](component.md) | Component input: C++ reference path and Windows switch traces. |
| [hdmi.md](hdmi.md) | HDMI input: C++ reference path and Windows switch traces. |
| [video-output.md](video-output.md) | What the device emits (MPEG-TS) and how the Linux driver should expose it. |
| [branch-history.md](branch-history.md) | What each historical git branch contains and why `dev` is now the base. |
| [evidence/](evidence/) | Raw evidence: decoded Windows USB traces, usbmon logs of the C++ driver, kernel logs, reference TS analysis. |

## Sources of truth, in order

1. **Windows USB traces** (`evidence/windows-traces/`): what the official driver
   actually sends. They are authoritative for ordering and values, but each one
   covers only the scenario that was captured.
2. **The C++ userspace implementation** (`../src`, from the original
   `tolga9009/elgato-gchd` project). It is a working, reverse-engineered
   implementation for HDMI, component, and composite **480i/576i** (and the
   progressive/HD modes listed in each input page). Its register names
   (`src/gchd_hardware.hpp`) come from Fujitsu script files and are the best
   names available.
3. **usbmon logs of the C++ driver** (`evidence/userspace-usbmon/`): the C++
   protocol as it appears on the wire. Use these to compare the kernel driver
   byte for byte.
4. **The DKMS driver** (`../dkms`). This is a port and is **not** a protocol
   reference. If it disagrees with 1–3, the driver is wrong until proven
   otherwise.

When the Windows traces and the C++ code disagree, both are recorded and the
difference is marked **DISCREPANCY**. Nothing is silently "corrected" toward
one source.

## Notation used in every page

### Register addresses

`bank:register` such as `0900:0018` means a 16-bit big-endian register accessed
by vendor control request `0xBC` with `wValue = bank` and `wIndex = register`.

### Mailbox ports and device addresses

The C++ code writes to *mailbox ports* `0x33`, `0x44`, `0x4C`, `0x4E`. On the
wire (HD-New commit word `09 00 PORT<<1 LEN`) the port appears doubled. The
Windows trace analyses name the target by that doubled, 8-bit value, which
looks like an I²C write address:

| C++ mailbox port | Wire / trace name | Role (best current interpretation) |
|---|---|---|
| `0x33` | `0x66` | On-board controller ("bridge"). Executes its own commands (`AB A9 …`, `28 28`, …) and relays reads/writes to the devices below. |
| `0x44` | `0x88` | Analog video decoder (composite / S-video front end, status registers `CA`, `E7`, `F1`, `FA`, …). |
| `0x4C` | `0x98` | Video back end / scaler (coefficient tables `90–A3`, retrigger `0D/0E`). |
| `0x4E` | `0x9C` | HDMI receiver / input front end (input selector register `51`, bank register `00`). |
| — (via `0x33`) | `0x94` | Auxiliary device written with the 3-byte form `94 RR VV` on port `0x33`. |

In these pages `0x88:28 = 8A` means "write value `8A` to register `28` of device
`0x88`", which in C++ is `mailWrite(0x44, {0x28, 0x8A})`.

Reads go through the bridge on port `0x33` with a 3-byte request whose first
byte selects the device:

| Read request on port `0x33` | Reads from | C++ helper |
|---|---|---|
| `89 89 RR` | device `0x88` register `RR` | inline `mailWrite(0x33,{0x89,0x89,RR}); mailRead(0x33,1)` |
| `99 89 RR` | device `0x98` register `RR` | inline, `pollOn0x9989ED()`, `readFrom0x9989EC()` |
| `9D CD RR` | device `0x9C` register `RR` | `readDevice0x9DCD(RR)` |

This mapping comes from the decoded Windows traces (`9D→9C`, `89→88`,
`99→98`). Treat the role column as an interpretation. Treat the address mapping
as established.

### Confidence markers

- **C++**: implemented in the C++ reference; worked on real hardware in 2016.
- **WIN**: observed in a Windows USB trace (name the trace).
- **KERNEL**: observed only from the DKMS driver; lower confidence.
- **UNKNOWN**: the effect is not established. Keep the operation, do not guess.
- **DISCREPANCY**: sources disagree; both values are listed.

Values such as the `0900:0014` shadow (`D3 9E`, `03 1E`) and `0900:001C` ready
words (`2E6B`, `6EEB`, …) are **session-specific**. Never hard-code them.
