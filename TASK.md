# TASK T-020a — Composite 480i: a stable picture in guvcview

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** delivery fix ready on `fix/v4l2-format-negotiation`; waiting for
  the hardware test
- **Type:** driver fix, hardware acceptance
- **Branch:** `fix/v4l2-format-negotiation` (stacked on
  `fix/analog-start-per-traces`)
- **TODO item:** Phase 3, T-020 (first step); Phase 4, T-033

## Why

The composite 480i path now reaches streaming: START completes in about
0.1 s and data flows (TODO "Hardware test log"). guvcview shows a picture
briefly and then crashes. The driver delivered at most one frame per USB
completion, dropped frames when no buffer was queued, could deliver frames
out of order, and started at an arbitrary picture.

## Changes on the branch

1. `907af08` format negotiation: S_INPUT sets the input's nominal format;
   the negotiated format never changes while buffers are allocated;
   TRY_FMT/S_FMT adjust instead of failing; G_PARM/S_PARM; analog formats
   are reported interlaced.
2. `374fc83` delivery: every frame is delivered in order while buffers are
   queued (no drops when a buffer is late); delivery starts at the first
   picture carrying an SPS after each (re)start.

## Acceptance criteria

1. guvcview shows a stable composite 480i picture for at least 60 s without
   crashing.
2. The ffmpeg check below records a stream that `ffprobe` reports as H.264
   720×480 without decode errors.
3. dmesg shows no `V4L2 buffer error` and no `dropping PES` warnings.

If guvcview still crashes while the ffmpeg recording is clean, the next step
is to compare guvcview's H.264 handling with the buffer contents (one PES per
buffer, field-coded pictures).

## Hardware test (maintainer)

```sh
git fetch origin && git checkout fix/v4l2-format-negotiation && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
guvcview -d /dev/video0
sudo dmesg > dmesg-guvcview.txt
```

Then, with guvcview closed:

```sh
sudo rmmod elgato_gchd; sudo dmesg -C; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffmpeg -f v4l2 -input_format h264 -i /dev/video0 -t 10 -c copy test.h264
ffprobe -v error -show_entries stream=codec_name,width,height,field_order test.h264
ffmpeg -v error -i test.h264 -f null -
```

Paste the outputs (guvcview, dmesg, ffprobe, the last ffmpeg command) into
the maintainer comments below.

## Findings

_(filled in during the task)_

---

## Maintainer comments

_Written by the maintainer only. Anything lasting is transferred to
`RULES.md` or `TODO.md` when the next task is prepared, and this section is
then emptied._

Ok, same behavior as before. Output was briefly visible just before guvcview crashed, but the image was black and white I think and the bottom 3rd/4th of the frame was garbled.

ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
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
Segmentation fault (core dumped)
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 

