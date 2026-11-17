# traces (analysis branch, never merged)

Raw USB traces and analysis material for the Elgato Game Capture HD. This is
an orphan branch: it shares no history with `dev`/`master` and is never
merged. Nothing here ships with the driver.

| Path | Contents |
|---|---|
| `traces/` | New trace recordings (pcapng/usbmon) go directly here, one file per session. |
| `traces/previous-analysis/` | Material collected before this branch existed: decoded trace analyses, usbmon logs of the userspace and kernel drivers, kernel logs, reference-TS analysis. See `INDEX.md` there. |

Naming suggestion for new recordings:
`<input>_<mode>_<scenario>.pcapng`, for example
`composite_480i_plug-in-to-capture.pcapng`, with an optional `.txt` next to it
describing source device, timing of events, and anything notable.
