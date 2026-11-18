# TASK T-055 — Output sizes for analog sources

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** output-2 START does not complete; test variants (c2574b9)
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

Build and load; `git log -1 --oneline` must show c2574b9 or later:

```sh
cd ~/elgato-gchd && git fetch origin && git checkout feat/output-size && git pull
git log -1 --oneline
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
```

A failed output-2 START leaves the device unusable until it is unplugged,
so each run starts with a fresh plug-in. Run with `out2_test` = **4**,
**5**, **6**, **1**, **0** in this order and stop at the first that shows a
picture:

```sh
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
# unplug and replug the device, then ONE of these lines, in this order
# for the successive runs:
sudo insmod ./elgato_gchd.ko out2_test=4
sudo insmod ./elgato_gchd.ko out2_test=5
sudo insmod ./elgato_gchd.ko out2_test=6
sudo insmod ./elgato_gchd.ko out2_test=1
sudo insmod ./elgato_gchd.ko out2_test=0
# then:
v4l2-ctl -d /dev/video0 -i 2
v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
ffplay -f mpegts /dev/video0
```

PS1 on composite, game running. Wait 15 s. Note whether a picture comes,
then Ctrl+C (if ffplay hangs, unplug). After each run:

```sh
sudo dmesg | grep -E "out2_test|switching|START (transition|failed)|state-change|SCMD timeout|completed 10|analog mode now|ep84|first data|stats" | head -40
```

Paste per run: N, picture yes/no, the ffplay `Stream #0:0` line (if any)
and the dmesg lines.

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

## Maintainer comments

N=4 (immediate exit):
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
[mpegts @ 0x7d653c000c80] Could not detect TS packet size, defaulting to non-FEC/DVHS
Input #0, mpegts, from '/dev/video0':B vq=    0KB sq=    0B f=0/0   
  Duration: N/A, bitrate: N/A
Failed to open file '/dev/video0' or configure filtergraph
    nan    :  0.000 fd=   0 aq=    0KB vq=    0KB sq=    0B f=0/0   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 

ff5@ff5-VirtualBox:~$ sudo dmesg | grep -E "out2_test|switching|START (transition|failed)|state-change|SCMD timeout|completed 10|analog mode now|ep84|first data|stats" | head -40
[59070.011983] elgato_gchd: `N' invalid for parameter `out2_test'
[59086.808939] elgato_gchd: `N' invalid for parameter `out2_test'
[59217.713517] elgato_gchd 1-2:1.0: state-change timeout: current=0x10 expected=0x11 state=0x0011 completion=0x0008
[59218.376022] elgato_gchd 1-2:1.0: state-change timeout: current=0x11 expected=0x10 state=0x0011 completion=0x0008
[59219.051409] elgato_gchd 1-2:1.0: state-change timeout: current=0x11 expected=0x10 state=0x0011 completion=0x0008
[59219.707986] elgato_gchd 1-2:1.0: state-change timeout: current=0x11 expected=0x10 state=0x0011 completion=0x0008
ff5@ff5-VirtualBox:~$ 
```

N=5 (good picture):
```console
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ sudo insmod ./elgato_gchd.ko out2_test=5
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 2
v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
ffplay -f mpegts /dev/video0
Video input set to 2 (Composite: Camera, no signal)
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
  14.48 A-V: -0.008 fd=   0 aq=   40KB vq= 1376KB sq=    0B f=0/0   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 

