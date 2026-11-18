# TASK T-055 — Output sizes for analog sources

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** implemented (b5d4a79); hardware test
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

Build and load; `git log -1 --oneline` must show b5d4a79 or later:

```sh
cd ~/elgato-gchd && git fetch origin && git checkout feat/output-size && git pull
git log -1 --oneline
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
sudo insmod ./elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
```

PS1 on composite, game running. For each size, set it, check the report,
play 15 s, close:

```sh
v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
v4l2-ctl -d /dev/video0 --get-fmt-video --list-framesizes=MPEG
ffplay -f mpegts /dev/video0
```

1. 640 × 480 (commands above).
2. 480 × 360: `-v width=480,height=360,pixelformat=MPEG`.
3. Standard again: `-v width=720,height=576,pixelformat=MPEG`.

Expected: ffplay's stream line shows 640x480 / 480x360 / 720x576 with a
picture each time. If the small sizes give no picture, the dmesg stats line
(`ep84_bytes=`) shows whether the data arrives on endpoint 0x84. Paste the
ffplay stream lines (the `Stream #0:0` line) and:

```sh
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|SCMD write succeeded|SCMD interrupt|callbacks suppressed|bulk-IN first reads|no analog family|pre-config raw_status|retrying detection"
```

## Findings

## Maintainer comments

640x480 and 480x360 didn't manage to open the ffplay window and I had to unplug the device to force the program to exit:

```console
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 2
Cannot open device /dev/video0, exiting.
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
v4l2-ctl -d /dev/video0 --get-fmt-video --list-framesizes=MPEG
ffplay -f mpegts /dev/video0
Format Video Capture:
	Width/Height      : 640/480
	Pixel Format      : 'MPEG' (MPEG-1/2/4)
	Field             : None
	Bytes per Line    : 0
	Size Image        : 65424
	Colorspace        : Rec. 709
	Transfer Function : Default (maps to Rec. 709)
	YCbCr/HSV Encoding: Default (maps to Rec. 709)
	Quantization      : Default (maps to Full Range)
	Flags             : 
ioctl: VIDIOC_ENUM_FRAMESIZES
	Size: Discrete 720x480
	Size: Discrete 640x480
	Size: Discrete 480x360
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
^Cff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -v width=480,height=360,pixelformat=MPEG
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 --get-fmt-video --list-framesizes=MPEG
Format Video Capture:
	Width/Height      : 480/360
	Pixel Format      : 'MPEG' (MPEG-1/2/4)
	Field             : None
	Bytes per Line    : 0
	Size Image        : 65424
	Colorspace        : Rec. 709
	Transfer Function : Default (maps to Rec. 709)
	YCbCr/HSV Encoding: Default (maps to Rec. 709)
	Quantization      : Default (maps to Full Range)
	Flags             : 
ioctl: VIDIOC_ENUM_FRAMESIZES
	Size: Discrete 720x480
	Size: Discrete 640x480
	Size: Discrete 480x360
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
^Cff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -v width=720,height=576,pixelformat=MPEG
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 --get-fmt-video --list-framesizes=MPEG
ffplay -f mpegts /dev/video0
Format Video Capture:
	Width/Height      : 720/480
	Pixel Format      : 'MPEG' (MPEG-1/2/4)
	Field             : Interlaced Top-Bottom
	Bytes per Line    : 0
	Size Image        : 65424
	Colorspace        : Rec. 709
	Transfer Function : Default (maps to Rec. 709)
	YCbCr/HSV Encoding: Default (maps to Rec. 709)
	Quantization      : Default (maps to Full Range)
	Flags             : 
ioctl: VIDIOC_ENUM_FRAMESIZES
	Size: Discrete 720x480
	Size: Discrete 640x480
	Size: Discrete 480x360
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
   7.11 A-V: -0.037 fd=   0 aq=   40KB vq= 1398KB sq=    0B f=0/0   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 
```

