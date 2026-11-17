# TASK T-043b — Composite 240p start; stutter diagnosis

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** fixes ready; waiting for the hardware test
- **Type:** driver fix + hardware test
- **Branch:** `fix/composite-240p-start` (stacked on `fix/buffer-timestamps`)
- **TODO items:** Phase 5 T-043, Phase 3 T-020

## Why

Last test (TODO "Hardware test log"): with a 240p source the decoder
reported 240p (`0x88:CA = 66`) but the driver configured 480i, and START
never completed. The driver now configures 240p as recorded in the traces.
The 480i stutter is still unexplained; the 5 s statistics line now shows
whether frames are discarded by the driver (`ring_dropped`).

## Acceptance criteria

1. Composite 240p, source in 240p before capture starts: dmesg shows
   "configuring 240p", START completes, ffplay shows a picture, the
   recording decodes without errors.
2. Composite 480i still works as before.
3. The statistics lines during 480i playback are reported (`ring_dropped`,
   `frames_waiting`).

## Hardware test (maintainer)

240p (source in 240p before starting):

```sh
git fetch origin && git checkout fix/composite-240p-start && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
sudo dmesg > dmesg-240p.txt
ffmpeg -f v4l2 -input_format h264 -i /dev/video0 -t 10 -c copy test240p.h264
ffmpeg -v error -i test240p.h264 -f null - ; echo "decode errors above (none = clean)"
```

480i (reload the module first), watch for stutter for ~60 s:

```sh
sudo rmmod elgato_gchd; sudo dmesg -C; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
sudo dmesg | grep -E "configuring|START transition|bulk-IN stats|no analog family"
```

Paste the outputs and what ffplay showed into the maintainer comments below.

## Findings

_(filled in during the task)_

---

## Maintainer comments

_Written by the maintainer only. Anything lasting is transferred to
`RULES.md` or `TODO.md` when the next task is prepared, and this section is
then emptied._

Good news, 240p is now working. It took a while to start but it's now working. There's still stutters. I guess when I move a working version out of the VM we'll se how it performs there.

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
  Duration: N/A, start: 9091.590775, bitrate: N/A
  Stream #0:0: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 60 fps, 60 tbr, 1000k tbn
[video4linux2,v4l2 @ 0x78740c000c80] Some buffers are still owned by the caller on close.