ff5@ff5-VirtualBox:~$ sudo dmesg | grep -E "out2_test|switching|START (transition|failed)|state-change|SCMD timeout|completed 10|analog mode now|ep84|first data|stats" | head -40
[59445.125540] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[59445.388100] elgato_gchd 1-2:1.0: switching output size: 0 -> 1
[59447.019319] elgato_gchd 1-2:1.0: encoder: transport output 2 (640x480), out2_test=5
[59447.166365] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[59447.166367] elgato_gchd 1-2:1.0: analog mode now 576i, output 1
[59447.564634] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[59452.033479] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=92 bytes=6029312 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=10
[59457.079894] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=195 bytes=12779520 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=49
[59462.127186] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=298 bytes=19529728 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=49
ff5@ff5-VirtualBox:~$ 
```

N=6 (corrupted picture):
```console
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 2
v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
ffplay -f mpegts /dev/video0
Video input set to 2 (Composite: Camera, no signal)
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
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 41400). 
[extract_extradata @ 0x7b0d64178200] Invalid NAL unit 0, skipping.  
[h264 @ 0x7b0d64006540] Invalid NAL unit 0, skipping.
[h264 @ 0x7b0d64006540] error while decoding MB 23 20, bytestream -26
[h264 @ 0x7b0d64006540] concealing 426 DC, 426 AC, 426 MV errors in I frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 48600).
[mpegts @ 0x7b0d64000c80] DTS 45000 < 48600 out of order 0B f=0/0   
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 52200). 
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 55800). 
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 63000). 
[mpegts @ 0x7b0d64000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 47160).
[mpegts @ 0x7b0d64000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 51480).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 70200).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 55800). 
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 73800).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 60120). 
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 77400).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 64440). 
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 84600)    nan    :  0.000 fd=   0 aq=[mpegts @ 0x7b0d64000c80] .B f=0/0   
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 68760). 
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 84600). 
[mpegts @ 0x7b0d64000c80] PES packet size mismatch
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 73080).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 91800).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 91800). 
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 77400).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 95400).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 81720). 
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 99000).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 86040). 
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 106200).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 90360). 
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 106200).
[mpegts @ 0x7b0d64000c80] PES packet size mismatch
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 94680).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 113400).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 99000). 
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 117000).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 103320).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 120600).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 107640).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 127800).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 111960).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 127800).
[mpegts @ 0x7b0d64000c80] PES packet size mismatch
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 118440).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 135000).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 122760).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 138600).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 142200).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 127080).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 149400).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 131400).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 149400).
[mpegts @ 0x7b0d64000c80] PES packet size mismatch
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 135720).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 153000).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 140040).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 156600).
[mpegts @ 0x7b0d64000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 140040).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 156600).
[mpegts @ 0x7b0d64000c80] PES packet size mismatch
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 144360).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 163800).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 148680).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 167400).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 153000).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 171000).
[mpegts @ 0x7b0d64000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 157320).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 174600).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 178200).
[mpegts @ 0x7b0d64000c80] PES packet size mismatch
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 161640).
[mpegts @ 0x7b0d64000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 165960).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 185400).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 170280).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 189000).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 174600).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 192600).
[mpegts @ 0x7b0d64000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 178920).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 196200).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 199800).
[mpegts @ 0x7b0d64000c80] PES packet size mismatch
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 183240).
[mpegts @ 0x7b0d64000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 187560).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 207000).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 191880).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 210600).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 196200).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 214200).
Input #0, mpegts, from '/dev/video0':B vq=    0KB sq=    0B f=0/0   
  Duration: N/A, start: 0.500000, bitrate: N/A
  Program 1 
  Stream #0:0[0x1011]: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, progressive), 640x480 [SAR 1:1 DAR 4:3], 25 fps, 25 tbr, 90k tbn
  Stream #0:1[0x10f]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
[h264 @ 0x7b0d64161500] Invalid NAL unit 0, skipping.
[mp2 @ 0x7b0d642d7b40] Header missing
    Last message repeated 1 times
