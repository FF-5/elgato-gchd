# Raw USB stream tracing (debug branch)

This branch adds an opt-in, bounded trace of the device's incoming USB bulk-IN stream. It records bytes **before** MPEG-TS alignment, PID filtering, PES parsing, or V4L2 buffering can change or discard them. Successful reads from both the normal receive thread and the stream-drain path are included.

Two exports are available: `data` is a readable text dump with transfer source (`rx` or `drain`), monotonic timestamp, stream offset, length, and hex bytes; `raw` is a binary file containing only the captured USB payload bytes concatenated in receive order. The binary export preserves the byte values and adds no headers. Transfer boundaries and timing remain in the text export.

## Enable and collect

Run these commands as root. If debugfs is not mounted, mount it first:

```sh
mountpoint -q /sys/kernel/debug || mount -t debugfs none /sys/kernel/debug
ls /sys/kernel/debug/elgato_gchd/
```

Choose the device directory shown by `ls` (usually the USB bus/device name, such as `1-2`). Set the path for your device:

```sh
TRACE=/sys/kernel/debug/elgato_gchd/1-2
cat "$TRACE/status"
```

For each test, clear the previous capture, then enable tracing:

```sh
echo 0 > "$TRACE/enable"
echo 1 > "$TRACE/clear"
echo 1 > "$TRACE/enable"
# Now run the capture application and present the test signal.
# When enough data has been collected:
echo 0 > "$TRACE/enable"
cat "$TRACE/status"
cat "$TRACE/data" > composite-240p.txt
cat "$TRACE/raw" > composite-240p.bin
```

Change `1-2` to the actual device directory. The trace is per device and initially disabled. The default raw-byte capacity is 16 MiB; `trace_bytes` can be set at module load time, up to 64 MiB, for example:

```sh
modprobe elgato_gchd trace_bytes=33554432
```

If the buffer or transfer-metadata table fills, tracing automatically stops rather than overwriting earlier bytes. Check `status` for `truncated=1`; if it is set, increase `trace_bytes` or collect a shorter sample. Each transfer is kept intact in the metadata; an exhausted metadata table also stops capture and marks it truncated. The trace buffer is allocated per detected device even while tracing is disabled, so this debug branch reserves about 16 MiB plus metadata per device by default.

## Suggested evidence set

Use the same input connector and capture settings wherever possible. Save a separate text dump for each test:

1. Composite with a stable 240p source.
2. Composite with a stable 480i source.
3. Composite while switching from 240p to 480i, including a few seconds before and after the switch.
4. Repeat for another output/input type if available (for example, component or HDMI).

Include the corresponding `status` output and note the source mode and approximate transition time in each filename or accompanying notes. The trace preserves the raw USB transfer boundaries and arrival timestamps, so we can compare transport packets, continuity counters, PES timestamps, and H.264 data without assuming that V4L2 buffer boundaries correspond to frames.

## Files

- `enable`: read/write `0` or `1`.
- `clear`: write `1` to clear the current capture and truncation state.
- `status`: captured byte/transfer counts, capacities, enabled state, and truncation status.
- `data`: read-only, human-readable transfer records and hex bytes.\n- `raw`: read-only binary payload, concatenated in receive order without transfer headers; use this for FFmpeg/ffprobe probing.

If debugfs is unavailable or trace allocation fails, device probing continues and tracing is disabled; normal capture does not depend on this facility.

## Probe the binary capture with FFmpeg

First preserve the raw capture and status output. Then try automatic probing:

```sh
ffprobe -hide_banner -show_format -show_streams composite-240p.bin
```

If automatic probing does not recognize the stream, test whether the concatenated bytes form MPEG transport stream data:

```sh
ffprobe -hide_banner -f mpegts -show_format -show_streams composite-240p.bin
ffmpeg -hide_banner -f mpegts -i composite-240p.bin -map 0 -c copy composite-remux.mkv
```

These are diagnostic attempts, not assumptions that the USB payload is already a standalone media file. If the bytes include proprietary framing, non-TS records, or an incomplete stream start, FFmpeg may fail to probe them even though the capture is useful. Keep the matching text dump because it retains USB transfer boundaries and timing metadata that the binary concatenation intentionally omits.
