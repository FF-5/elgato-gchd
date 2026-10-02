# Experimental native V4L2/DKMS driver

This directory contains the experimental native Linux kernel driver for the Elgato Game Capture HD.

The v4l2-dkms-driver branch converts the original libusb userspace streamer into a kernel USB/V4L2 driver. It:

- binds 0fd9:0044, 0fd9:004e, 0fd9:0051, and 0fd9:005d;
- creates a /dev/video* V4L2 capture device for a supported USB device;
- exposes H.264 capture through V4L2;
- exposes HDMI, Component, and Composite inputs;
- receives the device's MPEG-TS stream from USB bulk endpoint 0x81;
- extracts video PID 0x1011 and turns PES payloads into V4L2 buffers;
- keeps encoded frames in a bounded RAM ring;
- uploads device firmware through the kernel firmware API;
- contains the device-specific USB control, mailbox, state-machine, transcoder, and input-configuration code.

The driver contains no Qt GUI and does not require a userspace capture daemon.

> **Status:** experimental / pre-hardware-validation. The kernel-side plumbing is in place, but device-specific signal detection and some mode/calibration behavior are still being brought over from the original implementation.

## Where to look

If you are trying to understand a particular part of the driver, start here:

| What you want to understand | Where to look |
| --- | --- |
| USB device matching and driver entry point | [usb.c](./usb.c) — gchd_ids, gchd_probe(), gchd_disconnect() |
| **Device initialization** | [usb.c](./usb.c) — gchd_probe() → gchd_hw_init() |
| Initial device state / firmware upload | [usb.c](./usb.c) — gchd_hw_init(), gchd_load_firmware() |
| USB control transfers | [usb.c](./usb.c) — gchd_ctrl_read(), gchd_ctrl_write() |
| Mailbox protocol | [usb.c](./usb.c) — gchd_mail_write(), gchd_mail_read() |
| Device state commands | [usb.c](./usb.c) — gchd_scmd(), gchd_state_cmd(), gchd_wait_state() |
| Hardware enable bits | [usb.c](./usb.c) — gchd_do_enable() and EB_* definitions |
| Transcoder initialization | [transcoder.c](./transcoder.c) — gchd_transcoder_init() |
| Input/mode configuration | [input.c](./input.c) — gchd_input_configure() |
| Input-specific hardware sequences | [input.c](./input.c) — HDMI / Component / Composite branches in gchd_input_configure() |
| Encoder startup | [input.c](./input.c) — gchd_encoder_start() and post-encoder setup |
| V4L2 device registration | [core.c](./core.c) — gchd_v4l2_register() |
| V4L2 ioctl interface | [core.c](./core.c) — gchd_ioctl and the gchd_* format/input/timing functions |
| V4L2 streaming start/stop | [core.c](./core.c) — gchd_start(), gchd_stop() |
| USB receive thread | [core.c](./core.c) — gchd_rx() |
| MPEG-TS parsing / H.264 extraction | [core.c](./core.c) — gchd_ts() |
| Encoded-frame buffering | [core.c](./core.c) — gchd_ring_push(), gchd_ring_pop(), gchd_ring_free() |
| Delivery of encoded frames to V4L2 | [core.c](./core.c) — gchd_deliver() |
| V4L2 file operations | [fops.c](./fops.c) |
| Shared driver state and declarations | [elgato_gchd.h](./elgato_gchd.h) |
| DKMS package configuration | [dkms.conf](./dkms.conf) |
| Kernel module build inputs | [Makefile](./Makefile) |

## Device initialization flow

The most useful path to follow when debugging bring-up is:

    USB device appears
          |
          v
    usb_driver.probe()
          |
          +--> allocate struct gchd
          +--> reset active USB configuration
          +--> initialize locks / defaults / buffers
          |
          +--> gchd_v4l2_register()
          |       |
          |       +--> register v4l2_device
          |       +--> create V4L2 controls
          |       +--> initialize videobuf2 queue
          |       +--> video_register_device()
          |
          +--> gchd_hw_init()
          |       |
          |       +--> read initial enable/state registers
          |       +--> HDNew boot-state handshake when required
          |       +--> upload idle firmware
          |       +--> transition device to IDLE
          |       +--> query processor state
          |       +--> enable required hardware blocks
          |
          +--> gchd_transcoder_init()
          |       |
          |       +--> program common transcoder defaults
          |
          +--> start gchd-rx kernel thread
                  |
                  +--> wait for USB MPEG-TS data

There is an important distinction between **USB/device initialization** and **capture initialization**:

1. gchd_probe() performs the one-time device bring-up.
2. gchd_hw_init() loads idle firmware and brings the hardware into its idle state.
3. gchd_transcoder_init() programs common transcoder defaults.
4. A V4L2 application later starts streaming.
5. gchd_start() calls gchd_input_configure(), starts the encoder/state machine, and marks the device as streaming.
6. gchd_rx() receives MPEG-TS data and feeds encoded frames into V4L2 buffers.

