# TASK T-020a — Composite 480i: a stable picture in guvcview

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** delivery fix ready on `fix/v4l2-format-negotiation`; waiting for
  the hardware test
- **Type:** driver fix, hardware acceptance
- **Branch:** `fix/v4l2-format-negotiation` (stacked on
  `fix/analog-start-per-traces`)
- **TODO item:** Phase 3, T-020 (first step); Phase 4, T-033

## Why

The composite 480i path reaches streaming and the driver output is valid
H.264 (TODO "Hardware test log", third entry: ffmpeg recorded 720×480,
field order tt). guvcview still crashes; its symptoms point to guvcview
itself (see the test log). The default bitrate was twice the recorded value
and is fixed.

## Changes on the branch

1. `907af08` format negotiation (S_INPUT nominal format, no format change
   while buffers are allocated, TRY_FMT/S_FMT adjust, G_PARM/S_PARM).
2. `374fc83` delivery: frames delivered in order, no drops while waiting for
   a buffer, start at the first SPS.
3. `81bc5ec` default bitrate 11250 kbit/s at 720 wide, level 3.1.

## Acceptance criteria

1. `ffplay` shows a stable composite 480i picture for at least 60 s.
2. The decode check below prints no errors.
3. The recording is about 11–12 Mbit/s and `ffprobe` reports level 31.
4. dmesg shows no `V4L2 buffer error` and no `dropping PES` warnings.

guvcview is not an acceptance criterion while its 720-wide H.264 handling is
in question; try it once anyway and report the result.

## Hardware test (maintainer)

```sh
git fetch origin && git checkout fix/v4l2-format-negotiation && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0      # watch ~60 s, then q
sudo dmesg > dmesg-ffplay.txt
```

Recording and decode check:

```sh
ffmpeg -f v4l2 -input_format h264 -i /dev/video0 -t 10 -c copy test.h264
ffprobe -v error -show_entries stream=codec_name,profile,level,width,height,field_order test.h264
ffmpeg -v error -i test.h264 -f null - ; echo "decode errors above (none = clean)"
ls -l test.h264
```

Finally `guvcview -d /dev/video0` once. Paste the outputs into the
maintainer comments below.

## Findings

_(filled in during the task)_

---

## Maintainer comments

_Written by the maintainer only. Anything lasting is transferred to
`RULES.md` or `TODO.md` when the next task is prepared, and this section is
then emptied._

ffplay did seem to work kind of fine, but I noticed the picture was laggy or stuttering every now and then. Could be because I'm working in a VM?

ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ ffplay -f v4l2 -input_format h264 -i /dev/video0
ffplay version 6.1.1-3ubuntu5 Copyright (c) 2003-2023 the FFmpeg developers
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
Input #0, video4linux2,v4l2, from '/dev/video0':B sq=    0B f=0/0   
  Duration: N/A, start: 5097.037042, bitrate: N/A
  Stream #0:0: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 60 fps, 60 tbr, 1000k tbn

