## Experimental native V4L2/DKMS driver

This branch (v4l2-dkms-driver) converts the Elgato Game Capture HD from the original libusb userspace streamer into a native Linux USB/V4L2 driver.

### Current implementation

- binds the four Game Capture HD revisions supported by the original project: 0fd9:0044, 0fd9:004e, 0fd9:0051, and 0fd9:005d;
- creates a /dev/video* V4L2 capture device when a supported USB device is connected;
- exposes H.264 capture through the standard V4L2 API;
- exposes HDMI, Component, and Composite as standard V4L2 inputs;
- exposes discrete V4L2 frame sizes and frame intervals for the currently selected input;
- receives the existing MPEG-TS stream from bulk endpoint 0x81;
- extracts video PID 0x1011 and its PES payloads;
- keeps encoded frames in a bounded circular RAM buffer;
- drops the oldest buffered frames when the RAM/frame limit is reached;
- exposes standard V4L2 H.264 bitrate/profile/level controls;
- uses the legacy MB86H57/H58 firmware and mailbox protocol for 0044/004e/0051;
- uses the MB86M01/HDNew protocol for 005d;
- uses the kernel firmware API (request_firmware()) for encoder/idle firmware;
- includes DKMS metadata.

The driver contains no Qt GUI and does not require a userspace capture daemon.

### Important limitation

The kernel USB control-transfer primitives, mailbox paths, state commands, enable-state handling, firmware uploads, RAM buffering, and V4L2 plumbing are implemented. The device-specific signal/mode configuration is still only a partial translation of the original reverse-engineered C++ implementation.

In particular, HDMI signal detection and the full per-mode calibration/configuration sequences are not yet completely ported. The V4L2 controls also establish the Linux-facing interface but do not yet program every encoder parameter into the hardware.

Therefore this branch is still pre-hardware-validation and should not yet be considered a finished drop-in replacement for the original userspace driver.

### Build

Install matching Linux kernel headers and DKMS, then:

    sudo dkms add ./drivers/elgato_gchd
    sudo dkms install elgato-gchd/0.2.0

For a direct kernel-module build:

    make -C /lib/modules/$(uname -r)/build M=$PWD/drivers/elgato_gchd modules

The module is GPL-licensed because it uses GPL-only Linux kernel interfaces.

### Device lifetime

The V4L2 node is registered by the USB driver's probe() callback. Consequently, the module can be installed without an Elgato connected, but /dev/video* appears when a matching supported USB device is connected (or immediately when the device is already connected and the module subsequently binds to it).

### RAM buffering

The default encoded-frame ring is 32 frames / 64 MiB, whichever limit is reached first. The byte limit can be changed with the ring_bytes module parameter.

### Reference implementation

The original src/gchd/ implementation remains the reference for the device-specific USB control sequences, signal detection, and transcoder configuration. The remaining bring-up work is deliberately being kept in the kernel driver rather than adding a userspace capture helper.
