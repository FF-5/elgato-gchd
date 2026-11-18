# TASK T-053b — PAL S-video

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** PS1 without game may output 60 Hz; questions to the maintainer
- **Type:** driver fix + hardware test
- **Branch:** `fix/pal-svideo` (from `dev`); component (`feat/component`,
  T-051) continues after this
- **TODO items:** T-053b

## Why

The 7d8ed87 test log (S-video part included) shows the decoder reporting a
**60 Hz signal** on S-video throughout: `0x88:FA = 06/26` (low nibble 6),
lock `66` then `7E`, so 240p/480i was configured, correct for a 60 Hz
source. In the earlier S-video test with the same PS1 (game intro running)
it read `07/17` (50 Hz) and gave a correct 576i picture. Same driver code,
different signal.

Today the PS1 ran without a game. If its start-up menu outputs 60 Hz with
PAL colour ("PAL-60"), that explains S-video (correct 60 Hz handling; the
horizontal lines are how 240p looks in plain ffplay, see
`docs/video-output.md`: `-vf separatefields`) and composite (PAL colour at
60 Hz can confuse the composite decoder: `FA` low nibble `5`, then `D`).
The monitor test shows the PS1 output itself is fine.

## Questions (maintainer)

1. What does the PS1 show right now (start-up menu without disc, CD player,
   game)?
2. Does the monitor show 50 Hz or 60 Hz for the PS1 signal (on-screen
   info)?

## Hardware test (maintainer), depending on the answers

- If 60 Hz: PAL composite again with the game intro running (50 Hz), with
  `dev` and with this branch. PAL-60 becomes a separate item (a Windows
  recording of the start-up menu on composite would show how Windows
  handles it).
- If 50 Hz: the same commands as last time with the N64 on composite as a
  known-good check.

```sh
git fetch origin && git checkout fix/pal-svideo && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
```

## Findings

- 7d8ed87 test: composite no sync with and without the switch; S-video
  detected 60 Hz (`FA = 06/26`) and captured 240p/480i; earlier, with the
  game intro, the same PS1 gave 50 Hz (576i/288p).
- One input at a time is always used (PS1 has one output): not cabling.
- Control test: `dev` fails with PAL composite like this branch (`FA` low
  nibble `5`, then `D`).
- 5a189e2 test: composite no sync at all (`FA = ED/FD`) on a fresh start.
- 13b1a5b test: composite broken also after a fresh plug-in; cause: the
  `F5` retrigger enabled for composite polling (b17d75e).
- 847b592 test: composite after S-video still detected as 60 Hz although
  the recorded S-video → composite sequence is written.
- a675bc8 test: composite stretched vertically (60 Hz family detected for
  the PAL source); decoder still set for S-video / progressive.
- b17d75e test: PAL S-video works; composite after S-video needs a replug
  (decoder kept `0x88:02 = DC`).
- 13b3a79 and earlier: PAL S-video never locks; the PS1 outputs S-video
  (Windows recordings); cause: missing retrigger on `0x98:F5` bit 7.

---

## Maintainer comments