ff5@ff5-VirtualBox:~$ dmesg
[ 5052.965146] usbcore: registered new interface driver elgato_gchd
[ 5061.007677] usb 2-1: new high-speed USB device number 7 using ehci-pci
[ 5061.407483] usb 2-1: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[ 5061.407494] usb 2-1: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[ 5061.407496] usb 2-1: Product: GameCapture HD
[ 5061.407498] usb 2-1: Manufacturer: Elgato
[ 5061.407500] usb 2-1: SerialNumber: 04107F1267
[ 5061.409732] elgato_gchd 2-1:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[ 5061.413268] elgato_gchd 2-1:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[ 5061.434920] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=5038 comm=v4l_id
[ 5061.436128] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1395 comm=wireplumber
[ 5061.436381] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1395 comm=wireplumber
[ 5061.437007] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 5061.437017] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 5061.437382] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 5061.437411] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 5061.437417] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 5061.437428] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 5061.438548] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 5061.438578] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 5069.077507] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=5040 comm=v4l2-ctl
[ 5069.077693] elgato_gchd 2-1:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[ 5069.077705] elgato_gchd 2-1:1.0: input selected: input=2; detection deferred until device open
[ 5069.077713] elgato_gchd 2-1:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 5083.939066] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=5054 comm=read_thread
[ 5083.974859] elgato_gchd 2-1:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 5084.041339] elgato_gchd 2-1:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.044443] elgato_gchd 2-1:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.048388] elgato_gchd 2-1:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.052053] elgato_gchd 2-1:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.055654] elgato_gchd 2-1:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.059200] elgato_gchd 2-1:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.063506] elgato_gchd 2-1:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.066543] elgato_gchd 2-1:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.070133] elgato_gchd 2-1:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.073795] elgato_gchd 2-1:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.077593] elgato_gchd 2-1:1.0: firmware chunk offset=327680 size=4016 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.151386] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[ 5084.152783] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
[ 5084.829622] elgato_gchd 2-1:1.0: transcoder defaults loaded
[ 5084.832228] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[ 5084.833587] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x04
[ 5084.840015] elgato_gchd 2-1:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.842525] elgato_gchd 2-1:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.846240] elgato_gchd 2-1:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.850179] elgato_gchd 2-1:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.853813] elgato_gchd 2-1:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.857391] elgato_gchd 2-1:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.861194] elgato_gchd 2-1:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.865318] elgato_gchd 2-1:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.869117] elgato_gchd 2-1:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.872639] elgato_gchd 2-1:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.876286] elgato_gchd 2-1:1.0: firmware chunk offset=327680 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.880070] elgato_gchd 2-1:1.0: firmware chunk offset=360448 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.883733] elgato_gchd 2-1:1.0: firmware chunk offset=393216 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.887158] elgato_gchd 2-1:1.0: firmware chunk offset=425984 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.890627] elgato_gchd 2-1:1.0: firmware chunk offset=458752 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.894204] elgato_gchd 2-1:1.0: firmware chunk offset=491520 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.897678] elgato_gchd 2-1:1.0: firmware chunk offset=524288 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.901018] elgato_gchd 2-1:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.904771] elgato_gchd 2-1:1.0: firmware chunk offset=589824 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.908250] elgato_gchd 2-1:1.0: firmware chunk offset=622592 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.911524] elgato_gchd 2-1:1.0: firmware chunk offset=655360 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.915004] elgato_gchd 2-1:1.0: firmware chunk offset=688128 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.918729] elgato_gchd 2-1:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.922321] elgato_gchd 2-1:1.0: firmware chunk offset=753664 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.925691] elgato_gchd 2-1:1.0: firmware chunk offset=786432 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.929115] elgato_gchd 2-1:1.0: firmware chunk offset=819200 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.932510] elgato_gchd 2-1:1.0: firmware chunk offset=851968 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.936063] elgato_gchd 2-1:1.0: firmware chunk offset=884736 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.939741] elgato_gchd 2-1:1.0: firmware chunk offset=917504 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.943235] elgato_gchd 2-1:1.0: firmware chunk offset=950272 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.944617] elgato_gchd 2-1:1.0: firmware chunk offset=983040 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.948124] elgato_gchd 2-1:1.0: firmware chunk offset=1015808 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.949670] elgato_gchd 2-1:1.0: firmware chunk offset=1048576 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.953368] elgato_gchd 2-1:1.0: firmware chunk offset=1081344 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.957253] elgato_gchd 2-1:1.0: firmware chunk offset=1114112 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.961335] elgato_gchd 2-1:1.0: firmware chunk offset=1146880 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.965113] elgato_gchd 2-1:1.0: firmware chunk offset=1179648 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.968873] elgato_gchd 2-1:1.0: firmware chunk offset=1212416 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.972436] elgato_gchd 2-1:1.0: firmware chunk offset=1245184 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.976332] elgato_gchd 2-1:1.0: firmware chunk offset=1277952 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.980250] elgato_gchd 2-1:1.0: firmware chunk offset=1310720 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.983895] elgato_gchd 2-1:1.0: firmware chunk offset=1343488 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.987689] elgato_gchd 2-1:1.0: firmware chunk offset=1376256 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.991675] elgato_gchd 2-1:1.0: firmware chunk offset=1409024 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.995361] elgato_gchd 2-1:1.0: firmware chunk offset=1441792 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5084.999583] elgato_gchd 2-1:1.0: firmware chunk offset=1474560 size=26224 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 5085.041277] elgato_gchd 2-1:1.0: device idle, processor state 0x27f97b
[ 5085.858650] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6\n
[ 5085.977283] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[ 5086.007276] elgato_gchd 2-1:1.0: analog format initial status=0x7e; starting in 480i
[ 5086.007288] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 5086.007295] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5086.007300] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5091.959784] elgato_gchd 2-1:1.0: capture setup: encoder start completed
[ 5091.962864] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 5093.641443] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[ 5093.691375] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[ 5093.957013] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[ 5093.959323] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 5093.962075] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 5093.963897] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 5093.964339] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 5093.972594] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 5093.975248] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[ 5093.976526] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[ 5093.986348] elgato_gchd 2-1:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0000 ack=1
[ 5094.003420] elgato_gchd 2-1:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 5094.010755] elgato_gchd 2-1:1.0: START poll=1 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 5094.018031] elgato_gchd 2-1:1.0: START poll=2 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 5094.025599] elgato_gchd 2-1:1.0: START poll=3 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 5094.033202] elgato_gchd 2-1:1.0: START poll=4 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 5094.041090] elgato_gchd 2-1:1.0: START poll=5 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 5094.048889] elgato_gchd 2-1:1.0: START poll=6 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 5094.056426] elgato_gchd 2-1:1.0: START poll=7 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 5094.064384] elgato_gchd 2-1:1.0: START poll=8 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 5094.071708] elgato_gchd 2-1:1.0: START poll=9 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 5094.079029] elgato_gchd 2-1:1.0: START poll=10 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 5094.087214] elgato_gchd 2-1:1.0: START poll=11 state=0x0002 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0010 ack=1
[ 5094.097696] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[ 5094.453799] elgato_gchd 2-1:1.0: async bulk-IN data: read=1 ep=0x81 actual=65536 total_bytes=65536
[ 5094.499966] elgato_gchd 2-1:1.0: async bulk-IN data: read=2 ep=0x81 actual=65536 total_bytes=131072
[ 5094.544815] elgato_gchd 2-1:1.0: async bulk-IN data: read=3 ep=0x81 actual=65536 total_bytes=196608
[ 5094.590166] elgato_gchd 2-1:1.0: async bulk-IN data: read=4 ep=0x81 actual=65536 total_bytes=262144
[ 5094.634802] elgato_gchd 2-1:1.0: async bulk-IN data: read=5 ep=0x81 actual=65536 total_bytes=327680
[ 5098.960363] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=104 bytes=6815744 empty=0 errors=0 queued=0 start_pending=0
[ 5103.984314] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=219 bytes=14352384 empty=0 errors=0 queued=0 start_pending=0
[ 5109.008158] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=334 bytes=21889024 empty=0 errors=0 queued=0 start_pending=0
[ 5114.033626] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=449 bytes=29425664 empty=0 errors=0 queued=0 start_pending=0
[ 5119.057355] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=564 bytes=36962304 empty=0 errors=0 queued=0 start_pending=0
[ 5124.080250] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=679 bytes=44498944 empty=0 errors=0 queued=0 start_pending=0
[ 5129.104510] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=794 bytes=52035584 empty=0 errors=0 queued=0 start_pending=0
[ 5131.890032] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[ 5131.891712] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[ 5144.670726] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[ 5144.671869] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
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
  Duration: N/A, start: 5316.843071, bitrate: N/A
  Stream #0:0: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 60 fps, 60 tbr, 1000k tbn
