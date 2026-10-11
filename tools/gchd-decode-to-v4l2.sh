#!/usr/bin/env bash
set -Eeuo pipefail

# Decode the driver's compressed H.264 V4L2 capture stream and publish decoded
# frames to a v4l2loopback output device. Opening/streaming INPUT_DEVICE is
# intentionally what triggers hardware init and input detection in the DKMS
# driver; this helper never accesses the USB device directly.

INPUT_DEVICE="${1:-/dev/video0}"
OUTPUT_DEVICE="${2:-/dev/video10}"

if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "ffmpeg is required (install it with your distribution's package manager)." >&2
    exit 127
fi

if [[ ! -e "$INPUT_DEVICE" ]]; then
    echo "Capture device not found: $INPUT_DEVICE" >&2
    exit 1
fi

if [[ ! -e "$OUTPUT_DEVICE" ]]; then
    cat >&2 <<EOF
Output device not found: $OUTPUT_DEVICE

Create a virtual output first, for example:
  sudo modprobe v4l2loopback video_nr=${OUTPUT_DEVICE##*/video} card_label="Elgato Decoded Video" exclusive_caps=1

Install v4l2loopback-dkms if the module is not available. If you choose a
non-numeric output path, load the module manually with a numeric video_nr.
EOF
    exit 1
fi

exec ffmpeg \
    -hide_banner \
    -f v4l2 \
    -input_format h264 \
    -i "$INPUT_DEVICE" \
    -an \
    -vf format=yuv420p \
    -f v4l2 "$OUTPUT_DEVICE"
