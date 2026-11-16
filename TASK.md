# TASK T-001 — DKMS code cleanup: layout, named constants, USB-command helpers

- **Status:** done, merged to `dev` (awaiting maintainer review)
- **Type:** offline, code refactor, **no behaviour change** (build-only for
  rule E2; the proof is `tools/compare-objs.sh`)
- **Branch:** `refactor/t001-names-and-macros` from `origin/dev`
- **TODO item:** Phase 1, T-001

## Why

The DKMS driver is hard to read and review because the protocol is written
as raw numbers, and an earlier "cleanup" made it worse rather than better:

| Problem (measured on `dev` at the start of the task) | Example |
|---|---|
| 175 value aliases that only restate the number | `#define GCHD_VALUE_88 0x88` |
| 181 register aliases that only restate the number, grouped under wrong device names | `#define GCHD_VIDEO_REG_5B 0x5b`, `GCHD_COMPONENT_REG_51` |
| Wrong device names for mailbox ports | `GCHD_MAIL_PORT_ENCODER` (`0x44`) is the analog video decoder `0x88`; `..._VIDEO` (`0x4C`) is the back end `0x98`; `..._COMPONENT` (`0x4E`) is the front end `0x9C`, used by every input |
| Wrong meaning in names | `GCHD_REPLY_STATUS_COMPOSITE` (`0x66`) = 240p lock; `GCHD_REPLY_STATUS_SVIDEO` (`0x7E`) = 480i lock; `GCHD_QUERY_ENCODER_CONFIG` (`0xCA`) = decoder status register; `GCHD_SCMD_MODE_HDNEW` (`0xA0`) = INIT without firmware reload on both families |
| The same mailbox helper macros defined 5 times with different signatures | `MW`, `M3`, `R9`, `MR1`, `MRP`, `MW4C`, `MRC1` in `input.c` (4×) and `GCHD_MW`, `GCHD_R9`, … in `protocol_common.c` |
| Raw control-transfer parameters at call sites | `gchd_ctrl_write(d, 0xb9, 0x0000, …)`, version registers `0x0094/0x0098`, `0000:0010…001E` reads, `sparam` `wValue` `0x0000`/`0x0001` |
| Transcoder fields written by address | `P(0x152c,0,16,width)` instead of `v_hsize_out` |
| Dead code | `gchd_color_yuv()`, `gchd_post_encoder_calibration()`, `gchd_post_encoder_setup()` are never called |

## Goal

Someone reading `dkms/` next to `docs/commands.md` and a Windows trace should
see the same names in all three, and every repeated USB operation should be a
single, named call.

## Plan

1. **Protocol header.** Move protocol constants out of `elgato_gchd.h` into
   `gchd_protocol.h`, organised like `docs/commands.md`:
   - USB requests (`0xBC` register, `0xB8` SCMD, `0xB9` mailbox commit,
     `0xBD` HD mailbox), banks (`0800`, `0900`, `0000`), registers by name;
   - mailbox ports by **device role**: `GCHD_PORT_BRIDGE` (`0x33`),
     `GCHD_PORT_DECODER` (`0x44` ↔ `0x88`), `GCHD_PORT_BACKEND` (`0x4C` ↔ `0x98`),
     `GCHD_PORT_FRONTEND` (`0x4E` ↔ `0x9C`), `GCHD_DEV_AUX` (`0x94`);
   - bridge read prefixes (`89 89`, `99 89`, `9D CD`) and bridge commands
     (`AB A9 0F A4 55/5B`, `43 23 84`, …) with their documented replies;
   - SCMD commands, modes and states;
   - known device registers with a name **only where the role is documented**
     (`GCHD_FE_REG_BANK` `0x00`, `GCHD_FE_REG_INPUT_SEL` `0x51`,
     `GCHD_DEC_REG_LOCK` `0xCA`, `GCHD_DEC_LOCK_480I` `0x7E`,
     `GCHD_DEC_LOCK_240P` `0x66`, …). Registers without a known role stay as
     hex literals in the sequence, because the call site already names the
     device. Do not create `REG_5B 0x5b`-style aliases.
   - transcoder bit fields as `{addr, lsb, bits}` constants named like
     `src/gchd_hardware.hpp` (`GCHD_TC_V_HSIZE_OUT`), used through one
     `gchd_tc_set(d, FIELD, value)`.
2. **One mailbox API**, used everywhere and declared once:
   `gchd_fe_write(d, reg, val)`, `gchd_dec_write`, `gchd_be_write`,
   `gchd_aux_write`, `gchd_fe_read(d, reg, &v)`, `gchd_dec_read`,
   `gchd_be_read`, `gchd_bridge_cmd(d, bytes…)`, plus a single
   `GCHD_TRY(expr)` macro (returns on error; the name says so). Remove all
   local `MW`/`M3`/`R9`/… macros and the `GCHD_VALUE_*`/`GCHD_*_REG_*`
   aliases.
3. **Byte tables stay data.** Long trace-derived sequences (`gchd_seq()` tables,
   the 70-entry block read, colour tables) stay as `const` arrays, each with a
   `/* WIN … */` or `/* C++ … */` source comment.
