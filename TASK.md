# TASK T-055 — Output sizes for analog sources

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** scaled outputs work (out2_test=5); final version 618a117; hardware test
- **Type:** driver feature
- **Branch:** `feat/output-size` (from `dev` 0.5.2)
- **TODO items:** T-055 (stage 1)

## Why

The capture application offers three output sizes for composite and
S-video; the driver always encodes the source size (720 × 480 / 576).
Recorded (`docs/composite.md §10`, recordings in `traces/composite/`:
`gchd_288p_output_switch_small`, `gchd_288p_standard_640x480_output_small`,
`gchd_288p_mobile_bitrate_change_small`):

| Output | Encoder fields |
|---|---|
| Standard | source size, `v_ip_format = 0`, GOP 12, transport output 1 |
| Standard 640 × 480 | `v_ip_format = 2`, GOP 6, level 30, transport output 2 |
| Mobile 480 × 360 | `v_ip_format = 2`, GOP 6, VBR, level 22, transport output 2 |

## Implementation (plan)

1. Analysis of the three recordings: complete encoder field sets per size
   (NTSC and PAL where recorded), the restart sequence, and the bulk-IN
   data of transport output 2 (PIDs, endpoint).
2. V4L2: `S_FMT` with 640 × 480 or 480 × 360 on composite/S-video selects
   the output size (encoder restart while capturing, as recorded);
   720 × source lines selects the standard output. `ENUM_FRAMESIZES` lists
   the three. `G_FMT` reports the selected output size. Kept while the
   device stays connected, like the input and the pixel format.
3. Docs: `video-output.md`, `composite.md §10`.

## Hardware test (maintainer)

Build and load; `git log -1 --oneline` must show 618a117 or later:

```sh
cd ~/elgato-gchd && git fetch origin && git checkout feat/output-size && git pull
git log -1 --oneline
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
```

Fresh plug-in, then (no parameter any more):

```sh
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo insmod ./elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
```

PS1 on composite, game running. Each size: set it, ffplay 15 s, close.

1. `v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG`,
   `ffplay -f mpegts /dev/video0`
2. `width=480,height=360`
3. `width=720,height=576` (standard; check that it still works)
4. 640 × 480 again, then the N64 on composite (60 Hz source) with the
   same ffplay.
5. Optional: S-video with 480 × 360.

Expected: pictures in all; ffplay shows 640x480 / 480x360 progressive,
720x576 interlaced. Paste the ffplay `Stream #0:0` lines and:

```sh
sudo dmesg | grep -E "switching|START (transition|failed)|state-change|SCMD timeout|completed 10|analog mode now|first data|stats|shutdown" | head -60
```

## Findings
- b5d4a79 test (PS1 composite): format and ENUM_FRAMESIZES correct
  (640×480 / 480×360, field None; source size 720×480 before the first
  detection in the session). With 640×480 and 480×360 ffplay never got a
  stream and could not be ended without unplugging; the standard size
  worked (720×576, 25 fps). (Correction: the dmesg was there; I overlooked
  it.) It shows START refused by the device with the output-2 block
  (`SCMD timeout ... readback=0x0400`, twice), then a completion wait that
  never ended (the hang). Windows enters output 2 only from a running
  capture (all recordings); 1b1b8d0 does the same and bounds the wait.
- 1b1b8d0 test: the standard START completed, the switch to 640×480 /
  480×360 ran as recorded, but that START never completed (state stays
  STOP, 0.65 s; Windows 0.38 s). The next standard START failed too, the
  third attempt hung; unplug needed. Windows sends nothing else between
  NULL and START. Remaining differences made selectable (c2574b9, `out2_test`
  bits): `v_vinpelclk` 1, output 1 kept on, output 2 enabled in the
  encoder setup after the firmware INIT (as in the initialization
  recording).
- c2574b9 `out2_test` runs: **5 works** (640×480 progressive 25 fps,
  data on endpoint 0x84); 6 corrupt (both endpoints mixed, but START
  completed); 0 START never completes; 4 and 1 failed earlier, at the
  firmware start-up (TODO T-067). Decisive: output 2 enabled in the
  encoder setup after the firmware INIT, as Windows does. 618a117 makes this
  the default, `v_vinpelclk` 1 as recorded, output 2 read from 0x84.

## Maintainer comments

