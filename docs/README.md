# GCHD protocol documentation

This directory contains the maintained, topic-separated command and bring-up reference. Driver code remains under `drivers/` and userspace implementation under `src/`.

## Protocol references

- [Hardware initialization](hardware-init.md) — device startup, firmware/SCMD state, common USB transport and readiness checks.
- [HDMI input](hdmi.md) — HDMI selection, detection, format setup and evidence gaps.
- [Component input](component.md) — component selection and analog decoder path.
- [Composite input](composite.md) — composite selection, decoder configuration and source-format notes.
- [S-video input](s-video.md) — S-video selection and decoder configuration.
- [Capture engine](capture-engine.md) — shared SoC/transcoder setup and capture start/stop.
- [Source-format matrix](source-format-matrix.md) — status of each input/format combination.
- [Sequence template](sequence-template.md) — required format for documenting each observed sequence.
- [Composite 240p/480i trace analysis](composite-240p-480i-trace.md) — existing investigation of composite NTSC detection limits.

## Existing command reference

The root-level [COMMAND_REFERENCE.md](../COMMAND_REFERENCE.md) remains the detailed legacy byte-level reference while its content is being split into these topic files. Avoid silently changing or deleting legacy entries during the split; mark uncertain semantics and preserve raw evidence.

## Evidence conventions

- **Observed** — bytes/order are present in a trace or source code.
- **Repeated** — independently reproduced in more than one trace/session.
- **Inferred** — interpretation based on correlation; not yet established as protocol fact.
- **Unknown** — behavior or semantics not understood.
- **Session-dependent** — values vary between sessions/devices and must not be promoted to constants without further evidence.

A successful USB transfer is not, by itself, proof that the device accepted a command, locked to the input signal, or started capture. Record the acknowledgement/state check for each sequence.
