# TASK T-041a — Composite 480i ↔ 240p while capturing

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** control test (`dev` vs. this branch) and source mode needed
- **Type:** driver fix + hardware test
- **Branch:** `fix/mode-change-restart` (from `dev`)
- **TODO items:** Phase 5 T-041, T-043; Phase 3 T-023 (audio, not in this task)

## Why

The last three tests on this branch failed at or right after the first
START, including on a freshly plugged-in device. Before going further it has
to be clear whether this is caused by the changes on this branch or by the
source:

- In the 0c64469 and 5d31e07 tests the decoder reported 480i (`CA = 7E`) at
  detection, START never completed, and ~15 s later it reported 240p
  (`CA = 66`). In the 2025d5f test it reported 240p at detection and 480i
  0.3 s after START. START succeeds only when the back end is configured for
  the mode the source really outputs; a source that changes between 480i and
  240p (or a decoder that reports the wrong one) produces exactly these logs.
- `dev` (240p and 480i fresh starts accepted earlier) has none of the changes
  of this branch.

## Acceptance criteria (this test)

1. Known source mode, stated by the maintainer.
2. `dev` and this branch both started from a freshly plugged-in device with
   that source, without switching anything.

## Hardware test (maintainer)

Please first tell me **what the source is** (device, and whether it outputs
240p, 480i, or changes between them by itself, e.g. menu vs. game).

Test A: `dev`, source left in one mode, nothing switched.

```sh
git fetch origin && git checkout --detach origin/dev
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
# wait up to 30 s for the window; note whether a picture appears and stays
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested"
```

Test B: the same with this branch (`git checkout fix/mode-change-restart &&
git pull`, same commands; unplug and replug again before `insmod`).

Paste both outputs, labelled A and B, into the maintainer comments below.

The delay before the ffplay window opens: ffplay first analyses the stream
for up to 5 s before it shows anything. Adding
`-fflags nobuffer -probesize 32k -analyzeduration 500000` shortens that; it
is not needed for this test.

## Findings

- 8a4b2af: stopping the receive queue before NULL/STOP prevents NULL from
  completing.
- `gchd_240p-480i_composite`: NULL takes ~0.7 s and is preceded by the
  neutral back-end table; 480i → 240p is not decoder-only.
- 9382bcb test: the mode-change sequence completes, but every START after
  the first one in a power cycle streams only ~0.4 MB.
- `gchd_240p-480i_composite_input_part_*` (bulk data): the driver's receive
  queue handling during stop matches; no reads after STOP.
- 0c64469 / 5d31e07 / 2025d5f tests: decoder status flips between 480i and
  240p around the first START; START only completes for the matching mode.
- 2025d5f test: encoder programming before START differed from every
  recording (missing `dma_sel_out` etc.); fixed.
- 0c64469 test: START hangs or the stream stops when START is issued before
  the back end has settled (`0x98:F5` wait missing).
- `gchd_output_switch_hdmi`: output size changes with a fixed 1080p60 source
  (`docs/hdmi.md §4`, TODO T-055).

---

## Maintainer comments

My source changes automatically (N64 the start of Star Wars Racer between the menu and the demo). dev with a solid 240p input works well:

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
  Duration: N/A, start: 20026.448332, bitrate: N/A
  Stream #0:0: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 60 fps, 60 tbr, 1000k tbn