The NTSC N64 is working fine on both composite and S-Video:

ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
[33165.516341] usbcore: registered new interface driver elgato_gchd
[33233.820391] usb 1-2: new high-speed USB device number 31 using xhci_hcd
[33234.233963] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[33234.233973] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[33234.233976] usb 1-2: Product: GameCapture HD
[33234.233978] usb 1-2: Manufacturer: Elgato
[33234.233980] usb 1-2: SerialNumber: 04107F1267
[33234.236921] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[33234.240119] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[33235.668331] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[33235.668339] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[33235.668345] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[33244.809050] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[33245.337295] elgato_gchd 1-2:1.0: transcoder defaults loaded
[33245.405827] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[33245.539810] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[33245.541689] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[33245.541692] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[33245.606687] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[33245.608936] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[33245.608939] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[33245.670053] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[33245.672229] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[33245.672231] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[33245.736165] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[33245.738495] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[33245.738498] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[33245.802218] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[33245.804793] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[33245.804798] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[33245.868048] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[33245.872006] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33245.872017] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[33245.940603] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xcf mode=0xf
[33245.946664] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xce
[33245.946675] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[33246.112844] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xdf mode=0xf
[33246.117805] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xce
[33246.117816] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[33246.179824] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xcf mode=0xf
[33246.182952] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0x4e
[33246.182960] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[33246.256710] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[33246.259497] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[33246.259501] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[33246.363055] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[33246.445938] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[33246.512008] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[33246.582716] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[33246.647434] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[33246.709941] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xdd mode=0xd
[33246.780547] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xcd mode=0xd
[33246.856640] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xcd mode=0xd
[33246.925404] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xcd mode=0xd
[33246.989501] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x5d mode=0xd
[33247.053495] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x2f mode=0xf
[33247.123503] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x1e mode=0xe
[33247.196443] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x1e mode=0xe
[33247.264778] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x0e mode=0xe
[33247.325941] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[33247.334883] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[33247.336847] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[33247.393128] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[33247.401662] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[33247.403780] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[33247.465154] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[33247.476555] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[33247.479457] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[33247.543042] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[33247.554650] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[33247.557129] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[33247.557133] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[33247.557135] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[33247.557137] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[33248.348318] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[33249.151462] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[33249.171293] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[33249.171531] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[33249.176816] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[33249.177330] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[33249.177816] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[33249.310579] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[33249.683461] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[33249.729215] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[33249.773928] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[33249.818584] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[33249.863682] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[33256.371721] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[33257.845762] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[33257.846481] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[33257.847589] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[33257.848094] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[33257.848982] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[33257.968544] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[33257.968546] elgato_gchd 1-2:1.0: analog mode now 480i
[33258.342016] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[33258.387692] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[33258.433314] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[33258.478300] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[33258.522707] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[33278.931219] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[33280.236461] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[33280.236812] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[33280.238745] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[33280.239221] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[33280.239697] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[33280.359078] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[33280.359080] elgato_gchd 1-2:1.0: analog mode now 240p
[33280.736303] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[33280.778289] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[33280.822436] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[33280.870152] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[33280.913579] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[33338.571136] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[33338.571139] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[33338.571141] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[33344.427777] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[33345.096945] elgato_gchd 1-2:1.0: transcoder defaults loaded
[33345.162499] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[33345.264706] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[33345.315786] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[33345.318005] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[33345.318008] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33345.381001] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[33345.383624] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33345.383627] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33345.442702] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[33345.445234] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33345.445236] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33345.513659] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xdd mode=0xd
[33345.516040] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33345.516042] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33345.580256] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xcd mode=0xd
[33345.583527] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33345.583533] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33345.647545] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[33345.652010] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33345.652019] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33345.710958] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xdd mode=0xd
[33345.713296] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33345.713301] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33345.774626] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xcd mode=0xd
[33345.778172] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33345.778178] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33345.850833] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xcd mode=0xd
[33345.854490] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33345.854496] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33345.914898] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[33345.932906] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x7f mode=0xf
[33345.935357] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xee
[33345.935361] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33346.000456] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1e mode=0xe
[33346.072054] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[33346.140152] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[33346.157802] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33346.166701] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33346.168958] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[33346.234226] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33346.243838] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33346.246443] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[33346.319452] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33346.333699] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33346.336214] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[33346.336216] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[33346.336218] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[33346.336219] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[33347.125967] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[33347.228564] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0f mode=0xf
[33347.291903] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[33347.350885] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33347.360321] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33347.363137] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[33347.429880] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33347.441580] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33347.444351] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[33347.511160] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33347.521541] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33347.523720] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[33347.584499] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33347.593696] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33347.595809] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[33347.665850] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33347.679544] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33347.682019] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[33347.749813] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33347.761699] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33347.763825] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[33347.829517] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33347.839377] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33347.842251] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[33347.842254] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[33347.842256] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[33347.842259] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[33348.659316] elgato_gchd 1-2:1.0: back-end not settled: status 0xf2
[33348.668272] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[33348.683435] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[33348.683772] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[33348.689119] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[33348.689858] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[33348.690279] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[33348.789651] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[33349.162312] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[33349.207582] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[33349.252145] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[33349.298012] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[33349.342318] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[33355.377613] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[33356.773649] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[33356.773862] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[33356.774775] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[33356.775461] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=1
[33356.775951] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[33356.879666] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[33356.879668] elgato_gchd 1-2:1.0: analog mode now 480i
[33357.251822] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[33357.297631] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[33357.343261] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[33357.390186] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[33357.432905] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[33372.497207] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[33373.837679] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[33373.837902] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[33373.839425] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[33373.840083] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[33373.840638] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[33373.960337] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[33373.960339] elgato_gchd 1-2:1.0: analog mode now 240p
[33374.334175] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[33374.380278] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[33374.425605] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[33374.471169] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[33374.515704] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
ff5@ff5-VirtualBox:~$ 