[h264 @ 0x7b0d64161500] error while decoding MB 23 20, bytestream -26
[h264 @ 0x7b0d64161500] concealing 426 DC, 426 AC, 426 MV errors in I frame
[h264 @ 0x7b0d642563c0] concealing 664 DC, 664 AC, 664 MV errors in P frame
[h264 @ 0x7b0d64757c40] cabac decode of qscale diff failed at 21 20
[h264 @ 0x7b0d64757c40] error while decoding MB 21 20, bytestream 24207
[h264 @ 0x7b0d64757c40] concealing 428 DC, 428 AC, 428 MV errors in I frame
[h264 @ 0x7b0d64757c40] illegal short term buffer state detected
[h264 @ 0x7b0d64161500] cabac decode of qscale diff failed at 8 10
[h264 @ 0x7b0d64161500] error while decoding MB 8 10, bytestream 9665
[h264 @ 0x7b0d64161500] concealing 841 DC, 841 AC, 841 MV errors in P frame
[h264 @ 0x7b0d640e9040] concealing 336 DC, 336 AC, 336 MV errors in B frame
[h264 @ 0x7b0d64757c40] left block unavailable for requested intra mode
[h264 @ 0x7b0d64757c40] error while decoding MB 0 25, bytestream 5138
[h264 @ 0x7b0d64757c40] concealing 249 DC, 249 AC, 249 MV errors in B frame
[h264 @ 0x7b0d64161500] illegal short term buffer state detected
[h264 @ 0x7b0d64757c40] cabac decode of qscale diff failed at 8 3
[h264 @ 0x7b0d64757c40] error while decoding MB 8 3, bytestream 13481
[h264 @ 0x7b0d64757c40] concealing 1121 DC, 1121 AC, 1121 MV errors in I frame
[h264 @ 0x7b0d642563c0] concealing 295 DC, 295 AC, 295 MV errors in P frame
[h264 @ 0x7b0d642b1d00] cabac decode of qscale diff failed at 18 23
[h264 @ 0x7b0d642b1d00] error while decoding MB 18 23, bytestream 12708
[h264 @ 0x7b0d642b1d00] concealing 311 DC, 311 AC, 311 MV errors in P frame
[h264 @ 0x7b0d640e9040] co located POCs unavailableq=    0B f=2/2   
[h264 @ 0x7b0d642563c0] left block unavailable for requested intra mode
[h264 @ 0x7b0d642563c0] error while decoding MB 0 5, bytestream 133474
[h264 @ 0x7b0d642563c0] concealing 1049 DC, 1049 AC, 1049 MV errors in I frame
[h264 @ 0x7b0d64161500] concealing 874 DC, 874 AC, 874 MV errors in P frame
[h264 @ 0x7b0d640e9040] cabac decode of qscale diff failed at 36 7  
[h264 @ 0x7b0d640e9040] error while decoding MB 36 7, bytestream 50804
[h264 @ 0x7b0d640e9040] concealing 933 DC, 933 AC, 933 MV errors in P frame
[mp2 @ 0x7b0d642d7b40] Header missingB vq= 1655KB sq=    0B f=5/4   
[h264 @ 0x7b0d642563c0] illegal short term buffer state detected4   
[h264 @ 0x7b0d642563c0] cabac decode of qscale diff failed at 22 13
[h264 @ 0x7b0d642563c0] error while decoding MB 22 13, bytestream 48001
[h264 @ 0x7b0d642563c0] concealing 707 DC, 707 AC, 707 MV errors in B frame
[h264 @ 0x7b0d642b1d00] concealing 647 DC, 647 AC, 647 MV errors in B frame
[h264 @ 0x7b0d64161500] concealing 348 DC, 348 AC, 348 MV errors in P frame
[h264 @ 0x7b0d64757c40] concealing 438 DC, 438 AC, 438 MV errors in P frame
[h264 @ 0x7b0d640e9040] illegal short term buffer state detected6   
[h264 @ 0x7b0d642563c0] left block unavailable for requested intra mode
[h264 @ 0x7b0d642563c0] error while decoding MB 0 3, bytestream 23028
[h264 @ 0x7b0d642563c0] concealing 1129 DC, 1129 AC, 1129 MV errors in I frame
[h264 @ 0x7b0d642b1d00] co located POCs unavailableq=    0B f=8/7   
[h264 @ 0x7b0d64161500] concealing 991 DC, 991 AC, 991 MV errors in I frame
[h264 @ 0x7b0d640e9040] cabac decode of qscale diff failed at 24 10 
[h264 @ 0x7b0d640e9040] error while decoding MB 24 10, bytestream 11653
[h264 @ 0x7b0d640e9040] concealing 825 DC, 825 AC, 825 MV errors in P frame
[h264 @ 0x7b0d642b1d00] concealing 482 DC, 482 AC, 482 MV errors in P frame
[mp2 @ 0x7b0d642d7b40] Header missing
[h264 @ 0x7b0d64161500] illegal short term buffer state detected/9   
[h264 @ 0x7b0d64161500] cabac decode of qscale diff failed at 4 15
[h264 @ 0x7b0d64161500] error while decoding MB 4 15, bytestream 46012
[h264 @ 0x7b0d64161500] concealing 645 DC, 645 AC, 645 MV errors in B frame
[h264 @ 0x7b0d64757c40] concealing 335 DC, 335 AC, 335 MV errors in B frame
[h264 @ 0x7b0d640e9040] concealing 242 DC, 242 AC, 242 MV errors in P frame
[h264 @ 0x7b0d642563c0] concealing 388 DC, 388 AC, 388 MV errors in P frame
[h264 @ 0x7b0d642b1d00] illegal short term buffer state detected/11   
[h264 @ 0x7b0d64161500] left block unavailable for requested intra mode
[h264 @ 0x7b0d64161500] error while decoding MB 0 7, bytestream 15587
[h264 @ 0x7b0d64161500] concealing 969 DC, 969 AC, 969 MV errors in I frame
[h264 @ 0x7b0d64757c40] co located POCs unavailableq=    0B f=14/12   
[h264 @ 0x7b0d640e9040] left block unavailable for requested intra mode
[h264 @ 0x7b0d640e9040] error while decoding MB 0 5, bytestream 126279
[h264 @ 0x7b0d640e9040] concealing 1049 DC, 1049 AC, 1049 MV errors in I frame
[h264 @ 0x7b0d642b1d00] concealing 644 DC, 644 AC, 644 MV errors in P frame
[h264 @ 0x7b0d64757c40] cabac decode of qscale diff failed at 39 14   
[h264 @ 0x7b0d64757c40] error while decoding MB 39 14, bytestream 33939
[h264 @ 0x7b0d64757c40] concealing 650 DC, 650 AC, 650 MV errors in P frame
[h264 @ 0x7b0d640e9040] illegal short term buffer state detected/14   
[h264 @ 0x7b0d640e9040] concealing 100 DC, 100 AC, 100 MV errors in B frame
[mp2 @ 0x7b0d642d7b40] Header missingB vq= 1647KB sq=    0B f=17/15   
[h264 @ 0x7b0d642563c0] concealing 395 DC, 395 AC, 395 MV errors in B frame
[mp2 @ 0x7b0d642d7b40] Header missingB vq= 1739KB sq=    0B f=17/15   
[h264 @ 0x7b0d642b1d00] cabac decode of qscale diff failed at 23 26   
[h264 @ 0x7b0d642b1d00] error while decoding MB 23 26, bytestream 6486
[h264 @ 0x7b0d642b1d00] concealing 186 DC, 186 AC, 186 MV errors in P frame
[h264 @ 0x7b0d64161500] cabac decode of qscale diff failed at 3 22
[h264 @ 0x7b0d64161500] error while decoding MB 3 22, bytestream 9587
[h264 @ 0x7b0d64161500] concealing 366 DC, 366 AC, 366 MV errors in P frame
[h264 @ 0x7b0d64757c40] illegal short term buffer state detected/16   
[h264 @ 0x7b0d640e9040] cabac decode of qscale diff failed at 23 77   
[h264 @ 0x7b0d640e9040] error while decoding MB 23 7, bytestream 11381
[h264 @ 0x7b0d640e9040] concealing 946 DC, 946 AC, 946 MV errors in I frame
[h264 @ 0x7b0d642563c0] co located POCs unavailableq=    0B f=20/17   
[h264 @ 0x7b0d642b1d00] concealing 901 DC, 901 AC, 901 MV errors in I frame
[h264 @ 0x7b0d64757c40] concealing 625 DC, 625 AC, 625 MV errors in P frame
[h264 @ 0x7b0d642563c0] cabac decode of qscale diff failed at 18 13   
[h264 @ 0x7b0d642563c0] error while decoding MB 18 13, bytestream 30021
[h264 @ 0x7b0d642563c0] concealing 711 DC, 711 AC, 711 MV errors in P frame
[mp2 @ 0x7b0d642d7b40] Header missingB vq= 1620KB sq=    0B f=23/19   
[h264 @ 0x7b0d642b1d00] illegal short term buffer state detected
[h264 @ 0x7b0d64161500] concealing 269 DC, 269 AC, 269 MV errors in B frame
[h264 @ 0x7b0d640e9040] error while decoding MB 14 28, bytestream -6  
[h264 @ 0x7b0d640e9040] concealing 115 DC, 115 AC, 115 MV errors in P frame
[mp2 @ 0x7b0d642d7b40] Header missing
    Last message repeated 1 times
