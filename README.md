# traces (analysis branch, never merged)

USB trace recordings of the Elgato Game Capture HD and their analysis. Orphan
branch: shares no history with `dev`/`master`, never merged, nothing here
ships with the driver.

| Folder | Contents |
|---|---|
| `traces/composite/` | Composite input recordings |
| `traces/s-video/` | S-video input recordings |
| `traces/component/` | Component input recordings |
| `traces/hdmi/` | HDMI input recordings |
| `traces/input-switching/` | Initialization and input switching, no source |
| `traces/ANALYSIS-*.md` | Analysis notes per upload |

Each recording `<name>.pcapng` has a decoded listing `<name>.ops.txt`
(`tools/gchd-pcap-ops.py` on the `tools` branch). Recordings named
`*_noinput_*` start without a source; the source is switched on later and
capture starts. Most recordings end shortly after the first stream data.

Recordings that are too large can be reduced to control traffic only:
`tshark -r big.pcapng -Y "usb.transfer_type != 0x03" -w small.pcapng`.
