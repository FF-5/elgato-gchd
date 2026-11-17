# TASK T-043a — Smooth composite 480i, then composite 240p

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** timestamp fix ready; waiting for the hardware test
- **Type:** driver fix + hardware test
- **Branch:** `fix/buffer-timestamps` (from `dev`)
- **TODO items:** Phase 3 T-020 (delivery quality), Phase 5 T-041/T-043

## Why

Composite 480i is accepted and merged into `dev` (TODO "Hardware test log").
ffplay stuttered now and then: buffers carried the delivery time, and each
64 KB bulk transfer holds several fields, so fields arrived in bursts with
nearly identical timestamps. Buffers now carry the stream's own decoding
timestamps (`6aaf465`).

Next in the bring-up order is composite 240p. The encoder is already
programmed as for 480i (as in the traces). A 480i ↔ 240p change while
capturing still uses a full encoder restart in the driver (TODO T-041); the
traces show a decoder-only change for 480i → 240p.

## Acceptance criteria

1. Composite 480i in ffplay: no periodic stutter over 60 s.
2. Composite 240p (source already in 240p when capture starts): ffplay shows
   a stable picture; the recording decodes without errors and `ffprobe`
   reports 720×480, `field_order tt`.
3. Report what happens when the source switches 480i → 240p and back while
   ffplay runs (expected to be imperfect until T-041).

## Hardware test (maintainer)

```sh
git fetch origin && git checkout fix/buffer-timestamps && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0     # 480i, ~60 s
```

Then with the source in 240p (reload the module first):

```sh
sudo rmmod elgato_gchd; sudo dmesg -C; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0     # 240p; then switch 480i <-> 240p
sudo dmesg > dmesg-240p.txt
ffmpeg -f v4l2 -input_format h264 -i /dev/video0 -t 10 -c copy test240p.h264
ffprobe -v error -show_entries stream=codec_name,level,width,height,field_order test240p.h264
ffmpeg -v error -i test240p.h264 -f null - ; echo "decode errors above (none = clean)"
```

To view the 240p recording without field blending:
`ffplay -vf separatefields,scale=720:480:flags=neighbor test240p.h264`.

Paste the outputs and observations into the maintainer comments below.

## Findings

_(filled in during the task)_

---

## Maintainer comments

_Written by the maintainer only. Anything lasting is transferred to
`RULES.md` or `TODO.md` when the next task is prepared, and this section is
then emptied._

Output was still somewhat laggy with stutters. I also tried guvcview and it's still crashing. It might work with ffplay but it's not a sign that it might work with applications like OBS.

ff5@ff5-VirtualBox:~$ ffplay -f v4l2 -input_format h264 -i /dev/video0
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
  Duration: N/A, start: 6266.845429, bitrate: N/A
  Stream #0:0: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 60 fps, 60 tbr, 1000k tbn
[video4linux2,v4l2 @ 0x7b925c000c80] Some buffers are still owned by the caller on close.
^Cff5@ff5-VirtualBox:~$ 