dmesg:
```console
ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|SCMD write succeeded|SCMD interrupt|callbacks suppressed|bulk-IN first reads|no analog family|pre-config raw_status|retrying detection"
[57588.291403] usb 1-2: new high-speed USB device number 57 using xhci_hcd
[57588.681151] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[57588.681168] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[57588.681173] usb 1-2: Product: GameCapture HD
[57588.681177] usb 1-2: Manufacturer: Elgato
[57588.681180] usb 1-2: SerialNumber: 04107F1267
[57588.686163] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[57588.687389] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[57588.705290] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=27316 comm=v4l_id
[57588.706731] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1379 comm=wireplumber
[57588.706886] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1379 comm=wireplumber
[57588.707188] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57588.707198] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57588.707759] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57588.707788] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57588.707795] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57588.707802] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57588.708009] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57588.708031] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57589.040597] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=27318 comm=v4l2-ctl
[57589.040691] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[57589.040701] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[57589.040705] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[57597.245715] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=27319 comm=v4l2-ctl
[57597.251760] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=27320 comm=v4l2-ctl
[57597.381109] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=27334 comm=read_thread
[57597.381169] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[57597.508519] elgato_gchd 1-2:1.0: transcoder defaults loaded
[57597.576641] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[57597.969965] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[57597.971839] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[57597.971841] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[57597.971842] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[57597.971844] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[57598.740807] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[57598.956868] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[57598.972504] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[57598.972842] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[57598.978360] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[57598.979277] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[57598.979715] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[57599.029399] elgato_gchd 1-2:1.0: SCMD timeout: command=0x05 mode=0x00 data=0x0002 readback=0x0400 polls=500
[57599.029412] elgato_gchd 1-2:1.0: capture start transition failed: -110
[57600.142695] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[57600.144806] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[57600.144809] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[57600.144811] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[57600.144813] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[57600.357085] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[57600.376886] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[57600.377240] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[57600.382990] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[57600.383744] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[57600.384162] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[57600.430349] elgato_gchd 1-2:1.0: SCMD timeout: command=0x05 mode=0x00 data=0x0002 readback=0x0400 polls=500
[57600.430357] elgato_gchd 1-2:1.0: capture start transition failed: -110
[57601.497208] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[57601.499591] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[57601.499594] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[57601.499596] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[57601.499598] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[57601.709564] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[57601.724739] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[57601.724938] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[57601.730500] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[57601.731244] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[57601.732151] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[57606.763421] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=0 bytes=0 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=1 ring_dropped=0 frames_waiting=0
[57611.834631] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=0 bytes=0 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=1 ring_dropped=0 frames_waiting=0
[57616.928576] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=0 bytes=0 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=1 ring_dropped=0 frames_waiting=0
[57622.001688] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=0 bytes=0 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=1 ring_dropped=0 frames_waiting=0
[57623.666826] elgato_gchd 1-2:1.0: capture start transition failed: -32
[57623.674722] elgato_gchd 1-2:1.0: capture start failed: -32; recovery to stopped state failed: -32
[57623.674907] elgato_gchd 1-2:1.0: capture stop on last close failed: -32
[57623.675253] elgato_gchd 1-2:1.0: hardware shutdown failed at step 'read state': -32 (state 0xffff before, 0xffff now)
[57623.675257] elgato_gchd 1-2:1.0: hardware shutdown on last close failed: -32; keeping initialized state
[57623.676625] usb 1-2: USB disconnect, device number 57
[57632.288048] usb 1-2: new high-speed USB device number 58 using xhci_hcd
[57632.679242] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[57632.679250] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[57632.679252] usb 1-2: Product: GameCapture HD
[57632.679253] usb 1-2: Manufacturer: Elgato
[57632.679255] usb 1-2: SerialNumber: 04107F1267
[57632.683093] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[57632.686271] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[57632.703290] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=27345 comm=v4l_id
[57632.705275] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1379 comm=wireplumber
[57632.705813] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1379 comm=wireplumber
[57632.706201] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57632.706214] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57632.707482] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57632.707519] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57632.707528] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57632.707562] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57632.707866] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57632.707900] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57637.497784] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=27347 comm=v4l2-ctl
[57637.497906] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[57637.497920] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[57637.497926] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[57650.271766] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=27348 comm=v4l2-ctl
[57656.377196] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=27349 comm=v4l2-ctl
[57665.828182] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=27363 comm=read_thread
[57665.828290] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[57665.969511] elgato_gchd 1-2:1.0: transcoder defaults loaded
[57666.040256] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[57666.436552] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[57666.440003] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[57666.440006] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[57666.440009] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[57666.440010] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[57667.248833] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[57667.477192] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[57667.497439] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[57667.497755] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[57667.503649] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[57667.504223] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[57667.505081] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[57667.569007] elgato_gchd 1-2:1.0: SCMD timeout: command=0x05 mode=0x00 data=0x0002 readback=0x0400 polls=500
[57667.569016] elgato_gchd 1-2:1.0: capture start transition failed: -110
[57668.763107] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[57668.765738] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[57668.765740] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[57668.765743] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[57668.765744] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[57669.014010] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[57669.032741] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[57669.033096] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[57669.038815] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[57669.039335] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[57669.039872] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[57669.103075] elgato_gchd 1-2:1.0: SCMD timeout: command=0x05 mode=0x00 data=0x0002 readback=0x0400 polls=500
[57669.103084] elgato_gchd 1-2:1.0: capture start transition failed: -110
[57670.174574] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[57670.176886] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[57670.176890] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[57670.176892] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[57670.176895] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[57670.411106] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[57670.428938] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[57670.429180] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[57670.436085] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[57670.436662] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[57670.437158] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[57675.450078] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=0 bytes=0 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=1 ring_dropped=0 frames_waiting=0
[57680.449715] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=0 bytes=0 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=1 ring_dropped=0 frames_waiting=0
[57685.513584] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=0 bytes=0 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=1 ring_dropped=0 frames_waiting=0
[57690.528572] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=0 bytes=0 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=1 ring_dropped=0 frames_waiting=0
[57690.871794] usb 1-2: USB disconnect, device number 58
[57690.872696] elgato_gchd 1-2:1.0: capture start transition failed: -108
[57690.873394] elgato_gchd 1-2:1.0: capture start failed: -108; recovery to stopped state failed: -19
[57690.873437] elgato_gchd 1-2:1.0: capture stop on last close failed: -19
[57690.873443] elgato_gchd 1-2:1.0: hardware shutdown failed at step 'read state': -19 (state 0xffff before, 0xffff now)
[57690.873448] elgato_gchd 1-2:1.0: hardware shutdown on last close failed: -19; keeping initialized state
[57695.904197] usb 1-2: new high-speed USB device number 59 using xhci_hcd
[57696.286555] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[57696.286564] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[57696.286567] usb 1-2: Product: GameCapture HD
[57696.286569] usb 1-2: Manufacturer: Elgato
[57696.286570] usb 1-2: SerialNumber: 04107F1267
[57696.288164] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[57696.292889] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[57696.308857] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=27373 comm=v4l_id
[57696.310220] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1379 comm=wireplumber
[57696.310651] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1379 comm=wireplumber
[57696.311088] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57696.311100] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57696.311505] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57696.311531] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57696.311540] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57696.311546] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57696.311945] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57696.311967] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[57701.121013] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=27375 comm=v4l2-ctl
[57701.121058] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[57701.121062] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[57701.121064] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[57721.242853] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=27376 comm=v4l2-ctl
[57730.218372] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=27377 comm=v4l2-ctl
[57730.343144] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=27391 comm=read_thread
[57730.343223] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[57730.519103] elgato_gchd 1-2:1.0: transcoder defaults loaded
[57730.581735] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[57730.999790] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[57731.002047] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[57731.002050] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[57731.002052] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[57731.002054] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[57731.776722] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[57731.999329] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[57732.016645] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[57732.017103] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[57732.024452] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[57732.025078] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[57732.025731] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[57732.162657] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[57737.027577] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=21
[57742.050553] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[57743.779761] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
ff5@ff5-VirtualBox:~$ 
```
