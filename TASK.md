# TASK T-020 — MPEG-TS stream with audio

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** TS with audio and correct PMT; start-of-stream cleanup, full test
- **Type:** driver feature
- **Branch:** `feat/ts-audio` (from `dev`)
- **TODO items:** T-020, T-023 (stage 1)

## Why

Composite and S-video work for all NTSC and PAL modes (`dev` 0.4.1). The
driver still delivers only the H.264 video, one PES packet per buffer, and
drops the audio: the device multiplexes MPEG-1 Layer II audio (PID `0x010F`)
into its transport stream. Applications such as ffplay, ffmpeg and OBS can
use the complete transport stream directly, with sound. It is also the
input for the planned guvcview/OBS bridge (T-060).

## Implementation

1. A second pixel format on the same node: `V4L2_PIX_FMT_MPEG`, next to
   `V4L2_PIX_FMT_H264` (still the default). The selected format is kept
   while the device stays connected, like the input.
2. MPEG buffers carry 348 whole TS packets (65 424 bytes) as received from
   the device, without null packets (PID `0x1FFF`), starting with a PAT
   packet after every stream start or restart: video, audio (`0x010F`),
   PAT/PMT, PCR.
3. ffmpeg's V4L2 input does not know the TS format, so the standard way for
   encoder cards applies: select the format once with `v4l2-ctl`, then read
   the device like a file (`read()`), e.g. `ffplay -f mpegts /dev/video0`.

## Acceptance criteria

1. TS format on composite and S-video, NTSC and PAL: picture and sound;
   ffprobe shows video and audio inside "Program 1".
2. Decode messages only at encoder restarts (NTSC 480i ↔ 240p changes);
   no "PES packet size mismatch" / "Header missing" right at the start.
3. H.264 format unchanged.

## Hardware test (maintainer)

For each source and input (N64 composite, N64 S-video, PS1 composite, PS1
S-video):

```sh
git fetch origin && git checkout feat/ts-audio && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2               # 3 for S-video
v4l2-ctl -d /dev/video0 -v pixelformat=MPEG
ffplay -f mpegts /dev/video0               # picture and sound?
# close; then:
ffmpeg -f mpegts -i /dev/video0 -t 20 -c copy test.ts
ffmpeg -v error -i test.ts -f null - ; echo exit=$?
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
```

Paste the outputs into the maintainer comments below (labelled per source
and input).

## Findings

- 38057b9 test: TS format works, video H.264 and MP2 audio 48 kHz stereo
  320 kbit/s, 20 s recording decodes. ffprobe showed "Program 1" with an
  empty video stream on PID `0x11` and the real video under "No Program":
  the driver's PMT (taken over from the C++) had wrong bytes. Replaced with
  the PSI tables Windows writes (`input-switching/filtered_init_switch_to_component`
  1.07-1.14 s). Decode errors only at the start and at the encoder restart
  of a 240p → 480i change.
- 1e86b93 test (S-video NTSC): video and audio inside "Program 1"; sound.
  Decode messages at the start (partial PES before the PAT) and at the
  encoder restarts. Now video and audio start with a complete PES packet
  after the PAT.

---

## Maintainer comments

N64 composite NTSC works:

ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -v pixelformat=MPEG
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ ffplay -f mpegts /dev/video0 
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
Input #0, mpegts, from '/dev/video0':B vq=    0KB sq=    0B f=0/0   
  Duration: N/A, start: 0.500000, bitrate: N/A
  Program 1 
  Stream #0:0[0x1011]: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 29.97 fps, 59.94 tbr, 90k tbn
  Stream #0:1[0x10f]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
[mpegts @ 0x7daf78000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x7daf78000c80] Packet corrupt (stream = 1, dts = 338760).
[mpegts @ 0x7daf78000c80] Packet corrupt (stream = 0, dts = 351306).
[mpegts @ 0x7daf78000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x7daf78000c80] Packet corrupt (stream = 1, dts = 435960).
[mp2 @ 0x7daf78163100] Header missingB vq= 1572KB sq=    0B f=0/0   
[h264 @ 0x7daf78222780] Reference 6 >= 2q= 1502KB sq=    0B f=0/0   
[h264 @ 0x7daf78222780] error while decoding MB 30 13, bytestream 20335
[h264 @ 0x7daf782db1c0] mmco: unref short failure sq=    0B f=0/0   
[mp2 @ 0x7daf78163100] Header missingB vq= 1479KB sq=    0B f=0/0   
[h264 @ 0x7daf78222780] Reference 3 >= 2q= 1606KB sq=    0B f=0/0   
[h264 @ 0x7daf78222780] error while decoding MB 10 17, bytestream 9127
   6.71 A-V: -0.015 fd=   6 aq=   40KB vq= 1557KB sq=    0B f=0/0   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ ffmpeg -f mpegts -i /dev/video0 -t 20 -c copy test.ts
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
Input #0, mpegts, from '/dev/video0':
  Duration: N/A, start: 0.500000, bitrate: N/A
  Program 1 
  Stream #0:0[0x1011]: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 29.97 fps, 59.94 tbr, 90k tbn
  Stream #0:1[0x10f]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
