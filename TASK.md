# TASK T-051 — Component input, all modes

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** recording analysed; encoder values per mode in the driver;
  hardware test T-051 (first step) open
- **Type:** driver feature (stage 2)
- **Branch:** `feat/component` (from `dev` 0.5.10)
- **TODO items:** T-051 (stage 2)

## Why

Component input is selectable, but its configuration still follows older
code (the "C++ blocks"): front end, back end and encoder values were never
compared with a recording with a source, and component capture with a
source has not been confirmed (`docs/component.md §3`). Stage 2 brings
component to the level of composite and S-video (RULES D7: within an
input, the lowest mode first).

## Goal (acceptance criteria, draft)

1. Every mode the device offers on component is detected and captured
   with the recorded configuration: 480i, 480p, 576i, 576p, 720p, 1080i,
   1080p30 (USB 2.0 limit, maintainer).
2. Mode changes while capturing, as recorded.
3. Output sizes and the "Allow 50/60fps" option of the Windows software,
   as recorded.
4. Encoder settings of the HDMI/component group (bitrate, mode, profile)
   with the limits of the recordings; shown in the settings window and
   the OBS plugin.
5. V4L2 reporting (DV timings, size, frame rate, field order) right for
   each mode; `v4l2-compliance` without failures.

## Evidence

- `traces/component/gchd_component_modes_output-sizes_5060fps_small_part1`
  and `_part2` (2026-11-25, decoded `.ops.txt` next to them): 576i, then
  576p, 720p, 1080i, 1080p; output sizes; "Allow 50/60fps" on and off;
  order not noted (maintainer).
- No recording of 480i/480p yet (TODO R7).

## Plan

1. Split the recording into its change sequences (NULL … START), read the
   detection values (`0x9C:68/67`), the front-end and back-end writes and
   the encoder fields per sequence, and name each sequence (mode, output
   size, 50/60 fps option). Result: an analysis note on `traces` and a
   table in `docs/component.md`.
2. Ask for single-option recordings where the sequence is unclear.
3. Compare with the driver's component path; change one item at a time
   (RULES D5), 576i first.

## Done so far

- Analysis (`traces/ANALYSIS-2026-11-25-component.md`): 19 change
  sequences (source mode changes, output sizes, the 50/60 fps option),
  encoder fields per mode and size, `docs/component.md §4`.
- Detection values of the driver match the recording for all five modes;
  the first front-end block per mode (`9C:01/02/03/05/06/07/1E/1F`,
  `B2/B5`) matches as well.
- Encoder (`src/driver/transcoder.c`), component at the standard output
  size, now as recorded: bitrate 11250 / 15000 / 26250 / 30000 per mode
  (was 15.625 kbit/s per pixel of width: 20000 at 720p, 11250 at 576p),
  minimum 0.35·B from 26250, GOP 6 for 576p (was 12), level 30 / 31 / 40
  (was the generic table: 31 / 32 / 42 / 41), `system_rate` from the
  composite-type formula with the progressive surcharge (was
  `1.075 × (B + 320) + 256`), colour BT.709 for HD and SMPTE 170M for SD
  (was SMPTE 170M), `v_vinpelclk` 1 at 50 Hz (was 0), 1080p30 signalled
  60000/1001. All recorded values reproduced (checked by calculation);
  W=1 clean.

## Next

1. Hardware test of the encoder values (below).
2. Front end and back end per mode: compare the rest of the recorded
   sequence (front-end colour tables, back end `98`, bridge `0900:0018`)
   with the driver's component path.
3. Output sizes (1280 × 720, 640 × 480) and the 50/60 fps option for
   HD inputs: V4L2 interface to be decided (output sizes like composite;
   the option as a control).
