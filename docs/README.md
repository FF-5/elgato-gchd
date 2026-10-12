# GCHD protocol reference

This directory has one reference per hardware path. Each reference records command bytes, known meanings, ordered configuration, expected confirmation, and stream start.

- [Hardware](hardware.md) — USB command primitives, shared setup/start sequence, acknowledgements.
- [HDMI](hdmi.md)
- [Component](component.md)
- [Composite](composite.md)
- [S-video](s-video.md)

**Evidence rule:** values from a trace are not automatically universal constants. Session-dependent values are labeled. Unsupported input/format combinations are `NOT CAPTURED`.

Raw packet-level traces remain under `drivers/elgato_gchd/`; input documents link to their relevant traces. The legacy [COMMAND_REFERENCE.md](../COMMAND_REFERENCE.md) remains available for older command notes.