File 'test.ts' already exists. Overwrite? [y/N] y
Output #0, mpegts, to 'test.ts':
  Metadata:
    encoder         : Lavf60.16.100
  Stream #0:0: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], q=2-31, 29.97 fps, 59.94 tbr, 90k tbn
  Stream #0:1: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
Stream mapping:
  Stream #0:0 -> #0:0 (copy)
  Stream #0:1 -> #0:1 (copy)
Press [q] to stop, [?] for help
[mpegts @ 0x595ce74fee80] Packet corrupt (stream = 0, dts = 360315). 
[in#0/mpegts @ 0x595ce74fed80] corrupt input packet in stream 0
[mpegts @ 0x595ce74fee80] PES packet size mismatch
[mpegts @ 0x595ce74fee80] Packet corrupt (stream = 1, dts = 343080).
[out#0/mpegts @ 0x595ce7509540] video:26146kB audio:743kB subtitle:0kB other streams:0kB global headers:0kB muxing overhead: 3.011048%
size=   27698kB time=00:00:19.99 bitrate=11349.7kbits/s speed=1.39x    
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ ffmpeg -v error -i test.ts -f null - ; echo exit=$?
[mp2 @ 0x5cb297a0ee40] Header missing
[aist#0:1/mp2 @ 0x5cb2978fa980] Error submitting packet to decoder: Invalid data found when processing input
[h264 @ 0x5cb29797eb00] Reference 4 >= 2
[h264 @ 0x5cb29797eb00] error while decoding MB 18 24, bytestream 1426
exit=0
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 

ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
[sudo] password for ff5:           
[36962.884358] usbcore: registered new interface driver elgato_gchd
[36971.150684] workqueue: hub_event hogged CPU for >10000us 32 times, consider switching to WQ_UNBOUND
[36971.275038] usb 1-2: new high-speed USB device number 34 using xhci_hcd
[36971.659044] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[36971.659059] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[36971.659063] usb 1-2: Product: GameCapture HD
[36971.659067] usb 1-2: Manufacturer: Elgato
[36971.659070] usb 1-2: SerialNumber: 04107F1267
[36971.663223] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[36971.666828] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[36979.503404] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[36979.503416] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[36979.503420] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[36996.217306] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[36996.339643] elgato_gchd 1-2:1.0: transcoder defaults loaded
[36996.412862] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[36996.543868] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x0e mode=0xe
[36996.546553] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0x6e
[36996.546557] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[36996.610120] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x0e mode=0xe
[36996.613549] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[36996.613557] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[36996.673861] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x1e mode=0xe
[36996.676395] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[36996.676398] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[36996.745538] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x1e mode=0xe
[36996.748322] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xfe
[36996.748328] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[36996.818627] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[36996.828922] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[36996.831565] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[36996.831569] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[36996.905709] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[36996.916462] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[36996.918981] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[36996.918985] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[36996.979498] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[36996.988877] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[36996.990959] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[36996.990961] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[36996.990963] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[36996.990964] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[36997.779965] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[36997.901649] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[36997.916939] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[36997.917164] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[36997.939469] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[36997.940028] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[36997.940782] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[36998.056737] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[36998.428940] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[36998.473303] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[36998.519174] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[36998.564260] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[36998.608993] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[37020.714049] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[37020.835436] elgato_gchd 1-2:1.0: transcoder defaults loaded
[37020.895779] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[37021.026154] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x2e mode=0xe
[37021.028321] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0x6e
[37021.028323] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37021.102336] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[37021.113415] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[37021.115761] elgato_gchd 1-2:1.0: analog format status 0x6e not locked yet
[37021.115764] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37021.200836] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[37021.211867] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[37021.214077] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[37021.214079] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[37021.214082] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[37021.214084] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[37021.963029] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[37022.436039] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[37022.451256] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[37022.451564] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[37022.456455] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[37022.457014] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[37022.457935] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[37022.573836] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[37022.948990] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[37022.994703] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[37023.039733] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[37023.084561] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[37023.129461] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
ff5@ff5-VirtualBox:~$ 

N64 S-Video NTSC also worked fine:

ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 3
Video input set to 3 (S-Video: Camera, no signal)
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -v pixelformat=MPEG
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ ffplay -f mpegts /dev/video0
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
Input #0, mpegts, from '/dev/video0':B vq=    0KB sq=    0B f=0/0   
  Duration: N/A, start: 0.500000, bitrate: N/A
  Program 1 
  Stream #0:0[0x1011]: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 29.97 fps, 59.94 tbr, 90k tbn
  Stream #0:1[0x10f]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
[mpegts @ 0x714fcc000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x714fcc000c80] Packet corrupt (stream = 1, dts = 343080).
[mpegts @ 0x714fcc000c80] Packet corrupt (stream = 0, dts = 363318).
[mpegts @ 0x714fcc000c80] Packet corrupt (stream = 0, dts = 465420).
[mp2 @ 0x714fcc2c1380] Header missingB vq=  972KB sq=    0B f=0/0   
[mpegts @ 0x714fcc000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x714fcc000c80] Packet corrupt (stream = 1, dts = 444600).
[mp2 @ 0x714fcc2c1380] Header missingB vq=  863KB sq=    0B f=0/0   
   8.80 A-V: -0.015 fd=  24 aq=    7KB vq=  440KB sq=    0B f=1/1   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ ffmpeg -f mpegts -i /dev/video0 -t 20 -c copy test.ts
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
[mpegts @ 0x5bed0dadce80] Packet corrupt (stream = 0, dts = 267222).
Input #0, mpegts, from '/dev/video0':
  Duration: N/A, start: 0.500000, bitrate: N/A
  Program 1 
  Stream #0:0[0x1011]: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 29.97 fps, 59.94 tbr, 90k tbn
  Stream #0:1[0x10f]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
File 'test.ts' already exists. Overwrite? [y/N] y
Output #0, mpegts, to 'test.ts':
  Metadata:
    encoder         : Lavf60.16.100
  Stream #0:0: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], q=2-31, 29.97 fps, 59.94 tbr, 90k tbn
  Stream #0:1: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
Stream mapping:
  Stream #0:0 -> #0:0 (copy)
  Stream #0:1 -> #0:1 (copy)
Press [q] to stop, [?] for help
[in#0/mpegts @ 0x5bed0dadcd80] corrupt input packet in stream 0/A    
    Last message repeated 1 times
[vist#0:0/h264 @ 0x5bed0db21ac0] timestamp discontinuity (stream id=4113): -2535867, new offset= 2535867
[mpegts @ 0x5bed0dadce80] Packet corrupt (stream = 0, dts = 129084).
[in#0/mpegts @ 0x5bed0dadcd80] corrupt input packet in stream 0
[out#0/mpegts @ 0x5bed0dae7540] video:26787kB audio:747kB subtitle:0kB other streams:0kB global headers:0kB muxing overhead: 3.009597%
size=   28363kB time=00:00:19.98 bitrate=11625.3kbits/s speed=1.35x    
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ ffmpeg -v error -i test.ts -f null - ; echo exit=$?
[h264 @ 0x61384da83c80] Reference 3 >= 2
[h264 @ 0x61384da83c80] error while decoding MB 26 12, bytestream 12848
exit=0
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 


ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
[37194.792485] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[37194.792494] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[37194.792500] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[37208.214324] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[37208.311934] elgato_gchd 1-2:1.0: transcoder defaults loaded
[37208.373796] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[37208.522728] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[37208.524974] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0x6e
[37208.524976] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37208.591531] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[37208.593792] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[37208.593795] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37208.668177] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1e mode=0xe
[37208.670632] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xfe
[37208.670634] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37208.727070] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x16 mode=0x6
[37208.735712] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37208.737878] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[37208.737881] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37208.819389] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x16 mode=0x6
[37208.827721] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x16 masked_mode=0x6
[37208.829827] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[37208.829829] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37208.894441] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x16 mode=0x6
[37208.904101] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37208.906481] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[37208.906483] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[37208.906497] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=1
[37208.906499] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=1
[37209.704623] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[37209.791058] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x2f mode=0xf
[37209.793040] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xee
[37209.793044] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37209.848868] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x2e mode=0xe
[37209.851059] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[37209.851061] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37209.928747] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[37209.931476] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[37209.931483] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37209.991035] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x16 mode=0x6
[37210.000201] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37210.002329] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[37210.002331] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37210.061123] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x16 mode=0x6
[37210.071720] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37210.073962] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[37210.073966] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37210.129230] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x16 mode=0x6
[37210.137865] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37210.139967] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[37210.199485] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x16 mode=0x6
[37210.209563] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37210.211940] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[37210.271346] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37210.281442] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37210.283920] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[37210.347836] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37210.358590] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x16 masked_mode=0x6
[37210.360951] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[37210.420527] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x16 mode=0x6
[37210.432018] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x16 masked_mode=0x6
[37210.433989] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[37210.433990] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[37210.433992] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=1
[37210.433994] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=1
[37210.789943] elgato_gchd 1-2:1.0: back-end not settled: status 0xf2
[37210.800526] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[37210.818360] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[37210.818566] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[37210.824003] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[37210.824467] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=1
[37210.824905] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[37210.941116] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[37211.313032] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[37211.358608] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[37211.404400] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[37211.449453] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[37211.492889] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[37216.201590] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[37217.493743] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[37217.493967] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[37217.495158] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[37217.495653] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[37217.500445] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[37217.600168] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[37217.600173] elgato_gchd 1-2:1.0: analog mode now 240p
[37217.973250] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[37218.018866] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[37218.064491] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[37218.108446] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[37218.153730] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[37242.330550] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[37242.406000] elgato_gchd 1-2:1.0: transcoder defaults loaded
[37242.473509] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[37242.648384] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1e mode=0xe
[37242.650581] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0x6e
[37242.650584] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37242.782933] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[37242.785867] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[37242.785871] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37242.880274] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37242.888434] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37242.890631] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37242.890635] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37242.960629] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37242.969960] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37242.971838] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37242.971839] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37243.050300] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37243.065722] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37243.067995] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[37243.067997] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[37243.067999] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[37243.068001] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[37243.808680] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[37243.919838] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x2f mode=0xf
[37243.924014] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xee
[37243.924024] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37243.987950] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x3e mode=0xe
[37243.994607] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[37243.994625] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37244.064818] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x2e mode=0xe
[37244.068310] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[37244.068320] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37244.134184] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37244.153033] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37244.155646] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[37244.155649] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37244.254127] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37244.284350] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37244.287195] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37244.287201] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37244.354886] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37244.366169] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37244.368955] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37244.368960] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37244.438589] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37244.451241] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37244.453904] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37244.531374] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37244.545570] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37244.547822] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[37244.547826] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[37244.547829] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[37244.547831] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[37245.356347] elgato_gchd 1-2:1.0: back-end not settled: status 0xf2
[37245.370816] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[37245.385453] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[37245.385953] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[37245.391561] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[37245.392025] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[37245.392410] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[37245.491647] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[37245.866626] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[37245.911999] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[37245.956659] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[37246.002458] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[37246.046793] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[37247.371715] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[37248.875501] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[37248.875738] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[37248.876942] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[37248.877404] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=1
[37248.877901] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[37248.998031] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[37248.998033] elgato_gchd 1-2:1.0: analog mode now 480i
[37249.371434] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[37249.415900] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[37249.461112] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[37249.507109] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[37249.551057] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
ff5@ff5-VirtualBox:~$ 


