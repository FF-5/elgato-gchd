# TASK T-052 — S-video 480i and 240p

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** hardware test first (no driver change yet)
- **Type:** hardware test, then fixes as needed
- **Branch:** `feat/svideo` (from `dev`)
- **TODO items:** T-052

## Why

Composite is done on `dev` (0.3.0): 480i and 240p from a fresh start,
480i ↔ 240p changes while capturing, close and reopen. Next in the bring-up
order is S-video with the same resolutions. S-video shares the analog path
with composite; the recorded differences are `0x88:02 = DC` (composite `C9`)
and `0x88:14 = D2` (`docs/s-video.md`). The driver already sets both for
input 3, so S-video may work as it is. This test shows what is missing.

## Acceptance criteria

1. S-video (input 3), fresh start in 480i (menu) and in 240p (demo): picture.
2. 480i ↔ 240p changes while capturing: picture returns, as on composite.
3. Close and reopen works.
4. Switching between composite (2) and S-video (3) with `v4l2-ctl -i` while
   ffplay is closed: the next start uses the selected input.

## Hardware test (maintainer)

VirtualBox with the USB 3.0 (xHCI) controller; the N64 connected by S-video.

```sh
git fetch origin && git checkout feat/svideo && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 3
ffplay -f v4l2 -input_format h264 -i /dev/video0
# let the game switch menu <-> demo twice; close ffplay; wait until the
# ffplay process has exited (about 2 s); start ffplay again
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats"
```

If an S-video cable is not available, say so in the comments; the next
task is then PAL (needs a PAL source) or component.

Paste the output into the maintainer comments below and note when the game
switched.

## Findings

_(filled in during the task)_

---

## Maintainer comments

I conducted first this composite test turning off the source while ffplay is running and it worked fine:

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
  Duration: N/A, start: 8357.987314, bitrate: N/A
  Stream #0:0: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 60 fps, 60 tbr, 1000k tbn
[video4linux2,v4l2 @ 0x7c1304000c80] Some buffers are still owned by the caller on close.
ioctl(VIDIOC_QBUF): Bad file descriptor

ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 

ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats"
[sudo] password for ff5:           
[ 8309.783343] usbcore: registered new interface driver elgato_gchd
[ 8315.754621] usb 1-2: new high-speed USB device number 9 using xhci_hcd
[ 8316.134380] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[ 8316.134386] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[ 8316.134388] usb 1-2: Product: GameCapture HD
[ 8316.134390] usb 1-2: Manufacturer: Elgato
[ 8316.134391] usb 1-2: SerialNumber: 04107F1267
[ 8316.135779] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[ 8316.137205] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[ 8336.476441] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[ 8336.476449] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[ 8336.476454] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[ 8355.901342] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 8356.038329] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 8356.096317] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 8356.224863] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x3e mode=0xe
[ 8356.227252] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0x6e
[ 8356.227255] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[ 8356.282279] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x2e mode=0xe
[ 8356.284898] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[ 8356.284903] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[ 8356.339651] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x2e mode=0xe
[ 8356.342029] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[ 8356.342034] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[ 8356.397374] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x36 mode=0x6
[ 8356.406956] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x26 masked_mode=0x6
[ 8356.409657] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 8356.409662] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[ 8356.463122] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x36 mode=0x6
[ 8356.475372] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 8356.477968] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 8356.477972] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[ 8356.540266] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[ 8356.552932] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[ 8356.555272] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[ 8356.555274] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 8356.555276] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[ 8356.555278] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[ 8357.315439] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 8357.430240] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 8357.446041] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 8357.446400] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 8357.465031] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 8357.465731] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 8357.466419] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 8357.568559] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 8357.940723] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[ 8357.986361] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[ 8358.030525] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[ 8358.076815] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[ 8358.122023] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[ 8373.442338] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[ 8374.957721] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 8374.957987] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 8374.958951] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 8374.959397] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[ 8374.959881] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 8375.080281] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 8375.080283] elgato_gchd 1-2:1.0: analog mode now 240p
[ 8375.452884] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[ 8375.503241] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[ 8375.543908] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[ 8375.588696] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[ 8375.634001] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[ 8382.638194] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[ 8384.172968] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 8384.173162] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 8384.174233] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 8384.174782] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[ 8384.175250] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[ 8384.295964] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 8384.295969] elgato_gchd 1-2:1.0: analog mode now 480i
[ 8384.668438] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[ 8384.713411] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[ 8384.758028] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[ 8384.804424] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[ 8384.847871] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
ff5@ff5-VirtualBox:~$ 

