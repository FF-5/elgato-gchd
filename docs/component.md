# Component input reference

## Common commands and configuration

| Target / operation | Bytes written or expected read | Notes |
|---:|---|---|
| `0x88:06` | Write `86` | Digital/component prefix. |
| Port `0x33` read helper | `89 89 F8` | Read before `0x88:03`; meaning UNKNOWN. |
| `0x88:03` | Write `2F` | Digital/component prefix. |
| `0x9C:B3` | Write `CC` | Digital/component prefix. |
| page `CE` → `0x9C:27=CC` → page `CC` | Page select around `27=CC` | **Observed** |
| `0900:0018` | Write `02 0E` | Component routing state; written before final selector. |
| `0x9C` shared path | `AB=4C` , `1B=33` , `B7=CE` , `B8=DC` , `B8=CC` , `07=38` , `07=C8` | Common path writes before selector; preserve order. |
| `0x9C:51=ED` (mailbox `51 ED` ) | Component selector | **Observed** |
| Component-specific tail | `24 0C` , `21 CD` , `91 C8` , `0E 8C` , `11 EC` , `54 FC` , `71 4C` , `04 CC` | Full transaction context observed after selector in multi-switch no-input trace. |

## No-signal idle / keep-alive

**Evidence:** `gchd_input_switch_no_input.pcapng` (after both component selections).

Once the component selector `51 ED` and the component tail have run, the device enters a six-command keep-alive that differs from the HDMI loop:

```
AB 4C
9D CD 2B 00
9D CD 2A 00
9D CD 60 00
99 89 F5 00
9D CD 94 00
```

Full description and comparison with HDMI / composite / S-video idles are in [hardware.md](hardware.md).

## Sequences

See prior ordered switch sequence. Complete per-format signal-present sequences remain **NOT CAPTURED**.
