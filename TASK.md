# TASK T-053b — PAL S-video

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** composite after S-video: recorded switch sequence; waiting for the hardware test
- **Type:** driver fix + hardware test
- **Branch:** `fix/pal-svideo` (from `dev`); component (`feat/component`,
  T-051) continues after this
- **TODO items:** T-053b

## Why

Last test (a675bc8): S-video after composite works (576i, 288p change).
Composite gave a **vertically stretched picture**, both as the first test
and after switching back from S-video: the decoder reported the 60 Hz
family for the PAL source (`0x88:FA = 26`) and 720 × 480 was configured.

Cause: the analog decoder keeps its settings while powered, also across
module reloads. The previous S-video session left it set for S-video and
in progressive mode (`0x88:28 = 8A` after a 288p change). The driver wrote
only `02 = C9, 14 = D2` when going back to composite, and nothing at all
after a module reload. The recorded S-video → composite switch
(`input-switching/filtered_novideo_input_switching` 19.566-19.828) is
`02 = C9, 14 = D2, 3C = 6B`, a 0.22 s pause, `28 = 88`, `03 = 28`, then
the selector and routing. The driver now writes exactly that whenever the
decoder may still be set for S-video, including the first composite start
after loading the module.

## Acceptance criteria

1. Composite (PS1 PAL): 576i picture with correct proportions, also after
   S-video and after reloading the module without replugging.
2. S-video still works.

## Hardware test (maintainer)

PS1 connected to both composite and S-video. Do **not** unplug between the
steps.

```sh
git fetch origin && git checkout fix/pal-svideo && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 3
ffplay -f v4l2 -input_format h264 -i /dev/video0
# wait for a 288p change, close, wait until ffplay has exited
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
# close; reload the module without unplugging:
sudo rmmod elgato_gchd; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 3
ffplay -f v4l2 -input_format h264 -i /dev/video0
# close; then -i 2 and ffplay once more
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
```

For each composite session: proportions correct? dmesg should say
`configuring 576i` (or `288p`), not `240p`/`480i`.

## Findings

- a675bc8 test: composite stretched vertically (60 Hz family detected for
  the PAL source); decoder still set for S-video / progressive.
- b17d75e test: PAL S-video works; composite after S-video needs a replug
  (decoder kept `0x88:02 = DC`).
- 13b3a79 and earlier: PAL S-video never locks; the PS1 outputs S-video
  (Windows recordings); cause: missing retrigger on `0x98:F5` bit 7.

---

## Maintainer comments

First S-Video test worked fine:

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
  Duration: N/A, start: 27980.676086, bitrate: N/A
  Stream #0:0: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x576 [SAR 12:11 DAR 15:11], 60 fps, 60 tbr, 1000k tbn
[video4linux2,v4l2 @ 0x75041c000c80] Some buffers are still owned by the caller on close.
ioctl(VIDIOC_QBUF): Bad file descriptor

ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 


ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
[sudo] password for ff5:           
[27951.882880] usbcore: registered new interface driver elgato_gchd
[27959.298314] usb 1-2: new high-speed USB device number 22 using xhci_hcd
[27959.684269] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[27959.684276] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[27959.684277] usb 1-2: Product: GameCapture HD
[27959.684279] usb 1-2: Manufacturer: Elgato
[27959.684280] usb 1-2: SerialNumber: 04107F1267
[27959.685759] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[27959.687751] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[27964.703164] elgato_gchd 1-2:1.0: input switch requested: old=0 new=3 hw_initialized=0 streaming=0
[27964.703182] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[27964.703190] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[27973.280778] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[27973.474080] elgato_gchd 1-2:1.0: transcoder defaults loaded
[27973.540300] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[27973.685624] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[27973.688270] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[27973.688272] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[27973.754885] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[27973.759036] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[27973.759043] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[27973.818229] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[27973.821996] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[27973.822001] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[27973.892084] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[27973.894477] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[27973.894480] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[27973.958861] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[27973.961730] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[27973.961735] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[27974.019720] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[27974.022320] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[27974.022325] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[27974.097415] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[27974.099619] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[27974.099622] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[27974.160221] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[27974.162852] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[27974.162856] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[27974.220080] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[27974.222811] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[27974.222817] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[27974.278823] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[27974.280960] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[27974.280964] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[27974.345835] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[27974.417270] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[27974.478923] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xdf mode=0xf
[27974.615759] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xdf mode=0xf
[27974.687093] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xdf mode=0xf
[27974.757108] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0f mode=0xf
[27974.827447] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[27974.938785] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x26 mode=0x6
[27974.949299] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x26 masked_mode=0x6
[27974.951579] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[27975.014421] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[27975.033741] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[27975.037003] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[27975.096448] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x26 mode=0x6
[27975.114166] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[27975.116414] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[27975.179000] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[27975.189952] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[27975.195114] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[27975.195119] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[27975.195123] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[27975.195125] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[27975.984274] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[27976.078117] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1f mode=0xf
[27976.149095] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1e mode=0xe
[27976.212474] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[27976.274480] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x5e mode=0xe
[27976.336115] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[27976.401291] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x2e mode=0xe
[27976.474121] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[27976.487253] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[27976.489831] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[27976.550326] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[27976.562886] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[27976.566523] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[27976.626263] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[27976.639364] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[27976.641594] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[27976.702352] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[27976.715283] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[27976.718871] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[27976.778678] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[27976.789078] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[27976.791280] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[27976.851837] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[27976.862751] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[27976.864992] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[27976.865004] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[27976.865007] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[27976.865009] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[27979.449813] elgato_gchd 1-2:1.0: back-end not settled: status 0xf2
[27979.461976] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[27979.479077] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[27979.479312] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[27979.481833] elgato_gchd 1-2:1.0: source changed to 50 Hz during setup; configuring again
[27979.572159] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x17 mode=0x7
[27979.581842] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x17 masked_mode=0x7
[27979.584070] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[27979.584072] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[27979.584075] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x576 fps=50/1 interlaced=1
[27979.584076] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x576 fps=50/1 interlaced=1
[27979.798605] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[27979.814519] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[27979.814831] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[27979.820153] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[27979.820672] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x576 fps=50/1 interlaced=1
[27979.821168] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[27979.960800] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[27980.358556] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[27980.403877] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[27980.448687] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[27980.494817] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[27980.539529] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[27999.416138] elgato_gchd 1-2:1.0: analog mode change detected: 576i -> 288p
[27999.894609] elgato_gchd 1-2:1.0: analog mode now 288p
[28003.675122] elgato_gchd 1-2:1.0: analog mode change detected: 288p -> 576i
[28004.167333] elgato_gchd 1-2:1.0: analog mode now 576i
ff5@ff5-VirtualBox:~$ 