ff5@ff5-VirtualBox:~$ dmesg
[ 4089.663587] usbcore: registered new interface driver elgato_gchd
[ 4101.699027] workqueue: hub_event hogged CPU for >10000us 4 times, consider switching to WQ_UNBOUND
[ 4101.827485] usb 2-1: new high-speed USB device number 5 using ehci-pci
[ 4102.225952] usb 2-1: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[ 4102.225969] usb 2-1: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[ 4102.225975] usb 2-1: Product: GameCapture HD
[ 4102.225980] usb 2-1: Manufacturer: Elgato
[ 4102.225984] usb 2-1: SerialNumber: 04107F1267
[ 4102.228611] elgato_gchd 2-1:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[ 4102.231728] elgato_gchd 2-1:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[ 4102.249331] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=4594 comm=v4l_id
[ 4102.251090] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1395 comm=wireplumber
[ 4102.251690] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1395 comm=wireplumber
[ 4102.252241] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 4102.252259] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 4102.253101] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 4102.253142] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 4102.253165] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 4102.253175] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 4102.253354] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 4102.253391] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 4107.502706] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=4596 comm=v4l2-ctl
[ 4107.502870] elgato_gchd 2-1:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[ 4107.502880] elgato_gchd 2-1:1.0: input selected: input=2; detection deferred until device open
[ 4107.502886] elgato_gchd 2-1:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 4134.459790] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=4597 comm=guvcview
[ 4134.461294] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=4597 comm=guvcview
[ 4134.592553] elgato_gchd 2-1:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 4134.645308] elgato_gchd 2-1:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4134.647675] elgato_gchd 2-1:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4134.651455] elgato_gchd 2-1:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4134.655380] elgato_gchd 2-1:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4134.659235] elgato_gchd 2-1:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4134.662926] elgato_gchd 2-1:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4134.666794] elgato_gchd 2-1:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4134.670758] elgato_gchd 2-1:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4134.674412] elgato_gchd 2-1:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4134.678033] elgato_gchd 2-1:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4134.681566] elgato_gchd 2-1:1.0: firmware chunk offset=327680 size=4016 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4134.757137] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[ 4134.758486] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
[ 4135.503465] elgato_gchd 2-1:1.0: transcoder defaults loaded
[ 4135.506149] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[ 4135.507767] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x04
[ 4135.515370] elgato_gchd 2-1:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.518675] elgato_gchd 2-1:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.522746] elgato_gchd 2-1:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.526587] elgato_gchd 2-1:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.530387] elgato_gchd 2-1:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.534084] elgato_gchd 2-1:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.537802] elgato_gchd 2-1:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.541756] elgato_gchd 2-1:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.545947] elgato_gchd 2-1:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.549922] elgato_gchd 2-1:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.553953] elgato_gchd 2-1:1.0: firmware chunk offset=327680 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.557979] elgato_gchd 2-1:1.0: firmware chunk offset=360448 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.561603] elgato_gchd 2-1:1.0: firmware chunk offset=393216 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.565175] elgato_gchd 2-1:1.0: firmware chunk offset=425984 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.568721] elgato_gchd 2-1:1.0: firmware chunk offset=458752 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.572391] elgato_gchd 2-1:1.0: firmware chunk offset=491520 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.576257] elgato_gchd 2-1:1.0: firmware chunk offset=524288 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.579664] elgato_gchd 2-1:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.581184] elgato_gchd 2-1:1.0: firmware chunk offset=589824 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.584768] elgato_gchd 2-1:1.0: firmware chunk offset=622592 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.588457] elgato_gchd 2-1:1.0: firmware chunk offset=655360 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.592304] elgato_gchd 2-1:1.0: firmware chunk offset=688128 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.596281] elgato_gchd 2-1:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.600127] elgato_gchd 2-1:1.0: firmware chunk offset=753664 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.603869] elgato_gchd 2-1:1.0: firmware chunk offset=786432 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.607943] elgato_gchd 2-1:1.0: firmware chunk offset=819200 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.611796] elgato_gchd 2-1:1.0: firmware chunk offset=851968 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.615912] elgato_gchd 2-1:1.0: firmware chunk offset=884736 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.620013] elgato_gchd 2-1:1.0: firmware chunk offset=917504 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.624017] elgato_gchd 2-1:1.0: firmware chunk offset=950272 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.628152] elgato_gchd 2-1:1.0: firmware chunk offset=983040 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.631678] elgato_gchd 2-1:1.0: firmware chunk offset=1015808 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.635762] elgato_gchd 2-1:1.0: firmware chunk offset=1048576 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.639873] elgato_gchd 2-1:1.0: firmware chunk offset=1081344 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.643866] elgato_gchd 2-1:1.0: firmware chunk offset=1114112 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.647864] elgato_gchd 2-1:1.0: firmware chunk offset=1146880 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.651971] elgato_gchd 2-1:1.0: firmware chunk offset=1179648 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.655898] elgato_gchd 2-1:1.0: firmware chunk offset=1212416 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.659866] elgato_gchd 2-1:1.0: firmware chunk offset=1245184 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.663704] elgato_gchd 2-1:1.0: firmware chunk offset=1277952 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.668189] elgato_gchd 2-1:1.0: firmware chunk offset=1310720 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.672149] elgato_gchd 2-1:1.0: firmware chunk offset=1343488 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.675884] elgato_gchd 2-1:1.0: firmware chunk offset=1376256 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.679363] elgato_gchd 2-1:1.0: firmware chunk offset=1409024 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.683039] elgato_gchd 2-1:1.0: firmware chunk offset=1441792 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.686718] elgato_gchd 2-1:1.0: firmware chunk offset=1474560 size=26224 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 4135.732387] elgato_gchd 2-1:1.0: device idle, processor state 0x27f97b
[ 4136.621609] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6\n
[ 4136.748071] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 4136.776552] elgato_gchd 2-1:1.0: analog format initial status=0x7e; starting in 480i
[ 4136.776564] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 4136.776571] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 4136.776576] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 4143.132817] elgato_gchd 2-1:1.0: capture setup: encoder start completed
[ 4143.135126] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 4144.946106] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[ 4145.003454] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[ 4145.275750] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[ 4145.278648] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 4145.281176] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 4145.284901] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 4145.286073] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 4145.294345] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 4145.296989] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[ 4145.298687] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[ 4145.308986] elgato_gchd 2-1:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0000 ack=1
[ 4145.327060] elgato_gchd 2-1:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 4145.334799] elgato_gchd 2-1:1.0: START poll=1 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 4145.342596] elgato_gchd 2-1:1.0: START poll=2 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 4145.349681] elgato_gchd 2-1:1.0: START poll=3 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 4145.357330] elgato_gchd 2-1:1.0: START poll=4 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 4145.364976] elgato_gchd 2-1:1.0: START poll=5 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 4145.372883] elgato_gchd 2-1:1.0: START poll=6 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 4145.380535] elgato_gchd 2-1:1.0: START poll=7 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 4145.388189] elgato_gchd 2-1:1.0: START poll=8 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 4145.395638] elgato_gchd 2-1:1.0: START poll=9 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 4145.403513] elgato_gchd 2-1:1.0: START poll=10 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0010 ack=1
[ 4145.413543] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[ 4145.752852] elgato_gchd 2-1:1.0: async bulk-IN data: read=1 ep=0x81 actual=65536 total_bytes=65536
[ 4145.775794] elgato_gchd 2-1:1.0: async bulk-IN data: read=2 ep=0x81 actual=65536 total_bytes=131072
[ 4145.797259] elgato_gchd 2-1:1.0: async bulk-IN data: read=3 ep=0x81 actual=65536 total_bytes=196608
[ 4145.819887] elgato_gchd 2-1:1.0: async bulk-IN data: read=4 ep=0x81 actual=65536 total_bytes=262144
[ 4145.842748] elgato_gchd 2-1:1.0: async bulk-IN data: read=5 ep=0x81 actual=65536 total_bytes=327680
[ 4146.052772] guvcview[4615]: segfault at 741bb3971008 ip 0000741bc1988eeb sp 0000741bb5dfe708 error 4 in libc.so.6[741bc1828000+189000] likely on CPU 3 (core 3, socket 0)
[ 4146.052794] Code: 00 00 00 48 3b 15 65 23 08 00 0f 87 cf 01 00 00 4c 8d 04 11 49 31 c8 49 c1 e8 3f 81 e1 00 0f 00 00 44 01 c1 0f 84 8a 00 00 00 <c5> fe 6f 6c 16 e0 c5 fe 6f 74 16 c0 48 89 f9 48 83 cf 1f c5 fe 6f
[ 4146.916239] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[ 4146.917901] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[ 4152.683640] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[ 4152.684951] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[ 4161.008516] usb 2-1: USB disconnect, device number 5
[ 4161.011240] elgato_gchd 2-1:1.0: hardware shutdown on last close failed: -108; keeping initialized state
ff5@ff5-VirtualBox:~$ 

