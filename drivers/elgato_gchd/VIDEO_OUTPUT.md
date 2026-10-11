# Plug-and-play decoded V4L2 output

## Goal

The installed driver should behave like a normal camera: applications such as
OBS discover a usable, decoded V4L2 node without the user creating loopback
devices, finding node numbers, or launching a script.

The Elgato emits compressed video. Keep USB protocol and hardware lifecycle in
the kernel; decode in userspace using FFmpeg/libavcodec. Do not put a decoder in
kernel space.

## Existing kernel interface

The DKMS driver registers an encoded H.264 capture node. Firmware initialization
is deferred until V4L2 STREAMON, rather than occurring on USB probe or a brief
enumeration open. Signal detection/configuration is managed by the driver before
the RX thread is started.

The current TS parser queues PES payload chunks as continuous H.264 bytestream
buffers. This framing assumption still needs to be validated with actual device
captures and a real decoder before claiming compatibility.

## Target architecture

- **Kernel module:** USB protocol, lazy hardware initialization, input selection,
  signal detection, and compressed H.264 capture.
- **Managed bridge:** a userspace daemon uses FFmpeg/libavcodec to decode the
  compressed capture stream and publishes decoded frames to a second V4L2 node.
- **Automatic integration:** package the bridge, virtual output, permissions,
  and service activation alongside the driver. Node association must use stable
  device identity/udev metadata rather than hard-coded /dev/video numbers.

## Demand and lifecycle contract

1. USB probe registers the encoded capture node. It does not load firmware or
   start video bulk-IN capture.
2. The decoded output node may exist for discovery, but its existence and the
   bridge service being installed/running must not initialize the Elgato.
3. The first real consumer of the decoded output activates the bridge. The bridge
   opens the encoded node and starts streaming; that STREAMON is the hardware
   initialization boundary.
4. The driver detects and configures a valid input before starting video bulk-IN.
   If no signal is present, it retries detection without starting the video RX
   stream until a valid signal is available.
5. The bridge decodes frames and publishes them in the negotiated raw format.
6. When the last decoded-output consumer closes, the bridge stops reading the
   encoded node. STREAMOFF then stops capture and releases hardware resources.
7. Unplug, signal loss, input changes, and decoder errors must stop or restart
   the appropriate layer without leaving stale buffers or a wedged device.

## Output-node decision

Do not assume that creating a `v4l2loopback` device is sufficient to implement
on-demand activation. Its common configuration exposes a virtual camera but does
not provide a portable, simple consumer-open notification contract for this
daemon. A polling workaround based on process file descriptors would be fragile.

The implementation should therefore choose one of these, in order of preference:

1. A small dedicated virtual V4L2 output driver whose open/close lifecycle can
   signal the bridge daemon; or
2. A maintained virtual-camera backend with a verified consumer-demand API.

The bridge daemon should receive demand events, open the encoded source only
while demand exists, decode, and write frames to the virtual output. It should
not continuously stream the physical device just because a system service is
enabled.

## Implementation and validation order

1. Capture real USB data and confirm H.264 Annex-B/AVCC representation, SPS/PPS,
   access-unit framing, timestamps, and whether decoder input can be supplied as
   the current continuous bytestream.
2. Implement a minimal FFmpeg-based bridge and validate decode against a saved
   capture before adding automatic lifecycle management.
3. Implement a demand-aware virtual output and daemon integration.
4. Package module, userspace bridge, output backend, udev rules, and service so
   normal use is plug-and-play.
5. Test enumeration without initialization, no-signal startup, signal arrival
   and loss, last-consumer shutdown, multiple consumers, unplug/replug, and
   decoder failure.

This document describes the implementation contract and outstanding work; it
does not claim that the decoded virtual camera is already implemented.
