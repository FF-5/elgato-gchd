# Experimental native V4L2/DKMS driver

This directory contains the experimental native Linux kernel driver for the Elgato Game Capture HD.

The most important thing to understand when working on this driver is that the device protocol was reverse-engineered in the original libusb userspace implementation. The kernel driver is a port of that protocol, not the authoritative description of what the hardware is supposed to do.

For that reason, this README describes the **hardware initialization and capture setup in terms of the original userspace project first**, and then points to the corresponding kernel implementation. If a kernel-side sequence is unclear, go back to `src/gchd/` and follow the original call chain before changing the DKMS driver.

> **Status:** experimental / pre-hardware-validation. The kernel-side plumbing is in place, but some device-specific signal detection and mode/calibration behavior is still being brought over from the original implementation.

## Where to look

| Question | Start here |
| --- | --- |
| What happens when the application starts? | `src/main.cpp` → `GCHD::checkDevice()` → `GCHD::init()` |
| How is the USB device found? | `src/gchd.cpp` → `GCHD::openDevice()` |
| How is firmware selected? | `src/gchd.cpp` → `GCHD::checkFirmware()` |
| How is the USB interface claimed? | `src/gchd.cpp` → `GCHD::getInterface()` |
| **How does hardware initialization actually work?** | `src/gchd/configure.cpp` → `GCHD::configureDevice()` |
| Firmware upload and low-level state changes | `src/gchd/commands.cpp` → `dlfirm()`, `scmd()`, `completeStateChange()` |
| Enable bits / processor / encoder activation | `src/gchd/commands.cpp` → `sendEnableState()`, `doEnable()` |
| Mailbox protocol | `src/gchd/commands.cpp` → `mailWrite()`, `mailRead()` |
| Common transcoder setup | `src/gchd/transcoder.cpp` → `transcoderDefaultsInitialize()` |
| Encoder/output configuration | `src/gchd/transcoder.cpp` → `transcoderSetup()`, `transcoderFinalConfigure()` |
| HDMI-specific setup | `src/gchd/configure_hdmi.cpp` → `configureHDMI()` |
| Component-specific setup | `src/gchd/configure_component.cpp` → `configureComponent()` |
| Composite-specific setup | `src/gchd/configure_composite.cpp` → `configureComposite()` |
| Input settings and autodetection | `src/gchd/settings.cpp` + the three `configure_*` files |
| Hardware register/bit definitions | `src/gchd_hardware.hpp` |
| Userspace streaming after init | `src/streamer.cpp` / `src/gchd.cpp` → `GCHD::stream()` |
| Kernel USB entry point | `drivers/elgato_gchd/usb.c` → `gchd_probe()` |
| Kernel V4L2 streaming entry point | `drivers/elgato_gchd/core.c` → `gchd_start()` |

## Original userspace architecture

The original program is a useful reference because it separates the problem into layers:

```text
src/main.cpp
    │
    ├── parse command-line input/transcoder settings
    │
    └── GCHD gchd(...)
             │
             ├── checkDevice()
             │     ├── openDevice()
             │     └── checkFirmware()
             │
             └── init()
                   ├── getInterface()
                   └── setupConfiguration()
                         └── configureDevice()
                               ├── boot/idle firmware and state machine
                               ├── processor/encoder bring-up
                               ├── common hardware setup
                               ├── input-specific setup
                               ├── transcoder setup
                               └── SCMD_STATE_START
```

This distinction matters: `GCHD::init()` is not just one USB initialization call. It is the point where the program claims the USB interface and then executes a large, reverse-engineered hardware bring-up sequence.

## Original device initialization sequence

The sequence below is the conceptual sequence implemented by the original userspace project. It is intentionally described in terms of **what the hardware is being made to do**, rather than reproducing the long list of register writes.

### 1. Discover the device and choose its hardware family

`src/gchd.cpp` first initializes libusb and tries the supported Elgato VID/PIDs in `GCHD::openDevice()`.

Supported devices are grouped as:

| USB ID | Hardware family |
| --- | --- |
| `0fd9:0044`, `0fd9:004e`, `0fd9:0051` | original Game Capture HD / MB86H57/H58 |
| `0fd9:005d` | newer Game Capture HD / HDNew / MB86M01 |

