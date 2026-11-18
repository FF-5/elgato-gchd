# TASK T-020 — MPEG-TS stream with audio

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** N64 works; PS1 intermittently not seen on S-video; questions
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

## Questions (maintainer)

4255192 test: N64 on S-video works (240p → 480i, `ring_dropped=0`). PS1 on
S-video: no sync in both sessions (`0x88:FA = ED/FD`). With the same
detection code the PS1 locked in earlier runs (e.g. 33506, also after N64
sessions; e025e9f test). The driver handles the N64 consistently; the PS1
is sometimes seen and sometimes not.

1. What does the PS1 show when ffplay is started?
2. Same S-video cable and Elgato jack as for the N64? Is it the cable used
   with the monitor?
3. Reseat the S-video plug at the Elgato, power-cycle the PS1, try once
   more. If there is still no signal, start capture in the Windows
   software at that moment: if Windows shows a picture, a short recording
   of that start (reduced with the bulk-OUT filter) shows the difference.

## Hardware test (maintainer)

```sh
git fetch origin && git checkout feat/ts-audio && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 3
v4l2-ctl -d /dev/video0 -v pixelformat=MPEG
ffplay -f mpegts /dev/video0
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|callbacks suppressed"
```

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
- 0d47f71 test: N64 composite and S-video, PS1 composite work in TS mode
  (PS1 S-video not in the pasted output). Decode messages at the same
  stream time in every run (dts 343080 ≈ 3.8 s, ≈ 5 s), right after
  ffplay/ffmpeg finish analysing the first ~5 MB: the driver buffer held
  only 32 entries (2 MB in TS mode, ~0.5 s in H.264 mode) and dropped
  buffers while the application set up. Raised to 512 entries (the 64 MB
  byte limit applies).
- 0d47f71 test, PS1 S-video (log pasted later): after the firmware start
  the decoder reported the 60 Hz family for ~2 s, 240p/480i was
  configured, wrong picture (sound fine); the second run caught it before
  START. Cause: the block after the encoder firmware start
  (`gchd_post_encoder_prefix`) set the decoder to composite (`0x88:02 =
  C9`) also for S-video. Now `DC` for S-video. In addition the family is
  checked while capturing: two polls with the other family → configure
  again.
- e025e9f test, PS1 S-video: the start still reads 60 Hz first (the first
  detection runs before the firmware start); corrected 0.6 s after START by
  the new check. But during steady 576i capture the decoder read 60 Hz for
  two polls once, and the driver reconfigured to 240p and back. The
  Windows PAL recordings never show `FA` with the 60 Hz family, so the
  check now needs 8 polls (2 s) and logs FA/CA/E7/F1 when it fires.
  `ring_dropped=0` throughout; the 20 s recording decodes without errors.
- Maintainer: in the first Windows recording with the PS1 the Windows
  application showed 720×480 at first and settled on 720×576 later, while
  the picture was correct throughout. In that recording
  (`composite/filtered_288p_pal_composite`) `0x88:FA` reads `1F` → `17` →
  `07` (50 Hz) before the encoder is programmed, and the encoder writes are
  PAL (`v_format = 5`, 720×576): the 720×480 was the application's label.
  If the driver still needs a restart at the start often, wait for a
  stable family reading before the first configuration.
- Unclean exit of a program: the kernel closes its file handles and the
  driver's close path (clean hardware shutdown) runs. A new open waits:
  the close holds the user lock through the shutdown, and open takes it.

---

## Maintainer comments

I'm sorry it was my mistake. I didn't make sure that the S-Video cable was connected to the PS1 out. It still didn't lock to 576:

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
[mpegts @ 0x7b13d8000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x7b13d8000c80] Packet corrupt (stream = 1, dts = 137880).
[mpegts @ 0x7b13d8000c80] Packet corrupt (stream = 0, dts = 189144).
[mpegts @ 0x7b13d8000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x7b13d8000c80] Packet corrupt (stream = 1, dts = 170280).
Input #0, mpegts, from '/dev/video0':B vq=    0KB sq=    0B f=0/0   
  Duration: N/A, start: 0.500000, bitrate: N/A
  Program 1 
  Stream #0:0[0x1011]: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 25 fps, 50 tbr, 90k tbn
  Stream #0:1[0x10f]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