PS1 composite PAL also worked:

ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -v pixelformat=MPEG
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ ffplay -f mpegts /dev/video0
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
Input #0, mpegts, from '/dev/video0':B vq=    0KB sq=    0B f=0/0   
  Duration: N/A, start: 0.500000, bitrate: N/A
  Program 1 
  Stream #0:0[0x1011]: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x576 [SAR 12:11 DAR 15:11], 25 fps, 50 tbr, 90k tbn
  Stream #0:1[0x10f]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
[mpegts @ 0x778594000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x778594000c80] Packet corrupt (stream = 1, dts = 343080).
[mpegts @ 0x778594000c80] Packet corrupt (stream = 0, dts = 358200).
[mpegts @ 0x778594000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x778594000c80] Packet corrupt (stream = 1, dts = 455400).
[mpegts @ 0x778594000c80] Packet corrupt (stream = 0, dts = 473400).
[mp2 @ 0x7785942e6840] Header missingB vq= 1617KB sq=    0B f=0/0   
[h264 @ 0x778594378b40] Reference 2 >= 2q= 1574KB sq=    0B f=0/0   
[h264 @ 0x778594378b40] error while decoding MB 4 31, bytestream 29030
[mp2 @ 0x7785942e6840] Header missingB vq= 1494KB sq=    0B f=0/0   
  15.21 A-V: -0.003 fd=   4 aq=   40KB vq= 1432KB sq=    0B f=0/0   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ ffmpeg -f mpegts -i /dev/video0 -t 20 -c copy test.ts
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
Input #0, mpegts, from '/dev/video0':
  Duration: N/A, start: 0.500000, bitrate: N/A
  Program 1 
  Stream #0:0[0x1011]: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x576 [SAR 12:11 DAR 15:11], 25 fps, 50 tbr, 90k tbn
  Stream #0:1[0x10f]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
