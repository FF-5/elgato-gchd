# TASK T-020 — MPEG-TS stream with audio

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** implemented; waiting for the hardware test
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

1. `ffplay -f mpegts /dev/video0` shows the picture **with sound**
   (composite and S-video, NTSC and PAL).
2. A 20 s recording decodes without errors; `ffprobe` reports H.264 video
   and MP2 audio.
3. The H.264 format still works as before.

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
v4l2-ctl -d /dev/video0 -V                 # should show "MPEG"
ffplay -f mpegts /dev/video0               # picture and sound?
# close; then a 20 s recording and checks:
ffmpeg -f mpegts -i /dev/video0 -t 20 -c copy test.ts
ffprobe test.ts
ffmpeg -v error -i test.ts -f null - && echo DECODE-OK
# back to H.264 (as before):
v4l2-ctl -d /dev/video0 -v pixelformat=H264
ffplay -f v4l2 -input_format h264 -i /dev/video0
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
```

Paste the outputs (ffplay, ffprobe, the DECODE-OK line, dmesg) into the
maintainer comments below and say whether there was sound.

## Findings

_(filled in during the task)_

---

## Maintainer comments

This worked:

ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -v pixelformat=MPEG
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -V
Format Video Capture:
	Width/Height      : 720/480
	Pixel Format      : 'MPEG' (MPEG-1/2/4)
	Field             : Interlaced
	Bytes per Line    : 0
	Size Image        : 65424
	Colorspace        : Rec. 709
	Transfer Function : Default (maps to Rec. 709)
	YCbCr/HSV Encoding: Default (maps to Rec. 709)
	Quantization      : Default (maps to Full Range)
	Flags             : 
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
[mpegts @ 0x783b28000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x783b28000c80] Packet corrupt (stream = 2, dts = 137880).
[mpegts @ 0x783b28000c80] Stream #0: not enough frames to estimate rate; consider increasing probesize
[mpegts @ 0x783b28000c80] Could not find codec parameters for stream 0 (Video: h264 (HDMV / 0x564D4448), none): unspecified size
Consider increasing the value for the 'analyzeduration' (0) and 'probesize' (5000000) options
Input #0, mpegts, from '/dev/video0':
  Duration: N/A, start: 0.500000, bitrate: N/A
  Program 1 
  Stream #0:0[0x11]: Video: h264 (HDMV / 0x564D4448), none, 90k tbr, 90k tbn
  No Program
  Stream #0:1[0x1011]: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 29.97 fps, 59.94 tbr, 90k tbn
  Stream #0:2[0x10f]: Audio: mp2, 48000 Hz, stereo, fltp, 320 kb/s
[mp2 @ 0x783b28157cc0] Header missingB vq= 1740KB sq=    0B f=0/0   
[mpegts @ 0x783b28000c80] PES packet size mismatchsq=    0B f=1/1   
[mpegts @ 0x783b28000c80] Packet corrupt (stream = 2, dts = 241560).
[mpegts @ 0x783b28000c80] Packet corrupt (stream = 1, dts = 255210).
[mp2 @ 0x783b28157cc0] Header missingB vq= 1567KB sq=    0B f=1/1   
[h264 @ 0x783b2818fb80] Reference 3 >= 2q= 1542KB sq=    0B f=1/1   
[h264 @ 0x783b2818fb80] error while decoding MB 3 20, bytestream 20222
[h264 @ 0x783b2824e840] mmco: unref short failure sq=    0B f=1/1   
  16.76 A-V: -0.027 fd=  11 aq=   40KB vq= 1529KB sq=    0B f=1/1   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 

The first recording produced this:

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
[mpegts @ 0x58075e267e80] Stream #0: not enough frames to estimate rate; consider increasing probesize
[mpegts @ 0x58075e267e80] Could not find codec parameters for stream 0 (Video: h264 (HDMV / 0x564D4448), none): unspecified size
Consider increasing the value for the 'analyzeduration' (0) and 'probesize' (5000000) options
Input #0, mpegts, from '/dev/video0':
  Duration: N/A, start: 0.500000, bitrate: N/A
  Program 1 
  Stream #0:0[0x11]: Video: h264 (HDMV / 0x564D4448), none, 90k tbr, 90k tbn
  No Program
  Stream #0:1[0x1011]: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 29.97 fps, 59.94 tbr, 90k tbn
  Stream #0:2[0x10f]: Audio: mp2, 48000 Hz, stereo, fltp, 320 kb/s
Output #0, mpegts, to 'test.ts':
  Metadata:
    encoder         : Lavf60.16.100
  Stream #0:0: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], q=2-31, 29.97 fps, 59.94 tbr, 90k tbn
  Stream #0:1: Audio: mp2, 48000 Hz, stereo, fltp, 320 kb/s
Stream mapping:
  Stream #0:1 -> #0:0 (copy)
  Stream #0:2 -> #0:1 (copy)
Press [q] to stop, [?] for help
[mpegts @ 0x58075e267e80] Packet corrupt (stream = 1, dts = 405360).   
[in#0/mpegts @ 0x58075e267d80] corrupt input packet in stream 1
    Last message repeated 1 times
[vist#0:1/h264 @ 0x58075e3f86c0] timestamp discontinuity (stream id=4113): -4070734, new offset= 4070734
[mpegts @ 0x58075e267e80] PES packet size mismatch
[mpegts @ 0x58075e267e80] Packet corrupt (stream = 2, dts = 386280).
[aist#0:2/mp2 @ 0x58075e269280] timestamp discontinuity (stream id=271): -3816000, new offset= 7886734
[out#0/mpegts @ 0x58075e270900] video:22319kB audio:622kB subtitle:0kB other streams:0kB global headers:0kB muxing overhead: 3.000229%
size=   23629kB time=00:00:19.99 bitrate=9678.9kbits/s speed=1.42x    
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
  Duration: 00:00:20.08, start: 1.433367, bitrate: 9638 kb/s
  Program 1 
    Metadata:
      service_name    : Service01
      service_provider: FFmpeg
  Stream #0:0[0x100]: Video: h264 (High) ([27][0][0][0] / 0x001B), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 29.97 fps, 59.94 tbr, 90k tbn
  Stream #0:1[0x101]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ ffmpeg -v error -i test.ts -f null - && echo DECODE-OK
[h264 @ 0x5f2731e6ff80] error while decoding MB 40 23, bytestream -8
[mp2 @ 0x5f2731c61cc0] Header missing
[aist#0:1/mp2 @ 0x5f27318ea980] Error submitting packet to decoder: Invalid data found when processing input
DECODE-OK
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 


The H264 test didn't produce any audio output:

ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -v pixelformat=H264
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
  Duration: N/A, start: 34897.627431, bitrate: N/A
  Stream #0:0: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 60 fps, 60 tbr, 1000k tbn
[video4linux2,v4l2 @ 0x7a6494000c80] Some buffers are still owned by the caller on close.
ioctl(VIDIOC_QBUF): Bad file descriptor

ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 


dmesg for all tests:

ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
[sudo] password for ff5:           
[34640.899956] usbcore: registered new interface driver elgato_gchd
[34647.039145] usb 1-2: new high-speed USB device number 32 using xhci_hcd
[34647.416083] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[34647.416089] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[34647.416090] usb 1-2: Product: GameCapture HD
[34647.416092] usb 1-2: Manufacturer: Elgato
[34647.416093] usb 1-2: SerialNumber: 04107F1267
[34647.417850] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[34647.420017] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[34667.483586] elgato_gchd 1-2:1.0: input switch requested: old=0 new=3 hw_initialized=0 streaming=0
[34667.483605] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[34667.483615] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[34719.655060] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[34719.804025] elgato_gchd 1-2:1.0: transcoder defaults loaded
[34719.869953] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[34720.021399] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x3e mode=0xe
[34720.023643] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0x6e
[34720.023645] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34720.081064] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34720.091864] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34720.094068] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[34720.094070] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34720.171456] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34720.181230] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34720.183463] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[34720.183465] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34720.251505] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34720.262513] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34720.264569] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[34720.264571] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[34720.264572] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[34720.264574] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[34721.122442] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[34721.216497] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x77 mode=0x7
[34721.227883] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x67 masked_mode=0x7
[34721.230024] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[34721.230029] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34721.296246] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x76 mode=0x6
[34721.325531] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x76 masked_mode=0x6
[34721.328457] elgato_gchd 1-2:1.0: analog format status 0xce not locked yet
[34721.328463] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34721.391582] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x66 mode=0x6
[34721.402259] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x36 masked_mode=0x6
[34721.404998] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[34721.405001] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34721.471635] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34721.482731] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34721.485265] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[34721.485268] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34721.545330] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34721.555452] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34721.557933] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[34721.557936] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34721.616258] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34721.626495] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34721.629033] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[34721.629038] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34721.687109] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34721.697364] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34721.700015] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[34721.700017] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34721.758822] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34721.770842] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34721.773457] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[34721.834419] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34721.843665] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34721.845816] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[34721.951091] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34721.968416] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34721.971025] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[34721.971029] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[34721.971031] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[34721.971033] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[34722.698556] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[34722.713790] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[34722.714003] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[34722.719358] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[34722.720290] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[34722.720813] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[34722.819609] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[34723.077137] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[34723.192193] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[34723.237269] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[34723.283098] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[34723.327517] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[34723.373372] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[34724.697021] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[34724.697394] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[34724.698544] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[34724.699155] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=1
[34724.699693] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[34724.819640] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[34724.819643] elgato_gchd 1-2:1.0: analog mode now 480i
[34725.192245] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[34725.237254] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[34725.282236] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[34725.327578] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[34725.372560] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[34781.085217] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[34781.188844] elgato_gchd 1-2:1.0: transcoder defaults loaded
[34781.255657] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[34781.466797] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x2e mode=0xe
[34781.470288] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[34781.470295] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34781.669119] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x26 mode=0x6
[34781.679357] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34781.681856] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[34781.681860] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34781.737714] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34781.746462] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34781.748620] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[34781.748621] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[34781.748623] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[34781.748626] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[34782.531264] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[34782.658129] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x6f mode=0xf
[34782.662555] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xee
[34782.662568] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34782.768685] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x76 mode=0x6
[34782.785120] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x66 masked_mode=0x6
[34782.788036] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[34782.788043] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34782.868853] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x26 mode=0x6
[34782.881700] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34782.884434] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[34782.884438] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34782.973067] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34783.002246] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34783.005509] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[34783.005516] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34783.118183] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34783.150542] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34783.154025] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[34783.154027] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34783.230357] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34783.262643] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34783.267408] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[34783.267417] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34783.465330] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34783.485558] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34783.489312] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[34783.489318] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[34783.489323] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[34783.489326] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[34784.223484] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[34784.239070] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[34784.239291] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[34784.244659] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[34784.245215] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[34784.245594] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[34784.345010] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[34784.719052] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[34784.764577] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[34784.810016] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[34784.855007] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[34784.900741] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[34787.850189] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[34789.384709] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[34789.384998] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[34789.386622] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[34789.387140] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=1
[34789.387658] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[34789.508342] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[34789.508346] elgato_gchd 1-2:1.0: analog mode now 480i
[34789.881185] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[34789.926133] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[34789.971693] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[34790.015939] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[34790.062458] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[34893.914691] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[34893.982379] elgato_gchd 1-2:1.0: transcoder defaults loaded
[34894.045192] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[34894.192329] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1e mode=0xe
[34894.194566] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0x6e
[34894.194569] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34894.276644] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[34894.278762] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[34894.278764] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34894.350108] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[34894.352336] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[34894.352339] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34894.436512] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34894.445495] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34894.447703] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[34894.447707] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34894.504552] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34894.516860] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34894.519334] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[34894.519344] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[34894.519347] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[34894.519350] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[34895.297835] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[34895.381041] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x2f mode=0xf
[34895.383644] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xee
[34895.383649] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34895.443654] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x3e mode=0xe
[34895.445829] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[34895.445833] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34895.524311] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34895.533151] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34895.535253] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[34895.535255] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34895.591412] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34895.600537] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34895.602736] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[34895.602740] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34895.667840] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34895.677100] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34895.679358] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[34895.679361] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34895.767275] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34895.777811] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34895.780080] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[34895.780082] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[34895.843695] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34895.852211] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34895.857822] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[34895.918245] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34895.927086] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34895.929207] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[34895.986455] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[34895.994949] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[34895.997073] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[34895.997076] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[34895.997078] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[34895.997079] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[34896.734914] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[34896.750619] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[34896.750840] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[34896.756348] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[34896.756971] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[34896.757635] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[34896.856853] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[34897.231061] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[34897.275887] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[34897.320656] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[34897.366899] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[34897.410820] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[34917.232692] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[34918.694934] elgato_gchd 1-2:1.0: back-end not settled: status 0xf2
[34918.721848] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[34918.722195] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[34918.723303] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[34918.723764] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=1
[34918.724368] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[34918.844649] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[34918.844653] elgato_gchd 1-2:1.0: analog mode now 480i
[34919.217328] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[34919.262927] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[34919.307815] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[34919.352110] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[34919.396833] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
ff5@ff5-VirtualBox:~$ 
