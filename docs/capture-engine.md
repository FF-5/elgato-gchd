# Shared capture-engine configuration

## Scope

Configuration after input/decoder setup that appears to be shared across inputs: SoC/transcoder setup, coefficient/table writes, timing and control registers, initialization, and capture start/stop.

## Separation from input setup

Keep the input mux and decoder register programming in the input-specific document. Keep the capture-engine sequence here only when multiple traces confirm the same bytes/order; otherwise document the variant and link to the trace.

## Observed 240p sequence notes

The composite and S-video 240p bring-up analyses report a shared SoC configuration sequence and a final startup flow including:
- SCMD `04 A0 00 00` (INIT without reloading encode firmware in the legacy reference).
- Readiness/state check at `0x0800:0x2008`.
- SCMD `05 00 00 02` (START), followed by required state/completion confirmation.

The traces describe output dimensions of 720×480 even with a 240p source. Do not interpret those output dimensions as proof of the input's source timing.

## Capture start confirmation

Follow the generation-specific SCMD confirmation path in [hardware-init.md](hardware-init.md) and the legacy [command reference](../COMMAND_REFERENCE.md). Distinguish:
- USB transfer completion.
- SCMD command receipt.
- State-change completion.
- Expected SCMD state readback.
- Actual arrival of valid bulk video data.

For driver reliability, the final success condition should include evidence that capture data is arriving, not merely that START was submitted.

## Open questions

- Compare the entire SoC write sequence across all available input/format traces.
- Identify which coefficient/timing values vary by source format.
- Document stop, restart, input-change, and signal-loss behavior separately.
