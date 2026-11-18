# TASK T-020 — MPEG-TS stream with audio

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** TS works; family change (d578d60); composite polling retrigger (7d08899); hardware test
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

Build and load (once). `git log -1 --oneline` must show 7d08899 or later,
and the build must be run in the same directory as `insmod`:

```sh
cd ~/elgato-gchd && git fetch origin && git checkout feat/ts-audio && git pull
git log -1 --oneline
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# unplug and replug the device, then:
sudo insmod ./elgato_gchd.ko
```

Composite runs:

```sh
v4l2-ctl -d /dev/video0 -i 2
v4l2-ctl -d /dev/video0 -v pixelformat=MPEG
ffplay -f mpegts /dev/video0
```

S-video runs: the same with `-i 3`.

Note in your comment how you unplugged before `insmod`: cable pulled out
of the PC, or detached in the VirtualBox USB menu.

1. Composite, PS1 running: picture expected within a few seconds. Close.
2. Composite again (new ffplay), PS1: start ffplay with the PS1 off,
   power it on, wait until the game runs; press reset once, wait;
   power-cycle once, wait. Close.
3. Composite, N64: menu ↔ demo (480i ↔ 240p) a few times. Close.
4. Composite, PS1 again (after the N64 session; this failed last time).
5. S-video, PS1: as step 2.

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
- Correction: the "3dcf44a composite" test above was run with input 3
  (S-video) while the cables were on composite; it says nothing about
  composite.
- 3dcf44a test on composite: N64 locked at once (480i, `ring_dropped=0`).
  PS1 in the second session (after the N64): no sync for 50 s (`0x88:FA =
  ED/FD`); shutdown on close -5. In the recording Windows retriggers the
  back end on composite too whenever `0x98:F5` has bit 7 set while it
  waits for a signal (PS1 powering up: 128.99 s, 129.34 s, lock at
  129.44 s). The driver did this on S-video only; now on both (7d08899).
  The no-signal log line now shows `0x98:F5`.
---

## Maintainer comments

PS1 composite detected the video as NTSC so the aspect ratio was wrong every time. N64 was fine.

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
[mpegts @ 0x7ec684000c80] Packet corrupt (stream = 0, dts = 207162).
[mpegts @ 0x7ec684000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x7ec684000c80] Packet corrupt (stream = 1, dts = 185400).
Input #0, mpegts, from '/dev/video0':B vq=    0KB sq=    0B f=0/0   
  Duration: N/A, start: 0.500000, bitrate: N/A
  Program 1 
  Stream #0:0[0x1011]: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 25 fps, 50 tbr, 90k tbn
  Stream #0:1[0x10f]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
