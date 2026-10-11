# Decoded V4L2 output

The DKMS driver exposes the Elgato's encoded H.264 stream as a V4L2 **capture**
device. Decoding is deliberately kept in userspace: FFmpeg can be upgraded
independently of the kernel module, and decoder crashes or codec changes do not
take down the kernel.

## Lifecycle

1. Opening/querying the V4L2 node does not initialize device firmware.
2. Starting capture (V4L2 STREAMON) performs hardware initialization only if
   needed, then requests input detection.
3. The driver's detection worker probes the selected HDMI/component/composite
   input. It configures the encoder and starts bulk-IN packet capture only
   after valid signal timing has been detected.
4. FFmpeg reads the compressed H.264 capture queue, decodes it to YUV frames,
   and writes those frames to a separate V4L2 output node.
5. Stopping FFmpeg releases the capture queue; STREAMOFF stops packet capture.
   The driver can continue its lightweight signal-detection policy when no
   signal is present so a later source connection can be detected.

## Try it

On Debian/Ubuntu, install the userspace decoder and loopback device:

```sh
sudo apt install ffmpeg v4l2loopback-dkms v4l-utils
sudo modprobe v4l2loopback video_nr=10 card_label="Elgato Decoded Video" exclusive_caps=1
```

Find the driver's capture node with `v4l2-ctl --list-devices`. Confirm that it
advertises H.264:

```sh
v4l2-ctl --device=/dev/video0 --list-formats
```

Then run the helper (replace the input node if necessary):

```sh
chmod +x tools/gchd-decode-to-v4l2.sh
./tools/gchd-decode-to-v4l2.sh /dev/video0 /dev/video10
```

The virtual output should advertise uncompressed formats for applications such
as OBS. Check it with:

```sh
v4l2-ctl --device=/dev/video10 --list-formats-out
```

## Important implementation note

The device emits MPEG transport stream packets over USB. The current kernel
path reconstructs 188-byte TS packets, selects video PID 0x1011, strips PES
headers, and places payload chunks in H.264 V4L2 buffers. The driver marks this
format as a continuous compressed bytestream, but this path still needs
hardware-backed testing with FFmpeg to validate access-unit boundaries,
timestamps, and reconnect behavior. Treat the helper as the initial integration
path, not as proof that every supported source mode is already stable.

Keep FFmpeg and pixel-format conversion out of the DKMS module. The kernel
driver should own USB control, input detection, stream lifecycle, and the
compressed V4L2 queue; userspace should own decoding, color conversion, and
the virtual uncompressed V4L2 output.
