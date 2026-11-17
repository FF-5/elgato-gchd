# Branch history (assessed 2026-11-16)

From now on, `dev` is the integration branch. Feature branches start from
`dev` and merge back into `dev`. `master` receives only stable releases,
decided by the maintainer. The branches below are **frozen history**. Do not
develop on them; cherry-pick from them deliberately.

## Upstream (2016)

| Branch | Content |
|---|---|
| `master` | Squashed upstream C++ userspace driver (tolga9009 / Scott Dossey), last commit 2016-12-23. Starting point for the DKMS port (read-only; never modified here). Does not build on current GCC without adding `<cstdint>` includes. |
| `cherry3` | Ancestor of `master` (2016-11-03). Nothing unique. |
| `dev` (before 2026-11-16) | Pre-squash upstream history of the same C++ code plus three FIFO pause/close commits that never reached `master`. It is now merged with `master` (tree = `master`) and carries `dkms/`, `docs/`, `TODO.md`, `TASK.md`, `RULES.md`. |

## DKMS/V4L2 port (2026-10), linear lineage

```
master
 └─ debug-saved-enable-state    first V4L2 port (drivers/elgato_gchd), mailbox shadow fix
     └─ v4l2-dkms-driver        deferred init on STREAMON; analysis/ (reference TS); Windows no-input pcapng
         ├─ debug               side branch: debugfs raw bulk-IN trace (trace.c, RAW_USB_TRACE.md); its errors.txt shows an older build failure, the tip builds
         ├─ video-output-lazy-init  side branch: VIDEO_OUTPUT.md (decoded-output design)
         └─ v4l2-h264-access-units  PES → H.264 buffer delivery
             └─ diagnostic-input-baseline  docs/ written; Windows trace analyses (240p composite/S-video, 240p↔480i, input switching); trace_mailbox diagnostics
                 │   commit 2da03ea: 480i composite delivered ~2.9 MB/s of TS, ~17 s after START; driver still reported "START failed" (state 0x02)
                 └─ async-bulk-in-queue   40 async URBs (32×0x81 + 8×0x84) queued before START
                     └─ state-machine     latest; deleted the trace analyses and analysis/; removed the 480i→240p register-only helper; programs 240p as 480p
```

Tip of `state-machine` = tip of `async-bulk-in-queue` plus a merge commit.

## Where things went wrong (reading of the history)

1. The 240p work started before 480i was verified end to end. 480i data was
   reaching the host at `2da03ea`, but START completion was misdetected, and
   the received stream was never checked with a decoder.
2. 240p was modelled as a progressive mode (`v_format 8`, `v_ip_format 2`,
   full reconfiguration on 480i → 240p). Windows keeps the encoder in 480i and
   flips decoder registers only ([composite.md §4](composite.md#4-windows-480i--240p-while-capturing-trace_analysistxt-t--250285-s)).
3. The kernel extracts H.264 PES into a lossy ring and delivers at most one
   buffer per USB completion, so pictures are dropped and the decode is
   malformed regardless of the input mode
   ([video-output.md](video-output.md#recommended-linux-interface)).
4. Primary evidence (trace analyses, reference TS analysis, usbmon logs of the
   C++ driver) was deleted from the newest branches. It is restored under
   `docs/evidence/`.
5. Documentation treated the Windows traces as the only source and marked
   sequences the C++ implements and that worked as "NOT CAPTURED". It also
   presented an unbacked PAL sequence as "working".

## Files that existed only in history

The `docs/` pages that existed between 2026-10-11 and 2026-10-12
(`capture-engine.md`, `hardware-init.md`, `sequence-matrix.md`,
`analog-format-comparison.md`, `composite-240p-sequence.md`,
`s-video-240p-sequence.md`, …) and the root `COMMAND_REFERENCE.md`,
`SOURCE_AUDIT.md`, `*_COMMAND_LEDGER.md` were consolidated into the current
pages before deletion. Their content has been re-checked against the C++ and
the traces. `tools/gchd-decode-to-v4l2.sh` (deleted 2026-10-11) is superseded
by the ffmpeg examples in [video-output.md](video-output.md).

No decompiled Windows DLL was ever committed to any branch.
