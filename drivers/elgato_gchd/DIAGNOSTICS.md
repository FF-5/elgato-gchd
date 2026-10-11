# Input detection diagnostics

This branch adds a diagnostic-only trace for comparing a known-good 480i input
with a 240p input. It does not change detection decisions or hardware command
values.

## Enable raw mailbox logging

After loading the module, enable the parameter:

```sh
sudo sh -c 'echo Y > /sys/module/elgato_gchd/parameters/trace_mailbox'
```

The parameter logs mailbox TX command bytes and successful RX response bytes,
in order. It can be noisy because it includes the full initialization sequence.
Disable it after the capture:

```sh
sudo sh -c 'echo N > /sys/module/elgato_gchd/parameters/trace_mailbox'
```

## Capture a baseline

Use the same connector, cable path, driver build, and capture application for
both signals. Run one test with a known-good 480i source, then repeat with the
240p source. Avoid changing other settings between tests.

For each run:

```sh
sudo dmesg -C
# Enable trace_mailbox before starting the capture.
# Start the V4L2 capture application and let it run for about 10 seconds.
sudo dmesg > gchd-480i.log
```

For the second run, use `gchd-240p.log` as the output filename. If clearing the
kernel log is undesirable, instead save a timestamped `dmesg` snapshot after
each test.

The `gchd-diag` messages include:
- Raw HDMI timing measurements and sample counts.
- Raw component timing register values.
- Composite detection status before and after the low-nibble mask.
- The selected input mode and interlace decision.
- The intended START transition and whether it completed.
- Ordered mailbox TX/RX bytes when `trace_mailbox=Y`.

The existing bulk-IN logs report transfer results, actual byte counts, timeouts,
and cumulative bytes. Compare the first point where the 480i and 240p traces
diverge. A mailbox command log is emitted before the transaction is attempted;
an RX log is emitted only after a successful read.

This is an observation-only patch. It intentionally does not force 240p to
480i, change mode selection, or add speculative mailbox commands.