[mp2 @ 0x7ec6842c1380] Header missingB vq= 2011KB sq=    0B f=0/0   
[h264 @ 0x7ec6841e2ac0] cabac decode of qscale diff failed at 1 10  
[h264 @ 0x7ec6841e2ac0] error while decoding MB 1 10, bytestream -4
[mpegts @ 0x7ec684000c80] Packet corrupt (stream = 0, dts = 1269000).
[h264 @ 0x7ec6842da580] error while decoding MB 7 25, bytestream -8
[mpegts @ 0x7ec684000c80] PES packet size mismatchsq=    0B f=2/1   
[mpegts @ 0x7ec684000c80] Packet corrupt (stream = 1, dts = 1256760).
[mp2 @ 0x7ec6842c1380] Header missingB vq=    0KB sq=    0B f=2/2   
[mpegts @ 0x7ec684000c80] Packet corrupt (stream = 0, dts = 195150).
[mpegts @ 0x7ec684000c80] PES packet size mismatchsq=    0B f=3/2   
[mpegts @ 0x7ec684000c80] Packet corrupt (stream = 1, dts = 174600).
[mp2 @ 0x7ec6842c1380] Header missingB vq=    0KB sq=    0B f=3/2   
[mpegts @ 0x7ec684000c80] Packet corrupt (stream = 0, dts = 1189800).
[mpegts @ 0x7ec684000c80] PES packet size mismatchsq=    0B f=4/3   
[mpegts @ 0x7ec684000c80] Packet corrupt (stream = 1, dts = 1172520).
[mp2 @ 0x7ec6842c1380] Header missingB vq=    0KB sq=    0B f=4/4   
[mpegts @ 0x7ec684000c80] Packet corrupt (stream = 0, dts = 315270).
[h264 @ 0x7ec68430e000] error while decoding MB 40 23, bytestream -6
[mpegts @ 0x7ec684000c80] PES packet size mismatchsq=    0B f=5/4   
[mpegts @ 0x7ec684000c80] Packet corrupt (stream = 1, dts = 295560).
[mp2 @ 0x7ec6842c1380] Header missingB vq=    0KB sq=    0B f=5/4   
[mpegts @ 0x7ec684000c80] Packet corrupt (stream = 0, dts = 2629800).
[mpegts @ 0x7ec684000c80] PES packet size mismatchsq=    0B f=6/5   
[mpegts @ 0x7ec684000c80] Packet corrupt (stream = 1, dts = 2611080).
[mp2 @ 0x7ec6842c1380] Header missingB vq=    0KB sq=    0B f=6/6   
[mpegts @ 0x7ec684000c80] Packet corrupt (stream = 0, dts = 783738).
[h264 @ 0x7ec68430e000] error while decoding MB 18 24, bytestream -6
[mpegts @ 0x7ec684000c80] PES packet size mismatchsq=    0B f=7/6   
[mpegts @ 0x7ec684000c80] Packet corrupt (stream = 1, dts = 762120).
[mp2 @ 0x7ec6842c1380] Header missingB vq=    0KB sq=    0B f=7/6   
  15.08 A-V:  0.007 fd= 689 aq=    0KB vq=    0KB sq=    0B f=7/7   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 


ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|callbacks suppressed|bulk-IN first reads"
[45885.510857] usbcore: registered new interface driver elgato_gchd
[45890.243341] usb 1-2: new high-speed USB device number 43 using xhci_hcd
[45890.666998] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[45890.667020] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[45890.667026] usb 1-2: Product: GameCapture HD
[45890.667031] usb 1-2: Manufacturer: Elgato
[45890.667035] usb 1-2: SerialNumber: 04107F1267
[45890.673408] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[45890.676093] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[45897.162779] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[45897.162800] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[45897.162808] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[45910.709552] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[45911.159731] elgato_gchd 1-2:1.0: transcoder defaults loaded
[45911.226799] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[45911.484582] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[45911.567753] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[45911.614915] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[45911.622747] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[45911.625592] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[45911.625596] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[45911.625598] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[45911.625600] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[45912.401696] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[45913.117686] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[45913.133758] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[45913.134073] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[45913.139119] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[45913.139759] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[45913.140510] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[45913.256966] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[45914.611570] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=7 CA=fe E7=79); configuring again
[45916.101050] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[45916.101297] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[45916.102888] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[45916.103421] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[45916.104036] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[45916.226936] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[45916.226938] elgato_gchd 1-2:1.0: analog mode now 576i
[45921.134937] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=39
[45926.162394] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=37
[45931.023218] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=f CA=ee E7=f9); configuring again
[45931.190610] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=37
[45932.136699] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[45932.136912] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[45932.138319] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[45932.138940] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[45932.139576] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[45932.847300] elgato_gchd 1-2:1.0: state-change timeout: current=0x01 expected=0x02 state=0x0001 completion=0x011a
[45932.847631] elgato_gchd 1-2:1.0: START failed: ret=-110 state=0x0001 (r=0) enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[45932.847635] elgato_gchd 1-2:1.0: capture start transition failed: -110
[45932.877898] elgato_gchd 1-2:1.0: analog mode change failed: -110
[45933.141325] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[45933.160214] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[45933.170395] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[45933.174375] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[45933.174378] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[45933.174380] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[45933.174383] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[45933.963470] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[45933.979949] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[45933.980262] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0002
[45933.990994] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[45933.992059] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[45933.992517] elgato_gchd 1-2:1.0: START before SCMD: state=0x0002 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[45933.994139] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[45935.049880] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=f CA=66 E7=d9); configuring again
[45936.025300] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[45936.025496] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[45936.026700] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[45936.027319] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[45936.027702] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[45936.171114] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[45936.171117] elgato_gchd 1-2:1.0: analog mode now 576i
[45941.035016] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[45946.060587] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[45948.832256] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=f CA=ee E7=f9); configuring again
[45949.758606] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[45949.758890] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[45949.760127] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[45949.760952] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[45949.761357] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[45949.864482] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[45949.864484] elgato_gchd 1-2:1.0: analog mode now 240p
[45952.790490] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=6 CA=66 E7=e9); configuring again
[45953.910831] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[45953.911035] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[45953.912285] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[45953.913243] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[45953.913799] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[45954.056694] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[45954.056697] elgato_gchd 1-2:1.0: analog mode now 576i
[45958.920346] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[45963.946548] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[45968.974542] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[45974.011730] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=448 bytes=29360128 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[45979.049476] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=563 bytes=36896768 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[45982.369077] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=6 CA=e6 E7=e9); configuring again
[45983.824599] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[45983.824806] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[45983.826656] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[45983.827423] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[45983.827925] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[45983.947191] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[45983.947194] elgato_gchd 1-2:1.0: analog mode now 240p
[45988.825661] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[45991.660624] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[45993.090992] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[45993.091195] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[45993.092501] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[45993.093451] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[45993.093979] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[45993.213539] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[45993.213542] elgato_gchd 1-2:1.0: analog mode now 480i
[45998.134978] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[46003.157432] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[46008.178425] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
ff5@ff5-VirtualBox:~$ 


