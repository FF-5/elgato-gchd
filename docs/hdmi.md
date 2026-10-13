# HDMI input reference

## Common commands and configuration

| Target / operation | Bytes written or expected read | Notes |
|---:|---|---|
| `0x9C:51` | Write `45` | HDMI mux selector; full standalone recipe remains incomplete. |
| `0x9C:B7` ; mailbox read | Write `CC` ; read `9D CD 88` | Initial read/follow-up write; allow about 330 ms in this observed path. |
| `0900:0018` | Write/read `02 0A` | HDMI routing state word. |
| HDMI front-end registers | See ordered sequence below | Input setup; not verified across all HDMI modes. |

## Front-end register sequence observed in trace prefix

See prior tables.

### Idle / keep-alive after HDMI front-end setup (no signal)

**Observed:** no-signal HDMI configuration.

After the HDMI selector (`51 45`) and the front-end / parameter-table sequence, the device enters a long-running repeating loop:

```
AB 4C
9D CD 6A 00
99 89 F5 00
9D CD 94 00
```

(each followed by the normal HD-New mailbox handshake). The loop continues while the device stays configured for HDMI with no signal present.

This is the same pattern previously noted as idle/keep-alive behaviour. The present capture shows it is not merely a short prefix; it is the steady-state behaviour once HDMI setup has finished and no further input change or signal lock has occurred.

**Authoritative description of all input-specific no-signal keep-alives (HDMI, component, composite/S-video), the source-detect candidate, and multi-switch evidence from `gchd_input_switch_no_input.pcapng`** are kept in [hardware.md](hardware.md).

**Still NOT CAPTURED for HDMI:** complete per-format programming, signal-present detection, lock criteria, and the behaviour of the keep-alive once a signal appears.
