# Evidence

These are raw observations. The pages in `../` are derived from them. Never
edit these files; add new captures next to them and update the pages.

| Path | What it is | Captured with |
|---|---|---|
| `windows-traces/trace_analysis.txt` | Decoded Windows trace: composite 480i lock and START, then 480i → 240p (live), then 240p → 480i (restart). Source capture: `gchd_240p-480i_switch_to_composite.pcapng` (not in repo). | Windows driver, USBPcap, HD-New |
| `windows-traces/gchd_composite_240p_full_bringup.txt` | HDMI → composite with a 240p NTSC source, through START, including about 120 transcoder writes. Source: `filtered_240p_composite.pcapng` (not in repo). | Windows, HD-New |
| `windows-traces/gchd_svideo_240p_full_bringup.txt` | Same for S-video. Source: `filtered_240p_svideo.pcapng` (not in repo). | Windows, HD-New |
| `windows-traces/hdmi2composite_trace_analysis.txt` | HDMI → composite switch with a 240p source. | Windows, HD-New |
| `windows-traces/input_switch_trace_analysis.txt` | No-source switching HDMI ↔ component ↔ composite ↔ S-video. | Windows, HD-New |
| `windows-traces/gchd_win_trace_no_input.pcapng` | Raw USBPcap, power-on with no input. | Windows, HD-New |
| `userspace-usbmon/userspace_noinput.log` | usbmon of the **C++ driver**, no input: full init, selector `51=ED` (component chosen by cable autodetect), mode detection fails, full shutdown (`SCMD 01`, `02 01`). | Linux usbmon |
| `userspace-usbmon/userspace_composite_input_0/1.log` | usbmon of the **C++ driver**, composite source. One session split in two: `_0` covers firmware load → selector `51=CC`; `_1` continues to `SCMD 05 00 00 02`. | Linux usbmon |
| `kernel-logs/2da03ea-480i-data-flowing.txt` | DKMS dmesg at commit `2da03ea`: 480i composite, data flowing about 17 s after START, START misreported as failed. | DKMS driver |
| `kernel-logs/state-machine-errors.txt` | Last `errors.txt` on `state-machine` (recorded before the async bulk-IN commits): DKMS dmesg plus usbmon; START never completes (state stays `0x01`), `guvcview` hangs. | DKMS driver |
| `kernel-logs/kernel_noinput_0/1.log` | usbmon of the DKMS driver, no input. | DKMS driver |
| `reference-ts/` | ffprobe/CSV analysis of three 10 s TS recordings (low/medium/high bitrate, 720×480 field-coded, PIDs `0x64`/`0x66`). The `.ts` files themselves are not in the repo. | Probably the Elgato software |
| `debug-branch-trace/` | `trace.c` and instructions from the `debug` branch: a debugfs dump of raw bulk-IN bytes, useful for TS capture from the kernel driver. | — |

Missing and wanted (see `../../TODO.md`): a PAL trace, a composite 480i
capture of the raw TS from the C++ driver, and a Windows HDMI capture with a
signal.
