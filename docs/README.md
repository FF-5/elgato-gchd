# GCHD protocol reference

One reference per hardware/input path:

- [Hardware](hardware.md) — USB command primitives, shared acknowledgements, capture start.
- [HDMI](hdmi.md)
- [Component](component.md)
- [Composite](composite.md)
- [S-video](s-video.md)

Each input page records known command bytes, meanings, ordered setup, confirmation, and format coverage. Unsupported sequences are marked `NOT CAPTURED`, not inferred. Raw packet traces remain under `drivers/elgato_gchd/`; [legacy command reference](../COMMAND_REFERENCE.md) remains available.