Everything seemed to work. Look through the logs to see if something went wrong:
```console
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
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
  Stream #0:0[0x1011]: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, progressive), 640x480 [SAR 1:1 DAR 4:3], 25 fps, 25 tbr, 90k tbn
  Stream #0:1[0x10f]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
[mpegts @ 0x799890000c80] Packet corrupt (stream = 0, dts = 1283400).
[h264 @ 0x799890a1db80] error while decoding MB 20 28, bytestream -11
[h264 @ 0x799890a1db80] concealing 109 DC, 109 AC, 109 MV errors in B frame
[mpegts @ 0x799890000c80] PES packet size mismatchsq=    0B f=1/0   
[mpegts @ 0x799890000c80] Packet corrupt (stream = 1, dts = 1265400).
[mp2 @ 0x799890506e00] Header missingB vq=    0KB sq=    0B f=1/1   
   9.96 A-V: -0.033 fd=   2 aq=    0KB vq=    0KB sq=    0B f=1/1   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -v width=480,height=360,pixelformat=MPEG
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
  Duration: N/A, start: 1.200000, bitrate: N/A
  Program 1 
  Stream #0:0[0x1011]: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, progressive), 480x360 [SAR 1:1 DAR 4:3], 25 fps, 25 tbr, 90k tbn
  Stream #0:1[0x10f]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
[mpegts @ 0x7e8820000c80] Packet corrupt (stream = 0, dts = 1112400).
[h264 @ 0x7e8820387580] error while decoding MB 19 22, bytestream -8
[h264 @ 0x7e8820387580] concealing 60 DC, 60 AC, 60 MV errors in P frame
[mpegts @ 0x7e8820000c80] PES packet size mismatchsq=    0B f=1/1   
[mpegts @ 0x7e8820000c80] Packet corrupt (stream = 1, dts = 1030320).
[mp2 @ 0x7e882023b640] Header missingB vq=    0KB sq=    0B f=1/1   
  12.43 A-V: -0.000 fd=  11 aq=    0KB vq=  470KB sq=    0B f=1/1   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -v width=720,height=576,pixelformat=MPEG
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
[mpegts @ 0x7b3710000c80] Packet corrupt (stream = 0, dts = 534600).
[h264 @ 0x7b371023a740] error while decoding MB 40 15, bytestream -22
[mpegts @ 0x7b3710000c80] PES packet size mismatchsq=    0B f=1/0   
[mpegts @ 0x7b3710000c80] Packet corrupt (stream = 1, dts = 522360).
[mp2 @ 0x7b37102efc00] Header missingB vq=    0KB sq=    0B f=1/0   
[mpegts @ 0x7b3710000c80] Packet corrupt (stream = 0, dts = 3904200).
[mpegts @ 0x7b3710000c80] PES packet size mismatchsq=    0B f=2/1   
[mpegts @ 0x7b3710000c80] Packet corrupt (stream = 1, dts = 3885480).
[mp2 @ 0x7b37102efc00] Header missingB vq=    0KB sq=    0B f=2/2   
   7.03 A-V:  0.017 fd= 412 aq=    0KB vq=    0KB sq=    0B f=2/2   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
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
  Stream #0:0[0x1011]: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, progressive), 640x480 [SAR 1:1 DAR 4:3], 29.97 fps, 29.97 tbr, 90k tbn
  Stream #0:1[0x10f]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
[mpegts @ 0x73d2e4000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x73d2e4000c80] Packet corrupt (stream = 1, dts = 548280).
[mp2 @ 0x73d2e44f8980] Header missingB vq=  838KB sq=    0B f=0/0   
[h264 @ 0x73d2e42e6600] error while decoding MB 3 9, bytestream -10 
[h264 @ 0x73d2e42e6600] concealing 886 DC, 886 AC, 886 MV errors in P frame
   9.68 A-V: -0.013 fd=  21 aq=    8KB vq=  354KB sq=    0B f=1/1   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -v width=480,height=360,pixelformat=MPEG
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
  Duration: N/A, start: 1.200000, bitrate: N/A
  Program 1 
  Stream #0:0[0x1011]: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, progressive), 480x360 [SAR 1:1 DAR 4:3], 29.97 fps, 29.97 tbr, 90k tbn
  Stream #0:1[0x10f]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
[mpegts @ 0x7b6a70000c80] Packet corrupt (stream = 0, dts = 606498).
[mpegts @ 0x7b6a70000c80] PES packet size mismatch
[mpegts @ 0x7b6a70000c80] Packet corrupt (stream = 1, dts = 522720).
[mp2 @ 0x7b6a702524c0] Header missingB vq= 1496KB sq=    0B f=0/0   
[h264 @ 0x7b6a7021d600] error while decoding MB 20 13, bytestream -9
[h264 @ 0x7b6a7021d600] concealing 329 DC, 329 AC, 329 MV errors in B frame
   9.17 A-V:  0.015 fd=  60 aq=   38KB vq= 1284KB sq=    0B f=1/1   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 3
Video input set to 3 (S-Video: Camera, no signal)
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -v width=480,height=360,pixelformat=MPEG
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
[mpegts @ 0x7fda18000c80] Packet corrupt (stream = 0, dts = 480372).
Input #0, mpegts, from '/dev/video0':B vq=    0KB sq=    0B f=0/0   
  Duration: N/A, start: 1.200000, bitrate: N/A
  Program 1 
  Stream #0:0[0x1011]: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, progressive), 480x360 [SAR 1:1 DAR 4:3], 29.97 fps, 29.97 tbr, 90k tbn
  Stream #0:1[0x10f]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
[mpegts @ 0x7fda18000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x7fda18000c80] Packet corrupt (stream = 1, dts = 483840).
[mp2 @ 0x7fda18218540] Header missingB vq= 1329KB sq=    0B f=0/0   
  15.41 A-V: -0.008 fd=  29 aq=   40KB vq= 1257KB sq=    0B f=1/1   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 
```