ff5@ff5-VirtualBox:~$ ffmpeg -f v4l2 -input_format h264 -i /dev/video0 -t 10 -c copy test.h264
ffmpeg version 6.1.1-3ubuntu5 Copyright (c) 2000-2023 the FFmpeg developers
  built with gcc 13 (Ubuntu 13.2.0-23ubuntu3)
  configuration: --prefix=/usr --extra-version=3ubuntu5 --toolchain=hardened --libdir=/usr/lib/x86_64-linux-gnu --incdir=/usr/include/x86_64-linux-gnu --arch=amd64 --enable-gpl --disable-stripping --disable-omx --enable-gnutls --enable-libaom --enable-libass --enable-libbs2b --enable-libcaca --enable-libcdio --enable-libcodec2 --enable-libdav1d --enable-libflite --enable-libfontconfig --enable-libfreetype --enable-libfribidi --enable-libglslang --enable-libgme --enable-libgsm --enable-libharfbuzz --enable-libmp3lame --enable-libmysofa --enable-libopenjpeg --enable-libopenmpt --enable-libopus --enable-librubberband --enable-libshine --enable-libsnappy --enable-libsoxr --enable-libspeex --enable-libtheora --enable-libtwolame --enable-libvidstab --enable-libvorbis --enable-libvpx --enable-libwebp --enable-libx265 --enable-libxml2 --enable-libxvid --enable-libzimg --enable-openal --enable-opencl --enable-opengl --disable-sndio --enable-libvpl --disable-libmfx --enable-libdc1394 --enable-libdrm --enable-libiec61883 --enable-chromaprint --enable-frei0r --enable-ladspa --enable-libbluray --enable-libjack --enable-libpulse --enable-librabbitmq --enable-librist --enable-libsrt --enable-libssh --enable-libsvtav1 --enable-libx264 --enable-libzmq --enable-libzvbi --enable-lv2 --enable-sdl2 --enable-libplacebo --enable-librav1e --enable-pocketsphinx --enable-librsvg --enable-libjxl --enable-shared
  WARNING: library configuration mismatch
  avcodec     configuration: --prefix=/usr --extra-version=3ubuntu5 --toolchain=hardened --libdir=/usr/lib/x86_64-linux-gnu --incdir=/usr/include/x86_64-linux-gnu --arch=amd64 --enable-gpl --disable-stripping --disable-omx --enable-gnutls --enable-libaom --enable-libass --enable-libbs2b --enable-libcaca --enable-libcdio --enable-libcodec2 --enable-libdav1d --enable-libflite --enable-libfontconfig --enable-libfreetype --enable-libfribidi --enable-libglslang --enable-libgme --enable-libgsm --enable-libharfbuzz --enable-libmp3lame --enable-libmysofa --enable-libopenjpeg --enable-libopenmpt --enable-libopus --enable-librubberband --enable-libshine --enable-libsnappy --enable-libsoxr --enable-libspeex --enable-libtheora --enable-libtwolame --enable-libvidstab --enable-libvorbis --enable-libvpx --enable-libwebp --enable-libx265 --enable-libxml2 --enable-libxvid --enable-libzimg --enable-openal --enable-opencl --enable-opengl --disable-sndio --enable-libvpl --disable-libmfx --enable-libdc1394 --enable-libdrm --enable-libiec61883 --enable-chromaprint --enable-frei0r --enable-ladspa --enable-libbluray --enable-libjack --enable-libpulse --enable-librabbitmq --enable-librist --enable-libsrt --enable-libssh --enable-libsvtav1 --enable-libx264 --enable-libzmq --enable-libzvbi --enable-lv2 --enable-sdl2 --enable-libplacebo --enable-librav1e --enable-pocketsphinx --enable-librsvg --enable-libjxl --enable-shared --enable-version3 --disable-doc --disable-programs --disable-static --enable-libaribb24 --enable-libopencore_amrnb --enable-libopencore_amrwb --enable-libtesseract --enable-libvo_amrwbenc --enable-libsmbclient
  libavutil      58. 29.100 / 58. 29.100
  libavcodec     60. 31.102 / 60. 31.102
  libavformat    60. 16.100 / 60. 16.100
  libavdevice    60.  3.100 / 60.  3.100
  libavfilter     9. 12.100 /  9. 12.100
  libswscale      7.  5.100 /  7.  5.100
  libswresample   4. 12.100 /  4. 12.100
  libpostproc    57.  3.100 / 57.  3.100