4. Mode changes while capturing (the Windows software restarts with the
   new mode's values, sequence #17/#18).

Wanted recordings (single option each, a few seconds per state): 480i
and 480p; 576i/576p and 1080i with 1280 × 720 and 640 × 480 output; the
50/60 fps option toggled on 576p, 720p (640 × 480) and 1080i.

## Hardware test (maintainer)

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-051.sh > tests/T-051.sh
bash gchd-test.sh T-051 --branch feat/component
```

## Findings

## Maintainer comments

These are things I observed while running the traces:
- On 576p input the 50/60fps option worked only if I had the output mode set to 720 or 1080.
- On 1080i input setting the output to 720 was seen as 720p60. The 50/60fps switch did nothing.
- Each output mode seems to be locked to a given bitrate. For the traces with single input I changed the bitrate value on the slider from max to min and back to max while playing with the 50/60fps setting to see if it affected the bitrate and it did for some inputs.
- I uploaded a total of 8 traces. 480i/480p is missing, which I'll do after the tests with this PAL PS3 I'm using.

Please on the next test let me see the output before asking me to change the input, because I need to see the settings menu to change the PS3's output. Output of the log is as follows:

gchd-test T-051 20261125-233513 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-051-20261125-233513.log
== build
$ git fetch -q origin
$ git checkout -q feat/component
$ git pull -q --ff-only
commit: a9f1ff0 Component: encoder values per source mode as recorded (bitrate, minimum, GOP, level, system_rate, colour, v_vinpelclk, 1080p30 timing); docs/component.md §4 (T-051)
commit: a9f1ff0 Component: encoder values per source mode as recorded (bitrate, minimum, GOP, level, system_rate, colour, v_vinpelclk, 1080p30 timing); docs/component.md §4 (T-051)
build rc=0 warnings=0

=== step 1: load module (fresh plug-in)
PROMPT: Unplug the Elgato from the PC (or detach it in VirtualBox), wait 3 s, plug it in again.
$ sudo insmod /home/ff5/elgato-gchd/src/driver/elgato_gchd.ko
H.264 device: /dev/video0
module: 0.5.8 
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -v width=720,height=480,pixelformat=MPEG
--- dmesg step 1
[   76.769563] usbcore: deregistering interface driver elgato_gchd
[   81.893080] usb 1-2: new high-speed USB device number 4 using xhci_hcd
[   82.137728] usbcore: registered new interface driver elgato_gchd
[   82.279513] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[   82.279529] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[   82.279534] usb 1-2: Product: GameCapture HD
[   82.279537] usb 1-2: Manufacturer: Elgato
[   82.279540] usb 1-2: SerialNumber: 04107F1267
[   82.286545] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[   82.293085] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[   82.750751] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[   82.750813] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== step 2: component 576i
PROMPT: Set the component source to 576i (game or menu on screen); wait 5 s.
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -I -V
Video input : 1 (Component: no signal)
Format Video Capture:
	Width/Height      : 1280/720
	Pixel Format      : 'MPEG' (MPEG-1/2/4)
	Field             : None
	Bytes per Line    : 0
	Size Image        : 65424
	Colorspace        : Rec. 709
	Transfer Function : Default (maps to Rec. 709)
	YCbCr/HSV Encoding: Default (maps to Rec. 709)
	Quantization      : Default (maps to Full Range)
	Flags             : 
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~10570 kbit/s errors=1
[FAIL] 576i: stream 720x576, field order tt (expected /width=720 height=576 .*field_order=tt/, got: width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
[FAIL] 576i: is the picture right (no colour cast, sharp, motion smooth)? -- No, idea. I need to be able to see the output to be able to change what the PS3 outputs.
--- dmesg step 2
[  119.170773] elgato_gchd 1-2:1.0: input switch requested: old=2 new=1 hw_initialized=0 streaming=0
[  119.170828] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[  119.184649] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  119.479468] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  119.546289] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  119.555304] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb74 raw6867=0xa6b4
[  119.555308] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[  119.555310] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[  119.555312] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[  120.301172] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  122.091199] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  122.106324] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  122.106523] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  122.108883] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  122.109479] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[  122.109857] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x020e
[  122.192446] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  127.128165] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  130.102162] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 3: component 576p
PROMPT: Set the component source to 576p (game or menu on screen); wait 5 s.
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -I -V
Video input : 1 (Component: no signal)
Format Video Capture:
	Width/Height      : 1280/720
	Pixel Format      : 'MPEG' (MPEG-1/2/4)
	Field             : None
	Bytes per Line    : 0
	Size Image        : 65424
	Colorspace        : Rec. 709
	Transfer Function : Default (maps to Rec. 709)
	YCbCr/HSV Encoding: Default (maps to Rec. 709)
	Quantization      : Default (maps to Full Range)
	Flags             : 
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~9941 kbit/s errors=1
[PASS] 576p: stream 720x576, field order progressive
[PASS] 576p: is the picture right (no colour cast, sharp, motion smooth)? -- I can't see anything. Are you running ffplay? Because nothing is opening for me to see any picture.
--- dmesg step 3
[  211.010898] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[  211.023362] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  211.224842] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  211.292502] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  211.304362] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b7
[  211.304366] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[  211.304369] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[  211.304371] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[  212.086311] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  214.322752] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  214.337821] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  214.338021] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  214.339499] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  214.340075] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[  214.340490] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[  214.423591] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  219.359601] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  221.755976] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 4: component 720p
PROMPT: Set the component source to 720p (game or menu on screen); wait 5 s.
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -I -V
Video input : 1 (Component: no signal)
Format Video Capture:
	Width/Height      : 1280/720
	Pixel Format      : 'MPEG' (MPEG-1/2/4)
	Field             : None
	Bytes per Line    : 0
	Size Image        : 65424
	Colorspace        : Rec. 709
	Transfer Function : Default (maps to Rec. 709)
	YCbCr/HSV Encoding: Default (maps to Rec. 709)
	Quantization      : Default (maps to Full Range)
	Flags             : 
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~10361 kbit/s errors=1
[FAIL] 720p: stream 1280x720, field order progressive (expected /width=1280 height=720 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
[FAIL] 720p: is the picture right (no colour cast, sharp, motion smooth)? -- You need to give me a way to see some output so I can change the output of the PS3.
--- dmesg step 4
[  267.027034] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[  267.038681] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  267.208315] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  267.271673] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  267.281995] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b7
[  267.281998] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[  267.282001] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[  267.282003] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[  268.070735] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  270.044959] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  270.062539] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  270.062847] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  270.064438] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  270.065099] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[  270.065535] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[  270.148927] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  275.084756] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  277.744425] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 5: component 1080i
PROMPT: Set the component source to 1080i (game or menu on screen); wait 5 s.
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -I -V
Video input : 1 (Component: no signal)
Format Video Capture:
	Width/Height      : 1280/720
	Pixel Format      : 'MPEG' (MPEG-1/2/4)
	Field             : None
	Bytes per Line    : 0
	Size Image        : 65424
	Colorspace        : Rec. 709
	Transfer Function : Default (maps to Rec. 709)
	YCbCr/HSV Encoding: Default (maps to Rec. 709)
	Quantization      : Default (maps to Full Range)
	Flags             : 
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~10466 kbit/s errors=1
[FAIL] 1080i: stream 1920x1080, field order tt (expected /width=1920 height=1080 .*field_order=tt/, got: width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
[FAIL] 1080i: is the picture right (no colour cast, sharp, motion smooth)?
--- dmesg step 5
[  307.334145] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[  307.347486] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  307.445370] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  307.512123] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  307.523160] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b4
[  307.523167] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[  307.523172] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[  307.523176] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[  308.312790] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  310.292161] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  310.310573] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  310.310927] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  310.312515] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  310.313061] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[  310.313611] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[  310.396613] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  315.331863] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  318.019840] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 6: component 1080p
PROMPT: Set the component source to 1080p (game or menu on screen); wait 5 s.
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -I -V
Video input : 1 (Component: no signal)
Format Video Capture:
	Width/Height      : 1280/720
	Pixel Format      : 'MPEG' (MPEG-1/2/4)
	Field             : None
	Bytes per Line    : 0
	Size Image        : 65424
	Colorspace        : Rec. 709
	Transfer Function : Default (maps to Rec. 709)
	YCbCr/HSV Encoding: Default (maps to Rec. 709)
	Quantization      : Default (maps to Full Range)
	Flags             : 
$ comp_cap
width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1  ~10518 kbit/s errors=1
[FAIL] 1080p: stream 1920x1080, field order progressive (expected /width=1920 height=1080 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
[FAIL] 1080p: is the picture right (no colour cast, sharp, motion smooth)?
--- dmesg step 6
[  323.825244] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[  323.832530] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  323.932705] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  323.993878] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  324.004306] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xbb75 raw6867=0xa6b7
[  324.004310] elgato_gchd 1-2:1.0: gchd-diag detected input=1 mode=720x576 fps=50/1 interlaced=0 signal_present=1
[  324.004313] elgato_gchd 1-2:1.0: signal detected: input=1 mode=720x576 fps=50/1 interlaced=0
[  324.004316] elgato_gchd 1-2:1.0: capture setup: input=1 mode=720x576 fps=50/1 interlaced=0
[  324.781505] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  326.759035] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  326.775042] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  326.775350] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  326.776182] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  326.776768] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=1 mode=720x576 fps=50/1 interlaced=0
[  326.777229] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x020e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x020e
[  326.860821] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  331.797640] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=140 bytes=9175040 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  334.583869] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 7: 480i / 480p (if the source offers them)
480 modes available: n
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
--- dmesg step 7
[  348.040785] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[  348.040857] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== summary (T-051, a9f1ff0 Component: encoder values per source mode as recorded (bitrate, minimum, GOP, level, system_rate, colour, v_vinpelclk, 1080p30 timing); docs/component.md §4 (T-051))
FAIL | step 2 component 576i | 576i: stream 720x576, field order tt (expected /width=720 height=576 .*field_order=tt/, got: width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
FAIL | step 2 component 576i | 576i: is the picture right (no colour cast, sharp, motion smooth)? -- No, idea. I need to be able to see the output to be able to change what the PS3 outputs.
PASS | step 3 component 576p | 576p: stream 720x576, field order progressive
PASS | step 3 component 576p | 576p: is the picture right (no colour cast, sharp, motion smooth)? -- I can't see anything. Are you running ffplay? Because nothing is opening for me to see any picture.
FAIL | step 4 component 720p | 720p: stream 1280x720, field order progressive (expected /width=1280 height=720 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
FAIL | step 4 component 720p | 720p: is the picture right (no colour cast, sharp, motion smooth)? -- You need to give me a way to see some output so I can change the output of the PS3.
FAIL | step 5 component 1080i | 1080i: stream 1920x1080, field order tt (expected /width=1920 height=1080 .*field_order=tt/, got: width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
FAIL | step 5 component 1080i | 1080i: is the picture right (no colour cast, sharp, motion smooth)?
FAIL | step 6 component 1080p | 1080p: stream 1920x1080, field order progressive (expected /width=1920 height=1080 .*field_order=progressive/, got: width=720 height=576 sample_aspect_ratio=12:11 level=30 color_primaries=smpte170m field_order=progressive r_frame_rate=50/1 width=720 height=576 sample_aspect_r)
FAIL | step 6 component 1080p | 1080p: is the picture right (no colour cast, sharp, motion smooth)?
