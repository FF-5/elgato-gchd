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

## Hardware test results

**2026-11-17, composite 480i, branch `fix/analog-start-per-traces`:**

- Device side works: START completed after 10 completion polls (~0.1 s);
  65 KB bulk transfers followed immediately (previously START took ~17 s
  or never completed).
- guvcview crashed (read fault in `memcpy`). Cause: it negotiated the
  default 1280×720 (HDMI) format, then the input was switched to composite
  while streaming and the driver changed the size to 720×480 underneath it.
  Also logged: `TRY_FMT` rejected width/height 0, `G_PARM` unsupported.
- Fixed on `fix/v4l2-format-negotiation` (stacked on the analog branch).

## Hardware test (maintainer)

Select the input **before** starting the application; guvcview does not
renegotiate when the source size changes during capture.

```sh
git fetch origin && git checkout fix/v4l2-format-negotiation
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/videoN -i 2        # composite (3 = S-video)
v4l2-ctl -d /dev/videoN --get-fmt-video   # expect 720x480, interlaced
guvcview -d /dev/videoN
sudo dmesg > dmesg.txt
```

Independent check of the stream (does not depend on guvcview):

```sh
ffmpeg -f v4l2 -input_format h264 -i /dev/videoN -t 10 -c copy test.h264
ffprobe test.h264
```

Paste dmesg, what guvcview showed, and the `ffprobe` output into the
maintainer comments. The H.264 delivery path is still lossy (TODO T-020),
so some corruption is expected.

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

This is the output of dmesg and guvcview:

ff5@ff5-VirtualBox:~$ dmesg
[ 1746.629749] usbcore: registered new interface driver elgato_gchd
[ 1754.727080] usb 2-1: new high-speed USB device number 3 using ehci-pci
[ 1755.125145] usb 2-1: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[ 1755.125163] usb 2-1: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[ 1755.125169] usb 2-1: Product: GameCapture HD
[ 1755.125173] usb 2-1: Manufacturer: Elgato
[ 1755.125176] usb 2-1: SerialNumber: 04107F1267
[ 1755.127891] elgato_gchd 2-1:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[ 1755.131346] elgato_gchd 2-1:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[ 1755.150027] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=3740 comm=v4l_id
[ 1755.151534] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1395 comm=wireplumber
[ 1755.152310] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1395 comm=wireplumber
[ 1755.153236] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 1755.153252] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 1755.154840] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 1755.154891] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 1755.154904] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 1755.154914] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 1755.155178] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 1755.155215] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 1768.242204] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=3743 comm=guvcview
[ 1768.243023] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=3743 comm=guvcview
[ 1768.538466] elgato_gchd 2-1:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 1768.587399] elgato_gchd 2-1:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1768.590194] elgato_gchd 2-1:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1768.594052] elgato_gchd 2-1:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1768.597804] elgato_gchd 2-1:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1768.601672] elgato_gchd 2-1:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1768.605458] elgato_gchd 2-1:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1768.609237] elgato_gchd 2-1:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1768.613723] elgato_gchd 2-1:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1768.617255] elgato_gchd 2-1:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1768.620959] elgato_gchd 2-1:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1768.624571] elgato_gchd 2-1:1.0: firmware chunk offset=327680 size=4016 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1768.699422] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[ 1768.700789] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
[ 1769.375873] elgato_gchd 2-1:1.0: transcoder defaults loaded
[ 1769.378784] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[ 1769.379819] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x04
[ 1769.387672] elgato_gchd 2-1:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.390639] elgato_gchd 2-1:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.394618] elgato_gchd 2-1:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.398526] elgato_gchd 2-1:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.402378] elgato_gchd 2-1:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.406789] elgato_gchd 2-1:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.410807] elgato_gchd 2-1:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.414604] elgato_gchd 2-1:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.418757] elgato_gchd 2-1:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.422774] elgato_gchd 2-1:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.426402] elgato_gchd 2-1:1.0: firmware chunk offset=327680 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.430199] elgato_gchd 2-1:1.0: firmware chunk offset=360448 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.434546] elgato_gchd 2-1:1.0: firmware chunk offset=393216 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.438693] elgato_gchd 2-1:1.0: firmware chunk offset=425984 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.443589] elgato_gchd 2-1:1.0: firmware chunk offset=458752 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.447229] elgato_gchd 2-1:1.0: firmware chunk offset=491520 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.451332] elgato_gchd 2-1:1.0: firmware chunk offset=524288 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.455259] elgato_gchd 2-1:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.458664] elgato_gchd 2-1:1.0: firmware chunk offset=589824 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.462297] elgato_gchd 2-1:1.0: firmware chunk offset=622592 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.465803] elgato_gchd 2-1:1.0: firmware chunk offset=655360 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.469504] elgato_gchd 2-1:1.0: firmware chunk offset=688128 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.473366] elgato_gchd 2-1:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.476672] elgato_gchd 2-1:1.0: firmware chunk offset=753664 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.480328] elgato_gchd 2-1:1.0: firmware chunk offset=786432 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.484126] elgato_gchd 2-1:1.0: firmware chunk offset=819200 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.487953] elgato_gchd 2-1:1.0: firmware chunk offset=851968 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.491614] elgato_gchd 2-1:1.0: firmware chunk offset=884736 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.495264] elgato_gchd 2-1:1.0: firmware chunk offset=917504 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.499518] elgato_gchd 2-1:1.0: firmware chunk offset=950272 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.503252] elgato_gchd 2-1:1.0: firmware chunk offset=983040 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.506992] elgato_gchd 2-1:1.0: firmware chunk offset=1015808 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.510578] elgato_gchd 2-1:1.0: firmware chunk offset=1048576 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.514694] elgato_gchd 2-1:1.0: firmware chunk offset=1081344 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.518743] elgato_gchd 2-1:1.0: firmware chunk offset=1114112 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.522341] elgato_gchd 2-1:1.0: firmware chunk offset=1146880 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.526506] elgato_gchd 2-1:1.0: firmware chunk offset=1179648 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.530527] elgato_gchd 2-1:1.0: firmware chunk offset=1212416 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.534485] elgato_gchd 2-1:1.0: firmware chunk offset=1245184 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.538528] elgato_gchd 2-1:1.0: firmware chunk offset=1277952 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.542491] elgato_gchd 2-1:1.0: firmware chunk offset=1310720 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.546356] elgato_gchd 2-1:1.0: firmware chunk offset=1343488 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.550527] elgato_gchd 2-1:1.0: firmware chunk offset=1376256 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.554632] elgato_gchd 2-1:1.0: firmware chunk offset=1409024 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.558652] elgato_gchd 2-1:1.0: firmware chunk offset=1441792 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.562606] elgato_gchd 2-1:1.0: firmware chunk offset=1474560 size=26224 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 1769.607534] elgato_gchd 2-1:1.0: device idle, processor state 0x27f97b
[ 1769.787255] elgato_gchd 2-1:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[ 1769.787285] elgato_gchd 2-1:1.0: no signal on input=0; retrying detection
[ 1770.253276] elgato_gchd 2-1:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[ 1770.253288] elgato_gchd 2-1:1.0: no signal on input=0; retrying detection
[ 1770.708701] elgato_gchd 2-1:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[ 1770.708718] elgato_gchd 2-1:1.0: no signal on input=0; retrying detection
[ 1771.148155] elgato_gchd 2-1:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[ 1771.148171] elgato_gchd 2-1:1.0: no signal on input=0; retrying detection
[ 1771.603625] elgato_gchd 2-1:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[ 1771.603644] elgato_gchd 2-1:1.0: no signal on input=0; retrying detection
[ 1772.053990] elgato_gchd 2-1:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[ 1772.054022] elgato_gchd 2-1:1.0: no signal on input=0; retrying detection
[ 1772.502663] elgato_gchd 2-1:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[ 1772.502670] elgato_gchd 2-1:1.0: no signal on input=0; retrying detection
[ 1772.965919] elgato_gchd 2-1:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[ 1772.965934] elgato_gchd 2-1:1.0: no signal on input=0; retrying detection
[ 1773.412574] elgato_gchd 2-1:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xad4d samples6463=1 avg6463=0xb2b2 rgb=0
[ 1773.412602] elgato_gchd 2-1:1.0: no signal on input=0; retrying detection
[ 1773.479377] elgato_gchd 2-1:1.0: input switch requested: old=0 new=2 hw_initialized=1 streaming=1
[ 1773.484469] elgato_gchd 2-1:1.0: input selected: input=2; detection scheduled
[ 1774.333965] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6\n
[ 1774.453198] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 1774.486491] elgato_gchd 2-1:1.0: analog format initial status=0x7e; starting in 480i
[ 1774.486507] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 1774.486515] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 1774.486522] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 1780.648129] elgato_gchd 2-1:1.0: capture setup: encoder start completed
[ 1780.651218] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 1782.414308] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[ 1782.469961] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[ 1782.744245] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[ 1782.746676] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 1782.748984] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1782.751285] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1782.752968] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 1782.760834] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 1782.763250] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[ 1782.764895] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[ 1782.774421] elgato_gchd 2-1:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0000 ack=1
[ 1782.792367] elgato_gchd 2-1:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 1782.800806] elgato_gchd 2-1:1.0: START poll=1 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 1782.808838] elgato_gchd 2-1:1.0: START poll=2 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 1782.816792] elgato_gchd 2-1:1.0: START poll=3 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 1782.824758] elgato_gchd 2-1:1.0: START poll=4 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 1782.832718] elgato_gchd 2-1:1.0: START poll=5 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 1782.840655] elgato_gchd 2-1:1.0: START poll=6 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 1782.849129] elgato_gchd 2-1:1.0: START poll=7 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 1782.856754] elgato_gchd 2-1:1.0: START poll=8 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 1782.864759] elgato_gchd 2-1:1.0: START poll=9 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 1782.873183] elgato_gchd 2-1:1.0: START poll=10 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0010 ack=1
[ 1782.883752] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[ 1783.220698] elgato_gchd 2-1:1.0: async bulk-IN data: read=1 ep=0x81 actual=65536 total_bytes=65536
[ 1783.243380] elgato_gchd 2-1:1.0: async bulk-IN data: read=2 ep=0x81 actual=65536 total_bytes=131072
[ 1783.265701] elgato_gchd 2-1:1.0: async bulk-IN data: read=3 ep=0x81 actual=65536 total_bytes=196608
[ 1783.287891] elgato_gchd 2-1:1.0: async bulk-IN data: read=4 ep=0x81 actual=65536 total_bytes=262144
[ 1783.310394] elgato_gchd 2-1:1.0: async bulk-IN data: read=5 ep=0x81 actual=65536 total_bytes=327680
[ 1783.607888] show_signal_msg: 113 callbacks suppressed
[ 1783.607892] guvcview[3761]: segfault at 7cad079b0260 ip 00007cad14988eeb sp 00007cad08dfe708 error 4 in libc.so.6[7cad14828000+189000] likely on CPU 2 (core 2, socket 0)
[ 1783.607906] Code: 00 00 00 48 3b 15 65 23 08 00 0f 87 cf 01 00 00 4c 8d 04 11 49 31 c8 49 c1 e8 3f 81 e1 00 0f 00 00 44 01 c1 0f 84 8a 00 00 00 <c5> fe 6f 6c 16 e0 c5 fe 6f 74 16 c0 48 89 f9 48 83 cf 1f c5 fe 6f
[ 1784.562170] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[ 1784.563475] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[ 1790.404361] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[ 1790.406099] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
ff5@ff5-VirtualBox:~$ 

ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ guvcview
GUVCVIEW: version 2.0.8
V4L2_CORE: (UVCIOC_CTRL_MAP) Error: Inappropriate ioctl for device
V4L2_CORE: (UVCIOC_CTRL_MAP) Error: Inappropriate ioctl for device
V4L2_CORE: (UVCIOC_CTRL_MAP) Error: Inappropriate ioctl for device
V4L2_CORE: (UVCIOC_CTRL_MAP) Error: Inappropriate ioctl for device
V4L2_CORE: (UVCIOC_CTRL_MAP) Error: Inappropriate ioctl for device
V4L2_CORE: (UVCIOC_CTRL_MAP) Error: Inappropriate ioctl for device
V4L2_CORE: (UVCIOC_CTRL_MAP) Error: Inappropriate ioctl for device
V4L2_CORE: (UVCIOC_CTRL_MAP) Error: Inappropriate ioctl for device
V4L2_CORE: (UVCIOC_CTRL_MAP) Error: Inappropriate ioctl for device
{ VIDIOC_TRY_FMT (invalid values): width = 0, height = 0 }
v4L2_CORE: Unable to enumerate frame sizes :Invalid argument
{ VIDIOC_TRY_FMT (invalid values): width = 0, height = 0 }
v4L2_CORE: Unable to enumerate frame sizes :Invalid argument
{ VIDIOC_TRY_FMT (invalid values): width = 0, height = 0 }
v4L2_CORE: Unable to enumerate frame sizes :Invalid argument
{ VIDIOC_TRY_FMT (invalid values): width = 0, height = 0 }
v4L2_CORE: Unable to enumerate frame sizes :Invalid argument
ALSA lib pcm.c:2721:(snd_pcm_open_noupdate) Unknown PCM cards.pcm.rear
ALSA lib pcm.c:2721:(snd_pcm_open_noupdate) Unknown PCM cards.pcm.center_lfe
ALSA lib pcm.c:2721:(snd_pcm_open_noupdate) Unknown PCM cards.pcm.side
ALSA lib pcm.c:2721:(snd_pcm_open_noupdate) Unknown PCM cards.pcm.surround71
ALSA lib setup.c:547:(add_elem) Cannot obtain info for CTL elem (MIXER,'IEC958 Playback Default',0,0,0): No such file or directory
ALSA lib setup.c:547:(add_elem) Cannot obtain info for CTL elem (MIXER,'IEC958 Playback Default',0,0,0): No such file or directory
ALSA lib setup.c:547:(add_elem) Cannot obtain info for CTL elem (MIXER,'IEC958 Playback Default',0,0,0): No such file or directory
ALSA lib pcm.c:2721:(snd_pcm_open_noupdate) Unknown PCM cards.pcm.hdmi
ALSA lib pcm.c:2721:(snd_pcm_open_noupdate) Unknown PCM cards.pcm.hdmi
ALSA lib pcm.c:2721:(snd_pcm_open_noupdate) Unknown PCM cards.pcm.modem
ALSA lib pcm.c:2721:(snd_pcm_open_noupdate) Unknown PCM cards.pcm.modem
ALSA lib pcm.c:2721:(snd_pcm_open_noupdate) Unknown PCM cards.pcm.phoneline
ALSA lib pcm.c:2721:(snd_pcm_open_noupdate) Unknown PCM cards.pcm.phoneline
Cannot connect to server socket err = No such file or directory
Cannot connect to server request channel
jack server is not running or cannot be started
JackShmReadWritePtr::~JackShmReadWritePtr - Init not done for -1, skipping unlock
JackShmReadWritePtr::~JackShmReadWritePtr - Init not done for -1, skipping unlock
V4L2_CORE: (VIDIOC_G_PARM) error: Inappropriate ioctl for device
V4L2_CORE: Unable to set 1/25 fps
V4L2_CORE: (VIDIOC_G_PARM) error: Inappropriate ioctl for device
control[0]:(unknown - 0x6) 0x980001 'User Controls'
control[2]:(unknown - 0x6) 0x990001 'Codec Controls'
V4L2_CORE: Could not grab image (select timeout): Resource temporarily unavailable
V4L2_CORE: (VIDIOC_G_PARM) error: Inappropriate ioctl for device
V4L2_CORE: Unable to set 1/60 fps
V4L2_CORE: Could not grab image (select timeout): Inappropriate ioctl for device
V4L2_CORE: Could not grab image (select timeout): Inappropriate ioctl for device
V4L2_CORE: Could not grab image (select timeout): Inappropriate ioctl for device
V4L2_CORE: Could not grab image (select timeout): Inappropriate ioctl for device
V4L2_CORE: Could not grab image (select timeout): Inappropriate ioctl for device
V4L2_CORE: Could not grab image (select timeout): Inappropriate ioctl for device
V4L2_CORE: Could not grab image (select timeout): Inappropriate ioctl for device
V4L2_CORE: Could not grab image (select timeout): Inappropriate ioctl for device
V4L2_CORE: Could not grab image (select timeout): Inappropriate ioctl for device
V4L2_CORE: Could not grab image (select timeout): Inappropriate ioctl for device
V4L2_CORE: Could not grab image (select timeout): Inappropriate ioctl for device
V4L2_CORE: Could not grab image (select timeout): Inappropriate ioctl for device
Segmentation fault (core dumped)

