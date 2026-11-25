# tools (development branch, never merged)

Helper scripts used while developing the DKMS driver. This branch is an
orphan branch: it shares no history with `dev`/`master` and is never merged.

Use a script from any checkout without switching branches:

```sh
git show origin/tools:tools/compare-objs.sh > /tmp/compare-objs.sh && bash /tmp/compare-objs.sh OLD NEW
```

or keep a worktree: `git worktree add ../gchd-tools tools`.

| Script | Purpose |
|---|---|
| `tools/compare-objs.sh [--relaxed] [--alias old=new] OLD NEW` | Build the driver (`src/`, `dkms/` before T-070) at two revisions and compare machine code and string data. Proves a refactor is code-identical. |
| `tools/compare-mailbox-ops.py [--alias old=new] OLD NEW` | Compare the mailbox/USB operation stream of every function from preprocessed source. For idiom changes where machine code differs. |
| `tools/gchd-pcap-ops.py capture.pcapng [src/gchd_transcoder_fields.h] [--raw]` | Decode a USBPcap pcapng into protocol operations (register R/W, device writes/reads, SCMD, encoder fields, firmware and bulk summaries). |
| `tools/gchd-test.sh TESTNAME [--branch B] [--no-build]` | Interactive hardware test: builds and loads the module, runs `tools/tests/TESTNAME.sh`, checks captures with ffprobe (size, field order, frame rate, audio, decode errors) and dmesg (failures), prompts for physical actions and visual checks, writes `~/gchd-test-logs/TESTNAME-DATE.log` with a summary. Needs v4l-utils and ffmpeg. |
| `tools/tests/*.sh` | Test files for `gchd-test.sh` (helpers: `step`, `load_module [replug] [params]`, `prompt`, `confirm`, `set_input`, `set_fmt`, `capture SECS [W H FPS FIELD] [during=DELAY:COMMAND]`, `play SECS QUESTION`, `report`, `set_ctrl NAME=VALUE`, `expect DESC REGEX CMD...`). `smoke`: quick regression of the accepted composite features; `T-061`: bitrate, mode and stretch; `T-064`: remembered settings; `T-065`: console swap, source off/on, start/close cycles, encoder values per family; `T-071`: installer; `T-066`: v4l2-compliance. |
| `tools/drvemu/run.sh REPO RECORDING.ops.txt FROM_S TO_S` | Run the driver's own component configuration and encoder programming (`input.c`, `transcoder.c`, `protocol_common.c`, preprocessed by kbuild) in userspace with logged mailbox writes and reads answered from the recording, and diff its register writes with the recording between FROM_S and TO_S (`mock.c`, `app.c`, `cmp.py`). Needs kernel headers. |
| `tools/stream-restart.c DEVICE ROUNDS BUFFERS` | Hardware test: STREAMON, dequeue BUFFERS buffers, STREAMOFF, ROUNDS times on one open file handle (mmap I/O). Build: `gcc -O2 -o stream-restart stream-restart.c`. |
| `tools/gen-transcoder-fields.py HPP` | Regenerate `src/gchd_transcoder_fields.h` from the original `gchd_hardware.hpp` (git history before T-070: `git show fa14b83:src/gchd_hardware.hpp`). |

Scripts must be run from the root of a checkout of the repository (driver in `src/`; revisions before T-070 have it in `dkms/`).