[video4linux2,v4l2 @ 0x783344000c80] Some buffers are still owned by the caller on close.


ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested"
[19980.453224] usbcore: registered new interface driver elgato_gchd
[19987.515997] usb 2-1: new high-speed USB device number 21 using ehci-pci
[19987.918494] usb 2-1: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[19987.918515] usb 2-1: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[19987.918522] usb 2-1: Product: GameCapture HD
[19987.918529] usb 2-1: Manufacturer: Elgato
[19987.918533] usb 2-1: SerialNumber: 04107F1267
[19987.921437] elgato_gchd 2-1:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[19987.925907] elgato_gchd 2-1:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[19992.956712] elgato_gchd 2-1:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[19992.956719] elgato_gchd 2-1:1.0: input selected: input=2; detection deferred until device open
[19992.956723] elgato_gchd 2-1:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[20011.079492] elgato_gchd 2-1:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[20011.255302] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[20011.256824] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
[20011.932004] elgato_gchd 2-1:1.0: transcoder defaults loaded
[20011.934937] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[20011.936330] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x04
[20012.155255] elgato_gchd 2-1:1.0: device idle, processor state 0x27f97b
[20013.000716] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[20013.121243] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[20013.153843] elgato_gchd 2-1:1.0: analog format initial status=0x66; configuring 240p
[20013.153853] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[20013.153858] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[20013.153862] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[20019.212513] elgato_gchd 2-1:1.0: capture setup: encoder start completed
[20019.214821] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[20021.581073] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[20021.634345] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[20021.900030] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[20021.902490] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[20021.904975] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[20021.907801] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[20021.908737] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[20021.916931] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[20021.919568] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[20021.921021] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[20022.037497] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[20026.948131] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=105 bytes=6881280 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20031.972557] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=220 bytes=14417920 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20036.996155] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=335 bytes=21954560 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20042.020617] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=450 bytes=29491200 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20047.045511] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=565 bytes=37027840 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20052.068109] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=680 bytes=44564480 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20057.089662] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=795 bytes=52101120 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20062.112232] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=910 bytes=59637760 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20067.134000] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=1025 bytes=67174400 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20072.155833] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=1140 bytes=74711040 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20077.179781] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=1255 bytes=82247680 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20082.204098] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=1370 bytes=89784320 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20087.228286] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=1485 bytes=97320960 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20092.252752] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=1600 bytes=104857600 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20097.276163] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=1715 bytes=112394240 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20102.299940] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=1830 bytes=119930880 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20107.323451] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=1945 bytes=127467520 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20112.347358] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2060 bytes=135004160 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20117.368951] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2175 bytes=142540800 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20122.391696] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2290 bytes=150077440 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20127.415280] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2405 bytes=157614080 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20132.437225] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2520 bytes=165150720 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20137.458926] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2635 bytes=172687360 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20142.482887] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2750 bytes=180224000 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20147.094298] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[20147.095787] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
ff5@ff5-VirtualBox:~$ 

Solid 240p is also working fine with the fix branch:

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
  Duration: N/A, start: 20326.374735, bitrate: N/A
  Stream #0:0: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 60 fps, 60 tbr, 1000k tbn
[video4linux2,v4l2 @ 0x711144000c80] Some buffers are still owned by the caller on close.
ioctl(VIDIOC_QBUF): Bad file descriptor

ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 

ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested"
[20292.727628] usbcore: registered new interface driver elgato_gchd
[20297.239606] usb 2-1: new high-speed USB device number 22 using ehci-pci
[20297.665693] usb 2-1: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[20297.665699] usb 2-1: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[20297.665701] usb 2-1: Product: GameCapture HD
[20297.665708] usb 2-1: Manufacturer: Elgato
[20297.665710] usb 2-1: SerialNumber: 04107F1267
[20297.668030] elgato_gchd 2-1:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[20297.670404] elgato_gchd 2-1:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[20302.426564] elgato_gchd 2-1:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[20302.426575] elgato_gchd 2-1:1.0: input selected: input=2; detection deferred until device open
[20302.426582] elgato_gchd 2-1:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[20311.179150] elgato_gchd 2-1:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[20311.355770] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[20311.357262] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
[20311.969171] elgato_gchd 2-1:1.0: transcoder defaults loaded
[20311.971977] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[20311.974476] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x04
[20312.195940] elgato_gchd 2-1:1.0: device idle, processor state 0x27f97b
[20313.043797] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[20313.163797] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[20313.194767] elgato_gchd 2-1:1.0: analog format initial status=0x66; configuring 240p
[20313.194780] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[20313.194787] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[20313.194792] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[20319.282641] elgato_gchd 2-1:1.0: capture setup: encoder start completed
[20319.285038] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[20321.542279] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[20321.842808] elgato_gchd 2-1:1.0: capture setup: encoder programming completed
[20321.845337] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[20321.847812] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[20321.849389] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[20321.849922] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[20321.859094] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[20321.861862] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[20321.863154] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[20321.969271] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[20322.323235] elgato_gchd 2-1:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[20322.367637] elgato_gchd 2-1:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[20322.413178] elgato_gchd 2-1:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[20322.458052] elgato_gchd 2-1:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[20322.503021] elgato_gchd 2-1:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[20326.872119] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=105 bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20331.895245] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=220 bytes=14417920 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20336.919423] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=335 bytes=21954560 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20341.942371] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=450 bytes=29491200 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20346.965626] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=565 bytes=37027840 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20351.988741] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=680 bytes=44564480 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20353.630825] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[20353.632364] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[20354.316876] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[20354.318299] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[20354.544749] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[20354.567124] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[20354.568556] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
ff5@ff5-VirtualBox:~$ 