The selected `DeviceType` affects later state handling, firmware, and some register/mailbox behavior. The HDNew path cannot always use exactly the same state-change procedure as the older devices because some state-register reads have interrupt side effects.

### 2. Find the matching firmware pair

`GCHD::checkFirmware()` chooses two firmware images based on the detected hardware family:

- idle firmware: `MB86H57_H58_IDLE` or `MB86M01_ASSP_NSEC_IDLE`
- encoder firmware: `MB86H57_H58_ENC_H` or `MB86M01_ASSP_NSEC_ENC_H`

The userspace program searches several filesystem locations. The kernel driver replaces this with Linux `request_firmware()` and the firmware files installed for the DKMS driver.

The important architectural point is that there are **two firmware stages**. Loading the idle firmware is part of initial hardware bring-up; loading the encoder firmware happens later, after the device processor has been brought to the appropriate internal state.

### 3. Claim USB interface 0

`GCHD::getInterface()` does three things:

1. detaches an already-bound kernel driver if necessary;
2. sets USB configuration 1;
3. claims interface 0.

Only after this does the original program have exclusive access to the device protocol.

In the kernel driver this responsibility belongs to the USB driver core and `gchd_probe()`. The important difference is that the kernel driver does not use libusb or manually detach its own driver.

### 4. Read the hardware revision and establish a known register bank

`GCHD::configureDevice()` starts by calling `readVersion()` and printing the hardware revision. It then selects the base register bank with `BANKSEL`.

The original implementation also reads the enable-state shadow register before changing anything. This is important because the enable register is not just a collection of independent GPIO-like bits: the accompanying mailbox/shadow state is part of the communication protocol with another processor in the device.

Useful code:

- `src/gchd/configure.cpp` — beginning of `GCHD::configureDevice()`
- `src/gchd/commands.cpp` — `sendEnableState()` and `doEnable()`
- `src/gchd_hardware.hpp` — `ENABLE_REGISTER`, `MAIL_SEND_ENABLE_REGISTER_STATE`, and `EB_*` definitions

### 5. Determine whether the device already has boot firmware running

The original driver reads the state machine before deciding whether the idle firmware must be uploaded.

There are two cases:

**Cold / uninitialized device**

- the state is effectively zero;
- the device is treated as needing its firmware loaded;
- idle firmware is uploaded with `dlfirm()`;
- enable-state registers are read again after the upload.

**Already initialized device**

- the flash/firmware does not need to be loaded again;
- the original program resets the device with `SCMD_RESET`;
- it then forces the device back to `SCMD_IDLE`.

The exact state handling is different for the original Game Capture HD and HDNew. For HDNew, reading the state register can itself trigger an interrupt, so `configureDevice()` explicitly acknowledges the completion condition rather than blindly using the older `completeStateChange()` path.

### 6. Load idle firmware and bring the embedded processor up

On a cold device, `GCHD::configureDevice()` calls `dlfirm(firmwareIdle_)`.

This is more than a firmware copy. After the image is transferred, the program interacts with the device processor through the enable-state register and mailbox protocol.

The original sequence repeatedly queries mailbox port `0x33` for a device-mode value. Two values are particularly important:

- `0x334455` — the processor is not yet at the next initialization stage;
- `0x27f97b` — the processor has reached the stage where the rest of the bring-up can continue.

While the device reports `0x334455`, the program sends the saved enable state, enables the appropriate analog-input state, and sets `EB_FIRMWARE_PROCESSOR`.

This is why `sendEnableState()` and `doEnable()` are central to understanding the original init sequence. They are not merely convenience wrappers around register writes; they implement the synchronization between the host and the device-side processor.

### 7. Detect or select the input source

When the device reaches the `0x27f97b` stage for the first time, the original implementation interprets bits collected by `sendEnableState()` as cable/input detection information.

The logic distinguishes:

- HDMI;
- Component;
- Composite;
- no detected signal.

If the user requested `auto`, the detected source becomes the current input. If a source was explicitly selected, that selection is forced even if autodetection disagrees.

This is an important difference from simply saying “input configuration happens in `input.c`”. In the original implementation, **input selection is part of the hardware bring-up state machine**, before the large input-specific configuration functions are entered.

### 8. Initialize common transcoder defaults before loading encoder firmware

Once the device reaches the appropriate processor state, the original sequence calls:

`transcoderDefaultsInitialize()`

