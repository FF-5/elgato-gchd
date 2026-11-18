# TASK T-020 — MPEG-TS stream with audio

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** TS with audio works; PMT fixed, waiting for the hardware test
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

1. `ffplay -f mpegts /dev/video0`: picture and sound; ffprobe shows the
   video (`0x1011`) and audio (`0x010F`) **inside "Program 1"**, without
   the empty stream on PID `0x11`.
2. A 20 s recording decodes; decode errors only right at the start or at a
   mode change (the encoder restarts there).
3. H.264 format unchanged (video only, no audio, as designed).

## Hardware test (maintainer)

```sh
git fetch origin && git checkout feat/ts-audio && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
v4l2-ctl -d /dev/video0 -v pixelformat=MPEG
ffplay -f mpegts /dev/video0                 # picture and sound?
# close; then:
ffmpeg -f mpegts -i /dev/video0 -t 20 -c copy test.ts
ffprobe test.ts
ffmpeg -v error -i test.ts -f null - ; echo exit=$?
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
```

Paste the ffplay/ffmpeg/ffprobe output into the maintainer comments below.

## Findings

- 38057b9 test: TS format works, video H.264 and MP2 audio 48 kHz stereo
  320 kbit/s, 20 s recording decodes. ffprobe showed "Program 1" with an
  empty video stream on PID `0x11` and the real video under "No Program":
  the driver's PMT (taken over from the C++) had wrong bytes. Replaced with
  the PSI tables Windows writes (`input-switching/filtered_init_switch_to_component`
  1.07-1.14 s). Decode errors only at the start and at the encoder restart
  of a 240p → 480i change.

---

## Maintainer comments