S-Video was the same as composite, NTSC was detected instead of PAL and N64 ran fine.

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
[mpegts @ 0x7df6b4000c80] Packet corrupt (stream = 0, dts = 195150).
[mpegts @ 0x7df6b4000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x7df6b4000c80] Packet corrupt (stream = 1, dts = 174600).
Input #0, mpegts, from '/dev/video0':B vq=    0KB sq=    0B f=0/0   
  Duration: N/A, start: 0.500000, bitrate: N/A
  Program 1 
  Stream #0:0[0x1011]: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 25 fps, 50 tbr, 90k tbn
  Stream #0:1[0x10f]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
[mp2 @ 0x7df6b42c3180] Header missingB vq= 2013KB sq=    0B f=0/0   
[mpegts @ 0x7df6b4000c80] Packet corrupt (stream = 0, dts = 1247400).
[mpegts @ 0x7df6b4000c80] PES packet size mismatchsq=    0B f=1/1   
[mpegts @ 0x7df6b4000c80] Packet corrupt (stream = 1, dts = 1226520).
[mp2 @ 0x7df6b42c3180] Header missingB vq=  881KB sq=    0B f=1/1   
[mpegts @ 0x7df6b4000c80] Packet corrupt (stream = 0, dts = 291246).
[mpegts @ 0x7df6b4000c80] PES packet size mismatchsq=    0B f=3/2   
[mpegts @ 0x7df6b4000c80] Packet corrupt (stream = 1, dts = 271800).
[mp2 @ 0x7df6b42c3180] Header missingB vq=    0KB sq=    0B f=3/2   
[mpegts @ 0x7df6b4000c80] Packet corrupt (stream = 0, dts = 1269000).
[mpegts @ 0x7df6b4000c80] Packet corrupt (stream = 0, dts = 291246).
[h264 @ 0x7df6b42ea1c0] error while decoding MB 32 21, bytestream -32
[mpegts @ 0x7df6b4000c80] PES packet size mismatchsq=    0B f=5/4   
[mpegts @ 0x7df6b4000c80] Packet corrupt (stream = 1, dts = 271800).
[mp2 @ 0x7df6b42c3180] Header missingB vq=    0KB sq=    0B f=5/4   
[mpegts @ 0x7df6b4000c80] Packet corrupt (stream = 0, dts = 3868200).
[h264 @ 0x7df6b40e9b00] error while decoding MB 9 11, bytestream -6
[mpegts @ 0x7df6b4000c80] PES packet size mismatchsq=    0B f=6/5   
[mpegts @ 0x7df6b4000c80] Packet corrupt (stream = 1, dts = 3848760).
[mp2 @ 0x7df6b42c3180] Header missingB vq=    0KB sq=    0B f=6/6   
[mpegts @ 0x7df6b4000c80] Packet corrupt (stream = 0, dts = 831786).
   8.37 A-V: -0.042 fd= 398 aq=    0KB vq=    0KB sq=    0B f=7/7   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 


ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|callbacks suppressed|bulk-IN first reads"
[46186.787377] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[46186.787391] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[46186.787396] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[46201.428160] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[46201.746055] elgato_gchd 1-2:1.0: transcoder defaults loaded
[46201.807332] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[46201.925830] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[46201.927987] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0x6e 0x98:F5=0x62
[46201.927989] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[46201.989141] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x2e mode=0xe
[46201.991949] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0x6e 0x98:F5=0x62
[46201.991955] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[46202.048007] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x26 mode=0x6
[46202.057688] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x26 masked_mode=0x6
[46202.060267] elgato_gchd 1-2:1.0: analog format status 0x6e not locked yet
[46202.060270] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[46202.120312] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[46202.132521] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[46202.134624] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[46202.134626] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[46202.134628] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[46202.134630] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[46202.892766] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[46203.589958] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[46203.605402] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[46203.605601] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[46203.610767] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[46203.611297] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[46203.611750] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[46203.711290] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[46205.314937] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=6 CA=66 E7=d9); configuring again
[46206.350808] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[46206.351229] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[46206.352552] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[46206.353225] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x576 fps=50/1 interlaced=1
[46206.353762] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[46206.497039] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[46206.497042] elgato_gchd 1-2:1.0: analog mode now 576i
[46211.368346] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=40
[46216.400684] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=39
[46221.026823] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=e CA=ee E7=f9); configuring again
[46221.429935] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=39
[46222.137224] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[46222.137446] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[46222.138478] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[46222.139023] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[46222.139471] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[46222.244481] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[46222.244486] elgato_gchd 1-2:1.0: analog mode now 240p
[46224.909366] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=6 CA=66 E7=e9); configuring again
[46226.019578] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[46226.019766] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[46226.020794] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[46226.021445] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x576 fps=50/1 interlaced=1
[46226.021810] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[46226.165628] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[46226.165631] elgato_gchd 1-2:1.0: analog mode now 576i
[46231.035169] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[46236.064139] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[46239.588267] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=e CA=8e E7=89); configuring again
[46240.604636] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[46240.604913] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[46240.606178] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[46240.606998] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[46240.607533] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[46240.710542] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[46240.710545] elgato_gchd 1-2:1.0: analog mode now 240p
[46243.409729] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=e CA=66 E7=d9); configuring again
[46244.476420] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[46244.476656] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[46244.477682] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[46244.478351] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x576 fps=50/1 interlaced=1
[46244.479256] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[46244.622607] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[46244.622613] elgato_gchd 1-2:1.0: analog mode now 576i
[46249.490194] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[46254.521342] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[46259.551051] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[46264.588890] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=448 bytes=29360128 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[46269.627523] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=563 bytes=36896768 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[46274.666165] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=678 bytes=44433408 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[46279.705147] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=793 bytes=51970048 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[46284.743256] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=908 bytes=59506688 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[46287.078117] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=e CA=ee E7=e9); configuring again
[46287.963198] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[46287.963389] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[46287.964947] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[46287.965788] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[46287.966175] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[46288.085832] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[46288.085834] elgato_gchd 1-2:1.0: analog mode now 240p
[46292.964656] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[46296.345501] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[46297.650284] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[46297.650588] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[46297.651998] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[46297.652459] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=1
[46297.653198] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[46297.773299] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[46297.773303] elgato_gchd 1-2:1.0: analog mode now 480i
[46302.694589] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
ff5@ff5-VirtualBox:~$ 