from `src/gchd/transcoder.cpp`.

This establishes common transcoder parameters before the encoder firmware is loaded. The function programs defaults for things such as transport-stream behavior, clocking, video/audio paths, and other transcoder state.

This is deliberately separate from `transcoderSetup()` and `transcoderFinalConfigure()`: the latter two depend on the selected input/mode and are performed later.

### 9. Transition into encoder initialization and load encoder firmware

The original sequence then sends `SCMD_INIT`, loads the encoder firmware with `dlfirm(firmwareEnc_)`, and performs a small set of post-firmware reads.

The device can temporarily fall back to the earlier mailbox mode during this transition. Therefore the original code does not assume that the first state read after the firmware transfer is already the final encoder state; it polls until the expected state is reached.

This is one of the most important details to preserve when porting the sequence: firmware upload and firmware activation are separate phases with observable intermediate states.

### 10. Enable the encoder and trigger encoder-side initialization

After the encoder firmware is active, the original implementation:

1. sets `EB_ENCODER_ENABLE`;
2. waits for the device to return to the expected mailbox state;
3. performs a series of mailbox commands on ports such as `0x33` and `0x44`;
4. sets `EB_ENCODER_TRIGGER`;
5. waits for a specific acknowledgement value;
6. clears `EB_ENCODER_TRIGGER`.

The trigger bit is intentionally edge-like in the reverse-engineered protocol: it is set for a short operation and then cleared. Do not treat it as a persistent “encoder enabled” flag. The persistent encoder state is represented separately by `EB_ENCODER_ENABLE`.

For the low-level mechanics, read `src/gchd/commands.cpp` rather than trying to infer the protocol from the kernel wrapper names.

### 11. Run the common hardware/mailbox setup

After the processor and encoder are alive, `configureDevice()` executes a long common setup sequence consisting of mailbox writes, reads, bank changes, and register accesses.

Much of this sequence is still only partially understood. The original source explicitly labels some operations as unknown or as likely subroutines, and some groups were reconstructed from USB captures.

For documentation and porting purposes, the useful way to think about this region is:

```text
encoder processor running
        │
        ├── common device/mailbox configuration
        ├── hardware block setup
        ├── source/mux enable bits
        └── prepare input-specific configuration
```

The actual register sequence is in `src/gchd/configure.cpp`, roughly the middle of `GCHD::configureDevice()`.

### 12. Configure the selected physical input

Near the end of the common bring-up, the original code sets the source-related enable bits and dispatches to exactly one of:

```text
InputSource::HDMI      -> configureHDMI()
InputSource::Component -> configureComponent()
InputSource::Composite -> configureComposite()
```

The source files are:

- `src/gchd/configure_hdmi.cpp`
- `src/gchd/configure_component.cpp`
- `src/gchd/configure_composite.cpp`

These functions are not simple “set input” functions. Each one contains a substantial reverse-engineered sequence for the corresponding signal path.

#### HDMI

`configureHDMI()` reads HDMI signal information and determines the relevant video characteristics. It then configures HDMI-specific hardware registers/mailboxes, color space, and final transcoder state.

At the end it calls `transcoderFinalConfigure()`, `transcoderSetup()`, performs `SCMD_INIT` with mode `0xa0`, and transitions the device to `SCMD_STATE_START` using `completeStateChange()`.

#### Component

`configureComponent()` performs component-specific signal detection and mode interpretation. It maps measured signal values to supported modes such as 1080i, 1080p, 720p, PAL, and NTSC, then merges autodetected information with explicit user settings.

It also selects color space, programs the mode-dependent component registers, runs common setup blocks, configures the transcoder, and finally transitions the state machine to `SCMD_STATE_START`.

#### Composite

`configureComposite()` performs a smaller autodetection step based on the composite status value. It identifies NTSC/PAL, merges that with requested settings, performs the composite-specific mailbox/register sequence, then runs the common setup blocks and final transcoder configuration before entering `SCMD_STATE_START`.

### 13. Configure the transcoder for the actual capture mode

The final transcoder configuration is intentionally later than the initial defaults.

`src/gchd/transcoder.cpp` has three conceptually different stages:

