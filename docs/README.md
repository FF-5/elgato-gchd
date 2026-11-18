# Elgato Game Capture HD — protocol documentation

This folder describes the USB protocol of the Elgato Game Capture HD
(`0fd9:0044`, `0fd9:004e`, `0fd9:0051` and `0fd9:005d`) as used by the
`elgato_gchd` kernel driver in [`../src`](../src).

## Pages

| Page | Contents |
|---|---|
| [hardware.md](hardware.md) | USB transport, mailbox, system commands (SCMD), firmware loading, initialization, capture start and stop, shutdown. |
| [commands.md](commands.md) | Register and mailbox command reference, device registers, encoder register fields. |
| [composite.md](composite.md) | Composite input: 480i and 240p sequences, mode transitions. |
| [s-video.md](s-video.md) | S-video input. |
| [component.md](component.md) | Component input. |
| [hdmi.md](hdmi.md) | HDMI input. |
| [video-output.md](video-output.md) | Format of the stream produced by the device. |

## Conventions

### Status of the information

Each sequence is labelled with how it is known:

- **Observed**: recorded in USB traces of the device in operation. Ordering
  and values are as recorded.
- **Driver**: implemented in `src/`, not yet confirmed by a trace for that
  scenario.
- **Unknown**: the operation is part of the sequence but its effect is not
  documented. Unknown operations are kept in the sequence.

### Register addresses

`bank:register`, for example `0900:0018`, denotes a 16-bit big-endian register
accessed with vendor control request `0xBC`, `wValue = bank`,
`wIndex = register`.

### Mailbox ports and device addresses

The mailbox addresses four destinations. On the wire the port number appears
shifted left by one bit (commit word `09 00 PORT<<1 LEN`); this document names
devices by that 8-bit value.

| Mailbox port | Device | Role |
|---|---|---|
| `0x33` | `0x66` | On-board controller ("bridge"). Executes its own commands and relays register reads to the devices below. |
| `0x44` | `0x88` | Analog video decoder (composite, S-video). |
| `0x4C` | `0x98` | Video back end. |
| `0x4E` | `0x9C` | Input front end and HDMI receiver; holds the input selector. |
| via `0x33` | `0x94` | Auxiliary device, written with the 3-byte command `94 RR VV`. |

`0x88:28 = 8A` means "write value `8A` to register `28` of device `0x88`"; in
the driver this is `GCHD_DEC_W(d, 0x28, 0x8a)`.

Register reads go through the bridge with a 3-byte request followed by a
1-byte read:

| Request on port `0x33` | Reads | Driver macro |
|---|---|---|
| `89 89 RR` | device `0x88`, register `RR` | `GCHD_DEC_R` |
| `99 89 RR` | device `0x98`, register `RR` | `GCHD_BE_R` |
| `9D CD RR` | device `0x9C`, register `RR` | `GCHD_FE_R` |

### Session-specific values

Some registers hold values that differ between sessions, for example the
enable shadow `0900:0014` (`D3 9E`, `03 1E`) and the mailbox status
`0900:001C` (`2E6B`, `6EEB`, …). These are read and restored, never written
as constants.
