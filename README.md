# Elgato Game Capture HD — Linux V4L2 / DKMS driver

This branch contains the Linux kernel V4L2/DKMS implementation of the
Elgato Game Capture HD USB protocol.

The protocol implementation is being ported from the original
reverse-engineered userspace driver. The goal is **USB-transaction-level
compatibility**, not merely equivalent video output.

## Device support

| USB VID:PID | Device | Status |
|---|---|---|
| 0fd9:0044 | Game Capture HD | supported target |
| 0fd9:004e | Game Capture HD | supported target |
| 0fd9:0051 | Game Capture HD | supported target |
| 0fd9:005d | Game Capture HD | supported target |
| 0fd9:005c | Game Capture HD60 | unsupported |
| 0fd9:004f | Game Capture HD60 S | unsupported |

Firmware is intentionally not redistributed. The kernel driver expects the
same firmware images used by the original driver:

- old hardware: `mb86h57_h58_idle.bin`
- old hardware encoder: `mb86h57_h58_enc_h.bin`
- HD New idle: `mb86m01_assp_nsec_idle.bin`
- HD New encoder: `mb86m01_assp_nsec_enc_h.bin`

## Protocol architecture

The bring-up is ordered as follows:

```text
USB probe
  |
  +-- identify hardware family
  +-- detach/claim interface 0
  +-- select configuration 1
  |
  v
idle firmware
  |
  v
mailbox processor handshake
  |
  +-- enable-state transition
  +-- firmware processor enable
  +-- poll mailbox state
  |
  v
transcoder defaults
  |
  v
encoder initialization
  |
  +-- encoder firmware
  +-- control-register readback
  +-- encoder mailbox polling
  +-- encoder enable
  +-- trigger enable / completion polling
  |
  v
input-specific configuration
  |
  +-- HDMI
  |    +-- signal measurements
  |    +-- resolution / scan / rate detection
  |    +-- register-bank programming
  |    +-- common protocol blocks
  |    +-- color-space programming
  |
  +-- Component
  |    +-- 0x9dcd signal measurements
  |    +-- resolution / scan / rate detection
  |    +-- mode-specific register programming
  |    +-- common protocol blocks
  |    +-- color-space programming
  |
  +-- Composite
       +-- mailbox mode detection
       +-- PAL/NTSC branch
       +-- common protocol blocks
       +-- color-space programming
  |
  v
final transcoder configuration
  |
  v
SCMD_INIT
  |
  v
SCMD_STATE_CHANGE -> START
  |
  v
bulk IN endpoint 0x81
```

## Mailbox and register model

The original protocol uses several vendor control/mailbox paths. The kernel
implementation preserves the logical transactions rather than translating
them into a new device protocol.

Important protocol operations include:

- mailbox writes and reads on ports `0x33`, `0x44`, `0x4c`, and `0x4e`
- `0x9dcd` indexed measurements
- enable-state and enable-register transitions
- SCMD commands
- transcoder bitfield writes
- repeated mailbox polling with protocol-specific masks
- firmware transfers over bulk OUT endpoint `0x02`
- capture data over bulk IN endpoint `0x81`

The source-specific configuration preserves the reference driver's branch
ordering, including the waits used for signal lock and mailbox completion.

## Input branches

### HDMI

The HDMI path measures the input through the `0x9dcd` registers, determines
the supported resolution/scan mode, programs the corresponding `0x4e`
register banks, executes the common protocol blocks, and programs the
transcoder before issuing the START state transition.

### Component

The Component path performs the reference `0x66/0x65` and `0x68/0x67`
measurements, averages ten samples, waits for signal stabilization, detects:

- 1080p30
- 1080i60
- 720p60
- 576p50
- 576i50
- 480p60
- 480i60

It then follows the corresponding register-bank branches and completes the
common protocol sequence before SCMD START.

### Composite

The Composite path uses the reference mailbox mode probe. The low nibble of
the reply selects:

- `6` -> NTSC / 480i60
- `7` -> PAL / 576i50

It then follows the PAL/NTSC-specific register and common-block sequence.

## Shutdown

The shutdown path mirrors the reference ordering:

```text
stop stream
  |
  v
0x44 / 0x33 shutdown preamble
  |
  v
BANKSEL = 0
  |
  v
read enable state
  |
  v
disable transcoder output
  |
  v
SCMD_INIT
  |
  v
clear enable state
  |
  v
disable firmware processor
  |
  v
SCMD_IDLE
  |
  v
SCMD_RESET
```

## Current verification status

The implementation has been mechanically checked for balanced C braces and for
remaining C++ constructs in the kernel protocol files.

The following are ported into the kernel branch:

- hardware-family detection
- idle and encoder firmware sequencing
- pre-encoder processor handshake
- encoder mailbox polling
- common protocol blocks A/B1/B2/B3/C
- HDMI configuration sequence
- Component configuration sequence
- Composite configuration sequence
- final transcoder configuration
- SCMD initialization/start sequencing
- shutdown/reset ordering

**Not yet claimed as formally 1:1:** a real hardware usbmon comparison of every
control and bulk transaction, and a successful out-of-tree kernel build on a
target system. Those are the final verification steps before calling the
implementation protocol-identical.

## Source mapping

The protocol port is derived from the original reverse-engineering sources:

- `src/gchd/configure.cpp`
- `src/gchd/configure_hdmi.cpp`
- `src/gchd/configure_component.cpp`
- `src/gchd/configure_composite.cpp`
- `src/gchd/transcoder.cpp`
- `src/gchd_hardware.hpp`

Kernel implementation:

- `drivers/elgato_gchd/usb.c`
- `drivers/elgato_gchd/input.c`
- `drivers/elgato_gchd/protocol_common.c`
- `drivers/elgato_gchd/transcoder.c`

The protocol documentation intentionally describes USB operations, states,
branches, register banks, mailbox exchanges, waits, and final device states
rather than exposing userspace C++ function names.