| Function | Purpose |
| --- | --- |
| `transcoderDefaultsInitialize()` | common defaults needed while bringing up the transcoder/encoder |
| `transcoderFinalConfigure()` | final output-related configuration, including video/audio PID and output selection |
| `transcoderSetup()` | mode-dependent video/audio encoder parameters derived from the selected input settings |

The selected resolution, scan mode, refresh rate, bitrate, H.264 profile/level, audio bitrate, and related settings flow into this stage.

### 14. Enter the actual streaming state

The original input-specific configure functions finish with the same essential state transition:

```text
SCMD_INIT (mode 0xa0)
       │
       v
read SCMD_STATE_READBACK_REGISTER
       │
       v
SCMD_STATE_CHANGE -> SCMD_STATE_START
       │
       v
completeStateChange(..., SCMD_STATE_START)
```

The expected resulting state is `SCMD_STATE_START`.

This is the point where the hardware has been configured for the selected input/mode and the encoder output is enabled. It is **not** the same thing as the initial USB probe or idle-firmware initialization.

### 15. Userspace then starts receiving the MPEG-TS stream

After `GCHD::init()` returns, `main.cpp` creates a `Streamer` and calls `streamer.loop()`.

`GCHD::stream()` performs a libusb bulk transfer from endpoint `0x81` into a userspace buffer. The userspace streamer then handles the resulting transport stream.

This is the boundary where the original architecture differs strongly from the kernel driver: the original project has a separate userspace streaming loop, while the DKMS driver moves the USB receive path and MPEG-TS/H.264 extraction into the kernel.

## Original init sequence at a glance

```text
main.cpp
  │
  ├─ GCHD::checkDevice()
  │    ├─ openDevice()             find VID/PID + select DeviceType
  │    └─ checkFirmware()          select idle + encoder firmware
  │
  └─ GCHD::init()
       ├─ getInterface()            detach/set configuration/claim interface
       └─ setupConfiguration()
            └─ configureDevice()
                 │
                 ├─ read version + select register bank
                 ├─ inspect current state
                 ├─ [if cold] load idle firmware
                 ├─ force/reset to IDLE
                 ├─ mailbox handshake
                 ├─ enable firmware processor
                 ├─ detect/select input source
                 ├─ transcoderDefaultsInitialize()
                 ├─ SCMD_INIT
                 ├─ load encoder firmware
                 ├─ wait for encoder state
                 ├─ enable encoder
                 ├─ encoder trigger + mailbox setup
                 ├─ common hardware configuration
                 ├─ configure HDMI / Component / Composite
                 │       ├─ signal/mode detection
                 │       ├─ input-path register programming
                 │       └─ color-space setup
                 ├─ transcoderFinalConfigure()
                 ├─ transcoderSetup()
                 ├─ SCMD_INIT (0xa0)
                 └─ SCMD_STATE_START

  streamer.loop()
       └─ GCHD::stream() -> USB bulk IN 0x81
```

## How this maps to the DKMS driver

The kernel driver should be read as an implementation of the above hardware protocol, not as a replacement specification.

| Original userspace | Kernel driver |
| --- | --- |
| `GCHD::openDevice()` | USB ID table + `gchd_probe()` |
| `GCHD::checkFirmware()` | firmware selection in `usb.c` |
| `GCHD::getInterface()` | kernel USB interface binding |
| `GCHD::configureDevice()` | `gchd_hw_init()` + input/encoder setup |
| `read_config()` / `write_config()` | `gchd_ctrl_read()` / `gchd_ctrl_write()` |
| `mailWrite()` / `mailRead()` | `gchd_mail_write()` / `gchd_mail_read()` |
| `scmd()` / `stateConfirmedScmd()` / `completeStateChange()` | state helpers in `usb.c` |
| `dlfirm()` | `gchd_load_firmware()` |
| `transcoderDefaultsInitialize()` | `gchd_transcoder_init()` |
| `configureHDMI()` / Component / Composite | input configuration in `input.c` |
| `transcoderFinalConfigure()` / `transcoderSetup()` | transcoder/input configuration in `transcoder.c` and `input.c` |
| `GCHD::stream()` | `gchd_rx()` |
| userspace streamer / output handling | MPEG-TS parser + ring + videobuf2 in `core.c` |

If the kernel implementation appears to have lost a hardware operation, compare the corresponding original function rather than guessing a new sequence.

## Low-level protocol pieces worth understanding

### Control transfers