File 'test.ts' already exists. Overwrite? [y/N] y
Output #0, mpegts, to 'test.ts':
  Metadata:
    encoder         : Lavf60.16.100
  Stream #0:0: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x576 [SAR 12:11 DAR 15:11], q=2-31, 25 fps, 50 tbr, 90k tbn
  Stream #0:1: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
Stream mapping:
  Stream #0:0 -> #0:0 (copy)
  Stream #0:1 -> #0:1 (copy)
Press [q] to stop, [?] for help
[mpegts @ 0x64092c452e80] PES packet size mismatchits/s speed=N/A    
[mpegts @ 0x64092c452e80] Packet corrupt (stream = 1, dts = 338760).
[mpegts @ 0x64092c452e80] Packet corrupt (stream = 0, dts = 358200).
[in#0/mpegts @ 0x64092c452d80] corrupt input packet in stream 0
[out#0/mpegts @ 0x64092c45d540] video:27450kB audio:777kB subtitle:0kB other streams:0kB global headers:0kB muxing overhead: 2.891283%
size=   29043kB time=00:00:19.99 bitrate=11900.9kbits/s speed=1.31x    
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ ffmpeg -v error -i test.ts -f null - ; echo exit=$?
[mp2 @ 0x593666a5bb80] Header missing
[aist#0:1/mp2 @ 0x593666a1f980] Error submitting packet to decoder: Invalid data found when processing input
[h264 @ 0x593666d259c0] error while decoding MB 28 28, bytestream -24
exit=0
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 


ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
[37434.961917] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[37434.961923] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[37434.961927] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[37453.766241] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[37453.946642] elgato_gchd 1-2:1.0: transcoder defaults loaded
[37454.018261] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[37454.383604] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37454.385721] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37454.385725] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37454.446177] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37454.451283] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37454.451297] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37454.522735] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37454.525072] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37454.525078] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37454.591743] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37454.594734] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37454.594737] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37454.665920] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37454.670577] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37454.670584] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37454.762332] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37454.769132] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37454.769140] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37454.835402] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x47 mode=0x7
[37454.844748] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x57 masked_mode=0x7
[37454.847354] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[37454.847359] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37454.907459] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x66 mode=0x6
[37454.916651] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x66 masked_mode=0x6
[37454.918655] elgato_gchd 1-2:1.0: analog format status 0xce not locked yet
[37454.918657] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37454.979007] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x2e mode=0xe
[37454.981989] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[37454.981994] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37455.049991] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x26 mode=0x6
[37455.060921] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x26 masked_mode=0x6
[37455.063090] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[37455.063092] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37455.132819] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x26 mode=0x6
[37455.142397] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x26 masked_mode=0x6
[37455.144743] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37455.210846] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[37455.221682] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[37455.223807] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37455.287274] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[37455.296771] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[37455.299951] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37455.355695] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[37455.364926] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[37455.366922] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37455.441533] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[37455.455634] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[37455.458192] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37455.527475] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[37455.536718] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[37455.538846] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[37455.538848] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[37455.538850] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[37455.538852] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[37456.347488] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[37459.086774] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[37459.106759] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[37459.106973] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[37459.109221] elgato_gchd 1-2:1.0: source changed to 50 Hz during setup; configuring again
[37459.165698] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x07 mode=0x7
[37459.174689] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[37459.176815] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[37459.243244] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x07 mode=0x7
[37459.252079] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[37459.254209] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[37459.314555] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[37459.323352] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[37459.325618] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[37459.389651] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[37459.400088] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[37459.402209] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[37459.402212] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37459.475469] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[37459.491629] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[37459.494044] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[37459.494047] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37459.564198] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[37459.584164] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[37459.586541] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[37459.586545] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[37459.586548] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[37459.586550] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[37459.807172] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[37459.822049] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[37459.822375] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[37459.828056] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[37459.828846] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[37459.829300] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[37459.968021] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[37460.366521] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[37460.411450] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[37460.455929] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[37460.500628] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[37460.546738] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[37499.335419] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[37499.498663] elgato_gchd 1-2:1.0: transcoder defaults loaded
[37499.565874] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[37499.710181] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37499.712296] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[37499.712298] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37499.777421] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37499.780165] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37499.780168] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37499.860910] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37499.864102] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37499.864112] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37499.937456] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37499.946799] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37499.946813] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37500.019146] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37500.022875] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37500.022885] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37500.243805] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37500.250271] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37500.250285] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37500.343960] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37500.348985] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37500.348996] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37500.431244] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37500.436090] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37500.436103] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37500.510233] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37500.515409] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37500.515422] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37500.594673] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37500.598260] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37500.598270] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37500.670041] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37500.755140] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37500.850732] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37500.943824] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37501.026472] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37501.126849] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37501.227157] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37501.300977] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37501.390509] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37501.476209] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37501.556934] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37501.672072] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37501.746579] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37501.861371] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37502.014351] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37502.122882] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37502.227670] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37502.325238] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37502.445893] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37502.525488] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37502.624818] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37502.723849] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37502.826813] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37502.946370] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37503.039594] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37503.123517] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37503.221380] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37503.308090] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37503.396621] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37503.471673] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37503.547460] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37503.629188] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37503.718592] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37503.802266] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37503.876500] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37503.949095] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37504.018914] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37504.098848] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37504.199407] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37504.259397] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37504.318600] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37504.415666] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37504.498355] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37504.577788] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37504.667139] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37504.757225] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37504.760744] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37504.760753] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37504.852425] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37504.860305] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37504.860333] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37504.936670] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37504.941797] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37504.941808] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37505.007951] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37505.013683] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37505.013700] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37505.088937] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37505.094060] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37505.094070] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37505.168076] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37505.171540] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37505.171549] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37505.240168] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37505.245117] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37505.245128] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37505.325891] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37505.337494] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37505.337509] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37505.431192] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37505.440392] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37505.440405] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37505.522691] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37505.529410] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37505.529417] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37505.605746] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37505.698722] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37505.777267] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37505.849495] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37505.939115] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37505.999655] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37506.061631] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37506.164879] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37506.267776] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37506.357160] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37506.459177] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37506.544089] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37506.691805] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37506.780818] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37506.882417] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37506.976590] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37507.055928] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37507.256020] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37507.315267] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37507.377664] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37507.450460] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37507.526031] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37507.599925] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37507.661920] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37507.723985] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37507.806296] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37507.909077] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37507.994751] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37508.097729] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37508.185649] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37508.277097] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37508.358590] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37508.447192] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37508.512550] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37508.600260] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37508.695096] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37508.775707] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37508.862884] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37508.946245] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37509.027061] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37509.120180] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37509.229674] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37509.309367] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37509.402421] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37509.476582] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37509.556840] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37509.642897] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37509.733054] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37509.901708] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37509.909356] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37509.909373] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37509.987181] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37509.994971] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37509.994986] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37510.070797] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37510.081686] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37510.081703] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37510.138695] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37510.141734] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37510.141741] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37510.218208] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37510.227647] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37510.227667] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37510.285731] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37510.288050] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37510.288054] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37510.350435] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37510.353147] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37510.353156] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37510.431387] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37510.441064] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37510.441077] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37510.530772] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37510.536313] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37510.536320] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37510.614086] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37510.625090] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[37510.625109] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[37510.714576] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37510.798021] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37510.893085] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37510.974078] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37511.059474] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37511.154337] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37511.280702] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37511.369442] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37511.451748] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37511.566570] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37511.662103] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37511.771624] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37511.855772] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37511.944744] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37512.036997] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37512.120163] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37512.216052] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37512.305086] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37512.387027] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37512.466475] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37512.551906] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37512.697220] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37512.786596] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37512.898132] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37512.991261] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[37513.072558] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37513.200053] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37513.278245] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37513.357129] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37513.448715] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37513.559458] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37513.643195] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37513.723078] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[37513.809575] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x1f mode=0xf
[37513.903224] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x2e mode=0xe
[37514.003367] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[37514.015026] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[37514.017375] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37514.097597] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[37514.113492] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[37514.116322] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37514.214729] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[37514.233884] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[37514.236216] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[37514.236218] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[37514.236220] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[37514.236221] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[37515.008662] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[37518.474667] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[37518.489778] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[37518.490006] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[37518.492177] elgato_gchd 1-2:1.0: source changed to 50 Hz during setup; configuring again
[37518.578841] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x07 mode=0x7
[37518.594157] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[37518.597274] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[37518.597279] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[37518.597283] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[37518.597286] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[37518.829066] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[37518.851638] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[37518.851992] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[37518.859105] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[37518.859790] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[37518.860462] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[37518.997639] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[37519.396085] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[37519.441412] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[37519.486466] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[37519.531001] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[37519.576860] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
ff5@ff5-VirtualBox:~$ 


