# Raw USB stream tracing (debug branch)

This branch adds an opt-in, bounded trace of the device's incoming USB bulk-IN stream. On USB connection, the driver initializes the hardware, probes the currently selected input, and starts the receive thread after a signal is detected; no capture application or STREAMON is required to start that hardware traffic. The trace records bytes **before** MPEG-TS alignment, PID filtering, PES parsing, or V4L2 buffering can change or discard them. Successful reads from both the background receive thread and the stream-drain path are included.

The `data` export is text: each transfer has a source (`rx` or `drain`), monotonic timestamp, stream offset, and length, followed by the bytes in hexadecimal. The `raw` export is binary and contains only the captured USB payload bytes concatenated in receive order, without transfer headers.

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

For a newly connected device, wait for the hardware initialization and signal-detection messages in `dmesg`, then enable tracing. The receive thread can already be running, so tracing can be turned on without starting a capture application. For each test, clear the previous capture, then enable tracing:

```sh
echo 0 > "$TRACE/enable"
echo 1 > "$TRACE/clear"
echo 1 > "$TRACE/enable"
# Now run the capture application and present the test signal.
# When enough data has been collected:
echo 0 > "$TRACE/enable"
cat "$TRACE/status"
cat "$TRACE/data" > composite-240p.txt\ncat "$TRACE/raw" > composite-240p.bin
```

Change `1-2` to the actual device directory. The trace is per device and initially disabled. The default raw-byte capacity is 16 MiB; `trace_bytes` can be set at module load time, up to 64 MiB, for example:

```sh
modprobe elgato_gchd trace_bytes=33554432
```

If the buffer or transfer-metadata table fills, tracing automatically stops rather than overwriting earlier bytes. Check `status` for `truncated=1`; if it is set, increase `trace_bytes` or collect a shorter sample. Each transfer is kept intact in the metadata; an exhausted metadata table also stops capture and marks it truncated. The trace buffer is allocated per detected device even while tracing is disabled, so this debug branch reserves about 16 MiB plus metadata per device by default. Automatic startup currently probes the selected input; it does not yet scan all three connectors to discover which one has a signal.

## Suggested evidence set

Use the same input connector and capture settings wherever possible. Save a separate text dump for each test:

1. Connect the device with a stable source on the driver's currently selected input (the default is HDMI unless changed in code or remembered during this module lifetime).
2. Composite with a stable 240p source, selecting Composite through V4L2 if needed.
3. Composite with a stable 480i source.
4. Composite while switching from 240p to 480i, including a few seconds before and after the switch.
5. Repeat for another input type if available (for example, component or HDMI).

Include the corresponding `status` output and note the source mode and approximate transition time in each filename or accompanying notes. The trace preserves the raw USB transfer boundaries and arrival timestamps, so we can compare transport packets, continuity counters, PES timestamps, and H.264 data without assuming that V4L2 buffer boundaries correspond to frames.

## Files

- `enable`: read/write `0` or `1`.
- `clear`: write `1` to clear the current capture and truncation state.
- `status`: captured byte/transfer counts, capacities, enabled state, and truncation status.
- `data`: read-only, human-readable transfer records and hex bytes.\n- `raw`: read-only binary payload bytes concatenated in receive order; suitable for media probing.

If debugfs is unavailable or trace allocation fails, device probing continues and tracing is disabled; normal capture does not depend on this facility.