4. **Remove dead code** (the three functions above).
5. **Layout review.** Write `dkms/README.md` "Layout" with the proposed file
   split (for example `mailbox.c`, `scmd.c`, `init.c`, `input_hdmi.c`,
   `input_analog.c`, `input_component.c`, `transcoder.c`, `v4l2.c`, `rx.c`).
   Do moves that keep code identical in this task; anything else goes to
   T-002.

## Constraints

- `tools/compare-objs.sh origin/dev WORKTREE` must print **IDENTICAL** after
  every commit. If a step cannot be code-identical (for example helper
  functions that the compiler does not inline the same way), stop, list the
  differences in "Findings", and either restructure or move the step to T-002.
  Never weaken the check.
- Do not change any protocol value, order, delay, or log message text in this
  task. Fixing protocol behaviour comes later (T-020 onwards).
- Build with `W=1`, no warnings.
- Never touch `src/`.

## Acceptance criteria

1. `tools/compare-objs.sh origin/dev WORKTREE` → IDENTICAL (or every
   difference is listed and justified in "Findings" and approved by the
   maintainer below).
2. `grep -c 'GCHD_VALUE_\|_REG_[0-9A-F][0-9A-F] ' dkms/*.h` → 0.
3. No local mailbox macros remain in `.c` files; every mailbox access goes
   through the API in step 2.
4. Every name of a device, port, request, register and status value matches
   `docs/commands.md`.
5. `dkms/README.md` describes the new layout; `TODO.md` and `TASK.md` are
   updated (RULES E).

## Findings

### Result

| Metric | Before (`dev` `6a940cb`) | After |
|---|---|---|
| `dkms/` lines | 6 571 | 6 092 |
| `elgato_gchd.h` lines | 694 | 166 |
| Number-restating aliases (`GCHD_VALUE_xx`, `*_REG_xx`) | 356 | 0 |
| Local mailbox macro families in `.c` files | 5 | 0 |
| `sparam` writes with a field name | 0 | 154 of 157 |
| Dead functions | 3 | 0 |

New files: `gchd_protocol.h`, `gchd_mailbox.h`,
`gchd_transcoder_fields.h` (generated); tools: `tools/compare-objs.sh`,
`tools/compare-mailbox-ops.py`, `tools/gen-transcoder-fields.py`.

### Verification chain (one check per commit)

| Commit | Check | Result |
|---|---|---|
| inline 356 aliases | `compare-objs.sh` strict | IDENTICAL |
| `input.c` → `gchd_mailbox.h` | `compare-mailbox-ops.py` | IDENTICAL; machine code differs only in register/stack allocation |
| `protocol_common.c` → `gchd_mailbox.h` | `compare-mailbox-ops.py --alias …` | IDENTICAL except `gchd_be_poll_ed/read_ec`, checked by hand (same bytes `99 89 ED/EC`, same read) |
| protocol constant renames | `compare-objs.sh` strict | IDENTICAL (log text unchanged) |
| `usb.c` constants, enable bits | `compare-objs.sh` strict | only `gchd_hw_shutdown` differs; its ops stream is IDENTICAL |
| transcoder field names | `compare-objs.sh` strict | IDENTICAL |
| helper renames, dead code | `compare-objs.sh --alias …` | IDENTICAL |
| TS constants, `__maybe_unused` | `compare-objs.sh` strict | IDENTICAL |

Both tools were negative-tested: changing one selector byte (`0x45` → `0x46`)
or one register value (`0xce` → `0xcf`) is reported with the exact byte.

### Issues found (not fixed here; behaviour changes are out of scope)

1. **Possible porting bug:** `transcoder.c` writes `0x1004` lsb 0, 1 bit under
   the comment "stout_mode"; C++ `stout_mode` is `0x1004` bit 15. Marked
   `FIXME` in the code. → TODO T-004.
2. The analog "anti-lockup" block in `input.c` (around
   `gchd_configure_composite_exact`) deliberately ignores mailbox errors with raw
   `gchd_mail_write()` calls. → review in T-032.
3. `gchd_setup_subblock()` and `gchd_setup_subblock_exact()`, and the
   `*_exact` variants generally, are thin wrappers left over from earlier
   ports. → T-002.
4. Component/HDMI mode detection uses inline magic thresholds
   (`abs(value - 0xbbf4) < 10`, …). A named table `{value, mode}` would match
   `docs/component.md`, but it changes code. → T-002.
5. `core.c`'s TS parser keeps some literals (adaptation-field masks); it is
   replaced wholesale in T-020, so it was not polished further.

---

## Maintainer comments

_Add anything here that the next task must take into account: hardware
availability, what you saw on screen, priorities, things not to touch.
Comments here override the task text above._

- Do not touch the C++ code. Our job is only the DKMS driver in C.
- The C++ is a starting point, not the reference; our own Windows traces are
  the reference.
- The C++ handles neither input change nor resolution change. Input change is
  already in the DKMS driver; resolution change still needs testing with
  composite and HDMI.
- Next hardware session: 480i first, then 240p.