PS1 S-Video PAL didn't display correctly the video on first ffplay but the audio worked fine:

ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 3
Video input set to 3 (S-Video: Camera, no signal)
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -v pixelformat=MPEG
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ ffplay -f mpegts /dev/video0
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
[mpegts @ 0x7d23b8000c80] Packet corrupt (stream = 0, dts = 159114).
[mpegts @ 0x7d23b8000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x7d23b8000c80] Packet corrupt (stream = 1, dts = 137880).
Input #0, mpegts, from '/dev/video0':B vq=    0KB sq=    0B f=0/0   
  Duration: N/A, start: 0.500000, bitrate: N/A
  Program 1 
  Stream #0:0[0x1011]: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 29.97 fps, 59.94 tbr, 90k tbn
  Stream #0:1[0x10f]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
[mp2 @ 0x7d23b82c1380] Header missingB vq= 2059KB sq=    0B f=0/0   
[mpegts @ 0x7d23b8000c80] PES packet size mismatchsq=    0B f=1/1   
[mpegts @ 0x7d23b8000c80] Packet corrupt (stream = 1, dts = 224280).
[mpegts @ 0x7d23b8000c80] Packet corrupt (stream = 0, dts = 246201).
[mp2 @ 0x7d23b82c1380] Header missingB vq= 1592KB sq=    0B f=1/1   
[h264 @ 0x7d23b81e3480] Reference 2 >= 2q= 1684KB sq=    0B f=1/1   
[h264 @ 0x7d23b81e3480] error while decoding MB 39 14, bytestream 17383
  13.25 A-V: -0.015 fd=  23 aq=   40KB vq= 1810KB sq=    0B f=1/3   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ ffmpeg -f mpegts -i /dev/video0 -t 20 -c copy test.ts
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
Input #0, mpegts, from '/dev/video0':
  Duration: N/A, start: 0.500000, bitrate: N/A
  Program 1 
  Stream #0:0[0x1011]: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x576 [SAR 12:11 DAR 15:11], 25 fps, 50 tbr, 90k tbn
  Stream #0:1[0x10f]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
