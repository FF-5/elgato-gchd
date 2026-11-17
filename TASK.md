# TASK T-011 — Analyse the recorded USB traces

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** first upload analysed; driver changes on `fix/analog-start-per-traces` (not merged, awaiting the hardware test)
- **Type:** offline analysis + docs ("docs-only" for RULES F2)
- **Branches:** analysis output on `traces`; documentation changes on
  `docs/<topic>` from `dev`
- **TODO item:** Phase 2, T-011

## Why

The recorded traces are the protocol reference (RULES D1). The driver's
composite path still follows the C++ port in several places that differ from
earlier trace analyses (TODO "Known differences"). New recordings, starting
with composite 480i, decide what the driver must send.

## Inputs

- Recordings uploaded by the maintainer to `traces/` on the `traces` branch.

## Deliverables

1. For every recording, a decoded listing next to it on the `traces` branch
   (`<recording>.ops.txt`): one line per protocol operation (register R/W,
   mailbox write/read with device and register, SCMD, firmware chunk count,
   bulk transfer summary), with timestamps. The decoder script lives on the
   `tools` branch.
2. A short summary per recording (`<recording>.summary.md`): scenario,
   phases with time ranges, SCMD sequence, input selector and analog status
   values, encoder field writes that differ from the driver.
3. A comparison against the driver's sequence for the same scenario, as a
   list of differences added to `TODO.md` ("Known differences").
4. `docs/` updated with the newly observed sequences, following RULES C
   (neutral wording, Observed/Driver/Unknown labels).

## Acceptance criteria

- Every uploaded recording has a decoded listing and a summary.
- Every difference between the driver and the recordings is listed in
  `TODO.md` with the trace file and timestamp.
- Documentation changes pass RULES C (no references to the analysis process,
  Windows, or development files).
- No change to `dkms/` in this task.

## Findings

Full notes: `traces` branch, `traces/ANALYSIS-2026-11-17.md`; decoded
listings `traces/<input>/*.ops.txt` (decoder: `tools/gchd-pcap-ops.py`).

Driver changes made (branch `fix/analog-start-per-traces`, builds with W=1):

1. Analog inputs are encoded interlaced (`v_format` 4/5, `v_ip_format 0`,
   `v_in_source_type 3`), also for 240p/288p.
2. The back end after analog lock follows the recorded sequence
   (`gchd_analog_backend()`), replacing the C++ blocks A/B1/B2/B3/C.

Docs: `composite.md` (PAL observed, 288p status values, apply block),
`s-video.md` (PAL 288p).

## Where to resume

1. Compare `input-switching/filtered_init_switch_to_component.ops.txt` with the driver's
   `gchd_hw_init()` and component switch; compare
   `input-switching/filtered_novideo_input_switching.ops.txt` with `gchd_set_input()`.
2. 480i → 240p live change: replace the full restart in
   `gchd_mode_workfn()` with the decoder-only transition (TODO T-041).
3. 288p detection (TODO "Known differences").
5. `gchd_configure_composite_exact()` repeats `0x88:02`, `0x88:3C` and the
   three `0x94` writes after detection; the traces do not. Check against the
   noinput traces before removing.
6. Analyse the 480i recordings once uploaded (control traffic only:
   `tshark -r big.pcapng -Y "usb.transfer_type != 0x03" -w small.pcapng`).

## Hardware test (maintainer)

```sh
git fetch origin && git checkout fix/analog-start-per-traces
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C; sudo insmod elgato_gchd.ko
guvcview   # select the input with the connected 480i source, check the picture
sudo dmesg > dmesg.txt
```

Paste the `dmesg` output (or the relevant part) into the maintainer comments
below, with a note on what guvcview showed. The H.264 delivery path is still
lossy (TODO T-020), so some corruption is expected even if the device side is
correct.

---

## Maintainer comments

_Add anything here that the next task must take into account: hardware
availability, what you saw on screen, priorities, things not to touch.
Comments here override the task text above._

- Do not touch the C++ code. The work is the DKMS driver in C only.
- The C++ is a starting point, not the reference; the recorded traces are the
  reference.
- The C++ handles neither input change nor resolution change. Input change is
  in the DKMS driver; resolution change still needs testing with composite and
  HDMI.
- Next hardware session: 480i first, then 240p.
- Documentation ships with the driver: neutral, professional, only verifiable
  behaviour; no Windows references, no reverse-engineering references, nothing
  taken from prompts.
- Tools never go to `dev`/`master` (use the `tools` branch); analysis
  material goes to the `traces` branch.
- Never modify `udev-rules/`, `CMakeLists.txt`, `CREDITS.md`, `LICENSE`, the
  top-level `README.md` or `src/`.
- Input order: composite and S-video (all resolutions) first, then component
  (all resolutions), HDMI last.
