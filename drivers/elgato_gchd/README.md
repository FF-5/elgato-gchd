# Experimental native V4L2/DKMS driver

This directory contains the experimental native Linux kernel driver for the Elgato Game Capture HD.

The most important thing to understand when working on this driver is that the device protocol was reverse-engineered in the original libusb userspace implementation. The kernel driver is a port of that protocol, not the authoritative description of what the hardware is supposed to do.

The README describes the hardware initialization and capture setup as a generalized protocol sequence expressed in DKMS terminology. The original userspace project remains the protocol/ordering reference, but implementation and debugging should start in the DKMS files.

> **Status:** experimental / pre-hardware-validation. The kernel-side plumbing is in place, but some device-specific signal detection and mode/calibration behavior is still being brought over from the original implementation.

## Where to look

The entries below deliberately point to the **kernel driver**, not the original userspace source. Use the original project only when you need to verify protocol ordering or the meaning of a low-level operation.

| Question | Start here |
| --- | --- |
| Where does kernel-side device initialization begin? | `drivers/elgato_gchd/usb.c` → `gchd_hw_init()` |
| Where is the initial enable/state information saved? | `drivers/elgato_gchd/usb.c` → `gchd_hw_init()` → `d->hw_enable_state`, `d->hw_enable_register` |
| Where is the boot completion handshake handled? | `drivers/elgato_gchd/usb.c` → `gchd_interrupt_pend()`, `STATE_COMPLETE_INDEX` handling |
| Where is idle firmware loaded? | `drivers/elgato_gchd/usb.c` → `gchd_load_firmware()`, `FW_IDLE_OLD`, `FW_IDLE_NEW` |
| Where are reset/idle state transitions performed? | `drivers/elgato_gchd/usb.c` → `gchd_state_cmd()`, `gchd_scmd()` |
| Where is processor state checked/enabled? | `drivers/elgato_gchd/usb.c` → `gchd_processor_state()`, `gchd_enable_analog()`, `gchd_do_enable()` |
| Where are common transcoder defaults initialized? | `drivers/elgato_gchd/transcoder.c` → `gchd_transcoder_init()` |
| Where is the encoder processor started? | `drivers/elgato_gchd/input.c` → `gchd_encoder_start()` |
| Where is encoder firmware loaded? | `drivers/elgato_gchd/input.c` → `gchd_load_encoder_firmware()` |
| Where is mailbox communication implemented? | `drivers/elgato_gchd/usb.c` → `gchd_mail_write()`, `gchd_mail_read()` |
| Where is input-specific configuration performed? | `drivers/elgato_gchd/input.c` → `gchd_input_configure()` |
| Where are input-specific mailbox/register sequences issued? | `drivers/elgato_gchd/input.c` → `gchd_seq()` and the `d->input` branches |
| Where are common sub-blocks configured? | `drivers/elgato_gchd/input.c` → `gchd_setup_subblock()` |
| Where are color/mode registers programmed? | `drivers/elgato_gchd/input.c` → `gchd_color_yuv()`, `gchd_mode_regs()` |
| Where is post-encoder setup/calibration performed? | `drivers/elgato_gchd/input.c` → `gchd_post_encoder_setup()`, `gchd_post_encoder_calibration()`, `gchd_post_encoder_sweep()` |
| Where is the final input configuration state established? | `drivers/elgato_gchd/input.c` → `gchd_input_finalize()` |
| Where does V4L2 stream-on enter hardware setup? | `drivers/elgato_gchd/core.c` → `gchd_start()` |
| Where is the final capture/start state requested? | `drivers/elgato_gchd/core.c` / `usb.c` → `gchd_start()` → `gchd_state_cmd()` |
| Where does USB capture begin? | `drivers/elgato_gchd/core.c` → `gchd_rx()` |
| Where is MPEG-TS/PES processing performed? | `drivers/elgato_gchd/core.c` → `gchd_ts()`, `gchd_ring_push()`, `gchd_deliver()` |
| Where is hardware shutdown implemented? | `drivers/elgato_gchd/usb.c` → `gchd_hw_shutdown()` |

