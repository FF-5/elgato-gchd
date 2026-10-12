# Component input protocol

## Scope

Component input selection, analog-path configuration, signal/scan detection, mode-specific decoder programming, and handoff to shared capture setup.

## Known input selection

The legacy command reference records mailbox port `0x4E` command `51 ED`, observed as register-style write `0x9C:0x51 = 0xED`, as the component selector. It is listed alongside a shared HDMI/component prefix. Treat the full prefix and its reads/writes as an ordered sequence until tests establish which steps can safely be omitted.

## Required sequence records

For each component source format, document:
1. Common hardware initialization prerequisite.
2. Full component input-selection sequence and confirmation.
3. Analog decoder setup, including all page/register selections.
4. Signal detection and lock polling.
5. Format-specific writes and readbacks.
6. Shared capture-engine setup and capture-start confirmation.

Keep component-specific writes separate from the shared HDMI/component prefix. The legacy command reference lists a component-specific write group (`24 0C`, `21 CD`, `91 C8`, `0E 8C`, `11 EC`, `71 4C`, `04 CC`); preserve their observed order and confirm the call site before treating that group as a complete recipe.

## Format coverage

See [source-format-matrix.md](source-format-matrix.md). No complete component per-format bring-up sequence is currently asserted by this document.

## Open questions

- Capture full successful component bring-up traces for each supported format.
- Determine which component modes use the same decoder table and which require unique values.
- Verify signal-loss and no-lock behavior independently from successful signal acquisition.