I may have gotten an even better stream than before. Mind you I only tested S-Video NTSC. Once you've ironed out all issues I'll test composite and S-Video with NTSC and PAL.

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
[mpegts @ 0x76950c000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x76950c000c80] Packet corrupt (stream = 1, dts = 343080).
[mpegts @ 0x76950c000c80] Packet corrupt (stream = 0, dts = 363318).
[mpegts @ 0x76950c000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x76950c000c80] Packet corrupt (stream = 1, dts = 457560).
[mpegts @ 0x76950c000c80] Packet corrupt (stream = 0, dts = 477432).
[mp2 @ 0x76950c2c3e80] Header missingB vq= 1646KB sq=    0B f=0/0   
[mp2 @ 0x76950c2c3e80] Header missingB vq= 1567KB sq=    0B f=0/0   
[h264 @ 0x76950c227240] Reference 2 >= 2q= 1613KB sq=    0B f=0/0   
[h264 @ 0x76950c227240] error while decoding MB 18 18, bytestream 15088
[mpegts @ 0x76950c000c80] Packet corrupt (stream = 0, dts = 630585).
[mpegts @ 0x76950c000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x76950c000c80] Packet corrupt (stream = 1, dts = 608760).
[mp2 @ 0x76950c2c3e80] Header missingB vq= 1003KB sq=    0B f=0/0   
[h264 @ 0x76950c213140] error while decoding MB 33 22, bytestream -13
[mpegts @ 0x76950c000c80] Packet corrupt (stream = 0, dts = 1945899).
[h264 @ 0x76950c213140] error while decoding MB 12 27, bytestream -10
[mpegts @ 0x76950c000c80] PES packet size mismatchsq=    0B f=2/1   
[mpegts @ 0x76950c000c80] Packet corrupt (stream = 1, dts = 1928520).
[mp2 @ 0x76950c2c3e80] Header missingB vq=    0KB sq=    0B f=2/1   
   3.77 A-V: -0.004 fd=  48 aq=    0KB vq=    0KB sq=    0B f=2/2   
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
[mpegts @ 0x5f48b7416e80] PES packet size mismatchits/s speed=N/A    
[mpegts @ 0x5f48b7416e80] Packet corrupt (stream = 1, dts = 338760).
[mpegts @ 0x5f48b7416e80] Packet corrupt (stream = 0, dts = 357312).
[in#0/mpegts @ 0x5f48b7416d80] corrupt input packet in stream 0
[mpegts @ 0x5f48b7416e80] Packet corrupt (stream = 0, dts = 1360314). 
[in#0/mpegts @ 0x5f48b7416d80] corrupt input packet in stream 0
[vist#0:0/h264 @ 0x5f48b74effc0] timestamp discontinuity (stream id=4113): -14664645, new offset= 14664645
[mpegts @ 0x5f48b7416e80] PES packet size mismatchts/s speed=1.84x    
[mpegts @ 0x5f48b7416e80] Packet corrupt (stream = 1, dts = 1338840).
[aist#0:1/mp2 @ 0x5f48b744f7c0] timestamp discontinuity (stream id=271): 14664644, new offset= 1
[aist#0:1/mp2 @ 0x5f48b744f7c0] timestamp discontinuity (stream id=271): -14400000, new offset= 14400001
[vist#0:0/h264 @ 0x5f48b74effc0] timestamp discontinuity (stream id=4113): -264633, new offset= 14664634
[out#0/mpegts @ 0x5f48b7421540] video:23858kB audio:664kB subtitle:0kB other streams:0kB global headers:0kB muxing overhead: 2.996372%
size=   25257kB time=00:00:19.99 bitrate=10349.2kbits/s speed=1.47x    
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ ffprobe test.ts
ffprobe version 6.1.1-3ubuntu5 Copyright (c) 2007-2023 the FFmpeg developers
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
Input #0, mpegts, from 'test.ts':
  Duration: 00:00:20.07, start: 1.433367, bitrate: 10309 kb/s
  Program 1 
    Metadata:
      service_name    : Service01
      service_provider: FFmpeg
  Stream #0:0[0x100]: Video: h264 (High) ([27][0][0][0] / 0x001B), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 29.97 fps, 59.94 tbr, 90k tbn
  Stream #0:1[0x101]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ ffmpeg -v error -i test.ts -f null - ; echo exit=$?
[mp2 @ 0x55f47e3a4e00] Header missing
[aist#0:1/mp2 @ 0x55f47e0743c0] Error submitting packet to decoder: Invalid data found when processing input
[h264 @ 0x55f47e071a00] cabac decode of qscale diff failed at 37 2
[h264 @ 0x55f47e071a00] error while decoding MB 37 2, bytestream 756
[h264 @ 0x55f47e1ab040] illegal short term buffer state detected
[h264 @ 0x55f47e1ab040] Missing reference picture, default is 65548
[mp2 @ 0x55f47e3a4e00] Header missing
[aist#0:1/mp2 @ 0x55f47e0743c0] Error submitting packet to decoder: Invalid data found when processing input
[null @ 0x55f47e245ac0] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 599 >= 596
[null @ 0x55f47e245ac0] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 599 >= 597
[null @ 0x55f47e245ac0] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 599 >= 598
exit=0
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 


ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
[35389.180633] usbcore: registered new interface driver elgato_gchd
[35393.797987] usb 1-2: new high-speed USB device number 33 using xhci_hcd
[35394.189278] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[35394.189287] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[35394.189289] usb 1-2: Product: GameCapture HD
[35394.189291] usb 1-2: Manufacturer: Elgato
[35394.189293] usb 1-2: SerialNumber: 04107F1267
[35394.191359] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[35394.194293] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[35410.409448] elgato_gchd 1-2:1.0: input switch requested: old=0 new=3 hw_initialized=0 streaming=0
[35410.409457] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[35410.409463] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[35433.819518] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[35433.945079] elgato_gchd 1-2:1.0: transcoder defaults loaded
[35434.013181] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[35434.175261] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[35434.177853] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[35434.177855] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[35434.249237] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[35434.252819] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[35434.252824] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[35434.314098] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[35434.318475] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[35434.318480] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[35434.384780] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[35434.389607] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[35434.389612] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[35434.460505] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[35434.465545] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[35434.465551] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[35434.528110] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[35434.531515] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[35434.531521] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[35434.597783] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[35434.601046] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[35434.601051] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[35434.664914] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[35434.668578] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[35434.668585] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[35434.737121] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[35434.745097] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[35434.745103] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[35434.810921] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[35434.813376] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[35434.813379] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[35434.879771] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[35434.955972] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[35435.032799] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[35435.095027] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[35435.175916] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[35435.247624] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[35435.319893] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[35435.387948] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[35435.453130] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[35435.519666] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[35435.587969] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[35435.667798] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[35435.734586] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[35435.795042] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[35435.857590] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xef mode=0xf
[35435.929080] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xdf mode=0xf
[35436.030707] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xcf mode=0xf
[35436.243413] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[35436.475185] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[35436.535364] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xcd mode=0xd
[35436.612327] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[35436.688476] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xdd mode=0xd
[35436.759284] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[35436.825354] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xcd mode=0xd
[35436.891810] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xdd mode=0xd
[35436.952379] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x2f mode=0xf
[35437.021148] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x3e mode=0xe
[35437.093811] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[35437.161160] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[35437.220020] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[35437.230607] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[35437.233470] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[35437.292406] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[35437.302353] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[35437.304724] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[35437.367154] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[35437.376731] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[35437.378963] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[35437.449154] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[35437.462313] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[35437.465422] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[35437.540286] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[35437.559217] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[35437.561829] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[35437.561832] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[35437.561835] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[35437.561838] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[35438.345992] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[35438.436460] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x3f mode=0xf
[35438.506626] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[35438.574131] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[35438.594035] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[35438.596645] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[35438.663620] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[35438.673581] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[35438.676066] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[35438.749414] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[35438.760952] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[35438.764232] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[35438.820262] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[35438.828646] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[35438.831822] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[35438.831824] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[35438.831827] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[35438.831829] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[35439.582216] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[35439.597945] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[35439.598162] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[35439.603514] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[35439.604011] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[35439.604519] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[35439.704083] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[35440.077019] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[35440.121968] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[35440.168104] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[35440.211855] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[35440.257528] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[35446.427486] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[35447.966740] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[35447.966982] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[35447.967904] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[35447.968435] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=1
[35447.968924] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[35448.072677] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[35448.072680] elgato_gchd 1-2:1.0: analog mode now 480i
[35448.445119] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[35448.490276] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[35448.536215] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[35448.580752] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[35448.625130] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[35468.752777] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[35470.185270] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[35470.185635] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[35470.187452] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[35470.188239] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[35470.189310] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[35470.291703] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[35470.291709] elgato_gchd 1-2:1.0: analog mode now 240p
[35470.665052] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[35470.711237] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[35470.755765] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[35470.800541] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[35470.845243] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[35482.663264] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[35482.794055] elgato_gchd 1-2:1.0: transcoder defaults loaded
[35482.857796] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[35483.023746] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x2e mode=0xe
[35483.025894] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0x6e
[35483.025896] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[35483.100471] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[35483.106450] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[35483.106466] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[35483.213611] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x26 mode=0x6
[35483.223930] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[35483.226551] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[35483.226555] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[35483.282551] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[35483.291927] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[35483.294769] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[35483.294776] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[35483.294779] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[35483.294783] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[35484.076917] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[35484.165618] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x3f mode=0xf
[35484.168742] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xee
[35484.168750] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[35484.238808] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x3e mode=0xe
[35484.245032] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[35484.245043] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[35484.317894] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x2e mode=0xe
[35484.322678] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[35484.322686] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[35484.396232] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x26 mode=0x6
[35484.410718] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x26 masked_mode=0x6
[35484.413514] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[35484.413518] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[35484.485136] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[35484.499065] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[35484.501906] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[35484.501911] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[35484.587176] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[35484.607299] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[35484.610572] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[35484.610577] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[35484.682154] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[35484.696831] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[35484.699511] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[35484.699516] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[35484.766768] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[35484.778420] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[35484.781190] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[35484.858011] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[35484.878191] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[35484.882656] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[35484.882669] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[35484.882676] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[35484.882683] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[35485.678025] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[35485.700285] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[35485.700647] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[35485.706908] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[35485.707920] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[35485.708530] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[35485.839884] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[35486.214307] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[35486.260159] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[35486.305254] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[35486.349655] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[35486.394798] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[35499.908166] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[35501.340443] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[35501.340644] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[35501.342233] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[35501.343229] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=1
[35501.343698] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[35501.447539] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[35501.447543] elgato_gchd 1-2:1.0: analog mode now 480i
[35501.819933] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[35501.865382] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[35501.909991] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[35501.954932] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[35501.999366] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
ff5@ff5-VirtualBox:~$ 
