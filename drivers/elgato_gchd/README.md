## Experimental native V4L2/DKMS driver

This branch (v4l2-dkms-driver) starts the conversion of the Elgato Game Capture HD
(USB ID 0fd9:005d) from the original libusb userspace streamer into a native
Linux USB/V4L2 driver.

### Current implementation

- binds directly to USB VID:PID 0fd9:005d;
- creates a /dev/video* V4L2 capture device;
- exposes H.264 capture format;
- receives the existing MPEG-TS video stream from bulk endpoint 0x81;
- extracts the existing video PID (0x1011) and PES payloads;
- keeps encoded frames in a bounded circular RAM buffer;
- drops the oldest buffered frames when the RAM/frame limit is reached;
- exposes standard V4L2 H.264 bitrate/profile/level controls;
- includes DKMS metadata.

The driver contains no Qt GUI and does not require a userspace capture daemon.

### Important limitation

The original repository performs a substantial proprietary/device-specific
initialization sequence, including firmware loading and transcoder/input setup,
through libusb. That sequence has not yet been fully ported to kernel USB
control transfers in this branch. Consequently this is the first driver layer,
not yet a drop-in replacement for the original working userspace implementation.

The V4L2 controls currently establish the Linux-facing interface; their hardware
programming callbacks are intentionally isolated for the next porting step.

### Build

Install matching Linux kernel headers and DKMS, then:

    sudo dkms add ./drivers/elgato_gchd
    sudo dkms install elgato-gchd/0.1.0

For a direct kernel-module build:

    make -C /lib/modules/$(uname -r)/build M=$PWD/drivers/elgato_gchd modules

The module is GPL-licensed because it uses GPL-only Linux kernel interfaces.

### RAM buffering

The default encoded-frame ring is 32 frames / 64 MiB, whichever limit is
reached first. The byte limit can be changed with the ring_bytes module
parameter.

### Next porting step

The existing src/gchd.cpp and src/gchd/ implementation remains the reference
for the USB control/state machine. It needs to be translated from libusb
control transfers and C++ configuration code to kernel USB APIs before
the device can be expected to initialize reliably on a fresh plug-in.
