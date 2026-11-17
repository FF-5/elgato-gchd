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

Ok, this is what happened. guvcview opened and it struggled to show any output but just before it crashed I saw something. This is all outputs:

ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 --get-fmt-video
Format Video Capture:
	Width/Height      : 720/480
	Pixel Format      : 'H264' (H.264)
	Field             : Interlaced
	Bytes per Line    : 0
	Size Image        : 8388608
	Colorspace        : Rec. 709
	Transfer Function : Default (maps to Rec. 709)
	YCbCr/HSV Encoding: Default (maps to Rec. 709)
	Quantization      : Default (maps to Full Range)
	Flags             : 
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ guvcview -d /dev/video0
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
control[0]:(unknown - 0x6) 0x980001 'User Controls'
control[2]:(unknown - 0x6) 0x990001 'Codec Controls'
V4L2_CORE: Could not grab image (select timeout): Resource temporarily unavailable
^CSegmentation fault (core dumped)

ff5@ff5-VirtualBox:~$ dmesg
[ 2894.453884] usbcore: registered new interface driver elgato_gchd
[ 2907.991587] usb 2-1: new high-speed USB device number 4 using ehci-pci
[ 2908.406360] usb 2-1: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[ 2908.406584] usb 2-1: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[ 2908.406592] usb 2-1: Product: GameCapture HD
[ 2908.406597] usb 2-1: Manufacturer: Elgato
[ 2908.406601] usb 2-1: SerialNumber: 04107F1267
[ 2908.408662] elgato_gchd 2-1:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[ 2908.411672] elgato_gchd 2-1:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[ 2908.431411] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=4123 comm=v4l_id
[ 2908.433211] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1395 comm=wireplumber
[ 2908.433434] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1395 comm=wireplumber
[ 2908.433919] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 2908.433928] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 2908.434548] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 2908.434582] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 2908.434590] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 2908.434595] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 2908.434892] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 2908.434913] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 2952.515817] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=4142 comm=v4l2-ctl
[ 2952.515886] elgato_gchd 2-1:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[ 2952.515890] elgato_gchd 2-1:1.0: input selected: input=2; detection deferred until device open
[ 2952.515892] elgato_gchd 2-1:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 2984.961041] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=4148 comm=v4l2-ctl
[ 3000.177140] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=4149 comm=guvcview
[ 3000.177653] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=4149 comm=guvcview
[ 3000.323600] elgato_gchd 2-1:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 3000.357525] elgato_gchd 2-1:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3000.360582] elgato_gchd 2-1:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3000.364462] elgato_gchd 2-1:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3000.367937] elgato_gchd 2-1:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3000.372627] elgato_gchd 2-1:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3000.375653] elgato_gchd 2-1:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3000.379156] elgato_gchd 2-1:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3000.382700] elgato_gchd 2-1:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3000.384040] elgato_gchd 2-1:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3000.387841] elgato_gchd 2-1:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3000.391689] elgato_gchd 2-1:1.0: firmware chunk offset=327680 size=4016 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3000.469810] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[ 3000.471088] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
[ 3001.206325] elgato_gchd 2-1:1.0: transcoder defaults loaded
[ 3001.209398] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[ 3001.210834] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x04
[ 3001.218556] elgato_gchd 2-1:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.222415] elgato_gchd 2-1:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.226351] elgato_gchd 2-1:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.230531] elgato_gchd 2-1:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.234942] elgato_gchd 2-1:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.238809] elgato_gchd 2-1:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.242734] elgato_gchd 2-1:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.246401] elgato_gchd 2-1:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.250090] elgato_gchd 2-1:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.251665] elgato_gchd 2-1:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.255154] elgato_gchd 2-1:1.0: firmware chunk offset=327680 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.259308] elgato_gchd 2-1:1.0: firmware chunk offset=360448 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.263323] elgato_gchd 2-1:1.0: firmware chunk offset=393216 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.267387] elgato_gchd 2-1:1.0: firmware chunk offset=425984 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.271142] elgato_gchd 2-1:1.0: firmware chunk offset=458752 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.275185] elgato_gchd 2-1:1.0: firmware chunk offset=491520 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.279346] elgato_gchd 2-1:1.0: firmware chunk offset=524288 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.283347] elgato_gchd 2-1:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.287165] elgato_gchd 2-1:1.0: firmware chunk offset=589824 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.291028] elgato_gchd 2-1:1.0: firmware chunk offset=622592 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.295118] elgato_gchd 2-1:1.0: firmware chunk offset=655360 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.299133] elgato_gchd 2-1:1.0: firmware chunk offset=688128 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.303440] elgato_gchd 2-1:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.307427] elgato_gchd 2-1:1.0: firmware chunk offset=753664 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.311288] elgato_gchd 2-1:1.0: firmware chunk offset=786432 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.315087] elgato_gchd 2-1:1.0: firmware chunk offset=819200 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.318967] elgato_gchd 2-1:1.0: firmware chunk offset=851968 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.323142] elgato_gchd 2-1:1.0: firmware chunk offset=884736 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.326779] elgato_gchd 2-1:1.0: firmware chunk offset=917504 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.330668] elgato_gchd 2-1:1.0: firmware chunk offset=950272 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.334754] elgato_gchd 2-1:1.0: firmware chunk offset=983040 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.338583] elgato_gchd 2-1:1.0: firmware chunk offset=1015808 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.342360] elgato_gchd 2-1:1.0: firmware chunk offset=1048576 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.346293] elgato_gchd 2-1:1.0: firmware chunk offset=1081344 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.350300] elgato_gchd 2-1:1.0: firmware chunk offset=1114112 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.354247] elgato_gchd 2-1:1.0: firmware chunk offset=1146880 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.358240] elgato_gchd 2-1:1.0: firmware chunk offset=1179648 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.362266] elgato_gchd 2-1:1.0: firmware chunk offset=1212416 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.366172] elgato_gchd 2-1:1.0: firmware chunk offset=1245184 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.370276] elgato_gchd 2-1:1.0: firmware chunk offset=1277952 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.374144] elgato_gchd 2-1:1.0: firmware chunk offset=1310720 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.378006] elgato_gchd 2-1:1.0: firmware chunk offset=1343488 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.381821] elgato_gchd 2-1:1.0: firmware chunk offset=1376256 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.385315] elgato_gchd 2-1:1.0: firmware chunk offset=1409024 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.389211] elgato_gchd 2-1:1.0: firmware chunk offset=1441792 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.393025] elgato_gchd 2-1:1.0: firmware chunk offset=1474560 size=26224 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 3001.441764] elgato_gchd 2-1:1.0: device idle, processor state 0x27f97b
[ 3002.278580] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6\n
[ 3002.408268] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[ 3002.440943] elgato_gchd 2-1:1.0: analog format initial status=0x7e; starting in 480i
[ 3002.440958] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 3002.440968] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 3002.440975] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 3008.682923] elgato_gchd 2-1:1.0: capture setup: encoder start completed
[ 3008.685223] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 3010.423021] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[ 3010.476023] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[ 3010.751048] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[ 3010.753450] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 3010.756352] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 3010.759035] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 3010.759835] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 3010.769163] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 3010.771709] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[ 3010.773265] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[ 3010.783355] elgato_gchd 2-1:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0000 ack=1
[ 3010.803468] elgato_gchd 2-1:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 3010.811549] elgato_gchd 2-1:1.0: START poll=1 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 3010.819092] elgato_gchd 2-1:1.0: START poll=2 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 3010.827328] elgato_gchd 2-1:1.0: START poll=3 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 3010.835845] elgato_gchd 2-1:1.0: START poll=4 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 3010.842952] elgato_gchd 2-1:1.0: START poll=5 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 3010.851211] elgato_gchd 2-1:1.0: START poll=6 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 3010.860004] elgato_gchd 2-1:1.0: START poll=7 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 3010.868499] elgato_gchd 2-1:1.0: START poll=8 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 3010.876839] elgato_gchd 2-1:1.0: START poll=9 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0010 ack=0
[ 3010.885342] elgato_gchd 2-1:1.0: START poll=10 state=0x0002 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0010 ack=1
[ 3010.895961] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[ 3011.227375] elgato_gchd 2-1:1.0: async bulk-IN data: read=1 ep=0x81 actual=65536 total_bytes=65536
[ 3011.250398] elgato_gchd 2-1:1.0: async bulk-IN data: read=2 ep=0x81 actual=65536 total_bytes=131072
[ 3011.272033] elgato_gchd 2-1:1.0: async bulk-IN data: read=3 ep=0x81 actual=65536 total_bytes=196608
[ 3011.294617] elgato_gchd 2-1:1.0: async bulk-IN data: read=4 ep=0x81 actual=65536 total_bytes=262144
[ 3011.317394] elgato_gchd 2-1:1.0: async bulk-IN data: read=5 ep=0x81 actual=65536 total_bytes=327680
[ 3011.529549] guvcview[4167]: segfault at 7b60db9660c8 ip 00007b60ebf88eeb sp 00007b60e03fe708 error 4 in libc.so.6[7b60ebe28000+189000] likely on CPU 2 (core 2, socket 0)
[ 3011.529569] Code: 00 00 00 48 3b 15 65 23 08 00 0f 87 cf 01 00 00 4c 8d 04 11 49 31 c8 49 c1 e8 3f 81 e1 00 0f 00 00 44 01 c1 0f 84 8a 00 00 00 <c5> fe 6f 6c 16 e0 c5 fe 6f 74 16 c0 48 89 f9 48 83 cf 1f c5 fe 6f
[ 3012.234262] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[ 3012.235900] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[ 3018.087850] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[ 3018.089453] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
ff5@ff5-VirtualBox:~$ 