No, the second test on composite failed. Still the same vertically stretched output and very interlaced kind of look (I can see many horizontal lines as if it was a CRT filter).

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
  Duration: N/A, start: 28162.427698, bitrate: N/A
  Stream #0:0: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 60 fps, 60 tbr, 1000k tbn
[video4linux2,v4l2 @ 0x7fb0d4000c80] Some buffers are still owned by the caller on close.
ioctl(VIDIOC_QBUF): Bad file descriptor

ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 


ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
[28146.567070] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[28146.567072] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[28146.567074] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[28156.418572] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[28156.699142] elgato_gchd 1-2:1.0: transcoder defaults loaded
[28156.763854] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[28157.140931] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[28157.144003] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[28157.144007] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[28157.213964] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[28157.216907] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[28157.216912] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[28157.276555] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[28157.278841] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[28157.278845] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[28157.356383] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[28157.361425] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[28157.361430] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[28157.438088] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[28157.442834] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[28157.442847] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[28157.516463] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[28157.519682] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[28157.519692] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[28157.579674] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[28157.582352] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[28157.582359] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[28157.652505] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[28157.658615] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[28157.658625] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[28157.714341] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[28157.716593] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[28157.716597] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[28157.787805] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[28157.790119] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[28157.790123] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[28157.845876] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[28157.904962] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[28157.969523] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x0f mode=0xf
[28158.037742] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x3e mode=0xe
[28158.121112] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x1e mode=0xe
[28158.178619] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x0e mode=0xe
[28158.245584] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[28158.254176] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[28158.256273] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[28158.316536] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[28158.327115] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[28158.329800] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[28158.390942] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[28158.400904] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[28158.403041] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[28158.464146] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[28158.472521] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[28158.474637] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[28158.474639] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[28158.474641] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[28158.474643] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[28159.254386] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[28161.649923] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[28161.665621] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[28161.665950] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[28161.671136] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[28161.671681] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[28161.672123] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[28161.771724] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[28162.143756] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[28162.189895] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[28162.235667] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[28162.279076] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[28162.326040] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[28162.820061] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[28164.233028] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[28164.233262] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[28164.234765] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[28164.235438] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[28164.236136] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[28164.372418] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[28164.372421] elgato_gchd 1-2:1.0: analog mode now 480i
[28164.744962] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[28164.790079] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[28164.835259] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[28164.879896] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[28164.924310] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[28173.803503] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[28175.258128] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[28175.258736] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[28175.259759] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[28175.260281] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[28175.260855] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[28175.364139] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[28175.364142] elgato_gchd 1-2:1.0: analog mode now 240p
[28175.737078] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[28175.781718] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[28175.827412] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[28175.871337] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[28175.916629] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[28177.803005] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[28179.231947] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[28179.232152] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[28179.233361] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[28179.234042] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[28179.234548] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[28179.355630] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[28179.355633] elgato_gchd 1-2:1.0: analog mode now 480i
[28179.727061] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[28179.772120] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[28179.817897] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[28179.862034] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[28179.907690] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[28184.916216] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[28186.584632] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[28186.584859] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[28186.585823] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[28186.586304] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[28186.587207] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[28186.690667] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[28186.690669] elgato_gchd 1-2:1.0: analog mode now 240p
[28187.062750] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[28187.108716] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[28187.153567] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[28187.198340] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[28187.244568] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[28189.031683] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[28190.771368] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[28190.772189] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[28190.773061] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[28190.773713] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[28190.774114] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[28190.894050] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[28190.894053] elgato_gchd 1-2:1.0: analog mode now 480i
[28191.266834] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[28191.312773] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[28191.357225] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[28191.402837] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[28191.446954] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
ff5@ff5-VirtualBox:~$ 