The original `read_config()` and `write_config()` wrappers issue USB control transfers to access device registers. They also handle big-endian conversion when reconstructing integer values.

The kernel equivalents live in `usb.c` and are the lowest layer used by almost every higher-level hardware operation.

### Mailbox

`mailWrite()` and `mailRead()` communicate with internal device processors through mailbox ports. The enable-state register is also part of this synchronization mechanism on the original Game Capture HD.

When debugging a sequence that appears to “hang”, inspect mailbox readiness and completion before assuming that the following register write is wrong.

### State machine

The `SCMD_*` commands are the device-level state machine. `SCMD_IDLE`, `SCMD_INIT`, `SCMD_RESET`, and `SCMD_STATE_CHANGE` are used to move between initialization and encoding states.

`completeStateChange()` is especially important because sending a state command is not sufficient: the original code waits for the device completion indication, reads the resulting state, acknowledges the sticky completion bit, and verifies that the expected state was reached.

### Enable bits

The `EB_*` definitions in `src/gchd_hardware.hpp` document the reverse-engineered meaning of the main enable bits. In particular:

- `EB_FIRMWARE_PROCESSOR` — enables the processor whose firmware is being loaded;
- `EB_ANALOG_INPUT` — selects the analog-input side versus HDMI;
- `EB_ENCODER_ENABLE` — enables the encoder;
- `EB_ENCODER_TRIGGER` — transient encoder initialization trigger;
- `EB_COMPOSITE_MUX` — composite mux selection;
- `EB_ANALOG_MUX` — analog input mux selection.

Some of these meanings are explicitly documented as educated guesses in the original source. Preserve that uncertainty in the kernel documentation rather than turning hypotheses into facts.

## Capture data path

After the original hardware sequence reaches `SCMD_STATE_START`, the device emits an MPEG transport stream over USB bulk endpoint `0x81`.

In the original userspace implementation:

```text
Elgato hardware
      │
      └── USB bulk IN 0x81
             │
             v
        GCHD::stream()
             │
             v
       Streamer / userspace processing
```

In the kernel driver this becomes:

```text
Elgato hardware
      │
      └── USB bulk IN 0x81
             │
             v
          gchd_rx()
             │
             v
       188-byte MPEG-TS
             │
             v
          gchd_ts()
             │
             v
       video PID 0x1011 / PES
             │
             v
       gchd_ring_push()
             │
             v
       gchd_deliver()
             │
             v
          videobuf2
             │
             v
          /dev/video*
```

The kernel receive path therefore corresponds to the **post-init** portion of the original project, not to the hardware bring-up itself.

## Source files in the original project

| File | Role |
| --- | --- |
| `src/main.cpp` | program orchestration, settings, creation of `GCHD`, and start of streaming |
| `src/gchd.cpp` | libusb device discovery, firmware-file selection, interface claiming, top-level lifecycle, USB bulk reads |
| `src/gchd.hpp` | `GCHD` class and hardware-operation declarations |
| `src/gchd_hardware.hpp` | USB IDs, firmware constants, register definitions, enable bits, state constants |
| `src/gchd/configure.cpp` | top-level hardware initialization and shutdown sequence |
| `src/gchd/commands.cpp` | control transfers, mailbox, enable-state synchronization, SCMD state machine, firmware download, stream stop |
| `src/gchd/transcoder.cpp` | transcoder defaults and mode-dependent encoder/output configuration |
| `src/gchd/configure_hdmi.cpp` | HDMI signal detection and HDMI hardware configuration |
| `src/gchd/configure_component.cpp` | Component signal detection and mode-specific configuration |
| `src/gchd/configure_composite.cpp` | Composite signal detection and configuration |
| `src/gchd/settings.cpp` / `.hpp` | input/transcoder settings and validation/merging of autodetected values |
| `src/streamer.cpp` | userspace streaming/output loop |

The `src/gchd/psi_*` files implement MPEG PSI-related structures/parsing. They are useful when investigating transport-stream semantics, but they are not the place to start when investigating device initialization.

## Kernel driver source files

### `core.c`

Main V4L2-side implementation: V4L2 registration, streaming callbacks, receive thread, MPEG-TS/PES extraction, encoded-frame ring, and delivery to videobuf2.

