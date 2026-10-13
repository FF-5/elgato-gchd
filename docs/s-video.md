# S-video input reference

## Common commands and configuration

| Target / operation | Bytes written or expected read | Notes |
|---|---|---|
| `0x88:02` | Write `DC` | S-video Y/C decoder path. Composite uses `C9`. |
| `0x88:14` | Write `D2` | S-video path parameter. |
| `0x88:28` | Write `88` (signal-present path) or `8A` (no-input path, early after switch) | Lock-/path-correlated. On the no-input path the early write after status polls is `8A`; the later apply block may write `88` or `8A`. |
| `0x88:06`, `0x88:03` | Write `08`, `28` | Analog decoder setup. |
| `0x9C:B3`, `0x9C:00`, `0x9C:27` | Write `33`, `CE`, `33`, `CC` in order | Analog front-end bank/page setup. |
| `0x9C:51` | Write `44` | S-video selector on the HDMI→S-video path (signal-present and no-input). |
| `0x9C:51` | Write `EC` | S-video selector when switching from component or another analog input with no source. See composite.md “Selector 51 EC vs 51 44”. |
| `0x88:10–15` | `6F D4 D0 08 08 88`; then `6F EA D0 08 08 88`; then `6F EA D0 1B 1B 88` | Three distinct decoder-tuning passes. |
| `0x98:0D`, `0x98:0E` | `C8`, `05`, `04`; later `C8`, `15`, `14` | Backend retrigger/apply pulses; preserve order and timing. |
| Mailbox status helpers | `89 89 FA`, `89 89 CA`, `89 89 E7`, `89 89 F1`, `99 89 F5` | Polled during decoder setup and lock settling; several meanings remain UNKNOWN. |



## Session-dependent values

| Target / operation | Bytes written or expected read | Notes |
|---|---|---|
| `0900:0014` | Common shadow bytes include `D3 9E` and `03 1E` | Session-dependent; restore observed shadow, do not hard-code. |
| `0900:001C` | Values vary by session | Mailbox readiness/status; do not hard-code a single reply. |
| `99 89 8B` | Replies include `6E` and `2E` | Meaning UNKNOWN. |
| `0900:01B0` after start | Replies vary, including `00 10` and `01 50` | Meaning UNKNOWN. |

## Sequences

### 240p / 288p

#### NTSC-family 240p vs PAL-family 288p-equivalent

See prior ordered tables in this file for signal-present paths. No-input idle and source-found behaviour are shared with composite and documented in [hardware.md](hardware.md) and the no-input difference tables.

#### No-input then source-found: NTSC and PAL

Both NTSC and PAL no-input paths share the same early order (decoder-first) and the same source-found trigger (`89 89 CA` exits the `89 89 FA` / `99 89 F5` idle).

When the previous input was component (or another analog input), the selector written under no-source conditions is `51 EC` rather than `51 44`. See composite.md for the multi-switch evidence.

#### Other formats

##### 480i / 576i / 480p / 576p

Full sequences **NOT CAPTURED**.
