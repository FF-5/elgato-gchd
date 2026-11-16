# TASK T-001 — usbmon protocol decoder and kernel-vs-C++ init diff

- **Status:** ready
- **Type:** offline, tools + docs (no hardware needed; "build-only" for rule E2)
- **Branch:** `feat/usbmon-decoder` from `origin/dev`
- **TODO item:** Phase 1, T-001

## Why

The kernel driver was ported by reading the C++ code and Windows traces. The
port drifted from the reference in places nobody has checked byte for byte
(START completion handling, 240p handling, possibly the init sequence). We
already have wire-level logs of **both** drivers doing the same thing
(power-on with no input), so the drift can be measured offline before any
more hardware time is spent. The same tool will be reused for every later
phase (composite 480i in T-030, 240p in T-041).

## Inputs

| File | Content |
|---|---|
| `docs/evidence/userspace-usbmon/userspace_noinput.log` | C++ driver, power-on, no input (ends in "mode detection failed"). |
| `docs/evidence/userspace-usbmon/userspace_composite_input_0.log` + `_1.log` | C++ driver, composite source. **One session split into two files**: `_1` continues `_0` and reaches START. |
| `docs/evidence/kernel-logs/kernel_noinput_0.log`, `_1.log` | DKMS driver, no input. |
| `docs/evidence/kernel-logs/state-machine-errors.txt` | DKMS dmesg followed by usbmon lines. |
| `docs/hardware.md` §2 | Exact encoding of every primitive. |

usbmon text format (one line per event):
`URB-tag timestamp S|C Type:bus:dev:ep status|setup … = data`, for example
`S Co:2:006:0 s 40 bc 0900 0014 0002 2 = d39e` (setup =
`bmRequestType bRequest wValue wIndex wLength`).

## Deliverables

1. `tools/usbmon2ops.py` (Python 3, standard library only), which:
   - parses usbmon text and ignores non-usbmon lines (shell prompts, dmesg);
   - pairs `S`/`C` events by URB tag;
   - emits one line per **protocol operation**:
     - `REG_R bank:reg -> value` / `REG_W bank:reg = value`
     - `SCMD cmd mode data`
     - `MAIL_W port=0x33 dev=0x88 [bytes]`: a mailbox write; fold the
       `00C0` payload write, the `B9` commit, the interrupt, `0016`, the
       `001C` polls, the `23BE` dummy read and the `0014` shadow restore into one
       op, and keep a `--raw` flag to show them;
     - `MAIL_R dev=0x88 reg=0xCA -> 0x7E` for the `89 89 RR` / `99 89 RR` /
       `9D CD RR` read forms (device address mapping in `docs/README.md`);
     - `SPARAM addr lsb bits value [field-name]`: decode the 8-byte
       `wValue=0001` writes back to the field name, using a table generated
       from `src/gchd_hardware.hpp` (parse the file, do not hand-copy it);
     - `SLSI addr value`, `FW chunk n bytes`, `BULK_IN ep len`, `INT ep data`;
   - with `--normalize`, drops timestamps and collapses repeated identical
     polls (`REG_R 0900:001C` loops) into `… (xN)` so that two logs can be
     diffed.
2. `docs/analysis/kernel-vs-cpp-init.md`: an aligned diff of the C++ no-input
   init against the DKMS no-input init, from power-on to the first
   input-specific operation. For every divergence give the C++ location, the
   kernel location (`dkms/*.c:line`), the Windows evidence if any, and a verdict:
   *bug*, *harmless*, or *needs hardware test*.
3. A short section in the same file on the DKMS START sequence vs C++
   `completeStateChange()` (from `state-machine-errors.txt` and the
   `2da03ea` log), feeding T-020.
4. New TODO items for every *bug* verdict, added under the right phase.

## Acceptance criteria

- `python3 tools/usbmon2ops.py --normalize <log>` runs on all input logs (and on `_0`+`_1` concatenated)
  without crashing, and any unparsed lines are counted and reported.
- Running it on `userspace_composite_input_0.log` followed by `_1.log` shows,
  in order: idle firmware chunks, `SCMD 01`, `SCMD 04 00`, encode firmware
  chunks, the composite selector `0x9C:51 = CC`, `SCMD 04 A0` (twice in
  total), and `SCMD 05 00 00 02`. Spot-check against `docs/hardware.md` §5.
- `docs/analysis/kernel-vs-cpp-init.md` exists, every divergence has a
  verdict, and nothing is marked *bug* without a cited source.
- No change to `dkms/` in this task (findings become TODO items).
- `RULES.md` section E is satisfied (docs updated, TODO updated, `TASK.md`
  rewritten for the next task with this comments section preserved).

## Out of scope

- Fixing kernel code (that is T-010/T-020/T-030).
- Decoding the `.pcapng` Windows file (it can be a follow-up: tshark
  `usb.capdata` export into the same op format).

## Findings

_(fill in while working)_

---

## Maintainer comments

_Add anything here that the next task must take into account: hardware
availability, what you saw on screen, priorities, things not to touch.
Comments here override the task text above._

-
