# TASK T-020 — MPEG-TS stream with audio

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** TS works; PS1 starts in 576i; console swap fix (2b69662); hardware test
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

Build and load (once). `git log -1 --oneline` must show 2b69662 or later,
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

ffplay may keep running while you switch consoles or press reset (the
window keeps the size of the first console's frames).

1. Composite: start ffplay with the PS1 off, power it on. Expected:
   `configuring 576i` after the waiting line; picture in the right aspect.
2. Same session: reset once, power-cycle once. Expected: one restart each
   to 576i.
3. Same session: swap in the N64 (this failed last time: `START failed:
   ret=-110`). Expected: `decoder status changed ...`, then `analog mode
   now 240p`/`480i` without a START failure; then menu ↔ demo.
4. Same session: swap back to the PS1 (running game). Expected: one
   restart to 576i.
5. S-video: steps 1 to 4 again.

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
- 7d08899 test (composite and S-video; PS1 power-on, reset, power cycle,
  then N64; ffplay kept running): the PS1 locks on composite now (the
  back-end retrigger while polling ran: `0x98:F5 = F2, E2`). Each PS1
  start gave 240p first (`E7 = 59`, 60 Hz with PAL colour while the PS1
  boots), then the family change to 576i 1.5-3 s later; the driver
  followed every change. ffplay sized its window from the first 720×480
  frames and showed the later 576-line frames squeezed. One family change
  triggered during the reset blanking configured 240p and START timed out
  (-110): the source changed to 50 Hz during the setup. 33c23cf waits up
  to 5 s on such a 60 Hz signal (start and family change) and checks the
  source again before START after a restart.
- 41cd5c1 test (S-video; PS1 power-on, resets, then N64; ffplay kept
  running): PS1 started directly in 576i; the wait ended when the PS1
  switched to 50 Hz (3.5 s at power-on, about 4 s after a reset: the
  PS1's own boot time, not a fixed delay). In-game mode changes
  (576i ↔ 288p, 480i ↔ 240p) never wait. The N64 swap (signal drop, then
  60 Hz) failed START (-110) once before detection recovered: the decoder
  still held the 50 Hz apply block. Fixed in 2b69662 (apply block of the
  locked family after STOP, as recorded). Maintainer request: no wait when
  the source changes family without a signal drop; done in 2b69662.
  N64 picture distorted in ffplay because the window kept the PS1's
  576-line size; the guvcview/OBS service (T-060) will keep one output
  size (TODO).
---

## Maintainer comments

S-Video:

'''console
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
Input #0, mpegts, from '/dev/video0':B vq=    0KB sq=    0B f=0/0   
  Duration: N/A, start: 0.500000, bitrate: N/A
  Program 1 
  Stream #0:0[0x1011]: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x576 [SAR 12:11 DAR 15:11], 25 fps, 50 tbr, 90k tbn
  Stream #0:1[0x10f]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
[mpegts @ 0x70db3c000c80] Packet corrupt (stream = 0, dts = 1207800).
[h264 @ 0x70db3c2f9980] error while decoding MB 13 23, bytestream -102
[mpegts @ 0x70db3c000c80] PES packet size mismatchsq=    0B f=1/0   
[mpegts @ 0x70db3c000c80] Packet corrupt (stream = 1, dts = 1191960).
[mp2 @ 0x70db3c2e66c0] Header missingB vq=    0KB sq=    0B f=1/0   
[mpegts @ 0x70db3c000c80] Packet corrupt (stream = 0, dts = 1182600).
[mpegts @ 0x70db3c000c80] PES packet size mismatchsq=    0B f=2/1   
[mpegts @ 0x70db3c000c80] Packet corrupt (stream = 1, dts = 1168200).
[mp2 @ 0x70db3c2e66c0] Header missingB vq=    0KB sq=    0B f=2/1   
[mpegts @ 0x70db3c000c80] Packet corrupt (stream = 0, dts = 3061800).
[h264 @ 0x70db3c1eb500] error while decoding MB 9 17, bytestream -12
[mpegts @ 0x70db3c000c80] PES packet size mismatchsq=    0B f=3/2   
[mpegts @ 0x70db3c000c80] Packet corrupt (stream = 1, dts = 3043080).
[mp2 @ 0x70db3c2e66c0] Header missingB vq=    0KB sq=    0B f=3/3   
[mpegts @ 0x70db3c000c80] Packet corrupt (stream = 0, dts = 783738).
[h264 @ 0x70db3c1eb500] error while decoding MB 21 26, bytestream -14
[mpegts @ 0x70db3c000c80] PES packet size mismatchsq=    0B f=4/3   
[mpegts @ 0x70db3c000c80] Packet corrupt (stream = 1, dts = 762120).
[mp2 @ 0x70db3c2e66c0] Header missingB vq=    0KB sq=    0B f=4/4   
[mpegts @ 0x70db3c000c80] Packet corrupt (stream = 0, dts = 3084036).
[mpegts @ 0x70db3c000c80] PES packet size mismatchsq=    0B f=5/4   
[mpegts @ 0x70db3c000c80] Packet corrupt (stream = 1, dts = 3077640).
[mp2 @ 0x70db3c2e66c0] Header missingB vq=    0KB sq=    0B f=5/4   
   5.49 A-V: -0.030 fd= 310 aq=    0KB vq=    0KB sq=    0B f=5/5   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 


ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|callbacks suppressed|bulk-IN first reads"
[sudo] password for ff5:           
[48374.216853] usbcore: registered new interface driver elgato_gchd
[48380.924466] usb 1-2: new high-speed USB device number 45 using xhci_hcd
[48381.322403] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[48381.322425] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[48381.322430] usb 1-2: Product: GameCapture HD
[48381.322435] usb 1-2: Manufacturer: Elgato
[48381.322439] usb 1-2: SerialNumber: 04107F1267
[48381.326677] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[48381.330080] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[48394.280449] elgato_gchd 1-2:1.0: input switch requested: old=0 new=3 hw_initialized=0 streaming=0
[48394.280455] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[48394.280456] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[48412.823224] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[48413.074644] elgato_gchd 1-2:1.0: transcoder defaults loaded
[48413.132335] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[48413.257719] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[48413.259916] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e 0x98:F5=0x62
[48413.259918] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[48413.319204] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[48413.321521] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x62
[48413.321523] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[48413.383914] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[48413.386225] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x62
[48413.386228] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[48413.448103] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[48413.450736] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x62
[48413.450739] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[48413.516922] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[48413.521446] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x62
[48413.521456] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[48413.593907] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[48413.596571] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x62
[48413.596577] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[48413.663468] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[48413.665830] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x62
[48413.665836] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[48413.726788] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[48413.729136] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x62
[48413.729138] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[48413.788814] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[48413.791238] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x62
[48413.791240] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[48413.851533] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[48413.854207] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x62
[48413.854211] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[48413.941746] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[48414.008893] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[48414.074144] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[48414.133261] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1f mode=0xf
[48414.199871] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[48414.270204] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x7e mode=0xe
[48414.340174] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1e mode=0xe
[48414.400092] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[48414.466620] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x26 mode=0x6
[48414.474713] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48414.476817] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[48414.534515] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48414.543737] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48414.545886] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[48414.602375] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48414.611976] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48414.614354] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[48414.686028] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48414.695079] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48414.697357] elgato_gchd 1-2:1.0: 60 Hz signal with PAL colour (0x88:E7=59); waiting up to 5000 ms for 50 Hz
[48414.756317] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48414.766423] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48414.827996] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48414.836597] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48414.904109] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48414.914330] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48414.978008] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48414.987036] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48415.050943] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48415.061031] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48415.120769] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48415.129773] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48415.191774] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48415.200273] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48415.269495] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48415.278639] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48415.344690] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48415.353816] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48415.412970] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48415.422073] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48415.480474] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48415.489447] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48415.560011] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48415.570633] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48415.636027] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48415.645364] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48415.715607] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48415.724675] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48415.787000] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48415.799534] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48415.867868] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48415.877447] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48415.935666] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48415.944474] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48416.005968] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48416.018217] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48416.090215] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48416.102311] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48416.166385] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48416.177077] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48416.237286] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48416.248937] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48416.311465] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48416.321206] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48416.379540] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48416.389150] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48416.447697] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48416.456274] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48416.520897] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48416.532185] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48416.598831] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48416.610666] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48416.674753] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48416.686335] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48416.749541] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48416.758387] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48416.824967] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48416.837040] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48416.969059] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48416.980113] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48417.113489] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48417.127445] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48417.188007] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48417.197088] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48417.259508] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48417.269791] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48417.331287] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48417.339727] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48417.422229] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48417.433769] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48417.498368] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48417.508241] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48417.578938] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48417.587823] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48417.655434] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48417.668542] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48417.729865] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48417.740858] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48417.833026] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48417.845146] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48417.981456] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[48417.993806] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[48418.061276] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x74 mode=0x4
[48418.123049] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0e mode=0xe
[48418.193465] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1f mode=0xf
[48418.259736] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0f mode=0xf
[48418.262552] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xee 0x98:F5=0x62
[48418.262556] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[48418.321918] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1f mode=0xf
[48418.325352] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xe6 0x98:F5=0x62
[48418.325358] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[48418.384824] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x07 mode=0x7
[48418.393445] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x17 masked_mode=0x7
[48418.395629] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[48418.395632] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[48418.451836] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x07 mode=0x7
[48418.462495] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x07 masked_mode=0x7
[48418.464630] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[48418.464633] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[48418.527491] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x17 mode=0x7
[48418.537209] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x07 masked_mode=0x7
[48418.539352] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[48418.539354] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[48418.605035] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x17 mode=0x7
[48418.616388] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x07 masked_mode=0x7
[48418.618610] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[48418.618614] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[48418.691317] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x17 mode=0x7
[48418.702915] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x07 masked_mode=0x7
[48418.705838] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[48418.705845] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[48418.770857] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x17 mode=0x7
[48418.783846] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x07 masked_mode=0x7
[48418.786321] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[48418.786324] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[48418.848941] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x17 mode=0x7
[48418.859186] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x07 masked_mode=0x7
[48418.862309] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[48418.862313] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[48418.928630] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x17 mode=0x7
[48418.939122] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x07 masked_mode=0x7
[48418.941684] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[48418.941690] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[48418.941692] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x576 fps=50/1 interlaced=1
[48418.941694] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x576 fps=50/1 interlaced=1
[48419.731866] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[48419.941095] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[48419.956369] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[48419.956590] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[48419.961714] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[48419.962217] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x576 fps=50/1 interlaced=1
[48419.962670] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[48420.102514] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[48424.975601] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=21
[48430.004020] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[48434.567839] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=e CA=ee E7=f9); configuring again
[48435.032212] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[48435.544841] elgato_gchd 1-2:1.0: 60 Hz signal with PAL colour (0x88:E7=59); waiting up to 5000 ms for 50 Hz
[48439.701627] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[48439.702073] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[48439.707941] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[48439.708567] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x576 fps=50/1 interlaced=1
[48439.709088] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[48439.847585] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[48439.847589] elgato_gchd 1-2:1.0: analog mode now 576i
[48444.717388] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48449.748498] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48452.404723] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=e CA=ee E7=f9); configuring again
[48453.286807] elgato_gchd 1-2:1.0: 60 Hz signal with PAL colour (0x88:E7=59); waiting up to 5000 ms for 50 Hz
[48457.500043] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[48457.500289] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[48457.505763] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[48457.506245] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x576 fps=50/1 interlaced=1
[48457.506708] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[48457.645919] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[48457.645922] elgato_gchd 1-2:1.0: analog mode now 576i
[48462.514139] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48467.544985] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48472.581622] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48477.619410] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=448 bytes=29360128 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48482.658067] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=563 bytes=36896768 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48487.696657] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=678 bytes=44433408 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48490.740746] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=6 CA=e6 E7=e9); configuring again
[48492.125510] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[48492.125736] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[48492.131724] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[48492.132426] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[48492.132893] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[48492.231557] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[48492.231562] elgato_gchd 1-2:1.0: analog mode now 240p
[48497.154588] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48499.968922] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[48501.369536] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[48501.369908] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[48501.375213] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[48501.375798] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=1
[48501.376215] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[48501.475465] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[48501.475468] elgato_gchd 1-2:1.0: analog mode now 480i
[48506.395787] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48511.418979] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48516.441754] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48521.465266] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48526.486922] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48531.508928] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=680 bytes=44564480 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48535.182207] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=6 CA=ee E7=d9); configuring again
[48536.547958] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=774 bytes=50724864 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48542.118976] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[48542.119733] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[48542.125308] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[48542.126018] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x576 fps=50/1 interlaced=1
[48542.126555] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[48542.264801] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[48542.264803] elgato_gchd 1-2:1.0: analog mode now 576i
[48547.135894] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
ff5@ff5-VirtualBox:~$ 
'''

Composite:

'''console
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
  Stream #0:0[0x1011]: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x576 [SAR 12:11 DAR 15:11], 25 fps, 50 tbr, 90k tbn
  Stream #0:1[0x10f]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
[mpegts @ 0x77da20000c80] Packet corrupt (stream = 0, dts = 1179000).
[h264 @ 0x77da2000c5c0] error while decoding MB 6 26, bytestream -43
[mpegts @ 0x77da20000c80] PES packet size mismatchsq=    0B f=1/0   
[mpegts @ 0x77da20000c80] Packet corrupt (stream = 1, dts = 1168200).
[mp2 @ 0x77da21677640] Header missingB vq=    0KB sq=    0B f=1/0   
[mpegts @ 0x77da20000c80] Packet corrupt (stream = 0, dts = 1319400).
[mpegts @ 0x77da20000c80] PES packet size mismatchsq=    0B f=2/1   
[mpegts @ 0x77da20000c80] Packet corrupt (stream = 1, dts = 1302120).
[mp2 @ 0x77da21677640] Header missingB vq=    0KB sq=    0B f=2/1   
[mpegts @ 0x77da20000c80] Packet corrupt (stream = 0, dts = 2133000).
[mpegts @ 0x77da20000c80] PES packet size mismatchsq=    0B f=3/2   
[mpegts @ 0x77da20000c80] Packet corrupt (stream = 1, dts = 2112120).
[mp2 @ 0x77da21677640] Header missingB vq=    0KB sq=    0B f=3/3   
[mpegts @ 0x77da20000c80] Packet corrupt (stream = 0, dts = 771726).
[mpegts @ 0x77da20000c80] PES packet size mismatchsq=    0B f=4/3   
[mpegts @ 0x77da20000c80] Packet corrupt (stream = 1, dts = 751320).
[mp2 @ 0x77da21677640] Header missingB vq=    0KB sq=    0B f=4/3   
[mpegts @ 0x77da20000c80] Packet corrupt (stream = 0, dts = 3005958).
[mpegts @ 0x77da20000c80] PES packet size mismatchsq=    0B f=5/4   
[mpegts @ 0x77da20000c80] Packet corrupt (stream = 1, dts = 3006360).
[mp2 @ 0x77da21677640] Header missingB vq=    0KB sq=    0B f=5/5   
[mpegts @ 0x77da20000c80] Packet corrupt (stream = 0, dts = 309264).
[h264 @ 0x77da2031a840] error while decoding MB 30 4, bytestream -22
[mpegts @ 0x77da20000c80] PES packet size mismatchsq=    0B f=6/5   
[mpegts @ 0x77da20000c80] Packet corrupt (stream = 1, dts = 286920).
[mp2 @ 0x77da21677640] Header missingB vq=    0KB sq=    0B f=6/5   
  12.35 A-V:  0.086 fd= 899 aq=    0KB vq=    0KB sq=    0B f=6/6   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 


ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|callbacks suppressed|bulk-IN first reads"
[48680.475995] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[48680.476008] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[48680.476014] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[48696.219511] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[48696.740719] elgato_gchd 1-2:1.0: transcoder defaults loaded
[48696.803386] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[48697.176509] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[48697.178633] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x62
[48697.178635] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[48697.299888] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[48697.304612] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x62
[48697.304619] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[48697.368385] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[48697.371805] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x62
[48697.371808] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[48697.437732] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[48697.440365] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x62
[48697.440374] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[48697.501023] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[48697.503812] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x62
[48697.503815] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[48697.568998] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[48697.571212] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x62
[48697.571216] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[48697.642408] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[48697.644737] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x62
[48697.644741] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[48697.700791] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[48697.702998] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x62
[48697.703000] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[48697.768876] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[48697.771662] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x62
[48697.771668] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[48697.829227] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[48697.831775] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x62
[48697.831779] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[48697.895071] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[48697.967219] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[48698.039637] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[48698.116361] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[48698.179657] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[48698.199168] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x0f mode=0xf
[48698.262884] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x0e mode=0xe
[48698.323160] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x3e mode=0xe
[48698.395707] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x2e mode=0xe
[48698.460605] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[48698.481943] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x26 mode=0x6
[48698.491318] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x26 masked_mode=0x6
[48698.493807] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[48698.564645] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48698.576550] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48698.578851] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[48698.652195] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48698.664802] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48698.667109] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[48698.740372] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48698.763320] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48698.766451] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[48698.831615] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48698.842350] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48698.844740] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[48698.906891] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48698.920425] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48698.922950] elgato_gchd 1-2:1.0: 60 Hz signal with PAL colour (0x88:E7=59); waiting up to 5000 ms for 50 Hz
[48699.000979] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48699.016371] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48699.083962] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48699.098439] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48699.169960] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48699.185015] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48699.257554] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48699.281183] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48699.343736] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48699.354185] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48699.425509] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48699.438033] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48699.505807] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48699.517628] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48699.582941] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48699.595367] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48699.660861] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48699.672035] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48699.734100] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48699.745211] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48699.811484] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48699.827187] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48699.893601] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48699.906625] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48699.967229] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48699.977628] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48700.050768] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48700.067379] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48700.138928] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48700.151423] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48700.225760] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48700.247231] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48700.314157] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48700.330663] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48700.400386] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48700.411061] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48700.477699] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48700.489761] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48700.550758] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48700.563458] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48700.631407] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48700.648927] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48700.713123] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48700.729250] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48700.793915] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48700.806078] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48700.866213] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48700.875447] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48700.950589] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48700.967483] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48701.026773] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48701.037203] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48701.102983] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48701.126441] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48701.196837] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48701.211358] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48701.339092] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48701.348716] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48701.407948] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48701.416604] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48701.478671] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48701.488098] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48701.555074] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48701.566297] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48701.633405] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48701.649377] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48701.719631] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48701.747106] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48701.812229] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48701.823091] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48701.900034] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48701.909982] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48701.977722] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48701.988746] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48702.048731] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[48702.060242] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[48702.127411] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[48702.146047] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x26 mode=0x6
[48702.155106] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x36 masked_mode=0x6
[48702.220131] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x0f mode=0xf
[48702.224664] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0x66 0x98:F5=0x62
[48702.224681] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[48702.288891] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x0f mode=0xf
[48702.292041] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xee 0x98:F5=0x62
[48702.292049] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[48702.486971] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[48702.509021] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x0f mode=0xf
[48702.511292] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xee 0x98:F5=0xe2
[48702.511297] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[48702.590859] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x07 mode=0x7
[48702.615883] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[48702.619649] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[48702.619662] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[48702.691043] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[48702.709768] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[48702.719524] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[48702.721735] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[48702.721739] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[48702.793660] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x07 mode=0x7
[48702.814031] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[48702.818378] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[48702.818390] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[48702.888004] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[48702.897698] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[48702.899860] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[48702.899863] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[48702.961275] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[48702.972827] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[48702.974961] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[48702.974963] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[48702.974965] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[48702.974967] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[48703.810740] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[48704.037808] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[48704.054811] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[48704.055027] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[48704.062540] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[48704.063385] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[48704.063826] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[48704.180600] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[48709.088870] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=22
[48714.116139] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[48718.382555] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=e CA=ee E7=89); configuring again
[48719.145805] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=328 bytes=21495808 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=39
[48719.305791] elgato_gchd 1-2:1.0: 60 Hz signal with PAL colour (0x88:E7=59); waiting up to 5000 ms for 50 Hz
[48723.459337] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[48723.459601] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[48723.465172] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[48723.465687] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[48723.466129] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[48723.605294] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[48723.605297] elgato_gchd 1-2:1.0: analog mode now 576i
[48728.467397] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48733.494313] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48737.709708] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=f CA=ee E7=f9); configuring again
[48738.494485] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=326 bytes=21364736 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48738.546310] elgato_gchd 1-2:1.0: 60 Hz signal with PAL colour (0x88:E7=59); waiting up to 5000 ms for 50 Hz
[48742.942846] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[48742.943410] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[48742.948921] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[48742.949714] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[48742.950392] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[48743.089137] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[48743.089140] elgato_gchd 1-2:1.0: analog mode now 576i
[48747.951916] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48752.978625] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48758.015970] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48763.056896] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=448 bytes=29360128 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48766.211724] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=f CA=ce E7=f9); configuring again
[48767.912848] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[48767.913278] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[48767.918987] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[48767.919618] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[48767.920113] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[48768.035606] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[48768.035608] elgato_gchd 1-2:1.0: analog mode now 240p
[48772.957757] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48775.584106] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[48777.125973] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[48777.126250] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[48777.132855] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[48777.133846] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[48777.134452] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[48777.248798] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[48777.248801] elgato_gchd 1-2:1.0: analog mode now 480i
[48782.171257] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48787.193149] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48792.215223] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48797.239747] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48802.264314] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48807.288200] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=680 bytes=44564480 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48810.112289] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=6 CA=ee E7=d9); configuring again
[48811.669172] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[48811.669414] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[48811.675360] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[48811.675873] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[48811.676440] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[48811.791914] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[48811.791917] elgato_gchd 1-2:1.0: analog mode now 240p
[48814.199191] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=7 CA=fe E7=f9); configuring again
[48815.675356] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[48815.675671] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[48815.681014] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[48815.681585] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[48815.682057] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[48815.821583] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[48815.821586] elgato_gchd 1-2:1.0: analog mode now 576i
[48820.685785] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[48825.712164] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
ff5@ff5-VirtualBox:~$ 
'''