## Original protocol sequence — detailed reference\n\nThe sequence below is the original userspace protocol, reconstructed from the reverse-engineered project. It is intentionally expressed without C++ function names. It is the reference sequence against which the DKMS implementation should be compared.\n\nThe DKMS driver may combine several original operations into one helper, but it must preserve the observable ordering, branches, waits, mailbox exchanges, register readbacks, and state transitions described here.\n\n### 1. USB/device establishment\n\n```text\nDetect USB device\n    |\n    +-- Original Game Capture HD family\n    |      -> original idle firmware\n    |      -> original encoder firmware\n    |\n    +-- HDNew family\n           -> HDNew idle firmware\n           -> HDNew encoder firmware\n    |\n    v\nInitialize USB transport\n    |\n    v\nDetach an existing interface driver if required\n    |\n    v\nSelect USB configuration 1\n    |\n    v\nClaim interface 0\n    |\n    v\nDevice available for protocol initialization\n```\n\nThe original project recognizes the older Game Capture HD PIDs as one family and PID 0x005d as the newer revision. Its firmware selection is family-specific. The original project explicitly treats the HD60 and HD60 S as unsupported.\n\n### 2. Read the initial hardware state\n\n```text\nSelect register bank 0\n        |\n        v\nRead ENABLE_STATE\n        |\n        v\nRead ENABLE\n        |\n        v\nRead STATE\n        |\n        v\nMask STATE to the device-state bits\n```\n\nThe enable-state values are retained because they participate in later processor/firmware synchronization.\n\nAt this point the protocol branches:\n\n```text\nSTATE == 0\n    |\n    +--> cold/boot boundary\n    +--> load IDLE firmware\n    +--> establish post-firmware state\n    +--> continue to IDLE\n\nSTATE != 0\n    |\n    +--> existing firmware/state\n    +--> issue RESET\n    +--> continue to IDLE\n```\n\n### 3. HDNew cold-boot synchronization\n\nFor HDNew, a cold device has an additional completion path before the firmware transfer.\n\n```text\nSTATE == 0\n    |\n    v\nPend/wait for device interrupt\n    |\n    v\nPoll STATE_COMPLETE\n    |\n    v\nWait until completion bit 0x0004 is set\n    |\n    v\nAcknowledge/clear 0x0004\n    |\n    v\nLoad IDLE firmware\n```\n\nThis is a synchronization point, not optional diagnostic traffic.\n\n### 4. Load the IDLE firmware\n\nSelect the IDLE image according to hardware family:\n\n```text\nOriginal HD  -> mb86h57_h58_idle.bin\nHDNew        -> mb86m01_assp_nsec_idle.bin\n```\n\nThe firmware transfer is followed by the original post-firmware register operations before the normal state machine is entered.\n\nFor HDNew, the enable-state and enable-register values are refreshed and the initial control/register reads are performed after the firmware transfer.\n\n### 5. Reset an already-initialized device\n\nIf the initial state was non-zero, the original protocol does not upload IDLE firmware again.\n\n```text\nexisting state\n    |\n    v\nRESET command\n    |\n    v\nwait for reset completion/state\n    |\n    v\ncontinue with common IDLE entry\n```\n\nThe cold and warm paths therefore converge only after their respective firmware/reset operations have completed.\n\n### 6. Enter IDLE\n\nBoth paths converge on the common IDLE transition:\n\n```text\ndevice/firmware ready\n        |\n        v\nIDLE state command\n        |\n        v\nwait for completion\n        |\n        v\nverify IDLE state\n```\n\nThe original protocol uses the stopped/idle state as the stable base state before enabling the firmware processor.\n\n### 7. Establish firmware-processor state\n\nThe processor state is obtained through the device mailbox/status mechanism.\n\nThe important observed values are:\n\n```text\n0x334455\n    = processor/firmware startup state\n\n0x27F97B\n    = processor ready state used by the initialization sequence\n```\n\n```text\nread processor state\n       |\n       +-- 0x334455\n       |      |\n       |      v\n       |   configure analog/input-side enable state\n       |      |\n       |      v\n       |   enable firmware processor\n       |\n       +-- continue polling\n              |\n              v\n          0x27F97B\n```\n\nThe processor enable and the mailbox state are one synchronization sequence. Do not document them as an unrelated enable followed by an unrelated poll.\n\n### 8. Establish common transcoder defaults\n\nBefore encoder firmware configuration, the original protocol establishes the common transcoder defaults.\n\n```text\nprocessor ready\n      |\n      v\ncommon transcoder defaults\n      |\n      v\nencoder initialization\n```\n\nThese defaults are not the final input/mode configuration.\n\n### 9. Enter encoder initialization state\n\nThe encoder startup has a strict ordering:\n\n```text\nenable encoder/processor prerequisites\n        |\n        v\nenter encoder initialization state\n        |\n        v\nload ENCODER firmware\n        |\n        v\nenable encoder\n        |\n        v\npoll mailbox/processor state\n```\n\nThe encoder firmware is family-specific:\n\n```text\nOriginal HD  -> mb86h57_h58_enc_h.bin\nHDNew        -> mb86m01_assp_nsec_enc_h.bin\n```\n\n### 10. Wait for encoder firmware readiness\n\nThe original implementation uses repeated mailbox transactions to determine when the encoder firmware has reached the required state.\n\n```text\nwrite processor-state request\n        |\n        v\nread processor state\n        |\n        v\ntest masked ready condition\n        |\n        +-- not ready -> repeat\n        |\n        +-- ready     -> continue\n```\n\nThe implementation has a bounded retry count. A timeout/failure is therefore a protocol failure, not permission to continue as if the encoder were ready.\n\n### 11. Enable and trigger the encoder\n\n```text\nset encoder enable\n        |\n        v\npoll processor state\n        |\n        v\nwait for encoder-ready state\n        |\n        v\nperform encoder mailbox initialization\n        |\n        v\nassert encoder trigger\n        |\n        v\npoll mailbox for trigger completion\n        |\n        v\ndeassert encoder trigger\n```\n\nThe trigger is transient. It is not the same thing as the persistent encoder-enable bit.\n\nThe original sequence also contains fixed mailbox exchanges with expected reply bytes. These reads are synchronization/verification points and must not be silently removed when porting the protocol.\n\n### 12. Begin input-front-end configuration\n\nOnly after the processor and encoder prerequisites are established does the protocol enter the source-specific hardware configuration.\n\n```text\n                         input source\n                              |\n             +----------------+----------------+\n             |                |                |\n             v                v                v\n           HDMI           Component        Composite\n             |                |                |\n             +----------------+----------------+\n                              |\n                              v\n                    common final configuration\n```\n\nThe paths are not interchangeable. Their mailbox sequences, detection mechanisms, and placement of common blocks differ.\n\n### 13. HDMI path — front-end and signal detection\n\nThe HDMI path begins with fixed mailbox/register setup and then performs repeated signal measurements.\n\n```text\nHDMI front-end setup\n       |\n       v\nselect measurement bank\n       |\n       v\nsample timing/status registers\n       |\n       v\nrepeat measurements\n       |\n       v\nderive resolution / timing\n       |\n       v\ndetermine RGB/YUV indication\n       |\n       v\nmerge detected values with requested settings\n```\n\nThe original HDMI measurement uses repeated reads of timing-related register pairs. A measurement pass consists of multiple samples, followed by a delay, and the driver repeats the pass until the signal characteristics are sufficiently stable or the retry limit is reached.\n\nThe original source supports HDMI modes 480p60/NTSC, 576p60/PAL, 720p60, 1080i60, and 1080p60.\n\nThe detected mode is merged with explicit user settings. A forced setting takes precedence over an automatically detected value.\n\n### 14. HDMI color-space decision\n\n```text\nread HDMI color-space indication\n        |\n        +-- explicit color-space setting? -> use requested value\n        |\n        +-- no -> RGB indication -> RGB\n                    otherwise    -> YUV\n```\n\nThe original project notes that automatic HDMI/component color-space detection was historically unreliable; this stage must not be confused with a guaranteed hardware truth source.\n\n### 15. Component path — analog detection\n\nComponent uses a different measurement sequence from HDMI.\n\n```text\nComponent front-end setup\n        |\n        v\nselect measurement bank\n        |\n        v\nsample timing register pairs\n        |\n        v\n10 samples per pass\n        |\n        v\nwait approximately 200 ms\n        |\n        v\nrepeat until stable / retry limit\n        |\n        v\ninterpret PAL/NTSC + resolution + scan mode\n```\n\nThe original project supports 480i60/NTSC, 480p60/NTSC, 576i50/PAL, 576p50/PAL, 720p60, 1080i60, and 1080p60.\n\nComponent defaults to YUV when no explicit color-space setting is supplied.\n\n### 16. Composite path — PAL/NTSC detection\n\nComposite does not use the HDMI timing-detection procedure.\n\n```text\nComposite front-end setup\n        |\n        v\nread composite status\n        |\n        v\ninterpret PAL/NTSC indicator\n        |\n        +-- NTSC -> 480i60\n        |\n        +-- PAL  -> 576i50\n```\n\nThe original implementation treats composite as interlaced and performs additional mode-dependent register writes after the PAL/NTSC decision.\n\n### 17. Common hardware blocks\n\nAfter the input-specific early configuration, the source-specific paths converge on common register/mailbox blocks.\n\n```text\ninput-specific setup\n        |\n        v\nsetup sub-block\n        |\n        v\ncommon block A\n        |\n        v\ncommon block B1\n        |\n        v\ncommon block B2\n        |\n        v\ncommon block B3\n        |\n        v\ncommon block C\n```\n\nThe original source sometimes revisits a common block later in the same input path. Those repeated operations are part of the observed protocol and should not be collapsed merely because the resulting register values look identical.\n\n### 18. Mailbox bank selection and readbacks\n\nMany input configuration sequences use mailbox ports 0x33, 0x44, 0x4c, and 0x4e.\n\n```text\nselect mailbox/register bank\n        |\n        v\nwrite command/data\n        |\n        v\nread status/result\n        |\n        v\ninspect expected bits/bytes\n        |\n        v\nselect next bank\n        |\n        v\ncontinue\n```\n\nSeveral reads have explicit expected values in the original reverse-engineering source. They are protocol synchronization points even when the returned byte is not otherwise used.\n\n### 19. Unknown/reverse-engineered blocks\n\nThe original source contains long ordered mailbox sequences whose exact internal meaning was not established by the reverse engineering.\n\n```text\nobserved operation: known\nordering: known\nsemantic meaning: unknown\n```\n\nDo not replace such a block with an invented description unless there is independent evidence. For the DKMS port, preserve the byte sequence and ordering first.\n\n### 20. Final mode-dependent hardware programming\n\nOnce the effective input mode is known, the original protocol applies mode-dependent settings including resolution, progressive/interlaced state, PAL/NTSC timing, input color space, front-end register values, and encoder dimensions/timing.\n\n### 21. Final transcoder configuration\n\n```text\neffective input mode\n        |\n        v\nvideo dimensions / timing\n        |\n        v\nH.264 profile/level\n        |\n        v\nGOP / IDR configuration\n        |\n        v\nrate-control / bitrate configuration\n        |\n        v\naudio configuration\n        |\n        v\ncolor metadata\n        |\n        v\ntransport-stream configuration\n        |\n        v\nfinal transcoder state\n```\n\nThe original transcoder protocol uses masked register updates. This stage is separate from the earlier common transcoder defaults.\n\n### 22. Final initialization and START transition\n\nOnly after all input and transcoder configuration is complete does the original protocol request capture.\n\n```text\nfinal hardware/transcoder configuration\n        |\n        v\nfinal INIT command\n        |\n        v\nread current state\n        |\n        v\nrequest STATE_START\n        |\n        v\nwait for completion\n        |\n        v\nverify resulting START state\n        |\n        v\nstreaming permitted\n```\n\nThe final START state is the boundary between configuration and streaming.\n\n### 23. Runtime capture\n\n```text\ndevice\n  |\n  v\nUSB bulk IN endpoint 0x81\n  |\n  v\nMPEG transport stream\n  |\n  v\npacket/PES processing\n  |\n  v\nvideo delivery\n```\n\n### 24. Stream stop\n\n```text\nSTART / streaming\n        |\n        v\nrequest NULL state\n        |\n        v\nwait for NULL completion\n        |\n        v\nNULL / drain phase\n        |\n        v\nallow buffered USB data to drain\n        |\n        v\nrequest STOP state\n        |\n        v\nwait for STOP completion\n        |\n        v\nSTOPPED\n```\n\nStopping the receive loop alone is not equivalent to this protocol transition.\n\n### 25. Hardware shutdown\n\n```text\nSTOPPED\n   |\n   v\ndisable encoder\n   |\n   v\ndisable encoder trigger\n   |\n   v\ndisable firmware processor\n   |\n   v\nclear device enable-state register\n   |\n   v\nenter IDLE\n   |\n   v\nwait for IDLE completion\n   |\n   v\nissue shutdown RESET\n   |\n   v\nwait for final reset state\n```\n\nThe original protocol therefore has two distinct reset contexts: initialization reset for a device that already contains firmware/state, and final reset after encoder/processor state has been disabled.\n\n## Complete original sequence at a glance\n\n```text\nUSB discovery\n    |\n    v\nUSB configuration + interface claim\n    |\n    v\nselect bank 0\n    |\n    v\nread ENABLE_STATE / ENABLE / STATE\n    |\n    +-----------------------------+\n    |                             |\n STATE == 0                  STATE != 0\n    |                             |\n    v                             v\nHDNew boot completion?         RESET\n    |                             |\nload IDLE firmware                |\n    |                             |\n    +-------------+---------------+\n                  |\n                  v\n                IDLE\n                  |\n                  v\n        processor-state handshake\n                  |\n                  v\n          enable firmware processor\n                  |\n                  v\n            processor ready\n                  |\n                  v\n       common transcoder defaults\n                  |\n                  v\n        encoder initialization\n                  |\n                  v\n        load ENCODER firmware\n                  |\n                  v\n        encoder-ready polling\n                  |\n                  v\n           enable encoder\n                  |\n                  v\n        encoder mailbox setup\n                  |\n                  v\n         pulse encoder trigger\n                  |\n                  v\n       input-specific front-end\n                  |\n          +-------+-------+\n          |       |       |\n          v       v       v\n        HDMI   Component Composite\n          |       |       |\n          +-------+-------+\n                  |\n                  v\n      common/repeated hardware blocks\n                  |\n                  v\n       final input/mode programming\n                  |\n                  v\n        final transcoder programming\n                  |\n                  v\n             final INIT\n                  |\n                  v\n          STATE_CHANGE START\n                  |\n                  v\n              START state\n                  |\n                  v\n              STREAMING\n                  |\n                  v\n          STATE_CHANGE NULL\n                  |\n                  v\n               DRAIN\n                  |\n                  v\n          STATE_CHANGE STOP\n                  |\n                  v\n              STOPPED\n                  |\n                  v\n       disable encoder/processor\n                  |\n                  v\n                 IDLE\n                  |\n                  v\n           final RESET\n                  |\n                  v\n              USB release\n```\n\n## Original protocol vs. DKMS implementation\n\n| Original protocol phase | Current DKMS location |\n| --- | --- |\n| initial bank/state read | usb.c -> gchd_hw_init() |\n| save ENABLE_STATE / ENABLE | usb.c -> d->hw_enable_state, d->hw_enable_register |\n| HDNew cold-boot completion | usb.c -> gchd_interrupt_pend() / STATE_COMPLETE_INDEX |\n| IDLE firmware | usb.c -> gchd_load_firmware() |\n| warm-device reset | usb.c -> gchd_state_cmd() |\n| common IDLE transition | usb.c -> gchd_state_cmd() / gchd_scmd() |\n| processor-state polling | usb.c -> gchd_processor_state() |\n| processor enable | usb.c -> gchd_enable_analog(), gchd_do_enable() |\n| transcoder defaults | transcoder.c -> gchd_transcoder_init() |\n| encoder prerequisites/firmware | input.c -> gchd_encoder_start() |\n| encoder firmware | input.c -> gchd_load_encoder_firmware() |\n| encoder mailbox waits | input.c -> gchd_mail_write() / gchd_mail_read() |\n| input branch | input.c -> gchd_input_configure() |\n| input mailbox sequences | input.c -> gchd_seq() |\n| common input setup | input.c -> gchd_setup_subblock() |\n| color programming | input.c -> gchd_color_yuv() |\n| mode programming | input.c -> gchd_mode_regs() |\n| post-encoder stages | input.c -> gchd_post_encoder_*() |\n| final input state | input.c -> gchd_input_finalize() |\n| final stream state | core.c / usb.c -> gchd_start() / state helpers |\n| bulk capture | core.c -> gchd_rx() |\n| stream stop | core.c / usb.c -> stop/state helpers |\n| hardware shutdown | usb.c -> gchd_hw_shutdown() |\n\n### Important comparison rule\n\nDo not compare only function-to-function. Compare operation ordering, state before/after, mailbox request/response, waits, completion acknowledgement, enable-bit changes, input-specific branches, register-bank changes, repeated blocks, and the final state reached.\n\nA DKMS helper may combine several original userspace operations. It is not equivalent if it merely produces similar final register values while omitting a required intermediate handshake.\n\n## Original-project source map\n\n| Original source | Protocol responsibility |\n| --- | --- |\n| src/gchd.cpp | USB discovery, firmware selection, interface setup, bulk streaming lifecycle |\n| src/gchd/commands.cpp | control transfers, mailbox transport, state machine, enable-state synchronization, firmware download |\n| src/gchd/configure.cpp | top-level initialization/shutdown ordering |\n| src/gchd/configure_hdmi.cpp | HDMI detection/configuration |\n| src/gchd/configure_component.cpp | Component detection/configuration |\n| src/gchd/configure_composite.cpp | Composite detection/configuration |\n| src/gchd/transcoder.cpp | transcoder defaults and final encoder/output parameters |\n| src/gchd/settings.cpp/.hpp | effective input/mode/settings selection |\n| src/gchd_hardware.hpp | protocol constants, state values, enable bits |\n| src/streamer.cpp | userspace stream consumption/output |\n\n## Initialization ownership in the DKMS driver

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