[h264 @ 0x7b0d642563c0] illegal short term buffer state detected/21   
[mpegts @ 0x7b0d64000c80] PES packet size mismatchsq=    0B f=25/22   
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 200520).
[h264 @ 0x7b0d642b1d00] cabac decode of qscale diff failed at 27 13   
[h264 @ 0x7b0d642b1d00] error while decoding MB 27 13, bytestream 2314
[h264 @ 0x7b0d642b1d00] concealing 702 DC, 702 AC, 702 MV errors in I frame
[h264 @ 0x7b0d64161500] co located POCs unavailableq=    0B f=26/22   
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 217800).
[h264 @ 0x7b0d64757c40] left block unavailable for requested intra4x4 mode -1
[h264 @ 0x7b0d64757c40] error while decoding MB 0 12, bytestream 50411
[h264 @ 0x7b0d64757c40] concealing 769 DC, 769 AC, 769 MV errors in I frame
[h264 @ 0x7b0d642563c0] concealing 898 DC, 898 AC, 898 MV errors in B frame
[h264 @ 0x7b0d640e9040] concealing 718 DC, 718 AC, 718 MV errors in B frame
[h264 @ 0x7b0d642b1d00] cabac decode of qscale diff failed at 32 18
[h264 @ 0x7b0d642b1d00] error while decoding MB 32 18, bytestream 22415
[h264 @ 0x7b0d642b1d00] Cannot use next picture in error concealment
[h264 @ 0x7b0d642b1d00] concealing 497 DC, 497 AC, 497 MV errors in P frame
[mp2 @ 0x7b0d642d7b40] Header missingB vq= 1684KB sq=    0B f=28/24   
[h264 @ 0x7b0d64161500] concealing 180 DC, 180 AC, 180 MV errors in P frame
[mpegts @ 0x7b0d64000c80] PES packet size mismatch
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 204840).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 221400).
[mpegts @ 0x7b0d64000c80] PES packet size mismatch
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 204840).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 221400).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 209160).  
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 228600).
[h264 @ 0x7b0d64757c40] illegal short term buffer state detected/24   
[mp2 @ 0x7b0d642d7b40] Header missing
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 213480).  
[h264 @ 0x7b0d640e9040] cabac decode of qscale diff failed at 3 525   
[h264 @ 0x7b0d640e9040] error while decoding MB 3 5, bytestream 23645
[h264 @ 0x7b0d640e9040] concealing 1046 DC, 1046 AC, 1046 MV errors in P frame
[mp2 @ 0x7b0d642d7b40] Header missing
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 232200).
[h264 @ 0x7b0d642b1d00] concealing 917 DC, 917 AC, 917 MV errors in P frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 217800).
[h264 @ 0x7b0d64161500] illegal short term buffer state detected/26   
[h264 @ 0x7b0d64161500] cabac decode of qscale diff failed at 9 10
[h264 @ 0x7b0d64161500] error while decoding MB 9 10, bytestream 55631
[h264 @ 0x7b0d64161500] concealing 840 DC, 840 AC, 840 MV errors in B frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 235800).
[h264 @ 0x7b0d64757c40] concealing 839 DC, 839 AC, 839 MV errors in B frame
[h264 @ 0x7b0d640e9040] left block unavailable for requested intra4x4 mode -1
[h264 @ 0x7b0d640e9040] error while decoding MB 0 15, bytestream 23307
[h264 @ 0x7b0d640e9040] concealing 649 DC, 649 AC, 649 MV errors in I frame
[mpegts @ 0x7b0d64000c80] PES packet size mismatch
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 224280).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 243000).
[h264 @ 0x7b0d642563c0] left block unavailable for requested intra mode
[h264 @ 0x7b0d642563c0] error while decoding MB 0 15, bytestream 42379
[h264 @ 0x7b0d642563c0] concealing 649 DC, 649 AC, 649 MV errors in I frame
[h264 @ 0x7b0d642b1d00] concealing 533 DC, 533 AC, 533 MV errors in B frame
[h264 @ 0x7b0d64161500] cabac decode of qscale diff failed at 38 15
[h264 @ 0x7b0d64161500] error while decoding MB 38 15, bytestream 12826
[h264 @ 0x7b0d64161500] concealing 611 DC, 611 AC, 611 MV errors in B frame
[mp2 @ 0x7b0d642d7b40] Header missingB vq= 1741KB sq=    0B f=34/29   
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 228600).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 246600).
[h264 @ 0x7b0d64757c40] concealing 297 DC, 297 AC, 297 MV errors in P frame
[h264 @ 0x7b0d640e9040] cabac decode of qscale diff failed at 23 21   
[h264 @ 0x7b0d640e9040] error while decoding MB 23 21, bytestream 12395
[h264 @ 0x7b0d640e9040] concealing 386 DC, 386 AC, 386 MV errors in P frame
[h264 @ 0x7b0d642563c0] illegal short term buffer state detected
[mp2 @ 0x7b0d642d7b40] Header missingB vq= 1741KB sq=    0B f=35/30   
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 250200).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 232920).
[h264 @ 0x7b0d642b1d00] cabac decode of qscale diff failed at 14 8
[h264 @ 0x7b0d642b1d00] error while decoding MB 14 8, bytestream 15652
[h264 @ 0x7b0d642b1d00] concealing 915 DC, 915 AC, 915 MV errors in P frame
[mp2 @ 0x7b0d642d7b40] Header missingB vq= 1743KB sq=    0B f=36/31   
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 237240).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 257400).  
[h264 @ 0x7b0d64757c40] cabac decode of qscale diff failed at 39 13
[h264 @ 0x7b0d64757c40] error while decoding MB 39 13, bytestream 41039
[h264 @ 0x7b0d64757c40] concealing 690 DC, 690 AC, 690 MV errors in P frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 241560).  
[h264 @ 0x7b0d640e9040] illegal short term buffer state detected/32   
[h264 @ 0x7b0d640e9040] concealing 600 DC, 600 AC, 600 MV errors in B frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 257400).  
[h264 @ 0x7b0d642563c0] concealing 410 DC, 410 AC, 410 MV errors in B frame
[h264 @ 0x7b0d642b1d00] left block unavailable for requested intra mode
[h264 @ 0x7b0d642b1d00] error while decoding MB 0 16, bytestream 17944
[h264 @ 0x7b0d642b1d00] concealing 609 DC, 609 AC, 609 MV errors in I frame
[h264 @ 0x7b0d64161500] cabac decode of qscale diff failed at 5 163   
[h264 @ 0x7b0d64161500] error while decoding MB 5 16, bytestream 37226
[h264 @ 0x7b0d64161500] concealing 604 DC, 604 AC, 604 MV errors in I frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 264600).
[mpegts @ 0x7b0d64000c80] PES packet size mismatch
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 245880).
[h264 @ 0x7b0d64757c40] concealing 151 DC, 151 AC, 151 MV errors in B frame
[mp2 @ 0x7b0d642d7b40] Header missingB vq= 1766KB sq=    0B f=39/34   
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 250200).
[h264 @ 0x7b0d640e9040] concealing 216 DC, 216 AC, 216 MV errors in B frame
[h264 @ 0x7b0d642563c0] left block unavailable for requested intra mode
[h264 @ 0x7b0d642563c0] error while decoding MB 0 29, bytestream 4917
[h264 @ 0x7b0d642563c0] concealing 89 DC, 89 AC, 89 MV errors in P frame
[h264 @ 0x7b0d64161500] illegal short term buffer state detected
[h264 @ 0x7b0d64757c40] cabac decode of qscale diff failed at 35 11
[h264 @ 0x7b0d64757c40] error while decoding MB 35 11, bytestream 9966
[h264 @ 0x7b0d64757c40] concealing 774 DC, 774 AC, 774 MV errors in P frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 268200).
[mp2 @ 0x7b0d642d7b40] Header missingB vq= 1673KB sq=    0B f=42/37   
    Last message repeated 1 times
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 271800).  
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 254520).
[h264 @ 0x7b0d642563c0] cabac decode of qscale diff failed at 15 97   
[h264 @ 0x7b0d642563c0] error while decoding MB 15 9, bytestream 42981
[h264 @ 0x7b0d642563c0] concealing 874 DC, 874 AC, 874 MV errors in P frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 258840).  
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 279000).
[h264 @ 0x7b0d642b1d00] illegal short term buffer state detected/38   
[h264 @ 0x7b0d642b1d00] cabac decode of qscale diff failed at 10 18
[h264 @ 0x7b0d642b1d00] error while decoding MB 10 18, bytestream 44020
[h264 @ 0x7b0d642b1d00] concealing 519 DC, 519 AC, 519 MV errors in B frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 263160).  
[h264 @ 0x7b0d64161500] cabac decode of qscale diff failed at 13 16
[h264 @ 0x7b0d64161500] error while decoding MB 13 16, bytestream 13364
[h264 @ 0x7b0d64161500] concealing 596 DC, 596 AC, 596 MV errors in B frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 279000).  
[h264 @ 0x7b0d64757c40] left block unavailable for requested intra mode
[h264 @ 0x7b0d64757c40] error while decoding MB 0 18, bytestream 12846
[h264 @ 0x7b0d64757c40] concealing 529 DC, 529 AC, 529 MV errors in I frame
[h264 @ 0x7b0d640e9040] cabac decode of qscale diff failed at 4 17
[h264 @ 0x7b0d640e9040] error while decoding MB 4 17, bytestream 34729
[h264 @ 0x7b0d640e9040] concealing 565 DC, 565 AC, 565 MV errors in I frame
[h264 @ 0x7b0d642b1d00] cabac decode of qscale diff failed at 9 22
[h264 @ 0x7b0d642b1d00] error while decoding MB 9 22, bytestream 4542
[h264 @ 0x7b0d642b1d00] concealing 360 DC, 360 AC, 360 MV errors in B frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 286200).
[mpegts @ 0x7b0d64000c80] PES packet size mismatch
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 267480).
[h264 @ 0x7b0d64161500] concealing 29 DC, 29 AC, 29 MV errors in P frame
[h264 @ 0x7b0d64757c40] concealing 100 DC, 100 AC, 100 MV errors in P frame
[mp2 @ 0x7b0d642d7b40] Header missingB vq= 1616KB sq=    0B f=47/42   
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 271800).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 289800).
[h264 @ 0x7b0d640e9040] illegal short term buffer state detected
[h264 @ 0x7b0d642563c0] left block unavailable for requested intra mode
[h264 @ 0x7b0d642563c0] error while decoding MB 0 15, bytestream 2721
[h264 @ 0x7b0d642563c0] concealing 649 DC, 649 AC, 649 MV errors in P frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 293400).  
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 276120).
[h264 @ 0x7b0d64161500] cabac decode of qscale diff failed at 11 15   
[h264 @ 0x7b0d64161500] error while decoding MB 11 15, bytestream 33501
[h264 @ 0x7b0d64161500] concealing 638 DC, 638 AC, 638 MV errors in P frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 280440).
[h264 @ 0x7b0d64757c40] illegal short term buffer state detected/44   
[h264 @ 0x7b0d64757c40] concealing 49 DC, 49 AC, 49 MV errors in B frame
[h264 @ 0x7b0d640e9040] cabac decode of qscale diff failed at 16 26
[h264 @ 0x7b0d640e9040] error while decoding MB 16 26, bytestream 2984
[h264 @ 0x7b0d640e9040] concealing 193 DC, 193 AC, 193 MV errors in B frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 300600).
[h264 @ 0x7b0d642563c0] cabac decode of qscale diff failed at 33 20   
[h264 @ 0x7b0d642563c0] error while decoding MB 33 20, bytestream 1847
[h264 @ 0x7b0d642563c0] concealing 416 DC, 416 AC, 416 MV errors in I frame
[h264 @ 0x7b0d642b1d00] left block unavailable for requested intra mode
[h264 @ 0x7b0d642b1d00] error while decoding MB 0 21, bytestream 21736
[h264 @ 0x7b0d642b1d00] concealing 409 DC, 409 AC, 409 MV errors in I frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 284760).  
[h264 @ 0x7b0d64757c40] concealing 954 DC, 954 AC, 954 MV errors in P frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 300600).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 307800).  
[mpegts @ 0x7b0d64000c80] PES packet size mismatch
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 289080).
[h264 @ 0x7b0d642563c0] concealing 1073 DC, 1073 AC, 1073 MV errors in P frame
[mp2 @ 0x7b0d642d7b40] Header missingB vq= 1681KB sq=    0B f=53/48   
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 293400).
[h264 @ 0x7b0d642b1d00] illegal short term buffer state detected
[h264 @ 0x7b0d642b1d00] cabac decode of qscale diff failed at 36 12
[h264 @ 0x7b0d642b1d00] error while decoding MB 36 12, bytestream 52940
[h264 @ 0x7b0d642b1d00] concealing 733 DC, 733 AC, 733 MV errors in B frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 311400).  
[h264 @ 0x7b0d64161500] cabac decode of qscale diff failed at 2 8
[h264 @ 0x7b0d64161500] error while decoding MB 2 8, bytestream 26538
[h264 @ 0x7b0d64161500] concealing 927 DC, 927 AC, 927 MV errors in B frame
[h264 @ 0x7b0d64757c40] cabac decode of qscale diff failed at 8 179   
[h264 @ 0x7b0d64757c40] error while decoding MB 8 17, bytestream 25178
[h264 @ 0x7b0d64757c40] concealing 561 DC, 561 AC, 561 MV errors in P frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 315000).  
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 297720).
[h264 @ 0x7b0d640e9040] cabac decode of qscale diff failed at 18 17
[h264 @ 0x7b0d640e9040] error while decoding MB 18 17, bytestream 25287
[h264 @ 0x7b0d640e9040] concealing 551 DC, 551 AC, 551 MV errors in P frame
[h264 @ 0x7b0d642563c0] illegal short term buffer state detected/50   
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 302040).  
[h264 @ 0x7b0d642b1d00] left block unavailable for requested intra mode
[h264 @ 0x7b0d642b1d00] error while decoding MB 0 3, bytestream 24080
[h264 @ 0x7b0d642b1d00] concealing 1129 DC, 1129 AC, 1129 MV errors in I frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 322200).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 306360).  
[h264 @ 0x7b0d64161500] co located POCs unavailableq=    0B f=56/51   
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 322200).  
[mpegts @ 0x7b0d64000c80] PES packet size mismatch
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 306360).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 325800).
[h264 @ 0x7b0d64757c40] left block unavailable for requested intra mode
[h264 @ 0x7b0d64757c40] error while decoding MB 0 3, bytestream 135749
[h264 @ 0x7b0d64757c40] concealing 1129 DC, 1129 AC, 1129 MV errors in I frame
[h264 @ 0x7b0d642563c0] left block unavailable for requested intra mode
[h264 @ 0x7b0d642563c0] error while decoding MB 0 8, bytestream 10377
[h264 @ 0x7b0d642563c0] concealing 929 DC, 929 AC, 929 MV errors in P frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 310680).
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 329400).  
[h264 @ 0x7b0d64161500] left block unavailable for requested intra mode
[h264 @ 0x7b0d64161500] error while decoding MB 0 9, bytestream 44809
[h264 @ 0x7b0d64161500] concealing 889 DC, 889 AC, 889 MV errors in P frame
[mp2 @ 0x7b0d642d7b40] Header missing
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 315000).
[h264 @ 0x7b0d64757c40] illegal short term buffer state detected/53   
[h264 @ 0x7b0d64757c40] left block unavailable for requested intra mode
[h264 @ 0x7b0d64757c40] error while decoding MB 0 17, bytestream 41457
[h264 @ 0x7b0d64757c40] concealing 569 DC, 569 AC, 569 MV errors in B frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 333000).
[h264 @ 0x7b0d640e9040] concealing 585 DC, 585 AC, 585 MV errors in B frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 319320).
[h264 @ 0x7b0d642563c0] concealing 163 DC, 163 AC, 163 MV errors in P frame
[h264 @ 0x7b0d642b1d00] cabac decode of qscale diff failed at 2 21
[h264 @ 0x7b0d642b1d00] error while decoding MB 2 21, bytestream 18376
[h264 @ 0x7b0d642b1d00] concealing 407 DC, 407 AC, 407 MV errors in P frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 336600).
[h264 @ 0x7b0d64161500] illegal short term buffer state detected/55   
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 323640).  
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 343800).
[h264 @ 0x7b0d64757c40] left block unavailable for requested intra mode
[h264 @ 0x7b0d64757c40] error while decoding MB 0 4, bytestream 18290
[h264 @ 0x7b0d64757c40] concealing 1089 DC, 1089 AC, 1089 MV errors in I frame
[h264 @ 0x7b0d640e9040] co located POCs unavailableq=    0B f=62/56   
[h264 @ 0x7b0d642563c0] left block unavailable for requested intra4x4 mode -1
[h264 @ 0x7b0d642563c0] error while decoding MB 0 6, bytestream 131985
[h264 @ 0x7b0d642563c0] concealing 1009 DC, 1009 AC, 1009 MV errors in I frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 343800).
[mpegts @ 0x7b0d64000c80] PES packet size mismatch
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 1, dts = 330120).
[h264 @ 0x7b0d64161500] concealing 723 DC, 723 AC, 723 MV errors in P frame
[mpegts @ 0x7b0d64000c80] Packet corrupt (stream = 0, dts = 347400).
[h264 @ 0x7b0d640e9040] cabac decode of qscale diff failed at 9 128   
[h264 @ 0x7b0d640e9040] error while decoding MB 9 12, bytestream 43706
[h264 @ 0x7b0d640e9040] concealing 760 DC, 760 AC, 760 MV errors in P frame

ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 
```

N=1 (immediate exit):
```console
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 2
v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
ffplay -f mpegts /dev/video0
Video input set to 2 (Composite: Camera, no signal)
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
[mpegts @ 0x7a8630000c80] Could not detect TS packet size, defaulting to non-FEC/DVHS
Input #0, mpegts, from '/dev/video0':B vq=    0KB sq=    0B f=0/0   
  Duration: N/A, bitrate: N/A