Ok, weirdly now both PAL composite and S-Video seem to work on the PS1 (I forgot to clean dmesg before testing the PS1 but it's the last tests):

ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
[33165.516341] usbcore: registered new interface driver elgato_gchd
[33233.820391] usb 1-2: new high-speed USB device number 31 using xhci_hcd
[33234.233963] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[33234.233973] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[33234.233976] usb 1-2: Product: GameCapture HD
[33234.233978] usb 1-2: Manufacturer: Elgato
[33234.233980] usb 1-2: SerialNumber: 04107F1267
[33234.236921] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[33234.240119] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[33235.668331] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[33235.668339] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[33235.668345] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[33244.809050] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[33245.337295] elgato_gchd 1-2:1.0: transcoder defaults loaded
[33245.405827] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[33245.539810] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[33245.541689] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[33245.541692] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[33245.606687] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[33245.608936] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[33245.608939] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[33245.670053] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[33245.672229] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[33245.672231] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[33245.736165] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[33245.738495] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[33245.738498] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[33245.802218] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[33245.804793] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[33245.804798] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[33245.868048] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[33245.872006] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33245.872017] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[33245.940603] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xcf mode=0xf
[33245.946664] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xce
[33245.946675] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[33246.112844] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xdf mode=0xf
[33246.117805] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xce
[33246.117816] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[33246.179824] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xcf mode=0xf
[33246.182952] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0x4e
[33246.182960] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[33246.256710] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[33246.259497] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[33246.259501] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[33246.363055] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[33246.445938] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[33246.512008] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[33246.582716] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[33246.647434] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[33246.709941] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xdd mode=0xd
[33246.780547] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xcd mode=0xd
[33246.856640] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xcd mode=0xd
[33246.925404] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xcd mode=0xd
[33246.989501] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x5d mode=0xd
[33247.053495] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x2f mode=0xf
[33247.123503] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x1e mode=0xe
[33247.196443] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x1e mode=0xe
[33247.264778] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x0e mode=0xe
[33247.325941] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[33247.334883] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[33247.336847] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[33247.393128] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[33247.401662] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[33247.403780] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[33247.465154] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[33247.476555] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[33247.479457] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[33247.543042] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[33247.554650] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[33247.557129] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[33247.557133] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[33247.557135] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[33247.557137] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[33248.348318] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[33249.151462] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[33249.171293] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[33249.171531] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[33249.176816] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[33249.177330] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[33249.177816] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[33249.310579] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[33249.683461] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[33249.729215] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[33249.773928] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[33249.818584] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[33249.863682] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[33256.371721] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[33257.845762] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[33257.846481] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[33257.847589] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[33257.848094] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[33257.848982] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[33257.968544] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[33257.968546] elgato_gchd 1-2:1.0: analog mode now 480i
[33258.342016] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[33258.387692] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[33258.433314] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[33258.478300] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[33258.522707] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[33278.931219] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[33280.236461] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[33280.236812] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[33280.238745] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[33280.239221] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[33280.239697] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[33280.359078] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[33280.359080] elgato_gchd 1-2:1.0: analog mode now 240p
[33280.736303] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[33280.778289] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[33280.822436] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[33280.870152] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[33280.913579] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[33338.571136] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[33338.571139] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[33338.571141] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[33344.427777] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[33345.096945] elgato_gchd 1-2:1.0: transcoder defaults loaded
[33345.162499] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[33345.264706] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[33345.315786] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[33345.318005] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[33345.318008] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33345.381001] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[33345.383624] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33345.383627] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33345.442702] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[33345.445234] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33345.445236] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33345.513659] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xdd mode=0xd
[33345.516040] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33345.516042] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33345.580256] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xcd mode=0xd
[33345.583527] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33345.583533] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33345.647545] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[33345.652010] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33345.652019] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33345.710958] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xdd mode=0xd
[33345.713296] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33345.713301] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33345.774626] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xcd mode=0xd
[33345.778172] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33345.778178] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33345.850833] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xcd mode=0xd
[33345.854490] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33345.854496] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33345.914898] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[33345.932906] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x7f mode=0xf
[33345.935357] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xee
[33345.935361] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33346.000456] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1e mode=0xe
[33346.072054] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[33346.140152] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[33346.157802] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33346.166701] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33346.168958] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[33346.234226] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33346.243838] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33346.246443] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[33346.319452] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33346.333699] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33346.336214] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[33346.336216] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[33346.336218] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[33346.336219] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[33347.125967] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[33347.228564] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0f mode=0xf
[33347.291903] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[33347.350885] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33347.360321] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33347.363137] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[33347.429880] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33347.441580] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33347.444351] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[33347.511160] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33347.521541] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33347.523720] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[33347.584499] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33347.593696] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33347.595809] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[33347.665850] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33347.679544] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33347.682019] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[33347.749813] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33347.761699] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33347.763825] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[33347.829517] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33347.839377] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33347.842251] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[33347.842254] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[33347.842256] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[33347.842259] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[33348.659316] elgato_gchd 1-2:1.0: back-end not settled: status 0xf2
[33348.668272] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[33348.683435] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[33348.683772] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[33348.689119] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[33348.689858] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[33348.690279] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[33348.789651] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[33349.162312] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[33349.207582] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[33349.252145] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[33349.298012] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[33349.342318] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[33355.377613] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[33356.773649] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[33356.773862] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[33356.774775] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[33356.775461] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=1
[33356.775951] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[33356.879666] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[33356.879668] elgato_gchd 1-2:1.0: analog mode now 480i
[33357.251822] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[33357.297631] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[33357.343261] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[33357.390186] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[33357.432905] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[33372.497207] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[33373.837679] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[33373.837902] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[33373.839425] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[33373.840083] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[33373.840638] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[33373.960337] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[33373.960339] elgato_gchd 1-2:1.0: analog mode now 240p
[33374.334175] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[33374.380278] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[33374.425605] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[33374.471169] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[33374.515704] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[33506.768364] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[33507.195263] elgato_gchd 1-2:1.0: transcoder defaults loaded
[33507.262485] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[33507.391826] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[33507.407676] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[33507.409701] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[33507.409704] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33507.476755] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[33507.483946] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[33507.483965] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33507.553082] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[33507.556764] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33507.556772] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33507.616360] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[33507.619468] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33507.619473] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33507.684467] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[33507.690274] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33507.690283] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33507.748312] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[33507.751198] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33507.751202] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33507.816456] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[33507.819189] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33507.819194] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33507.881357] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[33507.883632] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33507.883638] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33507.950631] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[33507.953860] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33507.953866] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33508.021542] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[33508.024660] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[33508.024664] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[33508.085557] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[33508.157247] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[33508.224118] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[33508.289679] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[33508.360729] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[33508.440549] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[33508.510432] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[33508.579710] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[33508.598693] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1f mode=0xf
[33508.669667] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[33508.744643] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x6e mode=0xe
[33508.822588] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1e mode=0xe
[33508.888140] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[33508.908470] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33508.919355] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33508.922424] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[33508.990835] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33508.999466] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33509.001566] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[33509.066267] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33509.078944] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33509.081397] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[33509.081399] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[33509.081400] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[33509.081402] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[33509.886174] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[33509.971625] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0f mode=0xf
[33510.038740] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1f mode=0xf
[33510.109129] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[33510.179299] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1e mode=0xe
[33510.257979] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1e mode=0xe
[33510.331194] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33510.342099] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33510.344450] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[33510.406091] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33510.415982] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33510.418167] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[33510.481690] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33510.492321] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33510.494610] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[33510.566246] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33510.576494] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33510.580281] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[33510.643517] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33510.655775] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33510.657908] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[33510.718107] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33510.729083] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33510.732589] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[33510.795442] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[33510.805771] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[33510.808015] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[33510.808017] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[33510.808019] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[33510.808021] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[33513.259749] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[33513.275595] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[33513.275806] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[33513.278058] elgato_gchd 1-2:1.0: source changed to 50 Hz during setup; configuring again
[33513.340732] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[33513.364541] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x17 mode=0x7
[33513.373074] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x17 masked_mode=0x7
[33513.375289] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[33513.375291] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[33513.375293] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x576 fps=50/1 interlaced=1
[33513.375295] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x576 fps=50/1 interlaced=1
[33513.588516] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[33513.603647] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[33513.603842] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[33513.608797] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[33513.609582] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x576 fps=50/1 interlaced=1
[33513.609989] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[33513.749703] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[33514.147908] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[33514.192825] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[33514.237895] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[33514.282615] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[33514.328625] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[33588.973367] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[33588.973376] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[33588.973382] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[33595.083820] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[33595.143268] elgato_gchd 1-2:1.0: transcoder defaults loaded
[33595.206906] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[33595.579414] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[33595.589393] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[33595.591566] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[33595.591569] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[33595.591572] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[33595.591573] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[33596.400879] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[33596.783792] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[33596.801465] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[33596.801803] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[33596.806473] elgato_gchd 1-2:1.0: source changed to 576i during setup; configuring again
[33596.866650] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x07 mode=0x7
[33596.877571] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[33596.880118] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[33596.880119] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[33596.880121] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[33596.880123] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[33597.163433] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[33597.182432] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[33597.182671] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[33597.188462] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[33597.188938] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[33597.189507] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[33597.328327] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[33597.726229] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[33597.772179] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[33597.816653] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[33597.860817] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[33597.907672] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[33615.995331] elgato_gchd 1-2:1.0: analog mode change detected: 576i -> 288p
[33616.470080] elgato_gchd 1-2:1.0: analog mode now 288p
[33620.239772] elgato_gchd 1-2:1.0: analog mode change detected: 288p -> 576i
[33620.733528] elgato_gchd 1-2:1.0: analog mode now 576i
[33632.587283] elgato_gchd 1-2:1.0: analog mode change detected: 576i -> 288p
[33633.064482] elgato_gchd 1-2:1.0: analog mode now 288p
[33636.578295] elgato_gchd 1-2:1.0: analog mode change detected: 288p -> 576i
[33637.063418] elgato_gchd 1-2:1.0: analog mode now 576i
ff5@ff5-VirtualBox:~$ 