File 'test.h264' already exists. Overwrite? [y/N] y
Output #0, h264, to 'test.h264':
  Metadata:
    encoder         : Lavf60.16.100
  Stream #0:0: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], q=2-31, 60 fps, 60 tbr, 29.97 tbn
Stream mapping:
  Stream #0:0 -> #0:0 (copy)
Press [q] to stop, [?] for help
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: -1 >= -1
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 1 >= 1
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 2 >= 2
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 3 >= 0
    Last message repeated 1 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 3 >= 1
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 3 >= 2
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 3 >= 1
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 3 >= 2
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 3 >= 3
    Last message repeated 1 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 5 >= 5
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 6 >= 5
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 6 >= 6
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 7 >= 7
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 9 >= 9
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 11 >= 11
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 12 >= 12
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 15 >= 15
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 16 >= 16
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 19 >= 19
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 20 >= 20
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 22 >= 22
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 23 >= 22
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 23 >= 23
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 24 >= 24
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 26 >= 26
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 28 >= 28
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 29 >= 29
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 109 >= 109
    Last message repeated 11 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 109
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 110 >= 110
    Last message repeated 1 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 111 >= 111
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 113 >= 113
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 115 >= 115
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 116 >= 115
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 116 >= 116
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 117 >= 117
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 119 >= 119
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 121 >= 121
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 122 >= 122
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 125 >= 125
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 126 >= 126
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 129 >= 129
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 130 >= 130
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 132 >= 132
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 133 >= 132
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 133 >= 133
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 134 >= 134
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 136 >= 136
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 138 >= 138
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 140 >= 140
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 142 >= 142
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 143 >= 143
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 146 >= 146
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 147 >= 147
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 149 >= 149
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 150 >= 149
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 150 >= 150
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 151 >= 151
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 153 >= 153
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 155 >= 155
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 157 >= 157
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 159 >= 159
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 160 >= 160
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 163 >= 163
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 164 >= 164
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 167 >= 167
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 168 >= 168
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 170 >= 170
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 171 >= 170
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 171 >= 171
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 172 >= 172
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 174 >= 174
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 176 >= 176
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 178 >= 178
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 180 >= 180
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 181 >= 181
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 184 >= 184
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 185 >= 185
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 187 >= 187
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 188 >= 187
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 188 >= 188
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 189 >= 189
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 191 >= 191
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 193 >= 193
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 195 >= 195
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 197 >= 197
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 198 >= 198
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 201 >= 201
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 202 >= 202
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 205 >= 205
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 206 >= 206
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 208 >= 208
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 210 >= 210
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 212 >= 212
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 214 >= 214
    Last message repeated 1 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 214 >= 214
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 215 >= 215
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 218 >= 218
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 219 >= 219
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 222 >= 222
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 223 >= 223
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 225 >= 225
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 227 >= 227
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 229 >= 229
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 231 >= 231
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 233 >= 233
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 235 >= 235
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 236 >= 236
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 239 >= 239
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 240 >= 240
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 243 >= 243
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 244 >= 244
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 246 >= 246
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 248 >= 248
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 250 >= 250
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 252 >= 252
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 253 >= 253
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 256 >= 256
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 257 >= 257
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 260 >= 260
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 261 >= 261
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 263 >= 263
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 265 >= 265
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 267 >= 267
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 269 >= 269
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 271 >= 271
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 273 >= 273
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 274 >= 274
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 277 >= 277
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 278 >= 278
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 280 >= 280
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 282 >= 282
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 284 >= 284
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 286 >= 286
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 288 >= 288
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 290 >= 290
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 291 >= 291
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 294 >= 294
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 295 >= 295
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 298 >= 298
    Last message repeated 2 times
[h264 @ 0x59ffa22ac880] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 299 >= 299
[out#0/h264 @ 0x59ffa2290c40] video:11822kB audio:0kB subtitle:0kB other streams:0kB global headers:0kB muxing overhead: 0.000000%
size=   11822kB time=00:00:09.98 bitrate=9697.2kbits/s speed=1.53x    
[video4linux2,v4l2 @ 0x59ffa228be80] Some buffers are still owned by the caller on close.
^C

ff5@ff5-VirtualBox:~$ ffprobe -v error -show_entries stream=codec_name,profile,level,width,height,field_order test.h264
[STREAM]
codec_name=h264
profile=High
width=720
height=480
level=31
field_order=tt
[/STREAM]
ff5@ff5-VirtualBox:~$ ffmpeg -v error -i test.h264 -f null - ; echo "decode errors above (none = clean)" 
decode errors above (none = clean)
ff5@ff5-VirtualBox:~$ ls -l test.h264
-rw-rw-r-- 1 ff5 ff5 12105833 Nov 17 05:56 test.h264
ff5@ff5-VirtualBox:~$ 

