# TASK T-020 — MPEG-TS stream with audio

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** TS works; PS1 locks on composite and S-video; 60 Hz hold at PS1 start (33c23cf); hardware test
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

Build and load (once). `git log -1 --oneline` must show 41cd5c1 or later,
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

ffplay may keep running while you switch consoles or press reset.

1. Composite: start ffplay with the PS1 off, power it on. Expected: the
   ffplay window opens only once the PS1 is in 50 Hz, with the right
   aspect (`60 Hz signal with PAL colour ...; waiting up to 5000 ms` in
   dmesg, then `configuring 576i`).
2. Same session: press reset, wait; power-cycle, wait. Expected: one
   restart per reset (`decoder status changed ...`, the waiting line,
   `analog mode now 576i`). ffplay keeps its first window size, so check
   the aspect after closing and starting ffplay again while the game runs.
3. Same session: N64 instead of the PS1 (menu ↔ demo).
4. S-video: steps 1 to 3 again.

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
---

## Maintainer comments

I only tested S-Video with both PS1 and N64 while keeping ffplay open. PS1 opened right in PAL mode, but now N64 has a distorted look I guess because ffplay can't change it's mode. Now it seems to be working fine, but I'm concerned with some quality of life things:
- Isn't the PAL delay too long? Isn't there a better way to do it? I mean it works fine now, but I wonder if some frames are being lost because of this delay. I thought that upon first detection the delay needs to be longer, but for games that switch modes it might be too long, so in those cases a delay might not be required and it can be assumed that if a signal doesn't drop for long then the output remains in either PAL or NTSC.
- Can we fix this when we make the driver compatible with UVC compatible programs?

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
[h264 @ 0x71cc1c303780] error while decoding MB 20 35, bytestream -10
[mpegts @ 0x71cc1c000c80] PES packet size mismatchsq=    0B f=1/0   
[mpegts @ 0x71cc1c000c80] Packet corrupt (stream = 1, dts = 1215720).
[mp2 @ 0x71cc1c2e9280] Header missingB vq=    0KB sq=    0B f=1/0   
[mpegts @ 0x71cc1c000c80] Packet corrupt (stream = 0, dts = 1182600).
[mpegts @ 0x71cc1c000c80] PES packet size mismatchsq=    0B f=2/1   
[mpegts @ 0x71cc1c000c80] Packet corrupt (stream = 1, dts = 1170360).
[mp2 @ 0x71cc1c2e9280] Header missingB vq=    0KB sq=    0B f=2/1   
[mpegts @ 0x71cc1c000c80] Packet corrupt (stream = 0, dts = 3659400).
[mpegts @ 0x71cc1c000c80] PES packet size mismatch
[mpegts @ 0x71cc1c000c80] Packet corrupt (stream = 1, dts = 3641400).
[mp2 @ 0x71cc1c2e9280] Header missing
[mpegts @ 0x71cc1c000c80] Packet corrupt (stream = 0, dts = 753708).
[mpegts @ 0x71cc1c000c80] PES packet size mismatchsq=    0B f=4/3   
[mpegts @ 0x71cc1c000c80] Packet corrupt (stream = 1, dts = 740520).
[mp2 @ 0x71cc1c2e9280] Header missingB vq=    0KB sq=    0B f=4/3   
[mpegts @ 0x71cc1c000c80] Packet corrupt (stream = 0, dts = 1492446).
[h264 @ 0x71cc1c319640] error while decoding MB 11 18, bytestream -7
[mpegts @ 0x71cc1c000c80] PES packet size mismatchsq=    0B f=5/4   
[mpegts @ 0x71cc1c000c80] Packet corrupt (stream = 1, dts = 1472760).
[mp2 @ 0x71cc1c2e9280] Header missingB vq=    0KB sq=    0B f=5/4   
   8.68 A-V: -0.013 fd= 331 aq=    0KB vq=    0KB sq=    0B f=5/5   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 


ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|callbacks suppressed|bulk-IN first reads"
[47410.670715] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[47418.264433] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[47418.409105] elgato_gchd 1-2:1.0: transcoder defaults loaded
[47418.472251] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[47418.579015] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[47418.627045] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[47418.630240] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e 0x98:F5=0x72
[47418.630242] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[47418.690700] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[47418.693108] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e 0x98:F5=0x72
[47418.693112] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[47418.755079] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[47418.757843] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e 0x98:F5=0x72
[47418.757847] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[47418.819088] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[47418.821799] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e 0x98:F5=0x72
[47418.821802] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[47418.887342] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[47418.890446] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e 0x98:F5=0x72
[47418.890450] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[47418.955815] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[47418.958335] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[47418.958339] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[47419.018228] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[47419.020521] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[47419.020525] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[47419.086129] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[47419.091565] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[47419.091572] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[47419.156649] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[47419.160383] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[47419.160388] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[47419.219411] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[47419.222169] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[47419.222174] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[47419.394598] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[47419.460873] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[47419.532597] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[47419.600619] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[47419.666441] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[47419.748224] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[47419.816197] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xdf mode=0xf
[47419.913942] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xcf mode=0xf
[47419.972672] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xdf mode=0xf
[47420.029645] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xcf mode=0xf
[47420.088880] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[47420.107627] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x5f mode=0xf
[47420.171558] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x46 mode=0x6
[47420.180457] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x76 masked_mode=0x6
[47420.182700] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[47420.246469] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x26 mode=0x6
[47420.255946] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x3e masked_mode=0xe
[47420.320518] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x26 mode=0x6
[47420.330144] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47420.332820] elgato_gchd 1-2:1.0: analog format status 0xee not locked yet
[47420.395484] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[47420.413944] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47420.423333] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47420.425448] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[47420.480871] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47420.489625] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47420.492619] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[47420.551944] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47420.561729] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47420.564126] elgato_gchd 1-2:1.0: 60 Hz signal with PAL colour (0x88:E7=d9); waiting up to 5000 ms for 50 Hz
[47420.625567] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47420.635129] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47420.698980] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47420.709822] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47420.773650] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47420.783984] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47420.852483] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47420.866229] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47420.929521] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47420.939322] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47421.002672] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47421.015817] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47421.082263] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47421.091566] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47421.155307] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47421.164920] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47421.227288] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47421.237822] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47421.298606] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47421.309169] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47421.391138] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47421.402303] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47421.470716] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47421.483849] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47421.543775] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47421.553176] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47421.612129] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47421.621213] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47421.679803] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47421.688946] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47421.750328] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47421.759060] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47421.840439] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47421.850799] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47421.909427] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47421.918575] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47421.988653] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47421.998234] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47422.058798] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47422.070218] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47422.138972] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47422.151509] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47422.220634] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47422.229751] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47422.300743] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47422.310556] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47422.390016] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47422.400384] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47422.460105] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47422.469823] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47422.530041] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47422.538741] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47422.607049] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47422.618263] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47422.677245] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47422.685877] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47422.754911] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47422.763756] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47422.830653] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47422.843427] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47422.909531] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47422.919243] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47422.985307] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47422.996199] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47423.059673] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47423.069068] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47423.131174] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47423.140195] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47423.200789] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47423.209473] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47423.272486] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47423.284144] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47423.345592] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47423.360029] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47423.420043] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47423.429421] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47423.491776] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47423.501346] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47423.591342] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47423.603573] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47423.667050] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47423.679340] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47423.681486] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[47423.744442] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47423.755003] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47423.757126] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[47423.842572] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47423.856663] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47423.859848] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[47423.917587] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47423.926678] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47423.928906] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[47423.989912] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47424.000757] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47424.003815] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[47424.069072] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[47424.089250] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x26 mode=0x6
[47424.097904] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x0e masked_mode=0xe
[47424.097908] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[47424.162083] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x1f mode=0xf
[47424.166291] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xe6 0x98:F5=0x62
[47424.166296] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[47424.230376] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[47424.255970] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x0f mode=0xf
[47424.258164] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xe6 0x98:F5=0xf2
[47424.258167] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[47424.318473] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x07 mode=0x7
[47424.328631] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x17 masked_mode=0x7
[47424.330819] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[47424.330822] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[47424.393934] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[47424.412367] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x07 mode=0x7
[47424.423912] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x07 masked_mode=0x7
[47424.426042] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[47424.426045] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[47424.482480] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[47424.502377] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x17 mode=0x7
[47424.513615] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x07 masked_mode=0x7
[47424.516994] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[47424.575721] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x17 mode=0x7
[47424.587837] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x07 masked_mode=0x7
[47424.590325] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[47424.648222] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x17 mode=0x7
[47424.658159] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x17 masked_mode=0x7
[47424.660401] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[47424.722620] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x07 mode=0x7
[47424.731440] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x17 masked_mode=0x7
[47424.734953] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[47424.799361] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x07 mode=0x7
[47424.810387] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x17 masked_mode=0x7
[47424.812344] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[47424.872958] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x07 mode=0x7
[47424.882432] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x17 masked_mode=0x7
[47424.887329] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[47424.887333] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[47424.887336] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x576 fps=50/1 interlaced=1
[47424.887337] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x576 fps=50/1 interlaced=1
[47425.687259] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[47425.919469] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[47425.938845] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[47425.939104] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[47425.945104] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[47425.945669] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x576 fps=50/1 interlaced=1
[47425.946105] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[47426.064787] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[47430.980981] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=22
[47436.010293] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[47440.804780] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=f CA=ee E7=f9); configuring again
[47441.040032] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[47441.726635] elgato_gchd 1-2:1.0: 60 Hz signal with PAL colour (0x88:E7=59); waiting up to 5000 ms for 50 Hz
[47446.151634] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[47446.152131] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[47446.159675] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[47446.160515] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x576 fps=50/1 interlaced=1
[47446.161178] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[47446.297965] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[47446.297973] elgato_gchd 1-2:1.0: analog mode now 576i
[47451.168570] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[47456.200200] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[47458.747022] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=e CA=ee E7=f9); configuring again
[47459.713951] elgato_gchd 1-2:1.0: 60 Hz signal with PAL colour (0x88:E7=59); waiting up to 5000 ms for 50 Hz
[47463.827261] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[47463.827732] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[47463.833744] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[47463.834417] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x576 fps=50/1 interlaced=1
[47463.835277] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[47463.974689] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[47463.974692] elgato_gchd 1-2:1.0: analog mode now 576i
[47468.845087] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[47473.875057] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[47478.911607] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[47483.950342] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=448 bytes=29360128 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[47488.988647] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=563 bytes=36896768 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[47494.026284] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=678 bytes=44433408 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[47499.065034] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=793 bytes=51970048 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[47504.100017] elgato_gchd 1-2:1.0: decoder status changed while capturing (FA=f CA=ee E7=f9); configuring again
[47504.103211] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=908 bytes=59506688 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[47505.056935] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[47505.057275] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[47505.062861] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[47505.063356] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[47505.063919] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[47505.768136] elgato_gchd 1-2:1.0: state-change timeout: current=0x01 expected=0x02 state=0x0001 completion=0x011a
[47505.768696] elgato_gchd 1-2:1.0: START failed: ret=-110 state=0x0001 (r=0) enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[47505.768725] elgato_gchd 1-2:1.0: capture start transition failed: -110
[47505.796747] elgato_gchd 1-2:1.0: analog mode change failed: -110
[47506.057247] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0x06 mode=0x6
[47506.072464] elgato_gchd 1-2:1.0: gchd-diag detect s-video: raw_status=0x06 masked_mode=0x6
[47506.074677] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[47506.074680] elgato_gchd 1-2:1.0: gchd-diag detected input=3 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[47506.074682] elgato_gchd 1-2:1.0: signal detected: input=3 mode=720x480 fps=60/1 interlaced=0
[47506.074684] elgato_gchd 1-2:1.0: capture setup: input=3 mode=720x480 fps=60/1 interlaced=0
[47506.935212] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[47506.951986] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[47506.952290] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0002
[47506.958650] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[47506.959256] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[47506.959958] elgato_gchd 1-2:1.0: START before SCMD: state=0x0002 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[47506.961479] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[47512.000207] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=124 bytes=8126464 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[47513.637987] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[47514.934615] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[47514.934968] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[47514.940445] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[47514.941139] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=1
[47514.941581] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[47515.040524] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[47515.040527] elgato_gchd 1-2:1.0: analog mode now 480i
[47519.962356] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[47524.986155] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[47530.007820] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[47530.635113] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[47531.941495] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[47531.941796] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[47531.947144] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[47531.947634] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=3 mode=720x480 fps=60/1 interlaced=0
[47531.949410] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[47532.064677] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[47532.064680] elgato_gchd 1-2:1.0: analog mode now 240p
[47536.988112] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
ff5@ff5-VirtualBox:~$ 

'''
