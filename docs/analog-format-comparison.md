# Composite and S-video format sequences

Side-by-side comparison of observed analog-input bring-up. The two full NTSC-family 240p bring-ups in the DKMS driver directory are working references; both reach SCMD START. The PAL-family 288p-equivalent paths are also documented separately in the input references.

## 240p / 288p

| Stage | Composite | S-video | Notes |
|---|---|---|---|
| Input decoder | `0x88:02=C9` | `0x88:02=DC`, then `0x88:14=D2` | Select composite versus Y/C decoder path. |
| Initial mode value | Composite full bring-up does not show the S-video preamble | Read `89 89 FA`; write `0x88:28=88` | S-video 240p path waits about 206 ms before this write. |
| Analog front end | `0x88:06=08`, `0x88:03=28`; `0x9C:B3=33`; page `CE`, `0x9C:27=33`, page `CC` | Same shared setup after its decoder preamble | Preserve bank/page order. |
| Input selector | `0x9C:51=44` | `0x9C:51=44` | The earlier `0x9C:51=45` selects HDMI before switching. |
| Routing word | `02 0E` → `03 0E` → `03 0E` | `02 0E` → `03 0E` → `03 0E` | Preserve the intermediate and repeated final writes. |
| Decoder tuning | `6F D4 D0 08 08 88`; `6F EA D0 08 08 88`; `6F EA D0 1B 1B 88` to `0x88:10–15` | Same three passes | Do not collapse the three passes. |
| 240p apply block | Not present in the composite full bring-up | `0x88:07=8A`, `08=9B`, `09=7A`, `28=8A`, `06=08` | Specific to the observed S-video 240p path. |
| Backend | `0x98:04=95`, `70=D0`, `0F=88`; full `90–A3` coefficient block; `72=88`, `C0=88`, `61=98`, `63=89`, `64=96`, `65=88`, `04=8D`, `62=91` | Same full backend block in the NTSC-family 240p bring-up | Values match in these two working captures. |
| Apply / retrigger | `0x98:0D=C8`; pulse `0E=05` → `04` and `0E=15` → `14` during settling | Same pulse values | Preserve order and polling/settling between pulses. |
| Start | Configure SoC pipeline; queue bulk IN; SCMD `04 A0 00 00`; read `0800:2008 → 00 01`; SCMD `05 00 00 02` | Same start sequence | Both full bring-ups are working NTSC-family 240p captures. |
| PAL-family variant | Working 288p-equivalent sequence in [composite.md](composite.md) | Working 288p-equivalent sequence in [s-video.md](s-video.md) | PAL path details differ; use the per-input sequence, not the NTSC table above. |

## 480i / 576i

| Stage | Composite | S-video | Notes |
|---|---|---|---|
| Decoder / mode | `0x88:28=88` appears in analog setup/transition observations | `0x88:28=88` appears in S-video setup | This value alone does not establish a complete interlaced-mode sequence. |
| Complete ordered sequence | **NOT CAPTURED** | **NOT CAPTURED** | Do not infer from the 240p/288p paths. |

## 480p / 576p

| Stage | Composite | S-video | Notes |
|---|---|---|---|
| Input and decoder selection | **NOT CAPTURED** | **NOT CAPTURED** | No complete progressive 480/576-line bring-up is documented. |
| Mode-specific tuning / backend | **NOT CAPTURED** | **NOT CAPTURED** | Do not reuse the 240p/288p values as a confirmed progressive sequence. |
| Complete ordered sequence | **NOT CAPTURED** | **NOT CAPTURED** | Requires a working capture of each input and format. |

## Evidence notes

- The two NTSC-family 240p bring-ups are considered working: each reaches SCMD `05 00 00 02`.
- PAL-family 288p-equivalent composite and S-video paths are documented in their respective input pages.
- Handshake bytes at `0900:0014` and `0900:001C` are session-dependent and are not constants.