Then I switched to S-Video and everything worked fine. First I ran the 240p-480i source then I switched off the N64 while ffplay was running and ran the steady 240p source:

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
  Duration: N/A, start: 8631.496654, bitrate: N/A
  Stream #0:0: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 60 fps, 60 tbr, 1000k tbn
[video4linux2,v4l2 @ 0x71629c000c80] Some buffers are still owned by the caller on close.
ioctl(VIDIOC_QBUF): Bad file descriptor

ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 

ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats"
[ 8616.819508] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[ 8616.819515] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[ 8616.819520] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[ 8628.325152] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 8628.385787] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 8628.450826] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 8628.585227] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x3e mode=0xe
[ 8628.587446] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0x6e
[ 8628.587452] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[ 8628.648533] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[ 8628.651892] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[ 8628.651902] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[ 8628.707070] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x2e mode=0xe
[ 8628.709359] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[ 8628.709362] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[ 8628.764991] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1e mode=0xe
[ 8628.767639] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[ 8628.767644] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[ 8628.822090] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[ 8628.832103] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x16 masked_mode=0x6
[ 8628.834475] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 8628.834479] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[ 8628.891973] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[ 8628.910112] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[ 8628.912845] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 8628.912850] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[ 8628.969467] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x16 mode=0x6
[ 8628.980049] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[ 8628.982611] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 8628.982613] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[ 8629.269158] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x16 mode=0x6
[ 8629.284394] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[ 8629.286628] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[ 8629.286632] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 8629.286635] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=1
[ 8629.286637] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=1
[ 8630.091654] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 8630.176651] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1f mode=0xf
[ 8630.179442] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xee
[ 8630.179448] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[ 8630.232904] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[ 8630.235339] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[ 8630.235343] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[ 8630.288972] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[ 8630.291590] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xfe
[ 8630.291593] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[ 8630.354644] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[ 8630.371503] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x16 masked_mode=0x6
[ 8630.373701] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 8630.486054] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[ 8630.498329] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x16 masked_mode=0x6
[ 8630.501209] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 8630.562094] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x16 mode=0x6
[ 8630.573961] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x16 masked_mode=0x6
[ 8630.576286] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 8630.635825] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x16 mode=0x6
[ 8630.653719] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[ 8630.656131] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 8630.720044] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[ 8630.731684] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x16 masked_mode=0x6
[ 8630.734033] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 8630.796039] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[ 8630.806471] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x16 masked_mode=0x6
[ 8630.808499] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[ 8630.808501] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[ 8630.808503] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=1
[ 8630.808505] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=1
[ 8630.918418] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 8630.938300] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 8630.938632] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 8630.960663] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 8630.961400] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=1
[ 8630.961909] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 8631.077641] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 8631.449996] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[ 8631.495960] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[ 8631.541175] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[ 8631.585360] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[ 8631.631043] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[ 8644.448926] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[ 8646.065598] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 8646.065945] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 8646.068360] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 8646.068868] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[ 8646.069303] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 8646.188527] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 8646.188531] elgato_gchd 1-2:1.0: analog mode now 240p
[ 8646.562519] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[ 8646.607663] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[ 8646.653121] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[ 8646.698023] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[ 8646.742845] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[ 8675.272697] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[ 8676.579491] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 8676.579718] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 8676.581988] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 8676.582485] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=1
[ 8676.582999] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 8676.685572] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 8676.685574] elgato_gchd 1-2:1.0: analog mode now 480i
[ 8677.057993] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[ 8677.103836] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[ 8677.147566] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[ 8677.193872] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[ 8677.238538] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[ 8692.381404] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[ 8693.854271] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 8693.854873] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 8693.856624] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 8693.857235] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[ 8693.858030] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 8693.960347] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 8693.960355] elgato_gchd 1-2:1.0: analog mode now 240p
[ 8694.332967] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[ 8694.379634] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[ 8694.423192] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[ 8694.468509] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[ 8694.513571] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
ff5@ff5-VirtualBox:~$ 