ff5@ff5-VirtualBox:~$ dmesg
[ 9043.658001] elgato_gchd 2-1:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[ 9043.666846] elgato_gchd 2-1:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[ 9043.666865] usbcore: registered new interface driver elgato_gchd
[ 9043.668672] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=6954 comm=v4l_id
[ 9043.670634] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1395 comm=wireplumber
[ 9043.670926] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1395 comm=wireplumber
[ 9043.671846] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 9043.671869] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 9043.673486] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 9043.673538] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 9043.673552] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 9043.673562] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 9043.673676] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 9043.673713] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 9049.547537] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=6955 comm=v4l2-ctl
[ 9049.547607] elgato_gchd 2-1:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[ 9049.547612] elgato_gchd 2-1:1.0: input selected: input=2; detection deferred until device open
[ 9049.547614] elgato_gchd 2-1:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 9076.493325] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=6970 comm=read_thread
[ 9076.519173] elgato_gchd 2-1:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 9076.550634] elgato_gchd 2-1:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9076.554089] elgato_gchd 2-1:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9076.557725] elgato_gchd 2-1:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9076.561394] elgato_gchd 2-1:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9076.565353] elgato_gchd 2-1:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9076.568570] elgato_gchd 2-1:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9076.572159] elgato_gchd 2-1:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9076.575543] elgato_gchd 2-1:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9076.579161] elgato_gchd 2-1:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9076.582975] elgato_gchd 2-1:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9076.586355] elgato_gchd 2-1:1.0: firmware chunk offset=327680 size=4016 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9076.657888] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[ 9076.659395] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
[ 9077.322083] elgato_gchd 2-1:1.0: transcoder defaults loaded
[ 9077.324531] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[ 9077.326044] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x04
[ 9077.330794] elgato_gchd 2-1:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.334751] elgato_gchd 2-1:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.338791] elgato_gchd 2-1:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.342592] elgato_gchd 2-1:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.346295] elgato_gchd 2-1:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.350010] elgato_gchd 2-1:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.353403] elgato_gchd 2-1:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.357174] elgato_gchd 2-1:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.360694] elgato_gchd 2-1:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.364046] elgato_gchd 2-1:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.367550] elgato_gchd 2-1:1.0: firmware chunk offset=327680 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.371169] elgato_gchd 2-1:1.0: firmware chunk offset=360448 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.374718] elgato_gchd 2-1:1.0: firmware chunk offset=393216 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.378211] elgato_gchd 2-1:1.0: firmware chunk offset=425984 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.381320] elgato_gchd 2-1:1.0: firmware chunk offset=458752 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.384946] elgato_gchd 2-1:1.0: firmware chunk offset=491520 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.388133] elgato_gchd 2-1:1.0: firmware chunk offset=524288 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.391817] elgato_gchd 2-1:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.395379] elgato_gchd 2-1:1.0: firmware chunk offset=589824 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.399060] elgato_gchd 2-1:1.0: firmware chunk offset=622592 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.402505] elgato_gchd 2-1:1.0: firmware chunk offset=655360 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.406362] elgato_gchd 2-1:1.0: firmware chunk offset=688128 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.410529] elgato_gchd 2-1:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.414244] elgato_gchd 2-1:1.0: firmware chunk offset=753664 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.418042] elgato_gchd 2-1:1.0: firmware chunk offset=786432 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.421648] elgato_gchd 2-1:1.0: firmware chunk offset=819200 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.425195] elgato_gchd 2-1:1.0: firmware chunk offset=851968 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.428667] elgato_gchd 2-1:1.0: firmware chunk offset=884736 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.432220] elgato_gchd 2-1:1.0: firmware chunk offset=917504 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.435885] elgato_gchd 2-1:1.0: firmware chunk offset=950272 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.439640] elgato_gchd 2-1:1.0: firmware chunk offset=983040 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.443202] elgato_gchd 2-1:1.0: firmware chunk offset=1015808 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.446708] elgato_gchd 2-1:1.0: firmware chunk offset=1048576 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.450500] elgato_gchd 2-1:1.0: firmware chunk offset=1081344 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.454385] elgato_gchd 2-1:1.0: firmware chunk offset=1114112 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.458279] elgato_gchd 2-1:1.0: firmware chunk offset=1146880 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.462030] elgato_gchd 2-1:1.0: firmware chunk offset=1179648 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.465611] elgato_gchd 2-1:1.0: firmware chunk offset=1212416 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.469620] elgato_gchd 2-1:1.0: firmware chunk offset=1245184 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.473333] elgato_gchd 2-1:1.0: firmware chunk offset=1277952 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.477125] elgato_gchd 2-1:1.0: firmware chunk offset=1310720 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.480888] elgato_gchd 2-1:1.0: firmware chunk offset=1343488 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.484651] elgato_gchd 2-1:1.0: firmware chunk offset=1376256 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.488413] elgato_gchd 2-1:1.0: firmware chunk offset=1409024 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.491957] elgato_gchd 2-1:1.0: firmware chunk offset=1441792 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.495422] elgato_gchd 2-1:1.0: firmware chunk offset=1474560 size=26224 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9077.538513] elgato_gchd 2-1:1.0: device idle, processor state 0x27f97b
[ 9078.331929] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[ 9078.450558] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 9078.478477] elgato_gchd 2-1:1.0: analog format initial status=0x66; configuring 240p
[ 9078.478483] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[ 9078.478486] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[ 9078.478488] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[ 9084.391043] elgato_gchd 2-1:1.0: capture setup: encoder start completed
[ 9084.392808] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 9086.682784] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[ 9086.734354] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[ 9086.998663] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[ 9087.001488] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 9087.004083] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 9087.005971] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 9087.006502] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 9087.014961] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 9087.017475] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[ 9087.018899] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[ 9087.029445] elgato_gchd 2-1:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0000 ack=1
[ 9087.045881] elgato_gchd 2-1:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9087.053141] elgato_gchd 2-1:1.0: START poll=1 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9087.059798] elgato_gchd 2-1:1.0: START poll=2 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9087.066906] elgato_gchd 2-1:1.0: START poll=3 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9087.073631] elgato_gchd 2-1:1.0: START poll=4 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9087.080489] elgato_gchd 2-1:1.0: START poll=5 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9087.087765] elgato_gchd 2-1:1.0: START poll=6 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9087.094762] elgato_gchd 2-1:1.0: START poll=7 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9087.101525] elgato_gchd 2-1:1.0: START poll=8 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9087.108799] elgato_gchd 2-1:1.0: START poll=9 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9087.116205] elgato_gchd 2-1:1.0: START poll=10 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9087.123721] elgato_gchd 2-1:1.0: START poll=11 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0010 ack=0
[ 9087.131207] elgato_gchd 2-1:1.0: START poll=12 state=0x0002 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0010 ack=1
[ 9087.141191] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[ 9087.496242] elgato_gchd 2-1:1.0: async bulk-IN data: read=1 ep=0x81 actual=65536 total_bytes=65536
[ 9087.542332] elgato_gchd 2-1:1.0: async bulk-IN data: read=2 ep=0x81 actual=65536 total_bytes=131072
[ 9087.586289] elgato_gchd 2-1:1.0: async bulk-IN data: read=3 ep=0x81 actual=65536 total_bytes=196608
[ 9087.632278] elgato_gchd 2-1:1.0: async bulk-IN data: read=4 ep=0x81 actual=65536 total_bytes=262144
[ 9087.676160] elgato_gchd 2-1:1.0: async bulk-IN data: read=5 ep=0x81 actual=65536 total_bytes=327680
[ 9092.045417] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=105 bytes=6881280 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 9097.068602] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=220 bytes=14417920 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 9102.092620] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=335 bytes=21954560 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 9107.114272] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=450 bytes=29491200 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 9112.138462] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=565 bytes=37027840 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 9117.162081] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=680 bytes=44564480 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 9122.186187] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=795 bytes=52101120 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 9127.210895] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=910 bytes=59637760 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 9132.235619] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=1025 bytes=67174400 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 9135.837381] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[ 9135.838726] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[ 9147.159846] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[ 9147.161338] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[ 9204.105372] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 9204.125568] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[ 9204.127187] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
ff5@ff5-VirtualBox:~$ 