Important functions include `gchd_probe()`, `gchd_start()`, `gchd_stop()`, `gchd_rx()`, `gchd_ts()`, `gchd_deliver()`, and `gchd_v4l2_register()`.

### `usb.c`

Low-level device protocol and the kernel-side home for the original userspace hardware-control logic: USB control transfers, mailbox, state commands, enable state, firmware loading, initialization, and shutdown.

### `input.c`

Kernel-side port of input-specific configuration. This is where the HDMI/Component/Composite hardware sequences currently live after being moved out of the original C++ organization.

### `transcoder.c`

Kernel-side common transcoder setup. Compare it primarily with `src/gchd/transcoder.cpp` when validating register programming or encoder defaults.

### `fops.c`

V4L2/videobuf2 file operations and the glue between the video node and the driver.

### `elgato_gchd.h`

Shared kernel-driver structures, constants, USB IDs, firmware names, ring-buffer definitions, and declarations.

### `Makefile` / `dkms.conf`

Kernel module build and DKMS packaging metadata.

## Firmware

The original project expects an idle image and an encoder image. The kernel driver uses Linux firmware loading rather than reading files directly from the filesystem.

For firmware-related debugging, compare:

```text
original:  src/gchd.cpp -> checkFirmware() / configure.cpp -> dlfirm()
kernel:    usb.c       -> firmware selection / gchd_load_firmware()
```

## Debugging guide

### Device does not bind

Start with the kernel USB ID table and `gchd_probe()`. If it binds but fails immediately, compare the early part of `GCHD::init()` and `configureDevice()` in the original project.

### Firmware upload fails

Compare the kernel firmware path with `src/gchd/commands.cpp` → `dlfirm()`. Verify the selected hardware family and the idle-versus-encoder firmware stage.

### State transition times out

Compare the kernel state helper with `src/gchd/commands.cpp` → `scmd()`, `stateConfirmedScmd()`, and `completeStateChange()`. Check whether the driver is waiting for the same completion condition and acknowledging the same sticky state bit.

### Mailbox operation hangs

Compare `gchd_mail_write()` / `gchd_mail_read()` with `mailWrite()` / `mailRead()`, including readiness handling and the HDNew-specific path.

### `/dev/video*` exists but streaming fails

Start at the V4L2 stream-on path:

```text
VIDIOC_STREAMON
    -> gchd_start()
    -> input configuration
    -> encoder/state start
```

Then compare the selected input path against the original `configureHDMI()`, `configureComponent()`, or `configureComposite()`.

### Streaming starts but no frames arrive

Follow `gchd_rx()` → MPEG-TS parsing → PID `0x1011` → PES/frame buffering → videobuf2 delivery. At this point the hardware should already have completed the original init sequence and reached `SCMD_STATE_START`.

### Signal or mode detection is wrong

Do not start by changing the V4L2 code. Compare the corresponding original input-specific implementation first. The original project contains the signal measurements, mode mapping, and hardware register programming that the kernel driver is intended to reproduce.

## Shutdown in the original project

The reverse path is also useful when implementing or debugging disconnect/stream-stop handling.

`GCHD::uninitDevice()` first checks the state. If the device is in `SCMD_STATE_START` or `SCMD_STATE_NULL`, it calls `stopStream(true)`.

`stopStream()` changes the output state to `SCMD_STATE_NULL`, waits for completion while draining USB data, then changes the device to `SCMD_STATE_STOP` and waits again. `uninitDevice()` then performs the remaining reset/disable sequence before the USB interface is released.

This is why the original shutdown path should be treated as part of the protocol too; simply stopping the receive thread is not equivalent to putting the hardware back into a clean state.

## Build

Install matching Linux kernel headers and DKMS.

### DKMS

```sh
sudo dkms add ./drivers/elgato_gchd
sudo dkms install elgato-gchd/0.2.0
```

### Direct kernel-module build

```sh
make -C /lib/modules/$(uname -r)/build M=$PWD/drivers/elgato_gchd modules
```

## Current limitations

- The kernel driver is still experimental and needs hardware validation.
- HDMI signal detection is not yet a complete translation of the original implementation.
- Full per-mode calibration/configuration behavior is still being brought over.
- V4L2 controls establish the Linux-facing interface, but do not necessarily program every encoder parameter yet.
- The original userspace project remains the reference when a hardware sequence is unclear or appears incomplete in the kernel port.