[mp2 @ 0x7b13d82c0400] Header missingB vq= 1765KB sq=    0B f=0/0   
[mp2 @ 0x7b13d82c0400] Header missingB vq= 1991KB sq=    0B f=1/1   
[mpegts @ 0x7b13d8000c80] Packet corrupt (stream = 0, dts = 1175400).
[mpegts @ 0x7b13d8000c80] PES packet size mismatchsq=    0B f=3/2   
[mpegts @ 0x7b13d8000c80] Packet corrupt (stream = 1, dts = 1157400).
[mp2 @ 0x7b13d82c0400] Header missingB vq=    0KB sq=    0B f=3/2   
[mpegts @ 0x7b13d8000c80] Packet corrupt (stream = 0, dts = 707400).
[mpegts @ 0x7b13d8000c80] PES packet size mismatchsq=    0B f=4/3   
[mpegts @ 0x7b13d8000c80] Packet corrupt (stream = 1, dts = 690840).
[mp2 @ 0x7b13d82c0400] Header missingB vq=   54KB sq=    0B f=4/3   
[mpegts @ 0x7b13d8000c80] Packet corrupt (stream = 0, dts = 930600).
[mpegts @ 0x7b13d8000c80] PES packet size mismatchsq=    0B f=5/4   
[mpegts @ 0x7b13d8000c80] Packet corrupt (stream = 1, dts = 909000).
[mp2 @ 0x7b13d82c0400] Header missingB vq=    0KB sq=    0B f=5/4   
   9.74 A-V: -0.008 fd= 113 aq=    0KB vq=    0KB sq=    0B f=5/5   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 

ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|callbacks suppressed"
[41902.628325] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[41909.667043] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[41910.202840] elgato_gchd 1-2:1.0: transcoder defaults loaded
[41910.269779] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[41910.420408] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x6e mode=0xe
[41910.422771] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0x6e
[41910.422774] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[41910.484254] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x3e mode=0xe
[41910.488952] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[41910.488957] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[41910.551154] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1e mode=0xe
[41910.554273] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[41910.554279] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[41910.611257] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[41910.621793] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[41910.625035] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[41910.625037] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[41910.687029] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[41910.704351] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[41910.706975] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[41910.706978] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[41910.769310] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[41910.780082] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[41910.782543] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[41910.782552] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[41910.843535] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[41910.853938] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[41910.856583] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[41910.856585] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[41910.856587] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[41910.856589] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[41911.644784] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[41914.147220] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[41914.162869] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[41914.163104] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[41914.168521] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[41914.169010] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[41914.169480] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[41914.352498] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[41914.724981] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[41914.770342] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[41914.815420] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[41914.860664] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[41914.884714] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[41914.905146] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[41916.229954] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[41916.230172] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[41916.231738] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[41916.232413] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=1
[41916.232838] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[41916.352591] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[41916.352594] elgato_gchd 1-2:1.0: analog mode now 480i
[41916.724968] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[41916.771023] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[41916.816206] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[41916.860196] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[41916.905377] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[41917.957208] elgato_gchd 1-2:1.0: source is 50 Hz while capturing; configuring again
[41917.966276] elgato_gchd 1-2:1.0: decoder status: 0x88:FA=07 CA=7e E7=79 F1=62
[41917.997988] elgato_gchd 1-2:1.0: signal present: input=3; first video TS packet on PID 0x1011
[41918.827534] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x07 mode=0x7
[41918.840284] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x07 masked_mode=0x7
[41918.842453] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[41918.842455] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[41918.842457] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x576 fps=50/1 interlaced=1
[41918.842459] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x576 fps=50/1 interlaced=1
[41919.250507] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[41919.266387] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[41919.266600] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[41919.272289] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[41919.273055] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x576 fps=50/1 interlaced=1
[41919.273507] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[41919.392195] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[41919.790848] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[41919.835275] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[41919.880035] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[41919.926090] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[41919.971272] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[41924.307467] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=36
[41929.338940] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=35
[41931.430082] elgato_gchd 1-2:1.0: analog mode change detected: 576i -> 288p
[41931.910773] elgato_gchd 1-2:1.0: analog mode now 288p
[41933.239481] elgato_gchd 1-2:1.0: source is 60 Hz while capturing; configuring again
[41933.255057] elgato_gchd 1-2:1.0: decoder status: 0x88:FA=06 CA=66 E7=59 F1=7e
[41933.279739] elgato_gchd 1-2:1.0: signal present: input=3; first video TS packet on PID 0x1011
[41933.990643] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[41934.002276] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x16 masked_mode=0x6
[41934.004917] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[41934.004921] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[41934.004924] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[41934.004926] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[41936.385701] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[41936.409931] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[41936.410161] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[41936.412315] elgato_gchd 1-2:1.0: source changed to 50 Hz during setup; configuring again
[41936.467370] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x07 mode=0x7
[41936.477678] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x17 masked_mode=0x7
[41936.480003] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[41936.480006] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[41936.480008] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x576 fps=50/1 interlaced=1
[41936.480011] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x576 fps=50/1 interlaced=1
[41936.704994] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[41936.721202] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[41936.721583] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[41936.726812] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[41936.727273] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x576 fps=50/1 interlaced=1
[41936.727921] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[41936.867173] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[41937.264289] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[41937.310028] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[41937.354961] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[41937.399543] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[41937.445817] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[41941.738963] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[41942.463417] elgato_gchd 1-2:1.0: analog mode change detected: 576i -> 288p
[41942.939516] elgato_gchd 1-2:1.0: analog mode now 288p
[41944.287773] elgato_gchd 1-2:1.0: source is 60 Hz while capturing; configuring again
[41944.298283] elgato_gchd 1-2:1.0: decoder status: 0x88:FA=06 CA=66 E7=59 F1=7e
[41944.324419] elgato_gchd 1-2:1.0: signal present: input=3; first video TS packet on PID 0x1011
[41944.996100] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[41945.008870] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x16 masked_mode=0x6
[41945.011487] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[41945.011495] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[41945.011504] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[41945.011508] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[41947.439104] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[41947.457105] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[41947.457360] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[41947.459333] elgato_gchd 1-2:1.0: source changed to 50 Hz during setup; configuring again
[41947.521311] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x07 mode=0x7
[41947.530827] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x17 masked_mode=0x7
[41947.533379] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[41947.533382] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[41947.533385] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x576 fps=50/1 interlaced=1
[41947.533389] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x576 fps=50/1 interlaced=1
[41947.753864] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[41947.771994] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[41947.772449] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[41947.777786] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[41947.778596] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x576 fps=50/1 interlaced=1
[41947.779227] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[41947.918061] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[41948.316188] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[41948.360520] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[41948.405364] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[41948.450460] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[41948.496526] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[41952.789871] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[41955.694777] elgato_gchd 1-2:1.0: analog mode change detected: 576i -> 288p
[41956.170800] elgato_gchd 1-2:1.0: analog mode now 288p
[41957.752858] elgato_gchd 1-2:1.0: source is 60 Hz while capturing; configuring again
[41957.762950] elgato_gchd 1-2:1.0: decoder status: 0x88:FA=16 CA=66 E7=59 F1=7e
[41957.784424] elgato_gchd 1-2:1.0: signal present: input=3; first video TS packet on PID 0x1011
[41957.828039] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[41958.653959] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[41958.663860] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x16 masked_mode=0x6
[41958.666492] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[41958.666498] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[41958.666501] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[41958.666504] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[41961.137269] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[41961.156125] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[41961.156387] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[41961.158628] elgato_gchd 1-2:1.0: source changed to 50 Hz during setup; configuring again
[41961.218118] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x07 mode=0x7
[41961.234418] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x17 masked_mode=0x7
[41961.237281] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[41961.237285] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[41961.237288] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x576 fps=50/1 interlaced=1
[41961.237291] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x576 fps=50/1 interlaced=1
[41961.455659] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[41961.471161] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[41961.471382] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[41961.476511] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[41961.477305] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x576 fps=50/1 interlaced=1
[41961.478030] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[41961.617989] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[41962.014694] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[41962.060206] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[41962.104588] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[41962.149499] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[41962.196084] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[41966.489232] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[41971.526046] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
ff5@ff5-VirtualBox:~$ 