dmesg:
```console
ff5@ff5-VirtualBox:~$ sudo dmesg | grep -E "switching|START (transition|failed)|state-change|SCMD timeout|completed 10|analog mode now|first data|stats|shutdown" | head -60
[60225.464100] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[60225.723098] elgato_gchd 1-2:1.0: switching output size: 0 -> 1
[60227.495434] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[60227.495437] elgato_gchd 1-2:1.0: analog mode now 576i, output 1
[60227.892789] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[60232.410025] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=93 bytes=6094848 ep84_bytes=6094848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=11
[60237.458034] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=196 bytes=12845056 ep84_bytes=12845056 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=49
[60242.505032] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=299 bytes=19595264 ep84_bytes=19595264 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=49
[60248.402413] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[60248.402416] elgato_gchd 1-2:1.0: analog mode now 576i, output 1
[60248.802218] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[60253.268994] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=92 bytes=6029312 ep84_bytes=6029312 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[60258.269918] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=194 bytes=12713984 ep84_bytes=12713984 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[60259.795293] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[60300.450199] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[60300.707923] elgato_gchd 1-2:1.0: switching output size: 0 -> 2
[60302.484313] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[60302.484316] elgato_gchd 1-2:1.0: analog mode now 576i, output 2
[60302.877058] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[60307.411179] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=55 bytes=3604480 ep84_bytes=3604480 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[60312.441277] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=116 bytes=7602176 ep84_bytes=7602176 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=37
[60320.645569] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[60320.645571] elgato_gchd 1-2:1.0: analog mode now 576i, output 2
[60321.119147] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[60325.548079] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=44 bytes=2883584 ep84_bytes=2883584 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[60330.610013] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[60335.025867] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[60356.611568] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[60361.517501] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=22
[60368.956003] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[60368.956006] elgato_gchd 1-2:1.0: analog mode now 576i, output 0
[60373.821578] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[60378.850707] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[60383.876089] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[60388.901275] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=448 bytes=29360128 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[60393.933169] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=563 bytes=36896768 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[60398.971221] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=678 bytes=44433408 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[60404.008646] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=793 bytes=51970048 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[60409.047533] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=908 bytes=59506688 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[60413.462623] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[60413.462627] elgato_gchd 1-2:1.0: analog mode now 240p, output 0
[60418.385114] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[60421.583181] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[60434.178619] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[60434.437293] elgato_gchd 1-2:1.0: switching output size: 0 -> 1
[60436.213178] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[60436.213180] elgato_gchd 1-2:1.0: analog mode now 480i, output 1
[60436.587921] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[60441.153811] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=94 bytes=6160384 ep84_bytes=6160384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=11
[60445.083051] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[60445.083055] elgato_gchd 1-2:1.0: analog mode now 240p, output 1
[60445.459322] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[60450.024367] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=94 bytes=6160384 ep84_bytes=6160384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[60455.069209] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=197 bytes=12910592 ep84_bytes=12910592 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[60456.423542] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[60467.562277] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[60467.820220] elgato_gchd 1-2:1.0: switching output size: 0 -> 2
[60469.511584] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[60469.511587] elgato_gchd 1-2:1.0: analog mode now 240p, output 2
[60469.868459] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
ff5@ff5-VirtualBox:~$ 
```