So, if you are looking for **"what happens when I plug the device in?"**, start with [usb.c](./usb.c): gchd_probe(), gchd_hw_init(), and then [transcoder.c](./transcoder.c): gchd_transcoder_init().

If you are looking for **"what happens when an application starts capture?"**, start with [core.c](./core.c): gchd_start(), [input.c](./input.c): gchd_input_configure(), [usb.c](./usb.c): gchd_state_cmd(), and [core.c](./core.c): gchd_rx().

## Capture data path

Once streaming is active, the data path is approximately:

    Elgato hardware
         |
         | USB bulk IN 0x81
         v
    gchd_rx()
         |
         | 188-byte MPEG-TS packets
         v
    gchd_ts()
         |
         | select video PID 0x1011
         | collect PES payload
         v
    gchd_ring_push()
         |
         | bounded encoded-frame queue
         v
    gchd_ring_pop()
         |
         v
    gchd_deliver()
         |
         v
    videobuf2 buffer
         |
         v
    /dev/video*

The receive thread is in [core.c](./core.c). It reads USB bulk data, reassembles 188-byte MPEG-TS packets, extracts PID 0x1011, accumulates PES payloads, and places completed encoded frames into the ring.

The default buffering limits are:

- 32 encoded frames;
- 64 MiB total encoded data;
- 8 MiB maximum size for one frame.

When the byte or frame limit is reached, the oldest buffered frame is discarded. The byte limit can be changed with the ring_bytes module parameter.

## Source files

### core.c

The main V4L2-side implementation.

Responsibilities:

- module metadata and parameters;
- V4L2 mode definitions;
- encoded-frame ring buffer;
- MPEG-TS/PES parsing;
- USB receive kernel thread;
- delivery of encoded frames to videobuf2;
- V4L2 format/input/timing ioctls;
- V4L2 controls;
- V4L2 device registration;
- USB ID matching;
- probe() / disconnect().

Important functions:

- gchd_probe() — top-level device initialization;
- gchd_start() — V4L2 stream start;
- gchd_stop() — V4L2 stream stop;
- gchd_rx() — USB receive loop;
- gchd_ts() — MPEG-TS/PES extraction;
- gchd_deliver() — move an encoded frame into a V4L2 buffer;
- gchd_v4l2_register() — create/register the V4L2 node.

### usb.c

Low-level communication with the Game Capture HD.

Responsibilities:

- USB control transfers;
- register access;
- mailbox access;
- state-machine commands;
- hardware enable-state handling;
- firmware upload;
- device boot/idle initialization;
- hardware shutdown.

Important functions:

- gchd_hw_init() — device bring-up;
- gchd_hw_shutdown() — device shutdown;
- gchd_load_firmware() — firmware transfer over USB bulk OUT;
- gchd_mail_write() / gchd_mail_read() — mailbox protocol;
- gchd_scmd() — send a device state command;
- gchd_state_cmd() — send a state command and wait for completion;
- gchd_do_enable() — modify hardware enable bits.

This is the file to read first when investigating USB protocol or device boot problems.

### input.c

Input and encoder configuration.

Responsibilities:

- configure HDMI, Component, and Composite paths;
- configure signal/mode-dependent hardware;
- configure/start the encoder;
- perform the post-encoder initialization sequence;
- stop the configured input.

The main entry point is gchd_input_configure(). It is called from V4L2 stream start in core.c.

This file contains much of the device-specific configuration work translated from the original C++ implementation. It is the main place to look when adding or fixing a particular input mode.

### transcoder.c

Common transcoder configuration.

gchd_transcoder_init() programs shared transcoder defaults, including:

- MPEG-TS/PES identifiers;
- H.264 encoder defaults;
- audio defaults;
- output-path configuration.

It contains common setup rather than input-specific signal configuration.

### fops.c

The standard V4L2/videobuf2 file operations:

- open;
- release;
- read;
- poll;
- mmap;
- ioctl.

Most actual V4L2 behavior is in core.c; this file connects the V4L2 device to the videobuf2 file-operation helpers.

### elgato_gchd.h

Shared definitions used by all driver compilation units.

Contains:

- USB IDs and endpoint numbers;
- firmware-related constants;
- ring-buffer definitions;
- struct gchd;
- function declarations;
- hardware-family definitions.

If you need to understand the state carried around by the driver, start with struct gchd here.

### Makefile

Kernel module build description.

The module is built from:

    core.o
    fops.o
    usb.o
    transcoder.o
    input.o

into the elgato_gchd.ko module.

### dkms.conf

DKMS package metadata.

Defines:

- package name/version;
- module name;
- installation location;
- kernel build command;
- clean command;
- automatic installation.

## Hardware families and firmware

The driver treats the supported devices as two hardware families:

| USB ID | Family | Firmware |
| --- | --- | --- |
| 0fd9:0044 | MB86H57/H58 | gchd/MB86H57_H58_IDLE + gchd/MB86H57_H58_ENC_H |
| 0fd9:004e | MB86H57/H58 | gchd/MB86H57_H58_IDLE + gchd/MB86H57_H58_ENC_H |
| 0fd9:0051 | MB86H57/H58 | gchd/MB86H57_H58_IDLE + gchd/MB86H57_H58_ENC_H |
| 0fd9:005d | MB86M01 / HDNew | gchd/MB86M01_ASSP_NSEC_IDLE + gchd/MB86M01_ASSP_NSEC_ENC_H |

The family is selected in usb.c by the gchd_ids USB ID table.

The older three devices share the MB86H57/H58 protocol. The 005d device uses the newer HDNew/MB86M01 protocol, which is why several USB and mailbox operations have separate family-specific paths.

The driver uses Linux request_firmware() rather than embedding firmware in the kernel module. The idle firmware is uploaded during gchd_hw_init(). The encoder firmware is selected by gchd_load_encoder_firmware().

## V4L2 interface

The driver exposes a standard V4L2 capture device.

The main interface is implemented in core.c:

- pixel format: H.264;
- capture through videobuf2;
- HDMI / Component / Composite inputs;
- discrete frame sizes and intervals;
- DV timing enumeration/query for HDMI and Component;
- H.264 bitrate control;
- H.264 profile control;
- H.264 level control;
- source-change events.

The V4L2 device is created by gchd_v4l2_register() during USB probe. The module can therefore be installed/loaded without the capture device being connected. The /dev/video* node appears when a matching USB device is bound to the driver.

## Building

Install matching Linux kernel headers and DKMS.

### DKMS

    sudo dkms add ./drivers/elgato_gchd
    sudo dkms install elgato-gchd/0.2.0

### Direct kernel-module build

    make -C /lib/modules/$(uname -r)/build M=$PWD/drivers/elgato_gchd modules

The module is GPL-licensed because it uses GPL-only Linux kernel interfaces.

## Debugging guide

### Device does not bind

Start with:

1. [usb.c](./usb.c) — gchd_ids
2. [usb.c](./usb.c) — gchd_probe()
3. dmesg for USB-driver errors.

Supported IDs are 0fd9:0044, 0fd9:004e, 0fd9:0051, and 0fd9:005d.

### Device binds but initialization fails

Follow:

    gchd_probe()
      -> gchd_hw_init()
           -> firmware/state/mailbox operations in usb.c

This is the path to investigate USB control-transfer errors, firmware upload failures, state-transition timeouts, and mailbox problems.

### /dev/video* exists but streaming fails

Follow:

    VIDIOC_STREAMON
      -> gchd_start()
           -> gchd_input_configure()
           -> gchd_scmd()
           -> gchd_state_cmd()

The input configuration is in input.c, while low-level state/mailbox operations are in usb.c.

### Streaming starts but no frames arrive

Follow:

    gchd_rx()
      -> USB bulk IN 0x81
      -> MPEG-TS packet assembly
      -> gchd_ts()
      -> video PID 0x1011
      -> gchd_ring_push()
      -> gchd_deliver()

In particular, check whether the device is producing PID 0x1011 and whether gchd_ts() is seeing valid MPEG-TS packets.

### Signal is lost

Signal presence is currently inferred from receiving video packets. See gchd_ts() and gchd_signal_check() in core.c.

A source-change event is generated when the state changes, and the input is stopped after approximately 500 ms without video data.

## Relationship to the original implementation

The original [src/gchd/](../../src/gchd/) implementation remains the reference for the reverse-engineered device protocol, especially:

- device-specific USB/mailbox sequences;
- signal detection;
- HDMI/Component/Composite mode configuration;
- encoder configuration;
- calibration sequences.

When porting or debugging a hardware sequence, compare the corresponding kernel-driver function with the original C++ implementation.

The intended architecture is to keep the capture path in the kernel rather than reintroducing a userspace capture helper.

## Current limitations

The kernel USB control-transfer primitives, mailbox paths, state commands, firmware uploads, RAM buffering, and V4L2 plumbing are implemented.

The remaining work is primarily around device-specific signal/mode behavior and hardware validation. In particular:

- HDMI signal detection is not yet a complete translation of the original implementation;
- the full per-mode calibration/configuration behavior is still being brought over;
- V4L2 controls establish the Linux-facing interface, but do not yet necessarily program every encoder parameter into the hardware;
- the branch should still be considered experimental until tested against the supported hardware revisions.