ff5@ff5-VirtualBox:~$ dmesg
[ 6213.914418] usbcore: registered new interface driver elgato_gchd
[ 6230.702638] workqueue: hub_event hogged CPU for >10000us 8 times, consider switching to WQ_UNBOUND
[ 6230.854976] usb 2-1: new high-speed USB device number 9 using ehci-pci
[ 6231.275028] usb 2-1: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[ 6231.275049] usb 2-1: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[ 6231.275056] usb 2-1: Product: GameCapture HD
[ 6231.275062] usb 2-1: Manufacturer: Elgato
[ 6231.275066] usb 2-1: SerialNumber: 04107F1267
[ 6231.277266] elgato_gchd 2-1:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[ 6231.280994] elgato_gchd 2-1:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[ 6231.297409] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=5483 comm=v4l_id
[ 6231.298683] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1395 comm=wireplumber
[ 6231.299371] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1395 comm=wireplumber
[ 6231.299828] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 6231.299837] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 6231.300654] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 6231.300679] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 6231.300685] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 6231.300689] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 6231.301436] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 6231.301460] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 6236.093431] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=5485 comm=v4l2-ctl
[ 6236.093498] elgato_gchd 2-1:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[ 6236.093501] elgato_gchd 2-1:1.0: input selected: input=2; detection deferred until device open
[ 6236.093504] elgato_gchd 2-1:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 6253.256285] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=5499 comm=read_thread
[ 6253.282767] elgato_gchd 2-1:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 6253.319796] elgato_gchd 2-1:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6253.322791] elgato_gchd 2-1:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6253.326437] elgato_gchd 2-1:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6253.329810] elgato_gchd 2-1:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6253.333344] elgato_gchd 2-1:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6253.336909] elgato_gchd 2-1:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6253.340431] elgato_gchd 2-1:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6253.344091] elgato_gchd 2-1:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6253.347639] elgato_gchd 2-1:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6253.351019] elgato_gchd 2-1:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6253.354675] elgato_gchd 2-1:1.0: firmware chunk offset=327680 size=4016 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6253.427899] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[ 6253.429426] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
[ 6254.087078] elgato_gchd 2-1:1.0: transcoder defaults loaded
[ 6254.089217] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[ 6254.090681] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x04
[ 6254.095451] elgato_gchd 2-1:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.098748] elgato_gchd 2-1:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.102180] elgato_gchd 2-1:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.105565] elgato_gchd 2-1:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.108921] elgato_gchd 2-1:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.112488] elgato_gchd 2-1:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.116100] elgato_gchd 2-1:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.117415] elgato_gchd 2-1:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.119033] elgato_gchd 2-1:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.122580] elgato_gchd 2-1:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.124056] elgato_gchd 2-1:1.0: firmware chunk offset=327680 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.125568] elgato_gchd 2-1:1.0: firmware chunk offset=360448 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.127196] elgato_gchd 2-1:1.0: firmware chunk offset=393216 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.128658] elgato_gchd 2-1:1.0: firmware chunk offset=425984 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.132310] elgato_gchd 2-1:1.0: firmware chunk offset=458752 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.135973] elgato_gchd 2-1:1.0: firmware chunk offset=491520 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.139563] elgato_gchd 2-1:1.0: firmware chunk offset=524288 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.143448] elgato_gchd 2-1:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.147398] elgato_gchd 2-1:1.0: firmware chunk offset=589824 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.151223] elgato_gchd 2-1:1.0: firmware chunk offset=622592 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.154939] elgato_gchd 2-1:1.0: firmware chunk offset=655360 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.159789] elgato_gchd 2-1:1.0: firmware chunk offset=688128 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.163306] elgato_gchd 2-1:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.166948] elgato_gchd 2-1:1.0: firmware chunk offset=753664 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.170443] elgato_gchd 2-1:1.0: firmware chunk offset=786432 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.173942] elgato_gchd 2-1:1.0: firmware chunk offset=819200 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.177645] elgato_gchd 2-1:1.0: firmware chunk offset=851968 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.181405] elgato_gchd 2-1:1.0: firmware chunk offset=884736 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.185147] elgato_gchd 2-1:1.0: firmware chunk offset=917504 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.188652] elgato_gchd 2-1:1.0: firmware chunk offset=950272 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.192364] elgato_gchd 2-1:1.0: firmware chunk offset=983040 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.195711] elgato_gchd 2-1:1.0: firmware chunk offset=1015808 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.199251] elgato_gchd 2-1:1.0: firmware chunk offset=1048576 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.202750] elgato_gchd 2-1:1.0: firmware chunk offset=1081344 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.206230] elgato_gchd 2-1:1.0: firmware chunk offset=1114112 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.209749] elgato_gchd 2-1:1.0: firmware chunk offset=1146880 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.213135] elgato_gchd 2-1:1.0: firmware chunk offset=1179648 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.216512] elgato_gchd 2-1:1.0: firmware chunk offset=1212416 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.219908] elgato_gchd 2-1:1.0: firmware chunk offset=1245184 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.223391] elgato_gchd 2-1:1.0: firmware chunk offset=1277952 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.227013] elgato_gchd 2-1:1.0: firmware chunk offset=1310720 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.230691] elgato_gchd 2-1:1.0: firmware chunk offset=1343488 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.234600] elgato_gchd 2-1:1.0: firmware chunk offset=1376256 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.238127] elgato_gchd 2-1:1.0: firmware chunk offset=1409024 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.241610] elgato_gchd 2-1:1.0: firmware chunk offset=1441792 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.244960] elgato_gchd 2-1:1.0: firmware chunk offset=1474560 size=26224 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 6254.290099] elgato_gchd 2-1:1.0: device idle, processor state 0x27f97b
[ 6255.112988] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6\n
[ 6255.233299] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[ 6255.265167] elgato_gchd 2-1:1.0: analog format initial status=0x7e; starting in 480i
[ 6255.265179] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 6255.265184] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 6255.265226] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 6261.162572] elgato_gchd 2-1:1.0: capture setup: encoder start completed
[ 6261.164672] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 6262.838853] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[ 6262.889766] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[ 6263.147906] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[ 6263.150093] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 6263.152755] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 6263.154047] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 6263.154529] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 6263.162630] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 6263.164949] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[ 6263.166137] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[ 6263.175152] elgato_gchd 2-1:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0000 ack=1
[ 6263.190982] elgato_gchd 2-1:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 6263.197830] elgato_gchd 2-1:1.0: START poll=1 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 6263.204562] elgato_gchd 2-1:1.0: START poll=2 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 6263.211306] elgato_gchd 2-1:1.0: START poll=3 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 6263.218447] elgato_gchd 2-1:1.0: START poll=4 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 6263.225411] elgato_gchd 2-1:1.0: START poll=5 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 6263.232157] elgato_gchd 2-1:1.0: START poll=6 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 6263.238995] elgato_gchd 2-1:1.0: START poll=7 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 6263.245974] elgato_gchd 2-1:1.0: START poll=8 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 6263.253037] elgato_gchd 2-1:1.0: START poll=9 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 6263.260054] elgato_gchd 2-1:1.0: START poll=10 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 6263.267124] elgato_gchd 2-1:1.0: START poll=11 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 6263.274241] elgato_gchd 2-1:1.0: START poll=12 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0010 ack=0
[ 6263.281488] elgato_gchd 2-1:1.0: START poll=13 state=0x0002 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0010 ack=1
[ 6263.291397] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[ 6263.645106] elgato_gchd 2-1:1.0: async bulk-IN data: read=1 ep=0x81 actual=65536 total_bytes=65536
[ 6263.690151] elgato_gchd 2-1:1.0: async bulk-IN data: read=2 ep=0x81 actual=65536 total_bytes=131072
[ 6263.735486] elgato_gchd 2-1:1.0: async bulk-IN data: read=3 ep=0x81 actual=65536 total_bytes=196608
[ 6263.780024] elgato_gchd 2-1:1.0: async bulk-IN data: read=4 ep=0x81 actual=65536 total_bytes=262144
[ 6263.825133] elgato_gchd 2-1:1.0: async bulk-IN data: read=5 ep=0x81 actual=65536 total_bytes=327680
[ 6268.150961] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=104 bytes=6815744 empty=0 errors=0 queued=0 start_pending=0
[ 6273.175237] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=219 bytes=14352384 empty=0 errors=0 queued=0 start_pending=0
[ 6278.200001] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=334 bytes=21889024 empty=0 errors=0 queued=0 start_pending=0
[ 6283.224101] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=449 bytes=29425664 empty=0 errors=0 queued=0 start_pending=0
[ 6288.247631] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=564 bytes=36962304 empty=0 errors=0 queued=0 start_pending=0
[ 6293.271520] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=679 bytes=44498944 empty=0 errors=0 queued=0 start_pending=0
[ 6298.296499] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=794 bytes=52035584 empty=0 errors=0 queued=0 start_pending=0
[ 6303.319279] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=909 bytes=59572224 empty=0 errors=0 queued=0 start_pending=0
[ 6308.343421] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=1024 bytes=67108864 empty=0 errors=0 queued=0 start_pending=0
[ 6313.367084] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=1139 bytes=74645504 empty=0 errors=0 queued=0 start_pending=0
[ 6318.391872] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=1254 bytes=82182144 empty=0 errors=0 queued=0 start_pending=0
[ 6323.414780] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=1369 bytes=89718784 empty=0 errors=0 queued=0 start_pending=0
[ 6328.438779] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=1484 bytes=97255424 empty=0 errors=0 queued=0 start_pending=0
[ 6333.462837] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=1599 bytes=104792064 empty=0 errors=0 queued=0 start_pending=0
[ 6338.486896] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=1714 bytes=112328704 empty=0 errors=0 queued=0 start_pending=0
[ 6343.510862] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=1829 bytes=119865344 empty=0 errors=0 queued=0 start_pending=0
[ 6348.536136] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=1944 bytes=127401984 empty=0 errors=0 queued=0 start_pending=0
[ 6353.559189] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2059 bytes=134938624 empty=0 errors=0 queued=0 start_pending=0
[ 6358.582881] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2174 bytes=142475264 empty=0 errors=0 queued=0 start_pending=0
[ 6363.605252] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2289 bytes=150011904 empty=0 errors=0 queued=0 start_pending=0
[ 6368.628785] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2404 bytes=157548544 empty=0 errors=0 queued=0 start_pending=0
[ 6373.653042] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2519 bytes=165085184 empty=0 errors=0 queued=0 start_pending=0
[ 6378.676286] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2634 bytes=172621824 empty=0 errors=0 queued=0 start_pending=0
[ 6383.698542] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2749 bytes=180158464 empty=0 errors=0 queued=0 start_pending=0
[ 6388.722742] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2864 bytes=187695104 empty=0 errors=0 queued=0 start_pending=0
[ 6393.746698] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2979 bytes=195231744 empty=0 errors=0 queued=0 start_pending=0
[ 6398.771613] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3094 bytes=202768384 empty=0 errors=0 queued=0 start_pending=0
[ 6403.802139] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3209 bytes=210305024 empty=0 errors=0 queued=0 start_pending=0
[ 6408.820040] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3324 bytes=217841664 empty=0 errors=0 queued=0 start_pending=0
[ 6413.845645] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3439 bytes=225378304 empty=0 errors=0 queued=0 start_pending=0
[ 6418.868306] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3554 bytes=232914944 empty=0 errors=0 queued=0 start_pending=0
[ 6423.892900] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3669 bytes=240451584 empty=0 errors=0 queued=0 start_pending=0
[ 6428.916860] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3784 bytes=247988224 empty=0 errors=0 queued=0 start_pending=0
[ 6430.321706] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[ 6430.322994] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[ 6441.629531] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[ 6441.631296] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[ 6498.665709] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 6498.686792] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[ 6498.688208] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
ff5@ff5-VirtualBox:~$ 

