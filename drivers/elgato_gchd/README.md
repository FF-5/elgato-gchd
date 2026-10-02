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

## Hardware initialization and configuration

The sequence below describes the **hardware protocol that the DKMS driver must reproduce**, using the names used by the kernel driver. It is derived from the original userspace implementation, but it deliberately does not reproduce the userspace call graph or C++ API.

The important rule is: preserve the original ordering and synchronization points, while expressing each operation through the DKMS driver's state (`struct gchd`), register helpers, mailbox helpers, state commands, enable masks, and firmware helpers.

### 1. Establish the device state

Start from `gchd_hw_init()`.

Before changing the device, the driver should:

- select the base register bank with `BANKSEL_INDEX`;
- capture the current `ENABLE_STATE_INDEX` into `d->hw_enable_state`;
- capture the current `ENABLE_INDEX` into `d->hw_enable_register`;
- read `STATE_INDEX` and reduce it to the device state bits;
- handle the HDNew boot-state completion handshake before attempting firmware transfer.

The saved enable values matter later: the original protocol expects the host to preserve and reapply the device-side enable state during processor bring-up.

### 2. Bring a cold device through the idle-firmware stage

When the state read from `STATE_INDEX` is zero, treat the device as being at the boot boundary.

For HDNew, first complete the boot-state synchronization: consume the interrupt notification when applicable, wait for `STATE_COMPLETE_INDEX` bit `0x0004`, and acknowledge that completion. Only after this synchronization should firmware transfer begin.

Then select the idle image from the device family and call `gchd_load_firmware()` with the appropriate `FW_IDLE_*` value.

After the firmware transfer, perform the required post-firmware register access before changing state. On HDNew this includes refreshing `d->hw_enable_state` and `d->hw_enable_register` and completing the startup reads used by the original protocol.

For a device that was not cold, do not reload idle firmware. Reset the existing state with `gchd_state_cmd(..., SCMD_RESET, ...)` and continue through the same idle-state entry used by the cold path.

### 3. Enter the processor idle state

Use `gchd_state_cmd()` to place the device into the idle state expected by the original sequence.

Then obtain the processor/mode value through the DKMS mailbox helper. If the processor reports the initial startup value, use `gchd_enable_analog()` followed by `gchd_do_enable()` with `EB_FIRMWARE_PROCESSOR` to bring the firmware processor online.

Do not collapse these operations into a generic 'enable firmware' step. The mailbox state and the enable register are two parts of the same handshake.

### 4. Initialize the transcoder's common state

Run `gchd_transcoder_init()` before the encoder-specific configuration. This corresponds to the original common transcoder-default stage.

The purpose of this stage is to establish the baseline transcoder state that must exist before the encoder firmware and input-specific configuration are applied. Keep this separate from the later mode-specific settings.

### 5. Start the encoder processor

Encoder startup is a distinct phase. The DKMS sequence should:

1. enable the processor/encoder prerequisites with `gchd_do_enable()`;
2. enter the encoder initialization state with the appropriate `gchd_scmd()`/state command;
3. load the encoder image with `gchd_load_encoder_firmware()`;
4. enable the encoder through the appropriate `EB_*` bits;
5. use `gchd_mail_write()` and `gchd_mail_read()` on mailbox `0x33` to wait for the intermediate encoder-ready state;
6. perform the second mailbox wait until the device reaches the encoder-ready value;
7. clear the transient encoder trigger/processor enable as required by the protocol.

`gchd_encoder_start()` is the DKMS implementation of this phase. The important point for future changes is that firmware transfer, encoder enable, mailbox acknowledgement, and trigger handling are separate protocol stages.

### 6. Configure the selected input path

Once the encoder processor is ready, configuration follows the selected `d->input` path.

`gchd_input_configure()` should be understood as an ordered hardware transaction, not merely as a collection of input presets.

For each input path, the sequence is:

1. issue the input-specific mailbox/register setup with `gchd_seq()` and the corresponding mailbox commands;
2. configure the common hardware sub-blocks with `gchd_setup_subblock()`;
3. select the required color path with `gchd_color_yuv()` or the corresponding input-specific color configuration;
4. program the dimensions and mode-dependent registers with `gchd_mode_regs()`;
5. start/handshake the encoder with `gchd_encoder_start()` where required by the current implementation;
6. execute the post-encoder setup and calibration stages;
7. finalize the input state with `gchd_input_finalize()`.

The original userspace sequence must be treated as the ordering authority here. The DKMS implementation should preserve its waits and readbacks even when several low-level writes are represented by one helper such as `gchd_seq()`.

### 7. Preserve the input-specific differences

The three physical input paths are not interchangeable. Their early mailbox/register setup differs, while they converge on the common configuration stages.

Use the driver's `d->input` selection rather than reproducing userspace `InputSource` types:

| `d->input` path | Configuration intent |
| --- | --- |
| HDMI path | establish the digital input path and its detected video characteristics |
| Component path | establish the analog component path and its detected mode/color characteristics |
| Composite path | establish the composite path and its detected PAL/NTSC characteristics |

The input-specific configuration must occur after the processor/encoder prerequisites are established and before the final capture state is entered.

### 8. Apply mode and encoder parameters

Mode information gathered during input configuration is represented by the driver's input state, including values such as `d->input_width`, `d->input_height`, `d->input_fps_num`, and `d->input_fps_den`.

