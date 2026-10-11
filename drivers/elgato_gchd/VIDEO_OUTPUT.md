# Plug-and-play decoded V4L2 output

## Goal

The installed driver should behave like a normal camera: applications such as
OBS discover a usable V4L2 video node without the user creating loopback devices,
finding node numbers, or launching a script.

The Elgato sends compressed video. The kernel module should not embed FFmpeg or
perform software video decoding in kernel space. Use a small, automatically
installed userspace bridge for decoding and publishing decoded frames.

## Proposed architecture

- **DKMS kernel module:** USB protocol, lazy hardware initialization, input
  selection/signal detection, compressed-stream capture, and a V4L2 interface
  for the encoded stream.
- **Managed userspace bridge:** FFmpeg libraries or a supervised FFmpeg process
  decode the compressed stream and write decoded frames to a virtual V4L2
  output.
- **Packaging/integration:** Install and enable the bridge and its service
  configuration together with the driver. The service should associate the
  correct capture and output nodes automatically; no manual shell commands
  should be part of normal use.

A virtual output is needed because a V4L2 compressed-capture queue is not itself
a decoded camera feed. `v4l2loopback` is a reasonable first implementation for
the output node, but it should be created and managed automatically by packaging
and service logic, not by the end user.

## Required lifecycle

1. USB probe registers the device and its V4L2 nodes; it must not initialize
   firmware or start video packet capture merely because the device was plugged
   in or its node was enumerated.
2. When an application opens/starts streaming from the decoded output, the
   bridge becomes active and requests capture from the driver's compressed
   stream interface.
3. The driver initializes hardware only on the first real capture request.
4. Input detection runs before video bulk-IN capture. If no valid signal is
   present, do not start the video packet stream; keep the detection/retry
   policy needed to notice a later signal.
5. Once signal timing is detected and the input is configured, begin bulk-IN
   capture. The bridge decodes and forwards frames.
6. When the last consumer stops, stop capture and release resources cleanly.
   Device removal, signal loss, input changes, and decoder failure must all
   unwind safely and allow recovery.

The bridge must be demand-driven: merely starting a system service or creating
the virtual output must not trigger hardware initialization. If the selected
loopback mechanism cannot reliably report that a consumer has opened the output,
use a small V4L2 bridge daemon or another output implementation that can.

## Implementation order

1. Confirm the kernel V4L2 capture format is valid H.264 and test stream
   boundaries, timestamps, and decoding against actual hardware.
2. Implement and test the demand-driven bridge lifecycle.
3. Add automatic device association and service activation/deactivation.
4. Package the bridge and virtual-output setup so installation is one-time and
   normal operation is plug-and-play.
5. Test plug/unplug, no-signal startup, signal appearance/loss, multiple opens,
   suspend/resume, and decoder failure.

This is the target design, not a claim that the full automatic lifecycle has
already been implemented.