240p output is not showing anything:

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
^C  nan    :  0.000 fd=   0 aq=    0KB vq=    0KB sq=    0B f=0/0   

ff5@ff5-VirtualBox:~$ dmesg
[ 7130.079063] usbcore: registered new interface driver elgato_gchd
[ 7142.610863] usb 2-1: new high-speed USB device number 11 using ehci-pci
[ 7143.026959] usb 2-1: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[ 7143.026980] usb 2-1: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[ 7143.026986] usb 2-1: Product: GameCapture HD
[ 7143.026991] usb 2-1: Manufacturer: Elgato
[ 7143.027027] usb 2-1: SerialNumber: 04107F1267
[ 7143.031590] elgato_gchd 2-1:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[ 7143.034313] elgato_gchd 2-1:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[ 7143.047732] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=5662 comm=v4l_id
[ 7143.048858] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1395 comm=wireplumber
[ 7143.049117] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1395 comm=wireplumber
[ 7143.049550] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 7143.049561] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 7143.049845] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 7143.049870] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 7143.049876] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 7143.049881] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 7143.050383] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 7143.050414] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 7157.361782] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=5664 comm=v4l2-ctl
[ 7157.361827] elgato_gchd 2-1:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[ 7157.361829] elgato_gchd 2-1:1.0: input selected: input=2; detection deferred until device open
[ 7157.361831] elgato_gchd 2-1:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 7392.330584] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=5683 comm=read_thread
[ 7392.358084] elgato_gchd 2-1:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 7392.414604] elgato_gchd 2-1:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7392.417384] elgato_gchd 2-1:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7392.420910] elgato_gchd 2-1:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7392.424323] elgato_gchd 2-1:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7392.427843] elgato_gchd 2-1:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7392.431631] elgato_gchd 2-1:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7392.435466] elgato_gchd 2-1:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7392.438337] elgato_gchd 2-1:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7392.442024] elgato_gchd 2-1:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7392.445835] elgato_gchd 2-1:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7392.449862] elgato_gchd 2-1:1.0: firmware chunk offset=327680 size=4016 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7392.526828] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[ 7392.528614] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
[ 7393.219153] elgato_gchd 2-1:1.0: transcoder defaults loaded
[ 7393.221590] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[ 7393.222952] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x04
[ 7393.228461] elgato_gchd 2-1:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.231729] elgato_gchd 2-1:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.235756] elgato_gchd 2-1:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.239731] elgato_gchd 2-1:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.243760] elgato_gchd 2-1:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.247500] elgato_gchd 2-1:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.251294] elgato_gchd 2-1:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.254783] elgato_gchd 2-1:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.258567] elgato_gchd 2-1:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.262585] elgato_gchd 2-1:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.266503] elgato_gchd 2-1:1.0: firmware chunk offset=327680 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.270455] elgato_gchd 2-1:1.0: firmware chunk offset=360448 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.274114] elgato_gchd 2-1:1.0: firmware chunk offset=393216 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.277734] elgato_gchd 2-1:1.0: firmware chunk offset=425984 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.281564] elgato_gchd 2-1:1.0: firmware chunk offset=458752 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.285284] elgato_gchd 2-1:1.0: firmware chunk offset=491520 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.288927] elgato_gchd 2-1:1.0: firmware chunk offset=524288 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.292932] elgato_gchd 2-1:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.296834] elgato_gchd 2-1:1.0: firmware chunk offset=589824 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.300678] elgato_gchd 2-1:1.0: firmware chunk offset=622592 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.304904] elgato_gchd 2-1:1.0: firmware chunk offset=655360 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.308950] elgato_gchd 2-1:1.0: firmware chunk offset=688128 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.313071] elgato_gchd 2-1:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.316848] elgato_gchd 2-1:1.0: firmware chunk offset=753664 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.320536] elgato_gchd 2-1:1.0: firmware chunk offset=786432 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.323995] elgato_gchd 2-1:1.0: firmware chunk offset=819200 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.328020] elgato_gchd 2-1:1.0: firmware chunk offset=851968 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.331744] elgato_gchd 2-1:1.0: firmware chunk offset=884736 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.335448] elgato_gchd 2-1:1.0: firmware chunk offset=917504 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.339374] elgato_gchd 2-1:1.0: firmware chunk offset=950272 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.343072] elgato_gchd 2-1:1.0: firmware chunk offset=983040 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.346694] elgato_gchd 2-1:1.0: firmware chunk offset=1015808 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.350470] elgato_gchd 2-1:1.0: firmware chunk offset=1048576 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.354424] elgato_gchd 2-1:1.0: firmware chunk offset=1081344 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.358229] elgato_gchd 2-1:1.0: firmware chunk offset=1114112 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.361909] elgato_gchd 2-1:1.0: firmware chunk offset=1146880 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.365561] elgato_gchd 2-1:1.0: firmware chunk offset=1179648 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.369250] elgato_gchd 2-1:1.0: firmware chunk offset=1212416 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.372979] elgato_gchd 2-1:1.0: firmware chunk offset=1245184 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.376969] elgato_gchd 2-1:1.0: firmware chunk offset=1277952 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.380574] elgato_gchd 2-1:1.0: firmware chunk offset=1310720 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.384316] elgato_gchd 2-1:1.0: firmware chunk offset=1343488 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.387694] elgato_gchd 2-1:1.0: firmware chunk offset=1376256 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.391525] elgato_gchd 2-1:1.0: firmware chunk offset=1409024 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.395170] elgato_gchd 2-1:1.0: firmware chunk offset=1441792 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.398658] elgato_gchd 2-1:1.0: firmware chunk offset=1474560 size=26224 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7393.441055] elgato_gchd 2-1:1.0: device idle, processor state 0x27f97b
[ 7394.281817] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6\n
[ 7394.402781] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[ 7394.432151] elgato_gchd 2-1:1.0: analog format initial status=0x66; starting in 480i
[ 7394.432159] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 7394.432163] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 7394.432165] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 7400.617895] elgato_gchd 2-1:1.0: capture setup: encoder start completed
[ 7400.620625] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 7402.334259] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[ 7402.388444] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[ 7402.648392] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[ 7402.650592] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 7402.653247] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 7402.654343] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 7402.654666] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 7402.663218] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 7402.665638] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[ 7402.667142] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[ 7402.675030] elgato_gchd 2-1:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0000 ack=1
[ 7402.692275] elgato_gchd 2-1:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7402.699626] elgato_gchd 2-1:1.0: START poll=1 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7402.707551] elgato_gchd 2-1:1.0: START poll=2 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7402.715060] elgato_gchd 2-1:1.0: START poll=3 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7402.722656] elgato_gchd 2-1:1.0: START poll=4 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7402.730507] elgato_gchd 2-1:1.0: START poll=5 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7402.738270] elgato_gchd 2-1:1.0: START poll=6 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7402.746170] elgato_gchd 2-1:1.0: START poll=7 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7402.753500] elgato_gchd 2-1:1.0: START poll=8 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7402.760813] elgato_gchd 2-1:1.0: START poll=9 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7402.768344] elgato_gchd 2-1:1.0: START poll=10 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7402.776109] elgato_gchd 2-1:1.0: START poll=11 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7402.783209] elgato_gchd 2-1:1.0: START poll=12 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7402.790443] elgato_gchd 2-1:1.0: START poll=13 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7402.797836] elgato_gchd 2-1:1.0: START poll=14 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7402.805654] elgato_gchd 2-1:1.0: START poll=15 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7403.454409] elgato_gchd 2-1:1.0: START poll=100 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7404.186298] elgato_gchd 2-1:1.0: START poll=200 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7404.915092] elgato_gchd 2-1:1.0: START poll=300 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7405.680437] elgato_gchd 2-1:1.0: START poll=400 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7406.431264] elgato_gchd 2-1:1.0: START poll=500 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7407.196875] elgato_gchd 2-1:1.0: START poll=600 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7407.749503] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=0 bytes=0 empty=0 errors=0 queued=0 start_pending=1
[ 7407.964034] elgato_gchd 2-1:1.0: START poll=700 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7408.708401] elgato_gchd 2-1:1.0: START poll=800 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7409.452366] elgato_gchd 2-1:1.0: START poll=900 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 7410.202353] elgato_gchd 2-1:1.0: START poll=1000 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
ff5@ff5-VirtualBox:~$ 

