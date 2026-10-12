# GCHD command-sequence reference

**Start here:** [Sequence matrix](sequence-matrix.md). This is an implementation-checking reference: ordered command tables, expected responses, and trace links.

- [Hardware init](hardware-init.md)
- [HDMI](hdmi.md)
- [Component](component.md)
- [Composite](composite.md)
- [S-video](s-video.md)
- [Capture engine](capture-engine.md)
- [Composite 240p sequence](composite-240p-sequence.md)
- [S-video 240p sequence](s-video-240p-sequence.md)
- [Legacy byte-level reference](../COMMAND_REFERENCE.md)

Raw traces: [Composite 240p](../drivers/elgato_gchd/gchd_composite_240p_full_bringup.txt) · [S-video 240p](../drivers/elgato_gchd/gchd_svideo_240p_full_bringup.txt).

Every completed sequence must table the exact ordered commands, USB transfer/payload, expected reply/state, and evidence. `NOT CAPTURED` means no defensible sequence exists yet; never fill gaps with guesses.