Failed to open file '/dev/video0' or configure filtergraph
    nan    :  0.000 fd=   0 aq=    0KB vq=    0KB sq=    0B f=0/0   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 

ff5@ff5-VirtualBox:~$ sudo dmesg | grep -E "out2_test|switching|START (transition|failed)|state-change|SCMD timeout|completed 10|analog mode now|ep84|first data|stats" | head -40
[59694.146529] elgato_gchd 1-2:1.0: state-change timeout: current=0x10 expected=0x11 state=0x0011 completion=0x0008
[59694.868212] elgato_gchd 1-2:1.0: state-change timeout: current=0x11 expected=0x10 state=0x0011 completion=0x0008
[59695.516628] elgato_gchd 1-2:1.0: state-change timeout: current=0x11 expected=0x10 state=0x0011 completion=0x0008
[59696.176556] elgato_gchd 1-2:1.0: state-change timeout: current=0x11 expected=0x10 state=0x0011 completion=0x0008
ff5@ff5-VirtualBox:~$ 
```

N=0 (hangs even after Ctrl+C):
```console
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 2
v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
ffplay -f mpegts /dev/video0
Video input set to 2 (Composite: Camera, no signal)
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
^Cff5@ff5-VirtualBox:~/elgato-gchd/dkms$ =    0KB sq=    0B f=0/0   

ff5@ff5-VirtualBox:~$ sudo dmesg | grep -E "out2_test|switching|START (transition|failed)|state-change|SCMD timeout|completed 10|analog mode now|ep84|first data|stats" | head -40
[59837.020367] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[59837.276832] elgato_gchd 1-2:1.0: switching output size: 0 -> 1
[59838.942641] elgato_gchd 1-2:1.0: encoder: transport output 2 (640x480), out2_test=0
[59839.587984] elgato_gchd 1-2:1.0: state-change timeout: current=0x01 expected=0x02 state=0x0001 completion=0x011a
[59839.588347] elgato_gchd 1-2:1.0: START failed: ret=-110 state=0x0001 (r=0) enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[59840.757108] elgato_gchd 1-2:1.0: state-change timeout: current=0x01 expected=0x02 state=0x0001 completion=0x011a
[59840.757500] elgato_gchd 1-2:1.0: START failed: ret=-110 state=0x0001 (r=0) enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[59847.164198] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=0 bytes=0 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=1 ring_dropped=0 frames_waiting=0
[59852.232729] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=0 bytes=0 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=1 ring_dropped=0 frames_waiting=0
ff5@ff5-VirtualBox:~$ 
```
