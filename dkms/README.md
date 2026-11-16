# elgato_gchd — experimental V4L2/DKMS driver

Kernel driver for the Elgato Game Capture HD (`0fd9:0044/004e/0051/005d`).
It is a port of the C++ userspace driver in [`../src`](../src). The protocol is
documented in [`../docs`](../docs). This README covers only building, layout
and the driver's current state.

> **Status: not working end to end.** This code is the tip of the
> `state-machine` branch (copied 2026-11-16). It builds cleanly against Linux
> 6.8 headers. On hardware (HD-New `005d`) it initializes, detects composite,
> and either fails to complete START or delivers H.264 that decodes badly. See
> "Known defects" and [`../TODO.md`](../TODO.md).

## Build and install

Firmware files must be in `/lib/firmware/gchd/` under the names the module
requests (no `.bin` suffix):

```
gchd/MB86H57_H58_IDLE        gchd/MB86H57_H58_ENC_H          (Original HD)
gchd/MB86M01_ASSP_NSEC_IDLE  gchd/MB86M01_ASSP_NSEC_ENC_H    (HD-New)
```

Use `../firmware_extract/` to get them from the Windows or macOS package.

Direct build:

```sh
make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo insmod elgato_gchd.ko
```

DKMS:

```sh
sudo dkms add ./dkms
sudo dkms install elgato-gchd/0.2.0
```

## Module parameters

| Parameter | Default | Meaning |
|---|---|---|
| `trace_mailbox` | `N` | Log every mailbox TX/RX byte (very noisy, useful for comparing with `docs/evidence/userspace-usbmon/`). |
| `ring_bytes` | 64 MiB | Byte cap of the PES ring. |
| `signal_active_retries`, `signal_inactive_retries`, `signal_poll_delay_ms` | see `input.c` | Signal-detection polling. |

## Source layout

| File | Contents |
|---|---|
| `usb.c` | USB control transfers, mailbox, SCMD and state completion, enable bits, firmware load, hardware init (`gchd_hw_init`) and shutdown (`gchd_hw_shutdown`). Port of `src/gchd/commands.cpp` and the first half of `configure.cpp`. |
| `input.c` | Input-specific configuration and signal detection for HDMI, component, composite, S-video. Port of `configure.cpp` (second half) and `configure_*.cpp`. |
| `transcoder.c` | Encoder register programming. Port of `src/gchd/transcoder.cpp`. |
| `protocol_common.c` | Shared mailbox register tables/blocks. |
| `core.c` | V4L2 device, vb2 queue, lifecycle state machine, bulk-IN receive thread (`gchd_rx`), TS parsing (`gchd_ts`), frame ring, delivery, USB probe/disconnect. |
| `fops.c` | `v4l2_file_operations`. |
| `elgato_gchd.h` | Shared structures, constants, register and mailbox definitions. |

Hardware initialization is deferred until `VIDIOC_STREAMON`. USB probe only
registers the video node.

## Known defects (details and ordering in `../TODO.md`)

1. **240p is programmed as 480p** (`v_format 8`, `v_ip_format 2`) with a full
   reconfiguration on 480i ↔ 240p. Windows keeps the encoder in 480i and only
   flips analog decoder registers on 480i → 240p.
2. **Lossy H.264 delivery.** `gchd_ring_push()` drops the oldest PES when full,
   and `gchd_deliver()` delivers at most one PES per USB completion, so pictures
   are dropped. Audio and PTS/DTS are discarded. Plan: pass the TS through as
   `V4L2_PIX_FMT_MPEG` ([`../docs/video-output.md`](../docs/video-output.md)).
3. **START completion handling** differs from the C++ `completeStateChange()`.
   At commit `2da03ea` the device reached state `0x02` and streamed, while the
   driver reported START failed.
4. **Misleading names:** `GCHD_REPLY_STATUS_COMPOSITE` (`0x66`) is the 240p lock
   value of decoder register `0x88:CA`, and `GCHD_REPLY_STATUS_SVIDEO` (`0x7E`)
   is the 480i lock value. `GCHD_QUERY_ENCODER_CONFIG` (`0xCA`) is a decoder
   register, not an encoder query. `GCHD_MAIL_PORT_COMPONENT` (`0x4E`) is the
   `0x9C` front end used by every input. `GCHD_SCMD_MODE_HDNEW` (`0xA0`) is the
   "INIT without firmware reload" mode on both families.
5. `fops.c` exports a global symbol named `vb2_fops` with
   `EXPORT_SYMBOL_GPL`. This can clash with other modules and should be
   `static`.
6. The bulk-IN `0x84` endpoint is required at runtime (`-ENODEV` if missing),
   although the C++ never uses it.
