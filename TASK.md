# TASK T-020 — MPEG-TS stream with audio

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** TS works; family change (d578d60); composite start after S-video sessions (d2fe44f); hardware test
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

## Hardware test (maintainer)

Build and load (once):

```sh
git fetch origin && git checkout feat/ts-audio && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
```

Each run (input 3 = S-video, 2 = composite):

```sh
v4l2-ctl -d /dev/video0 -i 3
v4l2-ctl -d /dev/video0 -v pixelformat=MPEG
ffplay -f mpegts /dev/video0
```

Note in your comment how you unplugged before `insmod`: cable pulled out
of the PC, or detached in the VirtualBox USB menu.

1. Start with composite: PS1 on composite (fresh module, previous session
   was S-video): picture expected within a few seconds. Then the steps
   below.
2. PS1 on S-video: start ffplay with the PS1 off, power it on, wait until
   the game runs; press reset once, wait; power-cycle once, wait. Close
   ffplay.
3. The same on composite (`-i 2`).
4. N64 on composite or S-video: menu ↔ demo (480i ↔ 240p) a few times.

Expected: after each PS1 start or reset, `decoder status changed while
capturing (...)` within about 0.5 s of the switch to 50 Hz, then `analog
mode now 576i` (or 288p) and the picture returns about 3 s later. No such
line during a steady picture. N64 changes work as before (possibly via the
same line instead of `analog mode change detected`).

After each run:

