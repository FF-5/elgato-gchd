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
| `logs/hardware-test-log.md` | Results of the hardware tests of the driver |
| `logs/archive/` | Logs saved from the historical branches |

Each recording `<name>.pcapng` has a decoded listing `<name>.ops.txt`
(`tools/gchd-pcap-ops.py` on the `tools` branch). Recordings named
`*_noinput_*` start without a source; the source is switched on later and
capture starts. Most recordings end shortly after the first stream data.

Recordings that are too large can be reduced. Keep bulk OUT (endpoint
`0x02`), drop only the incoming stream:
`tshark -r big.pcapng -Y "usb.transfer_type != 0x03 || usb.endpoint_address.direction == 0" -w small.pcapng`.
Recordings reduced with `usb.transfer_type != 0x03` alone contain no bulk
transfers in either direction.