File 'test.ts' already exists. Overwrite? [y/N] y
Output #0, mpegts, to 'test.ts':
  Metadata:
    encoder         : Lavf60.16.100
  Stream #0:0: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x576 [SAR 12:11 DAR 15:11], q=2-31, 25 fps, 50 tbr, 90k tbn
  Stream #0:1: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
Stream mapping:
  Stream #0:0 -> #0:0 (copy)
  Stream #0:1 -> #0:1 (copy)
Press [q] to stop, [?] for help
[mpegts @ 0x5e28314dfe80] PES packet size mismatchits/s speed=N/A    
[mpegts @ 0x5e28314dfe80] Packet corrupt (stream = 1, dts = 343080).
[mpegts @ 0x5e28314dfe80] Packet corrupt (stream = 0, dts = 361800).
[in#0/mpegts @ 0x5e28314dfd80] corrupt input packet in stream 0
    Last message repeated 2 times
[out#0/mpegts @ 0x5e28314ea540] video:27345kB audio:777kB subtitle:0kB other streams:0kB global headers:0kB muxing overhead: 2.824382%
size=   28916kB time=00:00:19.99 bitrate=11848.7kbits/s speed=1.31x    
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ ffmpeg -v error -i test.ts -f null - ; echo exit=$?
[mp2 @ 0x63165bb6a980] Header missing
[aist#0:1/mp2 @ 0x63165badd980] Error submitting packet to decoder: Invalid data found when processing input
[h264 @ 0x63165bea1b40] Reference 3 >= 2
[h264 @ 0x63165bea1b40] error while decoding MB 32 31, bytestream 15849
[h264 @ 0x63165bc69940] decode_slice_header error
exit=0
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 


ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
[37677.996014] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[37677.996027] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[37677.996035] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[37695.239119] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[37695.901357] elgato_gchd 1-2:1.0: transcoder defaults loaded
[37695.961621] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[37696.101827] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x7e mode=0xe
[37696.104819] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0x6e
[37696.104824] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37696.169346] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x2e mode=0xe
[37696.171627] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[37696.171630] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37696.236615] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37696.247858] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37696.251221] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[37696.251231] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37696.342233] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37696.351557] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x26 masked_mode=0x6
[37696.353811] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37696.353814] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37696.424105] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37696.439398] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37696.441979] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[37696.441983] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[37696.441986] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[37696.441988] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[37697.227560] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[37697.324017] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x6f mode=0xf
[37697.327447] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xee
[37697.327453] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37697.389298] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x7e mode=0xe
[37697.391862] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[37697.391868] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37697.455749] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x6e mode=0xe
[37697.459136] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[37697.459140] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37697.520927] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1e mode=0xe
[37697.523793] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[37697.523797] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37697.582814] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37697.596977] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37697.599424] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[37697.599428] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37697.660073] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37697.670184] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37697.672476] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37697.672478] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37697.729099] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37697.740930] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37697.743289] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37697.806771] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37697.822720] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37697.825514] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37697.889087] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37697.900605] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37697.903490] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37697.973477] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37697.985785] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37697.988468] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37698.046864] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37698.060685] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37698.062991] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37698.135288] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37698.147762] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37698.149970] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[37698.149973] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[37698.149975] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[37698.149977] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[37699.687891] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[37699.702971] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[37699.703198] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[37699.708765] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[37699.709260] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[37699.709670] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[37699.859153] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[37700.231194] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[37700.277910] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[37700.323160] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[37700.367396] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[37700.411788] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[37700.669025] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[37701.922436] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[37701.922684] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[37701.924048] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[37701.924878] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=1
[37701.925378] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[37702.045254] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[37702.045258] elgato_gchd 1-2:1.0: analog mode now 480i
[37702.417509] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[37702.461966] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[37702.508278] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[37702.555335] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[37702.598049] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[37741.741004] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[37742.258168] elgato_gchd 1-2:1.0: transcoder defaults loaded
[37742.319927] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[37742.497103] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37742.500017] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[37742.500024] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37742.568151] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37742.570829] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[37742.570834] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37742.639823] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[37742.648076] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[37742.648094] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37742.710218] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[37742.712414] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[37742.712419] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37742.788501] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[37742.792158] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[37742.792165] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37742.881811] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[37742.891296] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[37742.891310] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37742.969489] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37742.977445] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[37742.977460] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37743.048543] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37743.052185] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[37743.052196] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37743.122780] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[37743.127217] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[37743.127226] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37743.196259] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[37743.201673] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[37743.201684] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37743.294515] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37743.385886] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[37743.458648] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37743.535031] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37743.612844] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37743.695422] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37743.769665] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37743.859342] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[37743.932730] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37744.015720] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37744.095574] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37744.169300] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37744.253249] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37744.333340] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37744.404843] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37744.476479] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[37744.568655] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37744.641788] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37744.729002] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37744.810229] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37744.878844] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[37744.947421] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[37745.017898] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37745.085939] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37745.143910] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[37745.210172] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37745.288788] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37745.364097] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[37745.438162] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[37745.503930] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[37745.588367] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[37745.678981] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[37745.774064] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1f mode=0xf
[37745.834635] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1e mode=0xe
[37745.908232] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1e mode=0xe
[37745.986762] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x2e mode=0xe
[37746.073696] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37746.081740] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37746.083859] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37746.150240] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37746.160835] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37746.163060] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37746.228019] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37746.241178] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37746.243681] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[37746.243684] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[37746.243688] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[37746.243691] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[37746.988447] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[37747.079541] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1f mode=0xf
[37747.149367] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[37747.210809] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1e mode=0xe
[37747.279006] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[37747.349887] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37747.362674] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37747.365213] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37747.433162] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37747.450906] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37747.453469] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37747.516302] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37747.528780] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37747.531069] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37747.531073] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37747.597761] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37747.609596] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37747.611839] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37747.611842] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37747.680137] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37747.708485] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37747.710652] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37747.710654] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37747.782225] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37747.794041] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37747.796352] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37747.796355] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37747.864654] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37747.876608] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37747.878831] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[37747.878834] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[37747.949729] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[37747.962398] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[37747.964616] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[37747.964619] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[37747.964622] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[37747.964624] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[37750.880546] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[37750.895339] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[37750.895521] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[37750.897542] elgato_gchd 1-2:1.0: source changed to 50 Hz during setup; configuring again
[37750.974582] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x07 mode=0x7
[37750.983621] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x17 masked_mode=0x7
[37750.985741] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[37750.985743] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[37750.985745] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x576 fps=50/1 interlaced=1
[37750.985747] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x576 fps=50/1 interlaced=1
[37751.199455] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[37751.214514] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[37751.214724] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[37751.220366] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[37751.221067] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x576 fps=50/1 interlaced=1
[37751.221579] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[37751.360210] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[37751.759479] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[37751.804281] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[37751.848938] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[37751.894446] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[37751.940006] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
ff5@ff5-VirtualBox:~$ 
