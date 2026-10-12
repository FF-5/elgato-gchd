# Hardware initialization and common protocol

## Scope

This document contains initialization that is required before input-specific configuration. Keep input mux selection and source-format programming in the corresponding input/format documents unless a sequence is demonstrated to be shared.

## Source of truth

- [Legacy command reference](../COMMAND_REFERENCE.md), especially USB transport, SCMD and readiness sections.
- Driver implementation: `drivers/elgato_gchd/core.c`.
- The original userspace implementation under `src/` is useful for identifying source-implemented behavior; trace-only observations must be labeled separately.

## USB command/transport conventions

The legacy reference documents these operation families:

- Vendor control reads and writes (request types `0xC0` and `0x40`).
- SCMD via vendor OUT request `0xB8`; payload length differs by device generation.
- Original-HD mailbox writes via `0xBD`.
- HD-New mailbox writes/reads via `0xBC` and `0xB9`.
- Interrupt endpoint `0x83` for synchronization in applicable HD-New flows.
- Bulk OUT endpoint `0x02` for firmware chunks in the original implementation.

These are operation families, not a complete initialization sequence. Consult the source and exact trace before implementing a particular device path.

## SCMD confirmation

The legacy reference describes the following general pattern where applicable:

1. Send the generation-specific SCMD payload.
2. For applicable HD-New commands, wait for the interrupt/readback path.
3. For state transitions, poll the completion register.
4. Read back the device state and compare it with the expected state.
5. Perform the acknowledgement/cleanup writes required by that call site.

Do not treat command receipt, interrupt completion, state-transition completion, and successful video capture as interchangeable confirmations.

## Initialization sequence inventory

| Stage | Status | Evidence / notes |
|---|---|---|
| USB device/interface readiness | Documented at transport level | Confirm exact sequence per device generation in source |
| Firmware download / startup | Documented in legacy reference | Chunk size and behavior differ by generation |
| SCMD idle/init transitions | Documented in legacy reference | Expected state values and follow-up checks are listed there |
| Common mailbox/readiness setup | Partially documented | Preserve ordering and readbacks from the relevant call site |
| Input-specific mux/decoder setup | Intentionally out of scope | See input-specific documents |
| Capture-engine setup/start | Shared later phase, not assumed to be initialization | See [capture-engine.md](capture-engine.md) |

## Rules for adding commands

For each step, record the exact USB setup fields, payload, transfer length, response bytes, poll condition, timeout/retry behavior, and evidence source. Clearly distinguish constants from values that vary per session. Unknown register semantics should remain marked unknown.