Input #0, video4linux2,v4l2, from '/dev/video0':
  Duration: N/A, start: 4349.369178, bitrate: N/A
  Stream #0:0: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 60 fps, 60 tbr, 1000k tbn
Output #0, h264, to 'test.h264':
  Metadata:
    encoder         : Lavf60.16.100
  Stream #0:0: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], q=2-31, 60 fps, 60 tbr, 29.97 tbn
Stream mapping:
  Stream #0:0 -> #0:0 (copy)
Press [q] to stop, [?] for help
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: -1 >= -1
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 1 >= 1
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 2 >= 2
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 3 >= 0
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 3 >= 1
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 3 >= 2
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 3 >= 3
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 4 >= 4
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 5 >= 5
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 6 >= 6
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 7 >= 7
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 8 >= 8
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 9 >= 9
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 10 >= 10
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 11 >= 11
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 12 >= 12
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 13 >= 13
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 14 >= 14
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 15 >= 15
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 16 >= 16
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 17 >= 17
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 18 >= 18
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 19 >= 19
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 20 >= 20
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 21 >= 21
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 22 >= 22
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 23 >= 23
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 24 >= 24
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 25 >= 25
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 26 >= 26
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 27 >= 27
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 28 >= 28
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 29 >= 29
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 30 >= 30
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 31 >= 31
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 32 >= 32
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 34 >= 34
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 35 >= 35
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 36 >= 36
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 37 >= 37
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 38 >= 38
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 39 >= 39
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 40 >= 40
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 41 >= 41
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 42 >= 42
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 44 >= 44
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 46 >= 46
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 47 >= 47
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 48 >= 48
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 50 >= 50
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 52 >= 52
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 54 >= 54
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 56 >= 56
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 58 >= 58
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 60 >= 60
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 62 >= 62
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 64 >= 64
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 66 >= 66
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 68 >= 68
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 70 >= 70
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 72 >= 72
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 74 >= 74
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 76 >= 76
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 78 >= 78
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 80 >= 80
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 82 >= 82
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 84 >= 84
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 86 >= 86
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 88 >= 88
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 90 >= 90
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 92 >= 92
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 94 >= 94
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 96 >= 96
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 98 >= 98
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 100 >= 100
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 102 >= 102
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 104 >= 104
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 106 >= 106
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 108 >= 108
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 112 >= 112
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 114 >= 114
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 116 >= 116
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 118 >= 118
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 120 >= 120
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 122 >= 122
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 124 >= 124
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 126 >= 126
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 128 >= 128
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 130 >= 130
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 132 >= 132
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 134 >= 134
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 136 >= 136
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 138 >= 138
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 140 >= 140
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 142 >= 142
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 144 >= 144
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 146 >= 146
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 148 >= 148
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 150 >= 150
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 152 >= 152
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 154 >= 154
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 156 >= 156
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 158 >= 158
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 160 >= 160
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 162 >= 162
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 164 >= 164
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 166 >= 166
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 168 >= 168
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 170 >= 170
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 172 >= 172
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 174 >= 174
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 176 >= 176
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 178 >= 178
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 179 >= 179
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 180 >= 180
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 181 >= 181
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 182 >= 182
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 183 >= 183
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 184 >= 184
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 185 >= 185
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 186 >= 186
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 187 >= 187
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 188 >= 188
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 189 >= 189
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 190 >= 190
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 191 >= 191
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 192 >= 192
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 193 >= 193
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 194 >= 194
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 195 >= 195
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 196 >= 196
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 197 >= 197
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 198 >= 198
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 199 >= 199
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 200 >= 200
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 201 >= 201
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 202 >= 202
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 203 >= 203
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 204 >= 204
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 205 >= 205
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 206 >= 206
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 207 >= 207
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 208 >= 208
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 209 >= 209
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 210 >= 210
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 211 >= 211
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 212 >= 212
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 213 >= 213
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 214 >= 214
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 215 >= 215
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 216 >= 216
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 217 >= 217
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 218 >= 218
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 219 >= 219
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 220 >= 220
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 221 >= 221
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 222 >= 222
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 223 >= 223
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 224 >= 224
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 225 >= 225
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 226 >= 226
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 227 >= 227
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 228 >= 228
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 229 >= 229
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 231 >= 231
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 233 >= 233
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 235 >= 235
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 237 >= 237
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 239 >= 239
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 241 >= 241
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 243 >= 243
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 245 >= 245
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 247 >= 247
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 249 >= 249
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 251 >= 251
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 253 >= 253
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 255 >= 255
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 257 >= 257
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 259 >= 259
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 261 >= 261
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 263 >= 263
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 265 >= 265
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 267 >= 267
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 269 >= 269
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 271 >= 271
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 273 >= 273
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 275 >= 275
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 277 >= 277
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 279 >= 279
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 281 >= 281
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 283 >= 283
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 285 >= 285
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 287 >= 287
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 289 >= 289
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 291 >= 291
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 293 >= 293
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 295 >= 295
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 297 >= 297
    Last message repeated 1 times
[h264 @ 0x5d519ca63140] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 299 >= 299
    Last message repeated 1 times
[out#0/h264 @ 0x5d519ca4ac40] video:27906kB audio:0kB subtitle:0kB other streams:0kB global headers:0kB muxing overhead: 0.000000%
size=   27906kB time=00:00:09.98 bitrate=22889.9kbits/s speed=0.995x    
[video4linux2,v4l2 @ 0x5d519ca45e80] Some buffers are still owned by the caller on close.


ff5@ff5-VirtualBox:~$ ffprobe -v error -show_entries stream=codec_name,width,height,field_order test.h264
[STREAM]
codec_name=h264
width=720
height=480
field_order=tt
[/STREAM]
ff5@ff5-VirtualBox:~$ 