Use that state when applying the encoder/transcoder parameters. The common transcoder defaults from `gchd_transcoder_init()` are not the final capture configuration.

Mode-dependent encoder parameters such as H.264 profile/level, bitrate, dimensions, timing, and audio-related settings should be applied after the corresponding input mode is known and before the device enters the final streaming state.

### 9. Enter the capture-ready state

After input configuration and final encoder/transcoder setup, use the DKMS state helpers to perform the final state transition.

The required behavior is:

1. issue the final initialization/state command with the mode required by the original protocol;
2. read the state/completion indication using the driver's state helpers;
3. request the streaming/start state with `gchd_state_cmd()` or the equivalent DKMS helper;
4. wait until the requested state is confirmed;
5. acknowledge any sticky completion indication before considering initialization complete.

The significant point is that `SCMD_STATE_START` is the **end of hardware configuration**, not the beginning of USB probing. A successful `gchd_hw_init()` alone does not mean the capture path is configured for a particular input mode.

### 10. Stream only after the hardware state is complete

Once the device has reached the final start state, the receive path can consume USB bulk endpoint `0x81`.

The DKMS data path is:

```text
USB bulk IN 0x81
      -> gchd_rx()
      -> MPEG-TS packets
      -> gchd_ts()
      -> video PID / PES extraction
      -> gchd_ring_push()
      -> gchd_deliver()
      -> videobuf2
      -> /dev/video*
```

Keep this separate from hardware configuration. If `gchd_rx()` is receiving data but no frames are delivered, the problem is downstream of the hardware initialization sequence; if no valid transport stream arrives, investigate the state/encoder/input configuration first.

## Initialization ownership in the DKMS driver

The original protocol spans several kernel functions. The following is the useful mental model when navigating the driver:

| Hardware phase | DKMS implementation |
| --- | --- |
| Save initial device state | `gchd_hw_init()` → `d->hw_enable_state`, `d->hw_enable_register` |
| Boot-state synchronization | `gchd_interrupt_pend()`, `STATE_COMPLETE_INDEX` handling |
| Idle firmware | `gchd_load_firmware()` with `FW_IDLE_OLD` / `FW_IDLE_NEW` |
| Reset/idle state machine | `gchd_state_cmd()` / `gchd_scmd()` |
| Processor bring-up | `gchd_processor_state()`, `gchd_enable_analog()`, `gchd_do_enable()` |
| Common transcoder defaults | `gchd_transcoder_init()` |
| Encoder firmware and handshake | `gchd_encoder_start()` / `gchd_load_encoder_firmware()` |
| Mailbox protocol | `gchd_mail_write()`, `gchd_mail_read()` |
| Input configuration | `gchd_input_configure()` |
| Input-specific register/mailbox sequence | `gchd_seq()` and input helpers in `input.c` |
| Mode/color configuration | `gchd_mode_regs()`, `gchd_color_yuv()` |
| Post-encoder calibration | `gchd_post_encoder_setup()`, `gchd_post_encoder_calibration()`, `gchd_post_encoder_sweep()` |
| Final input state | `gchd_input_finalize()` |
| Stream-on state transition | `gchd_start()` + `gchd_state_cmd()` |
| USB receive | `gchd_rx()` |

This table is intentionally organized by **hardware phase**, not by source-file order. When debugging initialization, follow the phase sequence from top to bottom.

## Configuration sequence during stream-on

Hardware initialization and capture-mode configuration are separate.

`gchd_probe()` establishes the driver, performs the hardware-level initialization, registers the V4L2 device, and prepares the driver for use. The actual input/mode setup is performed when streaming starts.

The stream-on path should therefore be understood as:

```text
V4L2 stream start
      -> gchd_start()
      -> gchd_input_configure()
           -> input-specific setup
           -> common sub-block setup
           -> color/mode setup
           -> encoder handshake
           -> post-encoder calibration
           -> final input configuration
      -> final state transition
      -> gchd_rx()
```

This is deliberately not a copy of the current `gchd_start()` implementation. It describes the sequence that the DKMS driver is expected to execute while preserving the ordering established by the original hardware protocol.

## Shutdown sequence

Shutdown is the reverse hardware protocol, not merely stopping the receive thread.

The driver should first stop the capture state, then clear transient encoder state, disable the encoder and firmware processor through `gchd_do_enable()`, clear `ENABLE_STATE_INDEX`, and finally return the device through the appropriate idle/reset state commands.

`gchd_hw_shutdown()` is the kernel entry point for this cleanup. When stream state is active, the higher-level stream-stop path must first perform the capture-state transition before hardware shutdown clears the processor/encoder enables.

## Using the original project as the protocol reference

The original userspace project remains useful as the source of **ordering and meaning**, but the DKMS README should be read entirely in terms of the kernel driver's state and helpers.

When investigating a missing or suspicious operation:

1. identify the hardware phase in the tables above;
2. find the corresponding DKMS helper;
3. compare that helper's ordering, waits, mailbox exchanges, and state acknowledgements with the original protocol;
4. preserve the DKMS abstractions (`struct gchd`, `d->input`, `d->family`, `d->hw_enable_state`, `d->hw_enable_register`, `EB_*`, `SCMD_*`, `FW_*`) rather than introducing userspace concepts into the kernel documentation.

The original source should therefore answer **what must happen and in what order**; the DKMS source should answer **how this driver performs each step**.

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