```sh
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|callbacks suppressed|bulk-IN first reads"
sudo dmesg -C
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
- f12225e test (PS1 S-video, cable now on the PS1 output): the PS1
  locks. At power-on and after each reset the PS1 outputs 60 Hz (`FA = 06`,
  `CA = 66`, `E7 = 59`) for a few seconds, then 576i; the driver followed
  every change but needed 2 s (8 polls) plus a full redetection each time.
  ffplay reports 720×480 because the stream started at 240p. Windows does
  the same at PS1 start (`input-switching/gchd_PAL_composite_svideo_small`):
  it starts in NTSC, then restarts in PAL when `0x88:E7` gets bit 7 or
  `0x88:FA` shows the other family, with its own family-change sequence
  and a `0x88:0A` write by `E7`. Implemented in d578d60 (2 polls instead
  of 8; `docs/composite.md §8.3`, §9).

- 3dcf44a test, composite first on a fresh module (previous sessions on
  S-video), PS1 then N64 swapped in: no sync for 144 s (`0x88:FA =
  ED/FD`), shutdown on close -5. The decoder keeps its S-video setting
  while the device has power, also across module reloads (and a replug
  in the VirtualBox menu does not cut power); the driver assumed
  composite after loading and skipped the S-video → composite sequence.
  In the recording Windows also retriggers the back end once right after
  that sequence (`0x98:F5 = E2`) and composite locks 0.6 s later; the
  driver skipped that retrigger on composite. Both fixed in d2fe44f.
---

## Maintainer comments

I'm sorry! I made a mistake in my previous test, I thought I changed the input to composite but I just copied your command and it was the S-Video one. Now I did it right. I tested first the N64 and then the PS1. N64 worked and PS1 didn't even open the ffplay output window. This test was done with the previous branch state.

ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
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
  10.55 A-V: -0.031 fd=   0 aq=   40KB vq= 1530KB sq=    0B f=0/0   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
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
^Cff5@ff5-VirtualBox:~/elgato-gchd/dkms$ ^C   0KB sq=    0B f=0/0   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 


ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|callbacks suppressed|bulk-IN first reads"
[45181.596855] usbcore: registered new interface driver elgato_gchd
[45185.291528] usb 1-2: new high-speed USB device number 42 using xhci_hcd
[45185.670721] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[45185.670742] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[45185.670748] usb 1-2: Product: GameCapture HD
[45185.670754] usb 1-2: Manufacturer: Elgato
[45185.670757] usb 1-2: SerialNumber: 04107F1267
[45185.677468] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[45185.679757] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[45193.097566] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[45193.097581] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[45193.097587] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[45203.802815] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[45203.975945] elgato_gchd 1-2:1.0: transcoder defaults loaded
[45204.037798] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[45204.168049] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x2e mode=0xe
[45204.170632] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0x6e
[45204.170635] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45204.236822] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x2e mode=0xe
[45204.239792] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[45204.239799] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45204.298917] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x3e mode=0xe
[45204.301319] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xfe
[45204.301322] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45204.410803] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[45204.421566] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[45204.423934] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[45204.423937] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45204.481532] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[45204.494455] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[45204.496659] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[45204.496662] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[45204.496664] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[45204.496666] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[45205.283373] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[45205.405467] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[45205.420508] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[45205.420709] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[45205.445118] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[45205.445717] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[45205.446420] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[45205.576601] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[45210.453996] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=22
[45215.478757] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[45220.488647] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=329 bytes=21561344 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=60
[45250.863886] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[45266.344046] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[45266.509400] elgato_gchd 1-2:1.0: transcoder defaults loaded
[45266.570541] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[45266.690391] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45266.692393] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[45266.692396] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45266.749112] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45266.751767] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[45266.751771] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45266.810499] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45266.813267] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[45266.813274] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45266.873635] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45266.875813] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[45266.875816] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45266.936862] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45266.939464] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e
[45266.939468] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45266.995596] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45266.997802] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45266.997805] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45267.055350] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45267.058183] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45267.058189] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45267.131431] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45267.134844] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45267.134851] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45267.193740] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45267.195845] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45267.195847] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45267.257833] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45267.260420] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45267.260425] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45267.327302] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45267.397296] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45267.461826] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45267.523508] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45267.588801] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45267.660729] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45267.723981] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45267.843981] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45267.910654] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45267.974980] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45268.035976] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45268.108931] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45268.174738] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45268.245567] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45268.424087] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45268.485523] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45268.546205] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45268.613032] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45268.690644] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45268.755445] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45268.826759] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45268.890062] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45268.954924] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45269.015964] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45269.081486] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45269.155806] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45269.250685] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45269.325497] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45269.397056] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45269.467355] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45269.530886] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45269.606958] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45269.683543] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45269.750920] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45269.818513] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45269.890008] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45269.954427] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45270.013777] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45270.135834] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45270.195941] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45270.254099] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45270.315831] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45270.384176] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45270.448485] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45270.507704] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45270.571186] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45270.636950] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45270.708449] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45270.777320] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45270.852140] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45270.917631] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45270.986542] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45271.049784] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45271.112578] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45271.174847] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45271.233653] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45271.311027] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45271.369608] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45271.428154] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45271.488205] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45271.545755] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45271.610198] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45271.671884] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45271.739718] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45271.744352] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45271.744363] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45271.812450] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45271.815652] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45271.815659] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45271.875233] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45271.878851] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45271.878863] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45271.939208] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45271.944383] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45271.944394] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45272.004646] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45272.007163] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45272.007168] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45272.067615] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45272.070461] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45272.070466] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45272.147739] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45272.153157] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45272.153170] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45272.217292] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45272.221029] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45272.221038] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45272.286943] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45272.290070] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45272.290075] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45272.348452] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45272.351262] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45272.351266] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45272.411926] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45272.541899] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45272.624291] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45272.689540] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45272.759321] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45272.825083] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45272.890786] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45272.954755] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45273.016421] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45273.079965] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45273.143364] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45273.207288] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45273.275268] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45273.337373] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45273.399280] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45273.458878] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45273.518328] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45273.576199] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45273.641986] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45273.702001] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45273.764097] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45273.827026] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45274.123108] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45274.185333] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45274.247908] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45274.314009] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45274.377344] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45274.440494] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45274.504167] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45274.567482] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45274.629196] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45274.693443] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45274.756146] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45274.828365] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45274.891506] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45274.957785] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45275.022708] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45275.094757] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45275.157305] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45275.218819] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45275.286418] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45275.361413] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45275.425859] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45275.491368] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45275.558791] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45275.639299] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45275.733337] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45275.796824] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45275.871879] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45275.938334] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45276.012562] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45276.086159] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45276.154806] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45276.220390] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45276.294178] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45276.364043] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45276.429998] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45276.492286] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45276.549577] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45276.611480] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45276.672636] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45276.737352] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45276.804603] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45276.808152] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45276.808159] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45276.899289] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45276.903246] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45276.903252] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45277.156840] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45277.163175] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45277.163182] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45277.230028] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45277.232958] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45277.232963] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45277.295506] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45277.298122] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45277.298125] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45277.365195] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45277.367999] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45277.368001] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45277.433995] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45277.436177] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45277.436180] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45277.493606] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45277.495963] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45277.495966] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45277.564990] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45277.567944] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45277.567948] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45277.631751] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45277.636204] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45277.636209] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45277.709218] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45277.781680] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45277.849065] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45277.922755] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45277.991442] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45278.062836] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45278.147801] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45278.215921] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45278.285780] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45278.347472] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45278.420135] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45278.486952] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45278.550665] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45278.622333] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45278.690100] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45278.754219] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45278.830953] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45278.899427] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45278.959964] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45279.027033] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45279.094263] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45279.160428] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45279.225733] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45279.296185] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45279.373873] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45279.433801] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45279.498935] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45279.564417] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45279.654012] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45279.723774] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45279.792964] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45279.864763] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45279.937686] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45280.015132] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45280.086230] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45280.147915] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45280.211170] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45280.276427] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45280.350093] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45280.424431] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45280.491192] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45280.567863] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45280.646259] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45280.716116] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45280.934754] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45280.997937] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45281.064246] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45281.132589] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45281.205325] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45281.278310] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45281.346261] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45281.413650] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45281.481342] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45281.549413] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45281.705763] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45281.780492] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45281.858174] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45281.861853] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45281.861860] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45281.939154] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45281.941602] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45281.941607] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45282.146732] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45282.148918] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45282.148920] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45282.213288] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45282.217845] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45282.217850] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45282.317866] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45282.321532] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45282.321537] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45282.385030] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45282.387623] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45282.387627] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45282.453376] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45282.455520] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45282.455523] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45282.523653] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45282.529742] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45282.529745] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45282.586029] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45282.588136] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45282.588138] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45282.648695] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45282.652462] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45282.652466] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45282.708810] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45282.782624] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45282.841924] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45282.913155] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45282.983211] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45283.047254] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45283.119388] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45283.190772] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45283.260930] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45283.334185] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45283.398452] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45283.465443] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45283.542291] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45283.607923] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45283.665595] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45283.726901] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45283.791823] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45283.858372] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45283.925870] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45283.996671] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45284.057955] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45284.129730] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45284.205032] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45284.278054] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45284.354462] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45284.421054] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45284.487645] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45284.561458] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45284.627393] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45284.700154] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45284.776073] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45284.848970] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45284.913196] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45284.984053] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45285.044211] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45285.108130] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45285.176172] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45285.236194] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45285.299347] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45285.362678] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45285.430925] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45285.521166] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45285.591929] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45285.651858] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45285.715009] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45285.774341] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45285.840386] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45285.911789] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45285.975715] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45286.038893] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45286.103359] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45286.174667] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45286.240301] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45286.312850] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45286.386142] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45286.453203] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45286.517594] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45286.580439] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45286.643020] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45286.711370] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45286.772627] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45286.840227] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45286.921776] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45286.925055] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45286.925062] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45286.981256] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45286.984250] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45286.984254] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45287.046471] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45287.049468] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45287.049473] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45287.120992] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45287.126470] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45287.126478] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45287.187403] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45287.191240] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45287.191246] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45287.256824] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45287.259577] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45287.259579] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45287.327130] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45287.329787] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45287.329792] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45287.397847] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45287.402139] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45287.402146] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45287.461933] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45287.464421] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45287.464426] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45287.529654] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45287.532475] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45287.532480] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45287.600153] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45287.674751] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45287.739280] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45287.820404] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45287.890222] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45287.970116] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45288.055688] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45288.130696] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45288.200842] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45288.271626] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45288.334427] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45288.394771] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45288.454444] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45288.533390] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45288.590680] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45288.654768] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45288.724084] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45288.792946] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45288.851970] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45288.922770] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45288.987850] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45289.051820] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45289.136117] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45289.222954] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45289.341275] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45289.413212] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45289.487224] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45289.560916] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45289.630867] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45289.702738] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45289.772837] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45289.844914] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45289.911451] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45289.991479] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45290.057970] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45290.128967] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45290.194012] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45290.269643] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45290.342823] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45290.412870] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45290.482578] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45290.545965] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45290.608680] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45290.674331] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45290.737197] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45290.804112] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45290.865070] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45290.931158] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45290.992053] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45291.061284] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45291.123113] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45291.190798] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45291.254722] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45291.316587] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45291.380561] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45291.444070] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45291.510204] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45291.569080] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45291.627230] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45291.687783] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45291.746681] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45291.809035] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45291.870302] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45291.929951] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45291.932455] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45291.932459] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45291.988999] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45291.991683] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45291.991687] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45292.054133] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45292.056465] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45292.056469] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45292.115709] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45292.118334] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45292.118339] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45292.191380] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45292.194272] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45292.194277] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45292.252994] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45292.255652] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45292.255657] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45292.312110] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45292.314702] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45292.314705] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45292.374887] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45292.377045] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45292.377047] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45292.436575] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45292.439415] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45292.439422] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45292.495365] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45292.497745] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45292.497748] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45292.557436] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45292.623969] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45292.684552] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45292.750135] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45292.811303] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45292.872998] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45292.933686] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45292.996766] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45293.055973] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45293.118826] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45293.183274] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45293.247060] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45293.317378] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45293.392183] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45293.451959] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45293.645037] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45293.718126] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45293.789096] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45293.858062] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45293.925674] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45293.998354] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45294.064723] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45294.124882] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45294.194696] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45294.263133] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45294.328024] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45294.398699] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45294.485028] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45294.544497] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45294.605568] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45294.666281] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45294.730154] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45294.796905] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45295.111386] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45295.173190] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45295.273666] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45295.350648] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45295.409517] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45295.480995] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45295.546003] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45295.610764] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45295.679956] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45295.757110] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45295.824563] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45295.886645] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45295.948681] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45296.008973] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45296.075835] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45296.138812] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45296.205565] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45296.275405] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45296.347492] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45296.419599] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45296.515588] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45296.583136] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45296.645359] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45296.711972] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45296.782390] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45296.856712] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45296.927008] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45296.986455] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45296.988832] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45296.988837] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45297.045318] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45297.049061] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45297.049065] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45297.121328] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45297.123696] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45297.123699] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45297.179699] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45297.182548] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45297.182551] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45297.250413] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45297.255524] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45297.255533] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45297.313606] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45297.316658] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45297.316664] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45297.373898] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45297.377427] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45297.377434] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45297.433562] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45297.435809] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45297.435812] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45297.498568] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45297.500915] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45297.500920] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45297.557132] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45297.559311] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45297.559314] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45297.615539] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45297.674264] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45297.756775] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45297.929752] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45298.166169] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45298.240672] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45298.306033] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45298.378905] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45298.440465] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45298.512382] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45298.585294] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45298.656972] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45298.717255] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45298.778436] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45298.843281] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45298.907407] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45298.968661] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45299.031339] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45299.096041] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45299.165434] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45299.235158] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45299.301075] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45299.375401] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45299.441582] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45299.505961] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45299.570404] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45299.639603] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45299.706451] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45299.783439] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45299.852175] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45299.915893] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45299.982959] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45300.051648] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45300.120834] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45300.187397] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45300.256707] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45300.322674] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45300.391546] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45300.465754] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45300.538284] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45300.625565] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45300.688549] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45300.753085] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45300.827255] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45300.902969] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45300.974648] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45301.045493] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45301.110084] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45301.174412] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45301.243973] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45301.302893] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45301.369786] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45301.439759] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45301.499326] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45301.561209] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45301.631195] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45301.692611] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45301.768072] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45301.844432] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45301.910132] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45301.980935] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45302.049182] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45302.052096] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45302.052104] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45302.111245] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45302.113630] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45302.113634] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45302.172875] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45302.177162] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45302.177172] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45302.239650] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45302.243406] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45302.243417] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45302.303492] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45302.306146] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45302.306151] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45302.364339] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45302.366514] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45302.366518] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45302.429311] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45302.432265] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45302.432270] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45302.497453] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45302.500417] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45302.500422] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45302.563397] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45302.566458] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45302.566464] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45302.633152] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45302.636286] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45302.636292] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45302.708079] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45302.774003] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45302.839060] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45302.898692] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45302.965881] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45303.034948] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45303.107148] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45303.176273] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45303.242717] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45303.321173] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45303.396733] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45303.475519] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45303.541600] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45303.599690] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45303.658421] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45303.718115] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45303.789752] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45303.857699] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45303.925097] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45303.991734] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45304.056305] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45304.130206] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45304.189862] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45304.250571] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45304.314253] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45304.375715] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45304.445361] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45304.519901] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45304.586234] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45304.647983] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45304.710931] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45304.775265] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45304.838334] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45304.906154] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45304.974462] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45305.046548] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45305.117720] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45305.230534] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45305.306965] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45305.372051] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45305.436654] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45305.497316] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45305.566969] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45305.631374] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45305.702234] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45305.805346] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45305.877127] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45305.949456] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45306.010536] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45306.074529] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45306.143675] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45306.230896] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45306.304426] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45306.388126] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45306.456156] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45306.528547] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45306.609107] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45306.672327] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45306.740175] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45306.807707] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45306.877780] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45306.953484] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45307.021112] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45307.094189] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45307.098625] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45307.098632] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45307.163031] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45307.166563] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45307.166571] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45307.228798] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45307.232606] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45307.232613] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45307.290705] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45307.294021] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45307.294029] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45307.354135] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45307.356731] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45307.356736] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45307.417697] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45307.420616] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45307.420619] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45307.475885] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45307.478517] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45307.478521] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45307.562238] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45307.565125] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45307.565132] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45307.620602] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45307.623194] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45307.623197] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45307.690442] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45307.693145] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45307.693149] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45307.759395] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45307.829237] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45307.899925] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45307.976465] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45308.049327] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45308.118582] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45308.188564] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45308.259588] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45308.339435] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45308.405727] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45308.465030] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45308.537419] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45308.603861] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45308.667441] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45308.732131] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45308.800819] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45308.868481] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45308.931112] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45308.993664] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45309.052018] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45309.122958] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45309.191634] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45309.268894] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45309.339676] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45309.404399] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45309.473046] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45309.536060] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45309.603640] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45309.662251] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45309.761929] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45310.018282] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45310.081373] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45310.149131] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45310.212575] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45310.279862] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45310.349744] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45310.412714] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45310.482302] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45310.543914] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45310.610375] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45310.680561] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45310.753704] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45310.820133] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45310.898560] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45310.961604] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45311.025063] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45311.090110] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45311.149322] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45311.208956] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45311.273085] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45311.332530] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45311.400863] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45311.473446] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45311.535528] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45311.596525] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45311.662109] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45311.724764] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45311.792649] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45311.868784] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45311.928172] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45312.010984] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45312.069797] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45312.147400] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45312.150489] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45312.150497] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45312.214796] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45312.217788] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45312.217795] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45312.276926] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45312.280277] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45312.280283] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45312.336449] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45312.340446] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45312.340453] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45312.399936] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45312.402751] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45312.402756] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45312.460920] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45312.464058] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45312.464063] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45312.532819] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45312.538141] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45312.538149] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45312.597556] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45312.600095] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45312.600099] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45312.659339] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45312.661653] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45312.661655] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45312.720205] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45312.723024] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee
[45312.723030] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[45312.782254] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45312.847569] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45312.919251] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45312.989667] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45313.057687] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45313.122008] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45313.189331] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45313.260381] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45313.318114] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45313.382057] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45313.459624] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45313.534183] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45313.604535] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45313.669263] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45313.742750] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45313.815106] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45313.894072] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45313.966148] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45314.029962] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45314.095281] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45314.161845] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45314.224976] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45314.284707] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45314.355717] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45314.421737] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45314.484522] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45314.554943] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45314.622041] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45314.755598] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45314.819022] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45314.886101] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45314.953959] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45315.026212] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45315.088898] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45315.157540] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45315.225765] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45315.290173] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45315.352151] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45315.416074] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45315.483563] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45315.552078] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45315.616372] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45315.682813] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45315.749319] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45315.813527] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45315.875413] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45315.934265] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45316.017387] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45316.092417] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45316.161384] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45316.233883] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45316.302668] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45316.373383] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45316.445962] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45316.508811] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[45316.574898] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[45316.669479] elgato_gchd 1-2:1.0: hardware shutdown on last close failed: -5; keeping initialized state
ff5@ff5-VirtualBox:~$ 