Start with the kernel USB ID table and `drivers/elgato_gchd/core.c` → `gchd_probe()`. If it binds but fails immediately, trace the DKMS initialization path into `drivers/elgato_gchd/usb.c` → `gchd_hw_init()`.

### Firmware upload fails

Start at `drivers/elgato_gchd/usb.c` → `gchd_load_firmware()` / `gchd_load_encoder_firmware()`. Verify the selected hardware family and the idle-versus-encoder firmware stage.

### State transition times out

Start at `drivers/elgato_gchd/usb.c` → `gchd_state_cmd()` / `gchd_scmd()` and trace the completion/state-read helpers. Check whether the driver waits for the required completion condition and acknowledges the sticky state bit.

### Mailbox operation hangs

Start at `drivers/elgato_gchd/usb.c` → `gchd_mail_write()` / `gchd_mail_read()`, including readiness handling and the HDNew-specific path.

### `/dev/video*` exists but streaming fails

Start at the V4L2 stream-on path:

```text
VIDIOC_STREAMON
    -> gchd_start()
    -> input configuration
    -> encoder/state start
```

Then inspect the selected `d->input` branch in `drivers/elgato_gchd/input.c` and trace its mode/color/configuration helpers. If something is missing, use the original project only as the protocol reference.

### Streaming starts but no frames arrive

Follow `gchd_rx()` → MPEG-TS parsing → PID `0x1011` → PES/frame buffering → videobuf2 delivery. At this point the hardware should already have completed the original init sequence and reached `SCMD_STATE_START`.

### Signal or mode detection is wrong

Start in `drivers/elgato_gchd/input.c` → `gchd_input_configure()` and its input-specific branches, then follow the mode/color/register helpers. Use the original project only to verify the expected signal measurements, mode mapping, and ordering when the DKMS implementation is incomplete.

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