The source that switches from 240p to 480i isn't showing any output as well:

ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
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
    nan    :  0.000 fd=   0 aq=    0KB vq=    0KB sq=    0B f=0/0   

ff5@ff5-VirtualBox:~$ dmesg
[ 7510.805810] usbcore: registered new interface driver elgato_gchd
[ 7516.471951] usb 2-1: new high-speed USB device number 13 using ehci-pci
[ 7516.889226] usb 2-1: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[ 7516.889253] usb 2-1: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[ 7516.889260] usb 2-1: Product: GameCapture HD
[ 7516.889266] usb 2-1: Manufacturer: Elgato
[ 7516.889271] usb 2-1: SerialNumber: 04107F1267
[ 7516.891735] elgato_gchd 2-1:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[ 7516.894676] elgato_gchd 2-1:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[ 7516.912018] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=5713 comm=v4l_id
[ 7516.913874] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1395 comm=wireplumber
[ 7516.914630] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1395 comm=wireplumber
[ 7516.915218] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 7516.915238] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 7516.915722] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 7516.915766] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 7516.915779] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 7516.915788] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 7516.916032] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 7516.916068] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 7523.374593] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=5716 comm=v4l2-ctl
[ 7523.374784] elgato_gchd 2-1:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[ 7523.374796] elgato_gchd 2-1:1.0: input selected: input=2; detection deferred until device open
[ 7523.374802] elgato_gchd 2-1:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 7534.611363] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=5730 comm=read_thread
[ 7534.636552] elgato_gchd 2-1:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 7534.671290] elgato_gchd 2-1:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7534.674239] elgato_gchd 2-1:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7534.677774] elgato_gchd 2-1:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7534.681223] elgato_gchd 2-1:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7534.684724] elgato_gchd 2-1:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7534.688347] elgato_gchd 2-1:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7534.691817] elgato_gchd 2-1:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7534.695405] elgato_gchd 2-1:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7534.698529] elgato_gchd 2-1:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7534.702053] elgato_gchd 2-1:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7534.705717] elgato_gchd 2-1:1.0: firmware chunk offset=327680 size=4016 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7534.777006] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[ 7534.778454] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
[ 7535.796707] elgato_gchd 2-1:1.0: transcoder defaults loaded
[ 7535.799649] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[ 7535.801005] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x04
[ 7535.808008] elgato_gchd 2-1:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.810212] elgato_gchd 2-1:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.813974] elgato_gchd 2-1:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.818037] elgato_gchd 2-1:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.821936] elgato_gchd 2-1:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.825629] elgato_gchd 2-1:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.829868] elgato_gchd 2-1:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.833964] elgato_gchd 2-1:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.837946] elgato_gchd 2-1:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.842259] elgato_gchd 2-1:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.845896] elgato_gchd 2-1:1.0: firmware chunk offset=327680 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.849947] elgato_gchd 2-1:1.0: firmware chunk offset=360448 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.854067] elgato_gchd 2-1:1.0: firmware chunk offset=393216 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.857725] elgato_gchd 2-1:1.0: firmware chunk offset=425984 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.861839] elgato_gchd 2-1:1.0: firmware chunk offset=458752 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.865506] elgato_gchd 2-1:1.0: firmware chunk offset=491520 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.869332] elgato_gchd 2-1:1.0: firmware chunk offset=524288 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.873188] elgato_gchd 2-1:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.876979] elgato_gchd 2-1:1.0: firmware chunk offset=589824 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.880764] elgato_gchd 2-1:1.0: firmware chunk offset=622592 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.885116] elgato_gchd 2-1:1.0: firmware chunk offset=655360 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.888171] elgato_gchd 2-1:1.0: firmware chunk offset=688128 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.892163] elgato_gchd 2-1:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.895853] elgato_gchd 2-1:1.0: firmware chunk offset=753664 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.899796] elgato_gchd 2-1:1.0: firmware chunk offset=786432 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.903854] elgato_gchd 2-1:1.0: firmware chunk offset=819200 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.907361] elgato_gchd 2-1:1.0: firmware chunk offset=851968 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.911038] elgato_gchd 2-1:1.0: firmware chunk offset=884736 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.914662] elgato_gchd 2-1:1.0: firmware chunk offset=917504 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.918507] elgato_gchd 2-1:1.0: firmware chunk offset=950272 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.921962] elgato_gchd 2-1:1.0: firmware chunk offset=983040 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.925858] elgato_gchd 2-1:1.0: firmware chunk offset=1015808 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.929658] elgato_gchd 2-1:1.0: firmware chunk offset=1048576 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.933524] elgato_gchd 2-1:1.0: firmware chunk offset=1081344 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.937288] elgato_gchd 2-1:1.0: firmware chunk offset=1114112 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.940609] elgato_gchd 2-1:1.0: firmware chunk offset=1146880 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.944174] elgato_gchd 2-1:1.0: firmware chunk offset=1179648 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.947978] elgato_gchd 2-1:1.0: firmware chunk offset=1212416 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.951262] elgato_gchd 2-1:1.0: firmware chunk offset=1245184 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.954548] elgato_gchd 2-1:1.0: firmware chunk offset=1277952 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.957870] elgato_gchd 2-1:1.0: firmware chunk offset=1310720 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.961244] elgato_gchd 2-1:1.0: firmware chunk offset=1343488 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.964502] elgato_gchd 2-1:1.0: firmware chunk offset=1376256 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.967988] elgato_gchd 2-1:1.0: firmware chunk offset=1409024 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.971268] elgato_gchd 2-1:1.0: firmware chunk offset=1441792 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7535.974550] elgato_gchd 2-1:1.0: firmware chunk offset=1474560 size=26224 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 7536.014290] elgato_gchd 2-1:1.0: device idle, processor state 0x27f97b
[ 7536.875353] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7536.875366] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7536.958190] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7536.958206] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7537.040084] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7537.040091] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7537.120735] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7537.120749] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7537.200613] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7537.200628] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7537.282619] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7537.282639] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7537.365051] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7537.365072] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7537.452525] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7537.452542] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7537.533959] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7537.533964] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7537.618647] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7537.618662] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7537.697352] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7537.779209] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7537.867735] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7537.955525] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7538.036613] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7538.121843] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7538.201512] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7538.282315] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7538.370887] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7538.458877] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7538.539629] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7538.624449] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7538.704767] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7538.797718] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7538.917524] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7538.999091] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7539.083557] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7539.165536] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7539.247049] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7539.329223] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7539.414007] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7539.496713] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7539.581404] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7539.663349] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7539.747790] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7539.831817] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7539.912696] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7539.997458] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7540.171057] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7540.256613] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7540.343167] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7540.424538] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7540.503990] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7540.586745] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7540.668562] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7540.749651] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7540.836479] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7540.918749] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7541.000171] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7541.084990] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7541.165582] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7541.246479] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7541.331287] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7541.412125] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7541.495679] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7541.580361] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7541.670028] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7541.753373] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7541.836244] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7541.917913] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7541.917927] gchd_detection_worker: 49 callbacks suppressed
[ 7541.917931] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7541.999592] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xf5 mode=0x5\n
[ 7541.999599] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7542.085121] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xf5 mode=0x5\n
[ 7542.085140] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7542.165845] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7542.165857] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7542.246411] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7542.246424] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7542.325579] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7542.325597] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7542.410345] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7542.410358] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7542.493414] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7542.493421] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7542.573814] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7542.573830] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7542.653136] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7542.653142] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7542.738794] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7542.826980] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7542.907731] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7542.992342] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7543.075474] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7543.160799] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7543.241504] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7543.340621] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7543.421990] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7543.503282] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7543.584618] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7543.672028] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7543.755411] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7543.837349] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7543.917378] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7543.999218] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7544.082198] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7544.161821] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7544.244204] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7544.326739] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7544.408577] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7544.489650] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7544.572455] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7544.655394] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7544.737254] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7544.817027] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7544.898597] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7544.981824] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7545.072472] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7545.154515] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7545.232954] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7545.314740] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7545.401667] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7545.483813] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7545.565733] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7545.647847] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7545.727972] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7545.810857] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7545.890856] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7545.973168] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7546.054767] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7546.142982] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7546.223862] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7546.307735] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7546.394326] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7546.478166] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7546.559718] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7546.641323] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7546.724512] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7546.811783] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7546.890290] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7546.975763] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7546.975784] gchd_detection_worker: 51 callbacks suppressed
[ 7546.975790] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7547.058772] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7547.058787] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7547.138956] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7547.138969] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7547.222624] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7547.222637] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7547.304523] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7547.304532] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7547.387016] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7547.387039] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7547.469664] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7547.469674] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7547.552811] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7547.552834] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7547.633631] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7547.633648] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7547.712669] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7547.712717] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7547.793720] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7547.878517] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7547.958479] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7548.039623] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7548.122358] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7548.205778] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7548.290723] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7548.372674] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7548.455961] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7548.548833] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7548.633988] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7548.714912] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7548.797787] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7548.879854] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7548.963458] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7549.045169] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7549.127552] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7549.211035] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7549.294102] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7549.378427] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7549.462220] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7549.543913] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7549.626174] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7549.707813] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7549.788063] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7549.869166] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7549.957500] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7550.044362] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7550.126271] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7550.207600] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7550.286919] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7550.366983] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7550.450532] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7550.532559] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7550.647148] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7550.730932] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7550.814232] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7550.906738] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7550.986443] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7551.068329] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7551.155369] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7551.235438] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7551.317379] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7551.399621] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7551.481444] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7551.561447] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7551.645187] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7551.729663] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7551.807772] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7551.889042] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7551.973567] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7552.061003] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7552.061014] gchd_detection_worker: 51 callbacks suppressed
[ 7552.061017] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7552.147852] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7552.147875] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7552.234569] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7552.234579] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7552.318674] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7552.318705] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7552.404085] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7552.404099] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7552.488366] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7552.488376] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7552.570675] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7552.570710] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7552.654412] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7552.654435] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7552.739473] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7552.739483] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7552.830768] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7552.830789] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7552.913209] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7552.998207] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7553.082750] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7553.164316] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7553.247231] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7553.328963] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7553.410489] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7553.490442] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7553.574252] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7553.657926] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7553.739354] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7553.821387] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7553.908265] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7553.989451] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7554.071347] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7554.150383] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7554.235514] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7554.315784] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7554.398502] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7554.498361] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7554.591857] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7554.674257] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7554.761458] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7554.849501] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7554.931723] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7555.012155] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7555.095049] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7555.182220] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7555.264194] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7555.353224] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7555.436820] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7555.518206] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7555.597341] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7555.679137] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7555.764843] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7555.851727] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7555.937777] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7556.021043] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7556.103914] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7556.192375] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7556.274114] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7556.356555] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7556.438338] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7556.517325] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7556.600054] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7556.684698] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7556.769306] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7556.851391] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7556.930011] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7557.011244] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7557.093485] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7557.093500] gchd_detection_worker: 50 callbacks suppressed
[ 7557.093503] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7557.175935] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7557.175964] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7557.255744] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7557.255755] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7557.337480] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7557.337493] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7557.420874] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7557.420897] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7557.502667] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7557.502678] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7557.587189] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7557.587197] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7557.674428] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7557.674440] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7557.757227] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7557.757242] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7557.838557] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7557.838564] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7557.927224] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7558.010736] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7558.090119] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7558.175404] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7558.258769] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7558.342729] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7558.430457] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7558.518768] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7558.601704] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7558.684605] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7558.766130] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7558.847606] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7558.929269] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7559.013635] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7559.101618] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7559.182814] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7559.263544] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7559.344019] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7559.429468] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7559.511247] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7559.598858] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7559.681864] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7559.762399] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7559.845904] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7559.927978] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7560.039693] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7560.120645] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7560.208482] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7560.291030] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7560.373163] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7560.461984] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7560.547577] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7560.632520] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7560.719749] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7560.804820] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7560.887812] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7560.969845] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7561.050546] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7561.133452] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7561.215160] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7561.297330] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7561.379239] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7561.461236] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7561.551517] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7561.630630] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7561.712730] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7561.793777] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7561.878646] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7561.962862] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7562.045823] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7562.131896] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7562.131908] gchd_detection_worker: 50 callbacks suppressed
[ 7562.131912] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7562.213799] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7562.213810] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7562.296876] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7562.296882] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7562.382089] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7562.382117] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7562.464403] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7562.464431] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7562.547209] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7562.547220] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7562.631027] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7562.631069] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7562.714748] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7562.714781] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7562.796254] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7562.796264] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7562.877287] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7562.877295] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7562.961343] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7563.045723] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7563.130015] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7563.213370] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7563.297150] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7563.381759] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7563.459280] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7563.541240] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7563.631788] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7563.715914] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7563.798606] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7563.882249] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7563.964641] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7564.045478] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7564.126297] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7564.211643] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7564.293091] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7564.376035] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7564.461442] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7564.541529] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7564.636765] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7564.718331] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7564.803783] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7564.890043] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7564.971463] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7565.053938] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7565.146196] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7565.228573] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7565.308348] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7565.395639] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7565.477867] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7565.566194] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7565.649845] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7565.733609] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7565.818892] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7565.905244] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7565.986739] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7566.068074] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7566.147224] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7566.225265] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7566.314733] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7566.395716] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7566.477919] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7566.563512] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7566.651501] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7566.737711] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7566.826622] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7566.907633] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7566.994364] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7567.078222] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7567.161526] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7567.161537] gchd_detection_worker: 50 callbacks suppressed
[ 7567.161540] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7567.243250] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7567.243261] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7567.323620] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7567.323628] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7567.411689] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7567.411701] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7567.493023] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7567.493033] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7567.578611] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7567.578627] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7567.661473] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7567.661483] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7567.744495] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7567.744504] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7567.826138] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7567.826155] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7567.908990] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7567.909000] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7567.988370] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7568.068277] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7568.149259] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7568.229311] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7568.316564] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7568.409243] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7568.494098] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7568.574623] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7568.657061] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7568.737727] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7568.820165] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7568.903301] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7568.988266] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7569.073736] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7569.153118] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7569.235423] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7569.317583] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7569.399243] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7569.480370] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7569.561926] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7569.644096] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7569.724779] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7569.806904] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7569.889511] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7569.971543] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7570.055532] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7570.134326] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7570.219762] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7570.303938] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7570.386495] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7570.469463] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7570.549337] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7570.634851] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7570.715096] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7570.797615] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7570.882767] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7570.965578] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7571.058001] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7571.138047] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7571.225673] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7571.310340] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7571.391465] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7571.475291] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7571.566956] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7571.649561] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7571.730764] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7571.813501] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7571.963313] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7572.048772] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7572.132324] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7572.223309] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7572.223333] gchd_detection_worker: 50 callbacks suppressed
[ 7572.223339] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7572.304889] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7572.304906] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7572.383004] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7572.383014] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7572.464333] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7572.464351] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7572.543685] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7572.543706] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7572.629506] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7572.629518] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7572.711896] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7572.711914] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7572.793202] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7572.793214] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7572.877059] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7572.877075] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7572.959263] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7572.959275] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7573.039524] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7573.121553] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7573.203031] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7573.287125] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7573.373309] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7573.458675] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7573.539152] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7573.628184] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7573.786802] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7573.873536] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7573.957073] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7574.038183] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7574.118270] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7574.264053] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7574.346275] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7574.424906] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7574.506005] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7574.589763] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7574.672185] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7574.758253] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7574.841894] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7574.924583] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7575.009244] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7575.093940] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7575.176213] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7575.256657] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7575.338166] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7575.422441] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7575.502892] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7575.588254] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7575.673152] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7575.755128] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7575.842023] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7575.923471] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7576.005576] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7576.089378] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7576.180194] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7576.262327] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7576.344256] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7576.431157] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7576.516601] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7576.601228] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7576.689544] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7576.772160] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7576.855713] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7576.936781] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7577.017211] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7577.107090] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7577.200349] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7577.282827] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7577.282839] gchd_detection_worker: 49 callbacks suppressed
[ 7577.282841] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7577.367144] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7577.367166] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7577.445455] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7577.445472] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7577.525988] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7577.525995] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7577.618482] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7577.618495] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7577.698970] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7577.698988] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7577.785906] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7577.785925] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7577.868759] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7577.868768] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7577.948361] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7577.948366] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7578.027713] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7578.027719] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7578.110651] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7578.193649] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7578.291740] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7578.374928] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7578.456202] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7578.540279] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7578.622697] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7578.706929] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7578.792820] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7578.873531] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7578.954751] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7579.035976] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7579.119625] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7579.206867] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7579.290364] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7579.372795] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7579.452624] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7579.533033] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7579.617882] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7579.708392] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7579.797360] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7579.880391] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7579.962833] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7580.041466] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7580.125379] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7580.206995] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7580.292350] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7580.377892] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7580.461378] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7580.542507] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7580.629035] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7580.707843] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7580.794956] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7580.875578] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7580.960732] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7581.041608] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7581.123476] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7581.205970] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7581.287672] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7581.369328] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7581.450280] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7581.530637] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7581.608996] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7581.688538] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7581.770346] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7581.852894] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7581.934981] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7582.016458] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7582.099988] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7582.179585] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7582.261744] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7582.344466] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7582.344483] gchd_detection_worker: 51 callbacks suppressed
[ 7582.344488] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7582.427377] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7582.427390] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7582.508598] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7582.508610] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7582.595053] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7582.595067] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7582.679758] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7582.679765] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7582.764887] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7582.764904] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7582.854213] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7582.854226] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7582.941277] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7582.941291] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7583.030617] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7583.030624] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7583.115075] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7583.115087] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7583.202011] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7583.282934] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7583.368116] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7583.451093] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7583.537147] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7583.621980] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7583.709067] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7583.796667] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7583.879472] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7583.965879] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7584.051061] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7584.131905] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7584.217550] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7584.301240] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7584.382595] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7584.465585] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7584.548353] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7584.627510] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7584.710826] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7584.794133] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7584.881232] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7584.967929] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7585.048287] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7585.133591] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7585.215410] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7585.299368] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7585.381924] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7585.463551] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7585.544312] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7585.631213] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7585.715874] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7585.805713] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7585.887720] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7585.968274] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7586.049489] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7586.134582] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7586.221974] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7586.306255] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7586.389821] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7586.471039] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7586.552028] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7586.638605] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7586.725836] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7586.806724] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7586.888481] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7586.973552] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7587.058380] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7587.138697] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7587.221597] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7587.307056] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7587.391088] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7587.391103] gchd_detection_worker: 50 callbacks suppressed
[ 7587.391107] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7587.470757] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7587.470772] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7587.551890] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7587.551899] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7587.634682] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7587.634698] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7587.719062] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7587.719086] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7587.810626] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7587.810643] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7587.894521] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7587.894587] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7587.978408] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7587.978427] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7588.065826] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7588.065852] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7588.151560] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7588.151570] elgato_gchd 2-1:1.0: no signal on input=2; retrying detection
[ 7588.232653] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7588.316614] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7588.399395] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7588.478418] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7588.558965] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7588.642417] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7588.725778] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7588.806396] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7588.885158] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7588.963047] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7589.042121] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7589.122059] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7589.207264] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7589.300767] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7589.388072] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7589.469386] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7589.557486] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7589.643244] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7589.725786] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7589.805091] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7589.888396] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7589.971646] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7590.050638] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7590.136265] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7590.216747] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7590.301213] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7590.383383] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7590.469835] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7590.552140] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7590.635501] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7590.716305] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7590.796942] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7590.878823] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7590.960770] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7591.039972] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7591.121475] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7591.198224] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7591.276127] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7591.356793] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7591.435951] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7591.518530] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7591.796605] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd\n
[ 7591.880475] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
[ 7591.964393] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd\n
ff5@ff5-VirtualBox:~$ 
