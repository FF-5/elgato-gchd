# elgato_gchd — V4L2 driver for the Elgato Game Capture HD

Linux kernel driver for the Elgato Game Capture HD (`0fd9:0044`,
`0fd9:004e`, `0fd9:0051`, `0fd9:005d`). The USB protocol is described in
[`../docs`](../docs).

> **Status: experimental.** The driver initializes the device, selects inputs
> and detects signals. Capture output is not yet reliable.

## Firmware

The device needs two firmware files, loaded through the kernel firmware
loader from `/lib/firmware/gchd/`:

| Model | Files |
|---|---|
| `0044`, `004e`, `0051` | `gchd/MB86H57_H58_IDLE`, `gchd/MB86H57_H58_ENC_H` |
| `005d` | `gchd/MB86M01_ASSP_NSEC_IDLE`, `gchd/MB86M01_ASSP_NSEC_ENC_H` |

The files are not distributed with the driver. `../firmware_extract/`
contains a script that extracts them from the vendor installer.

## Build and install

Direct build:

```sh
make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo insmod elgato_gchd.ko
```

DKMS:

```sh
sudo dkms add ./dkms
sudo dkms install elgato-gchd/0.5.3
```

## Module parameters

| Parameter | Default | Meaning |
|---|---|---|
| `trace_mailbox` | `N` | Log every mailbox transfer. |
| `ring_bytes` | 64 MiB | Maximum size of the frame buffer. |
| `signal_active_retries` | 3 | Signal measurement retries when timing is detected. |
| `signal_inactive_retries` | 1 | Signal measurement retries when no timing is detected. |
| `signal_poll_delay_ms` | 200 | Delay between signal measurement batches. |

## Video device

Inputs: HDMI (0), Component (1), Composite (2), S-Video (3). The hardware is
initialized on `VIDIOC_STREAMON`. The output format is described in
[`../docs/video-output.md`](../docs/video-output.md).

## Source layout

| File | Contents |
|---|---|
| `gchd_protocol.h` | Protocol constants: USB requests, register banks and registers, enable bits, SCMD commands and states, mailbox ports and devices, bridge commands, device registers, TS PIDs. |
| `gchd_mailbox.h` | Device access macros: `GCHD_FE_W/_R/_RX` (front end `0x9C`), `GCHD_DEC_*` (decoder `0x88`), `GCHD_BE_*` (back end `0x98`), `GCHD_AUX_W` (`0x94`), `GCHD_BRIDGE_W/_R/_SKIP`, `GCHD_TC_SET` (encoder field), `GCHD_SEQ_*` (table entries). These macros return from the calling function on error. |
| `gchd_transcoder_fields.h` | Encoder register fields, `GCHD_TC_<FIELD>` = `address, lsb, bits`. |
| `elgato_gchd.h` | Driver structures, configuration and prototypes. |
| `usb.c` | Control transfers, mailbox transport, SCMD and state completion, enable bits, firmware loading, initialization and shutdown. |
| `input.c` | Encoder start, input configuration and signal detection for each input, mode changes. |
| `protocol_common.c` | Back-end blocks shared by several inputs. |
| `transcoder.c` | Encoder defaults and per-mode configuration. |
| `core.c` | V4L2 device, buffer queue, device lifecycle, bulk-IN reception, USB probe and disconnect. |
| `fops.c` | V4L2 file operations. |

Example of the access macros:

```c
GCHD_FE_W(d, GCHD_FE_REG_INPUT_SEL, GCHD_FE_SEL_HDMI);   /* 0x9C:51 = 45 */
GCHD_DEC_R(d, GCHD_DEC_REG_LOCK, v);                     /* read 0x88:CA */
GCHD_TC_SET(d, GCHD_TC_V_HSIZE_OUT, 720);
```
