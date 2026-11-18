# TASK T-053b — PAL S-video

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** composite after S-video still stretched; diagnosis (register dump, recording)
- **Type:** driver fix + hardware test
- **Branch:** `fix/pal-svideo` (from `dev`); component (`feat/component`,
  T-051) continues after this
- **TODO items:** T-053b

## Why

Last test (847b592): S-video works (the family check before START caught a
brief NTSC reading and started 576i). **Composite after S-video is still
stretched:** the decoder keeps reporting the 60 Hz family (NTSC) for the PAL
PS1, and CA flips between 66 and 7E every few seconds, although the driver
now writes the full recorded S-video → composite sequence. After a fresh
plug-in, composite PAL worked (seventeenth test).

So S-video leaves some other decoder state behind that the recorded switch
does not reset. The only recording of that switch was made without a
source, so it cannot show what Windows does with a signal. Two ways to find
it:

1. **Decoder register dump:** new module parameter `dec_dump=1` logs the
   decoder registers `0x88:00`–`7F` right before analog detection. A dump
   of a working composite start (fresh plug-in) and of a failing one (after
   S-video) shows which registers differ.
2. **Windows recording** of the PS1 on S-video, switched to composite while
   running.

## Acceptance criteria (this test)

1. Two dumps: composite after a fresh plug-in, and composite after S-video.
2. If possible, the Windows recording.

## Hardware test (maintainer)

PS1 connected to composite and S-video, as before.

```sh
git fetch origin && git checkout fix/pal-svideo && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko dec_dump=1
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0      # composite: picture OK?
# close, wait until ffplay has exited
v4l2-ctl -d /dev/video0 -i 3
ffplay -f v4l2 -input_format h264 -i /dev/video0      # S-video, wait for a 288p change
# close, wait
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0      # composite again: stretched?
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
```

The important lines are `dec 00:` … `dec 70:` (eight per session).

Windows recording (if possible): PS1 on S-video, capture running, then
select composite in the Windows software and wait for the picture. Reduce
with
`tshark -r big.pcapng -Y "usb.transfer_type != 0x03 || usb.endpoint_address.direction == 0" -w small.pcapng`
and upload to `traces/input-switching/`.

Paste the output into the maintainer comments below.

## Findings

- 847b592 test: composite after S-video still detected as 60 Hz although
  the recorded S-video → composite sequence is written.
- a675bc8 test: composite stretched vertically (60 Hz family detected for
  the PAL source); decoder still set for S-video / progressive.
- b17d75e test: PAL S-video works; composite after S-video needs a replug
  (decoder kept `0x88:02 = DC`).
- 13b3a79 and earlier: PAL S-video never locks; the PS1 outputs S-video
  (Windows recordings); cause: missing retrigger on `0x98:F5` bit 7.

---

## Maintainer comments

No, composite still shows a vertically stretched image with horizontal lines. Now another issue appeared, partial old frames being randomly flashed. PAL composite was working before this branch, so what changed?

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
  Duration: N/A, start: 29205.328515, bitrate: N/A
  Stream #0:0: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 60 fps, 60 tbr, 1000k tbn
[video4linux2,v4l2 @ 0x7d044c000c80] Some buffers are still owned by the caller on close.
ioctl(VIDIOC_QBUF): Bad file descriptor

ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 

ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
[sudo] password for ff5:           
[29180.706725] usbcore: registered new interface driver elgato_gchd
[29185.934417] usb 1-2: new high-speed USB device number 23 using xhci_hcd
[29186.318175] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[29186.318197] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[29186.318203] usb 1-2: Product: GameCapture HD
[29186.318208] usb 1-2: Manufacturer: Elgato
[29186.318212] usb 1-2: SerialNumber: 04107F1267
[29186.324433] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[29186.327989] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[29190.485154] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[29190.485157] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[29190.485158] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[29199.251378] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[29199.441235] elgato_gchd 1-2:1.0: transcoder defaults loaded
[29199.510418] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[29199.942721] elgato_gchd 1-2:1.0: dec 00: 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e
[29199.978594] elgato_gchd 1-2:1.0: dec 10: 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e
[29200.015046] elgato_gchd 1-2:1.0: dec 20: 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e
[29200.053394] elgato_gchd 1-2:1.0: dec 30: 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e ee 6e 6e 6e 6e
[29200.094550] elgato_gchd 1-2:1.0: dec 40: 6e 0d 6e 6e ee ee ee 6e 6e 6e 6e 6e 5f 6e 6e 6e
[29200.132446] elgato_gchd 1-2:1.0: dec 50: 6e 6e 6e 6e ee 74 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e
[29200.167364] elgato_gchd 1-2:1.0: dec 60: 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e
[29200.203144] elgato_gchd 1-2:1.0: dec 70: 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e 6e
[29200.207520] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xdf mode=0xf
[29200.209753] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xce
[29200.209757] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[29200.272917] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xcf mode=0xf
[29200.275582] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xce
[29200.275588] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[29200.340153] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xdf mode=0xf
[29200.343149] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xce
[29200.343157] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[29200.400391] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xdf mode=0xf
[29200.403850] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xce
[29200.403857] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[29200.462766] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x0f mode=0xf
[29200.465915] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xee
[29200.465921] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[29200.537525] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x1e mode=0xe
[29200.540822] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[29200.540830] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[29200.603943] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x3e mode=0xe
[29200.609360] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0x8e
[29200.609371] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[29200.668485] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x2e mode=0xe
[29200.671795] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[29200.671806] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[29200.738452] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x2e mode=0xe
[29200.741604] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[29200.741610] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[29200.809799] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x26 mode=0x6
[29200.822687] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x26 masked_mode=0x6
[29200.825493] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[29200.825497] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[29200.885458] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[29200.894541] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[29200.896757] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[29200.956430] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[29200.965855] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[29200.968096] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[29200.968098] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[29200.968100] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[29200.968103] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[29201.746361] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[29204.510914] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[29204.526721] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[29204.526942] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[29204.532249] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[29204.532762] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[29204.533221] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[29204.682989] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[29205.055325] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[29205.101112] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[29205.145898] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[29205.190158] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[29205.235581] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[29205.459787] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[29206.856319] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[29206.856571] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[29206.857541] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[29206.858082] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[29206.858601] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[29206.978977] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[29206.978980] elgato_gchd 1-2:1.0: analog mode now 480i
[29207.351379] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[29207.396723] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[29207.442205] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[29207.486827] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[29207.533208] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
ff5@ff5-VirtualBox:~$ 
