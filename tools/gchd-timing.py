#!/usr/bin/env python3
"""Latency tables for the elgato_gchd driver and the Windows recordings.

    gchd-timing.py driver LOG...      gchd-timing lines (module parameter
                                      timing=1) from dmesg or test logs
    gchd-timing.py trace OPS...       decoded recordings (*.ops.txt on the
                                      traces branch)
    gchd-timing.py both --driver LOG... --trace OPS...

Driver: every request (capture start, control, output size, input) or
source change starts an operation; the table lists, per operation kind and
phase, the time since the request in ms (count, average, min, max).
Requests within 50 ms of each other without a device command in between
count as one operation (one v4l2-ctl call setting two controls).

Recordings: a change sequence starts at the STATE_CHANGE NULL command; the
phases are the following STOP, INIT and START commands (the recordings do
not show the user's action, and bulk IN data was filtered out). For the
comparison the driver's operations with a NULL command are measured from
that command as well.
"""

import re
import statistics
import sys
from collections import OrderedDict, defaultdict

MARK = re.compile(r"gchd-timing: (\d+) (.+?)\s*$")
OPS_SCMD = re.compile(r"^\s*([\d.]+)\s+SCMD\s+(\w+)\s+mode=(\w+)\s+data=(\w+)")

STATE_NAMES = {"0x02": "START", "0x01": "STOP", "0x04": "NULL", "0x11": "IDLE"}


def norm(event):
    """Phase name of a driver mark."""
    m = re.match(r"state (0x[0-9a-f]+) reached", event)
    if m:
        return "state %s reached" % STATE_NAMES.get(m.group(1), m.group(1))
    m = re.match(r"stream first-data", event)
    if m:
        return "stream first-data"
    m = re.match(r"(scmd \w+)", event)
    if m:
        return m.group(1)
    return event


def driver_ops(paths):
    """Operations: (kind, [(ms since start, phase)])."""
    ops = []
    cur = None
    for path in paths:
        with open(path, errors="replace") as f:
            for line in f:
                m = MARK.search(line)
                if not m:
                    continue
                us, event = int(m.group(1)), m.group(2)
                starts = event.startswith("request") or event.startswith("source mode") \
                    or event.startswith("source family")
                if starts:
                    kind = re.sub(r" (0x[0-9a-f]+|\d+x\d+|\d+)$", "", event)
                    kind = kind.replace("request ", "")
                    if (cur and (us - cur["t0"]) < 50000 and
                            not any(p.startswith("scmd") for _, p in cur["ev"])):
                        if kind not in cur["kind"]:
                            cur["kind"] += "+" + kind
                        continue
                    cur = {"kind": kind, "t0": us, "ev": []}
                    ops.append(cur)
                    continue
                if cur is None:
                    continue
                cur["ev"].append(((us - cur["t0"]) / 1000.0, norm(event)))
    return [(o["kind"], o["ev"]) for o in ops]


def trace_ops(paths):
    """Change sequences of the recordings: (name, [(ms since NULL, phase)])."""
    ops = []
    for path in paths:
        cur = None
        with open(path, errors="replace") as f:
            for line in f:
                m = OPS_SCMD.match(line)
                if not m:
                    continue
                t = float(m.group(1)) * 1000.0
                cmd, data = m.group(2), m.group(4)
                if cmd == "STATE_CHANGE":
                    phase = {"0004": "scmd NULL", "0001": "scmd STOP",
                             "0002": "scmd START"}.get(data, "scmd STATE")
                else:
                    phase = "scmd " + cmd
                if phase == "scmd NULL":
                    cur = {"t0": t, "ev": [(0.0, phase)]}
                    ops.append(cur)
                elif cur is not None:
                    cur["ev"].append((t - cur["t0"], phase))
                    if phase == "scmd START":
                        cur = None
    return [("change (recordings)", o["ev"]) for o in ops]


def from_null(ops):
    """Driver operations with a NULL command, measured from it."""
    out = []
    for kind, ev in ops:
        t0 = next((t for t, p in ev if p == "scmd NULL"), None)
        if t0 is None:
            continue
        out.append((kind + " (driver)",
                    [(t - t0, p) for t, p in ev if t >= t0]))
    return out


def table(ops, title):
    """Markdown table: kind, phase, n, avg, min, max (ms)."""
    by_kind = OrderedDict()
    for kind, ev in ops:
        phases = by_kind.setdefault(kind, OrderedDict())
        seen = set()
        for t, p in ev:
            if p in seen:
                continue
            seen.add(p)
            phases.setdefault(p, []).append(t)
    lines = ["### " + title, "",
             "| Operation | Phase | n | avg ms | min ms | max ms |",
             "|---|---|---|---|---|---|"]
    for kind, phases in by_kind.items():
        n_ops = sum(1 for k, _ in ops if k == kind)
        for p, ts in sorted(phases.items(), key=lambda kv: statistics.mean(kv[1])):
            lines.append("| %s (%d) | %s | %d | %.0f | %.0f | %.0f |" % (
                kind, n_ops, p, len(ts), statistics.mean(ts), min(ts), max(ts)))
    return "\n".join(lines) + "\n"


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    mode, args = argv[1], argv[2:]
    if mode == "driver":
        print(table(driver_ops(args), "Driver: ms since the request"))
    elif mode == "trace":
        print(table(trace_ops(args), "Recordings: ms since the NULL command"))
    elif mode == "both":
        drv = []
        trc = []
        target = None
        for a in args:
            if a == "--driver":
                target = drv
            elif a == "--trace":
                target = trc
            elif target is not None:
                target.append(a)
        dops = driver_ops(drv)
        print(table(dops, "Driver: ms since the request"))
        print(table(from_null(dops) + trace_ops(trc),
                    "Driver and recordings: ms since the NULL command"))
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