The 240p to 480i is not working properly I saw it output the 240p signal but the 480i output is not showing.

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
  Duration: N/A, start: 9364.278916, bitrate: N/A
  Stream #0:0: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 60 fps, 60 tbr, 1000k tbn
9491.51 M-V:  0.114 fd= 235 aq=    0KB vq=    0KB sq=    0B f=0/276   


ff5@ff5-VirtualBox:~$ dmesg
[ 9312.881112] usbcore: registered new interface driver elgato_gchd
[ 9331.194224] usb 2-1: new high-speed USB device number 15 using ehci-pci
[ 9331.602067] usb 2-1: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[ 9331.602089] usb 2-1: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[ 9331.602095] usb 2-1: Product: GameCapture HD
[ 9331.602100] usb 2-1: Manufacturer: Elgato
[ 9331.602104] usb 2-1: SerialNumber: 04107F1267
[ 9331.604510] elgato_gchd 2-1:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[ 9331.610081] elgato_gchd 2-1:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[ 9331.632621] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=7016 comm=v4l_id
[ 9331.633831] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1395 comm=wireplumber
[ 9331.634227] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1395 comm=wireplumber
[ 9331.635267] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 9331.635278] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 9331.635586] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 9331.635611] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 9331.635623] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 9331.635628] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 9331.636044] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 9331.636063] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=1393 comm=pipewire
[ 9337.049459] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=7018 comm=v4l2-ctl
[ 9337.049584] elgato_gchd 2-1:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[ 9337.049591] elgato_gchd 2-1:1.0: input selected: input=2; detection deferred until device open
[ 9337.049596] elgato_gchd 2-1:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 9349.333138] elgato_gchd 2-1:1.0: GCHD lifecycle: V4L2 open requested by pid=7033 comm=read_thread
[ 9349.363157] elgato_gchd 2-1:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 9349.412243] elgato_gchd 2-1:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9349.415591] elgato_gchd 2-1:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9349.419197] elgato_gchd 2-1:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9349.423095] elgato_gchd 2-1:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9349.426692] elgato_gchd 2-1:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9349.430215] elgato_gchd 2-1:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9349.433648] elgato_gchd 2-1:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9349.437279] elgato_gchd 2-1:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9349.440606] elgato_gchd 2-1:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9349.444065] elgato_gchd 2-1:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9349.447607] elgato_gchd 2-1:1.0: firmware chunk offset=327680 size=4016 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9349.522041] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[ 9349.523439] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
[ 9350.247764] elgato_gchd 2-1:1.0: transcoder defaults loaded
[ 9350.250315] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[ 9350.252060] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x04
[ 9350.256019] elgato_gchd 2-1:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.258594] elgato_gchd 2-1:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.262098] elgato_gchd 2-1:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.263753] elgato_gchd 2-1:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.267173] elgato_gchd 2-1:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.271013] elgato_gchd 2-1:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.274578] elgato_gchd 2-1:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.278275] elgato_gchd 2-1:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.282173] elgato_gchd 2-1:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.286156] elgato_gchd 2-1:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.290000] elgato_gchd 2-1:1.0: firmware chunk offset=327680 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.294350] elgato_gchd 2-1:1.0: firmware chunk offset=360448 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.297994] elgato_gchd 2-1:1.0: firmware chunk offset=393216 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.301668] elgato_gchd 2-1:1.0: firmware chunk offset=425984 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.305152] elgato_gchd 2-1:1.0: firmware chunk offset=458752 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.308935] elgato_gchd 2-1:1.0: firmware chunk offset=491520 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.312339] elgato_gchd 2-1:1.0: firmware chunk offset=524288 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.316094] elgato_gchd 2-1:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.317710] elgato_gchd 2-1:1.0: firmware chunk offset=589824 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.321221] elgato_gchd 2-1:1.0: firmware chunk offset=622592 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.324859] elgato_gchd 2-1:1.0: firmware chunk offset=655360 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.328601] elgato_gchd 2-1:1.0: firmware chunk offset=688128 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.332464] elgato_gchd 2-1:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.336216] elgato_gchd 2-1:1.0: firmware chunk offset=753664 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.340309] elgato_gchd 2-1:1.0: firmware chunk offset=786432 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.343864] elgato_gchd 2-1:1.0: firmware chunk offset=819200 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.345194] elgato_gchd 2-1:1.0: firmware chunk offset=851968 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.348617] elgato_gchd 2-1:1.0: firmware chunk offset=884736 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.351997] elgato_gchd 2-1:1.0: firmware chunk offset=917504 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.355469] elgato_gchd 2-1:1.0: firmware chunk offset=950272 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.359137] elgato_gchd 2-1:1.0: firmware chunk offset=983040 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.362885] elgato_gchd 2-1:1.0: firmware chunk offset=1015808 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.366219] elgato_gchd 2-1:1.0: firmware chunk offset=1048576 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.370413] elgato_gchd 2-1:1.0: firmware chunk offset=1081344 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.373871] elgato_gchd 2-1:1.0: firmware chunk offset=1114112 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.377243] elgato_gchd 2-1:1.0: firmware chunk offset=1146880 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.380957] elgato_gchd 2-1:1.0: firmware chunk offset=1179648 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.382353] elgato_gchd 2-1:1.0: firmware chunk offset=1212416 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.383841] elgato_gchd 2-1:1.0: firmware chunk offset=1245184 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.387471] elgato_gchd 2-1:1.0: firmware chunk offset=1277952 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.391042] elgato_gchd 2-1:1.0: firmware chunk offset=1310720 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.394400] elgato_gchd 2-1:1.0: firmware chunk offset=1343488 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.395739] elgato_gchd 2-1:1.0: firmware chunk offset=1376256 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.397069] elgato_gchd 2-1:1.0: firmware chunk offset=1409024 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.398402] elgato_gchd 2-1:1.0: firmware chunk offset=1441792 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.402279] elgato_gchd 2-1:1.0: firmware chunk offset=1474560 size=26224 OUT ep 0x02 maxpacket=512 speed=3 alt=0
[ 9350.446203] elgato_gchd 2-1:1.0: device idle, processor state 0x27f97b
[ 9351.277241] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[ 9351.398188] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 9351.431591] elgato_gchd 2-1:1.0: analog format initial status=0x66; configuring 240p
[ 9351.431606] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[ 9351.431613] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[ 9351.431619] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[ 9357.344876] elgato_gchd 2-1:1.0: capture setup: encoder start completed
[ 9357.347079] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 9359.357626] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[ 9359.411225] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[ 9359.668771] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[ 9359.671318] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 9359.674044] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 9359.676301] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 9359.677374] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 9359.685915] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 9359.688220] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[ 9359.689602] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[ 9359.697281] elgato_gchd 2-1:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0000 ack=1
[ 9359.714697] elgato_gchd 2-1:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9359.722134] elgato_gchd 2-1:1.0: START poll=1 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9359.729247] elgato_gchd 2-1:1.0: START poll=2 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9359.736286] elgato_gchd 2-1:1.0: START poll=3 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9359.743683] elgato_gchd 2-1:1.0: START poll=4 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9359.750936] elgato_gchd 2-1:1.0: START poll=5 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9359.758052] elgato_gchd 2-1:1.0: START poll=6 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9359.765251] elgato_gchd 2-1:1.0: START poll=7 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9359.772584] elgato_gchd 2-1:1.0: START poll=8 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9359.779728] elgato_gchd 2-1:1.0: START poll=9 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9359.787256] elgato_gchd 2-1:1.0: START poll=10 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9359.794695] elgato_gchd 2-1:1.0: START poll=11 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9359.802189] elgato_gchd 2-1:1.0: START poll=12 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9359.809704] elgato_gchd 2-1:1.0: START poll=13 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0000 ack=0
[ 9359.817063] elgato_gchd 2-1:1.0: START poll=14 state=0x0002 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0010 ack=1
[ 9359.826958] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[ 9360.184417] elgato_gchd 2-1:1.0: async bulk-IN data: read=1 ep=0x81 actual=65536 total_bytes=65536
[ 9360.230386] elgato_gchd 2-1:1.0: async bulk-IN data: read=2 ep=0x81 actual=65536 total_bytes=131072
[ 9360.274652] elgato_gchd 2-1:1.0: async bulk-IN data: read=3 ep=0x81 actual=65536 total_bytes=196608
[ 9360.320073] elgato_gchd 2-1:1.0: async bulk-IN data: read=4 ep=0x81 actual=65536 total_bytes=262144
[ 9360.364558] elgato_gchd 2-1:1.0: async bulk-IN data: read=5 ep=0x81 actual=65536 total_bytes=327680
[ 9364.690432] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=104 bytes=6815744 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 9369.713599] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=219 bytes=14352384 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 9374.734742] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=334 bytes=21889024 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 9378.964805] elgato_gchd 2-1:1.0: analog mode change detected: 240p -> 480i
[ 9379.122317] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[ 9379.123766] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[ 9390.420681] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[ 9390.422025] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[ 9447.027654] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[ 9447.029130] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
[ 9447.048497] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 9448.511016] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[ 9448.562576] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[ 9448.827248] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[ 9448.830009] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 9448.832922] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0011
[ 9448.834642] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 9448.835190] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 9448.845074] elgato_gchd 2-1:1.0: START before SCMD: state=0x0011 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 9448.847569] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[ 9448.849064] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[ 9448.858878] elgato_gchd 2-1:1.0: START poll=0 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9448.866397] elgato_gchd 2-1:1.0: START poll=1 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9448.874416] elgato_gchd 2-1:1.0: START poll=2 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9448.881728] elgato_gchd 2-1:1.0: START poll=3 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9448.888886] elgato_gchd 2-1:1.0: START poll=4 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9448.896160] elgato_gchd 2-1:1.0: START poll=5 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9448.903125] elgato_gchd 2-1:1.0: START poll=6 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9448.909897] elgato_gchd 2-1:1.0: START poll=7 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9448.917257] elgato_gchd 2-1:1.0: START poll=8 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9448.924269] elgato_gchd 2-1:1.0: START poll=9 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9448.931325] elgato_gchd 2-1:1.0: START poll=10 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9448.938207] elgato_gchd 2-1:1.0: START poll=11 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9448.945431] elgato_gchd 2-1:1.0: START poll=12 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9448.952172] elgato_gchd 2-1:1.0: START poll=13 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9448.959742] elgato_gchd 2-1:1.0: START poll=14 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9448.967056] elgato_gchd 2-1:1.0: START poll=15 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9449.595382] elgato_gchd 2-1:1.0: START poll=100 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9450.311933] elgato_gchd 2-1:1.0: START poll=200 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9451.039907] elgato_gchd 2-1:1.0: START poll=300 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9451.772700] elgato_gchd 2-1:1.0: START poll=400 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9452.510681] elgato_gchd 2-1:1.0: START poll=500 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9453.258178] elgato_gchd 2-1:1.0: START poll=600 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9453.932923] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=0 bytes=0 empty=0 errors=0 queued=0 start_pending=1 ring_dropped=0 frames_waiting=0
[ 9453.988424] elgato_gchd 2-1:1.0: START poll=700 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9454.722978] elgato_gchd 2-1:1.0: START poll=800 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9455.445585] elgato_gchd 2-1:1.0: START poll=900 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9456.176337] elgato_gchd 2-1:1.0: START poll=1000 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9456.892159] elgato_gchd 2-1:1.0: START poll=1100 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9457.592605] elgato_gchd 2-1:1.0: START poll=1200 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9458.315806] elgato_gchd 2-1:1.0: START poll=1300 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9459.029128] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=0 bytes=0 empty=0 errors=0 queued=0 start_pending=1 ring_dropped=0 frames_waiting=0
[ 9459.041217] elgato_gchd 2-1:1.0: START poll=1400 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9459.766910] elgato_gchd 2-1:1.0: START poll=1500 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9460.490895] elgato_gchd 2-1:1.0: START poll=1600 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9461.230260] elgato_gchd 2-1:1.0: START poll=1700 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9461.957974] elgato_gchd 2-1:1.0: START poll=1800 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9462.671924] elgato_gchd 2-1:1.0: START poll=1900 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9463.387133] elgato_gchd 2-1:1.0: state-change timeout: current=0x11 expected=0x02 state=0x0011 completion=0x011a
[ 9463.396645] elgato_gchd 2-1:1.0: START failed: ret=-110 state=0x0011 (r=0) enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 9463.396660] elgato_gchd 2-1:1.0: capture start transition failed: -110
[ 9463.396670] elgato_gchd: input 2 signal lost; scheduling redetection
[ 9463.559184] elgato_gchd 2-1:1.0: 240p-to-480i reconfiguration failed: -110
[ 9463.589673] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[ 9463.707276] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[ 9463.735533] elgato_gchd 2-1:1.0: analog format initial status=0x7e; configuring 480i
[ 9463.735538] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 9463.735540] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 9463.735542] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 9465.202104] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[ 9465.251756] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[ 9465.512107] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[ 9465.514582] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 9465.517026] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0011
[ 9465.519351] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 9465.520045] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 9465.529650] elgato_gchd 2-1:1.0: START before SCMD: state=0x0011 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 9465.532321] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[ 9465.533967] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[ 9465.544264] elgato_gchd 2-1:1.0: START poll=0 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9465.551629] elgato_gchd 2-1:1.0: START poll=1 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9465.559091] elgato_gchd 2-1:1.0: START poll=2 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9465.566180] elgato_gchd 2-1:1.0: START poll=3 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9465.573872] elgato_gchd 2-1:1.0: START poll=4 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9465.580669] elgato_gchd 2-1:1.0: START poll=5 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9465.588000] elgato_gchd 2-1:1.0: START poll=6 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9465.595303] elgato_gchd 2-1:1.0: START poll=7 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9465.602963] elgato_gchd 2-1:1.0: START poll=8 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9465.610366] elgato_gchd 2-1:1.0: START poll=9 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9465.618032] elgato_gchd 2-1:1.0: START poll=10 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9465.625096] elgato_gchd 2-1:1.0: START poll=11 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9465.633075] elgato_gchd 2-1:1.0: START poll=12 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9465.640258] elgato_gchd 2-1:1.0: START poll=13 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9465.647588] elgato_gchd 2-1:1.0: START poll=14 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9465.655385] elgato_gchd 2-1:1.0: START poll=15 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9466.273332] elgato_gchd 2-1:1.0: START poll=100 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9466.995882] elgato_gchd 2-1:1.0: START poll=200 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9467.724356] elgato_gchd 2-1:1.0: START poll=300 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9468.483102] elgato_gchd 2-1:1.0: START poll=400 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9469.210272] elgato_gchd 2-1:1.0: START poll=500 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9469.937393] elgato_gchd 2-1:1.0: START poll=600 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9470.611892] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=0 bytes=0 empty=0 errors=0 queued=0 start_pending=1 ring_dropped=0 frames_waiting=0
[ 9470.685417] elgato_gchd 2-1:1.0: START poll=700 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9471.430602] elgato_gchd 2-1:1.0: START poll=800 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9472.154849] elgato_gchd 2-1:1.0: START poll=900 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9472.886937] elgato_gchd 2-1:1.0: START poll=1000 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9473.609101] elgato_gchd 2-1:1.0: START poll=1100 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9474.326744] elgato_gchd 2-1:1.0: START poll=1200 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9475.072524] elgato_gchd 2-1:1.0: START poll=1300 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9475.612257] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=0 bytes=0 empty=0 errors=0 queued=0 start_pending=1 ring_dropped=0 frames_waiting=0
[ 9475.795997] elgato_gchd 2-1:1.0: START poll=1400 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9476.520317] elgato_gchd 2-1:1.0: START poll=1500 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9477.243903] elgato_gchd 2-1:1.0: START poll=1600 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9477.976589] elgato_gchd 2-1:1.0: START poll=1700 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9478.696508] elgato_gchd 2-1:1.0: START poll=1800 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9479.433701] elgato_gchd 2-1:1.0: START poll=1900 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9480.158997] elgato_gchd 2-1:1.0: state-change timeout: current=0x11 expected=0x02 state=0x0011 completion=0x011a
[ 9480.168080] elgato_gchd 2-1:1.0: START failed: ret=-110 state=0x0011 (r=0) enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 9480.168086] elgato_gchd 2-1:1.0: capture start transition failed: -110
[ 9480.168182] elgato_gchd: input 2 signal lost; scheduling redetection
[ 9480.333285] elgato_gchd 2-1:1.0: capture start failed: -110; recovered to IDLE, retrying detection
[ 9480.363624] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[ 9480.487297] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 9480.517803] elgato_gchd 2-1:1.0: analog format initial status=0x66; configuring 240p
[ 9480.517814] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[ 9480.517819] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[ 9480.517822] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[ 9482.534094] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[ 9482.586132] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[ 9482.842928] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[ 9482.845351] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 9482.848020] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0011
[ 9482.849353] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 9482.849662] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 9482.858033] elgato_gchd 2-1:1.0: START before SCMD: state=0x0011 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 9482.860637] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[ 9482.862079] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[ 9482.870237] elgato_gchd 2-1:1.0: START poll=0 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9482.877969] elgato_gchd 2-1:1.0: START poll=1 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9482.885808] elgato_gchd 2-1:1.0: START poll=2 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9482.892996] elgato_gchd 2-1:1.0: START poll=3 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9482.900105] elgato_gchd 2-1:1.0: START poll=4 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9482.907804] elgato_gchd 2-1:1.0: START poll=5 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9482.915192] elgato_gchd 2-1:1.0: START poll=6 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9482.922436] elgato_gchd 2-1:1.0: START poll=7 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9482.929671] elgato_gchd 2-1:1.0: START poll=8 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9482.936822] elgato_gchd 2-1:1.0: START poll=9 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9482.944360] elgato_gchd 2-1:1.0: START poll=10 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9482.951788] elgato_gchd 2-1:1.0: START poll=11 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9482.959339] elgato_gchd 2-1:1.0: START poll=12 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9482.966916] elgato_gchd 2-1:1.0: START poll=13 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9482.974253] elgato_gchd 2-1:1.0: START poll=14 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9482.981602] elgato_gchd 2-1:1.0: START poll=15 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9483.591941] elgato_gchd 2-1:1.0: START poll=100 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9484.323103] elgato_gchd 2-1:1.0: START poll=200 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9485.043521] elgato_gchd 2-1:1.0: START poll=300 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9485.768094] elgato_gchd 2-1:1.0: START poll=400 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9486.493884] elgato_gchd 2-1:1.0: START poll=500 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9487.229249] elgato_gchd 2-1:1.0: START poll=600 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9487.949062] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=0 bytes=0 empty=0 errors=0 queued=0 start_pending=1 ring_dropped=0 frames_waiting=0
[ 9487.956858] elgato_gchd 2-1:1.0: START poll=700 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9488.676206] elgato_gchd 2-1:1.0: START poll=800 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9489.452665] elgato_gchd 2-1:1.0: START poll=900 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9490.208629] elgato_gchd 2-1:1.0: START poll=1000 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9490.967645] elgato_gchd 2-1:1.0: START poll=1100 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9491.748798] elgato_gchd 2-1:1.0: START poll=1200 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9492.481893] elgato_gchd 2-1:1.0: START poll=1300 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9493.043993] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=0 bytes=0 empty=0 errors=0 queued=0 start_pending=1 ring_dropped=0 frames_waiting=0
[ 9493.236242] elgato_gchd 2-1:1.0: START poll=1400 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9494.010439] elgato_gchd 2-1:1.0: START poll=1500 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9494.796896] elgato_gchd 2-1:1.0: START poll=1600 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9495.513294] elgato_gchd 2-1:1.0: START poll=1700 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9496.242112] elgato_gchd 2-1:1.0: START poll=1800 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9496.958256] elgato_gchd 2-1:1.0: START poll=1900 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9497.691779] elgato_gchd 2-1:1.0: state-change timeout: current=0x11 expected=0x02 state=0x0011 completion=0x011a
[ 9497.702062] elgato_gchd 2-1:1.0: START failed: ret=-110 state=0x0011 (r=0) enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 9497.702075] elgato_gchd 2-1:1.0: capture start transition failed: -110
[ 9497.702087] elgato_gchd: input 2 signal lost; scheduling redetection
[ 9497.851982] elgato_gchd 2-1:1.0: capture start failed: -110; recovered to IDLE, retrying detection
[ 9498.897687] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[ 9499.016662] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[ 9499.045746] elgato_gchd 2-1:1.0: analog format initial status=0x66; configuring 240p
[ 9499.045751] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[ 9499.045754] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[ 9499.045756] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[ 9501.175959] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[ 9501.227823] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[ 9501.475161] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[ 9501.477524] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[ 9501.479707] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0011
[ 9501.481181] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 9501.481696] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 9501.489453] elgato_gchd 2-1:1.0: START before SCMD: state=0x0011 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 9501.491874] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[ 9501.493272] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[ 9501.502781] elgato_gchd 2-1:1.0: START poll=0 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9501.509960] elgato_gchd 2-1:1.0: START poll=1 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9501.516924] elgato_gchd 2-1:1.0: START poll=2 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9501.524093] elgato_gchd 2-1:1.0: START poll=3 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9501.531530] elgato_gchd 2-1:1.0: START poll=4 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9501.538689] elgato_gchd 2-1:1.0: START poll=5 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9501.546381] elgato_gchd 2-1:1.0: START poll=6 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9501.553591] elgato_gchd 2-1:1.0: START poll=7 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9501.561081] elgato_gchd 2-1:1.0: START poll=8 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9501.568740] elgato_gchd 2-1:1.0: START poll=9 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9501.576278] elgato_gchd 2-1:1.0: START poll=10 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9501.585188] elgato_gchd 2-1:1.0: START poll=11 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9501.592452] elgato_gchd 2-1:1.0: START poll=12 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9501.600144] elgato_gchd 2-1:1.0: START poll=13 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9501.607800] elgato_gchd 2-1:1.0: START poll=14 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9501.615624] elgato_gchd 2-1:1.0: START poll=15 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9502.238796] elgato_gchd 2-1:1.0: START poll=100 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9502.956073] elgato_gchd 2-1:1.0: START poll=200 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
[ 9503.697866] elgato_gchd 2-1:1.0: START poll=300 state=0x0011 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
ff5@ff5-VirtualBox:~$ 
