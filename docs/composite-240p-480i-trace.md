# Composite 240p ↔ 480i USB trace notes

Trace analyzed: `gchd_240p-480i_composite_input.pcapng` (about 93.5 MB, 73,206 captured packets, approximately 63.0 seconds). This is a reverse-engineering note, not yet a definitive 240p detector specification.

## Important result: the current composite detector does not distinguish 240p from 480i

The existing original userspace implementation in `src/gchd/configure_composite.cpp` has the composite mode-detection sequence:

```text
mailWrite(port=0x33, bytes=89 89 fa)
value = mailRead(port=0x33, length=1)[0]
value &= 0x0f

value == 0x06  -> NTSC, 60 Hz
value == 0x07  -> PAL, 50 Hz
```

It explicitly initializes `autodetectScanMode` to `Interlaced`. Thus the known detector classifies NTSC versus PAL; it does **not** establish a separate 240p-versus-480i result. Do not treat `89 89 fa` by itself as a 240p signature. A separate timing/scan-mode measurement or encoder-stream evidence is still needed to prove how the software distinguishes those two NTSC-family signals.

## Byte sequences observed around the two apparent transitions

The capture contains the mailbox write `89 89 fa 00` repeatedly, roughly every 0.7–0.9 seconds, across the recording. Two notable occurrences of the NTSC-specific mailbox command `89 89 fd 00` appear at about:

- **18.427 s** from capture start
- **52.543 s** from capture start

Both are followed by the same surrounding setup pattern. In USB control-transfer data, the relevant writes appear as:

```text
40 bc 00 08 c0 00 04 00  89 89 fa 00
40 bc 00 08 c0 00 04 00  89 89 ca 00
40 bc 00 08 c0 00 02 00  07 8a
40 bc 00 08 c0 00 02 00  08 9b
40 bc 00 08 c0 00 02 00  09 7a
40 bc 00 08 c0 00 02 00  28 88   (first occurrence)
40 bc 00 08 c0 00 04 00  89 89 fd 00
...
40 bc 00 08 c0 00 02 00  06 08
```

At the later occurrence, the write near the end of that short register sequence is `28 8a` rather than `28 88`. The trace also shows `89 89 ca 00` close to both events. The `40 bc ...` prefix is the USB vendor control-transfer setup; the bytes after it are the command/register payload, not one contiguous video-frame signature.

The `fd` command is consistent with the existing source: after detecting NTSC, `configure_composite.cpp` writes `89 89 fd` and expects a one-byte reply of `0x6e`. In this trace, the corresponding read sequence includes a response containing `6e` at approximately 18.431 s and 52.547 s. This is evidence of the NTSC configuration sequence being executed at those points, **not proof that `fd` means 240p**.

## What this trace does and does not establish

- The device/host repeatedly exchanges vendor control transfers while the MPEG-TS stream is active.
- `89 89 fa` is the documented composite NTSC/PAL detection command in the original implementation.
- `89 89 fd` and the nearby register writes identify an NTSC-specific configuration sequence; the same pattern appears at both noted transition points.
- No unique, independently verified byte value has yet been tied to “240p” as opposed to “480i”. The current source detector maps both to the NTSC branch unless another part of the software/hardware path provides scan-mode information.
- The capture uses a 65,535-byte snap length: many records have captured length 65,535 but original length 65,563. Those records are truncated by 28 bytes, so the stream should not be treated as a byte-perfect continuous recording.

## Next step to identify a true 240p signature

Correlate the video elementary stream (or its codec sequence headers) with the manually observed switch times, and separately decode the mailbox replies to `89 89 fa` across the whole capture. Compare values immediately before, during, and after each switch. If the reply low nibble remains `0x06` for both signals—as the current code would lead us to expect—then the differentiator must be another register/measurement or stream metadata, not the NTSC/PAL detector itself.

Do not add a hard-coded 240p value to the driver until that correlation identifies a stable, repeatable field.
