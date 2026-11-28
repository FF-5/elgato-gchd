# OBS source "Elgato Game Capture HD"

An OBS Studio source for the Elgato Game Capture HD with the `elgato_gchd`
driver. It finds the device by itself, captures video and audio from the
driver's video device and offers the device settings in the source
properties.

## Behaviour

- **Device:** the first video device of the `elgato_gchd` driver; found
  again after unplugging and plugging in.
- **Video and audio:** the MPEG transport stream of the device is decoded
  with FFmpeg (H.264 video, MPEG audio) and passed to OBS with common
  timestamps. Size changes of the source (for example between NTSC and
  PAL) are followed without a restart.
- **Deinterlacing:** OBS deinterlacing, set to Yadif 2x, top field first
  when the source is created (Deinterlacing in the source's context menu).
- **Properties:** input (HDMI, component, composite, S-video), output
  size (composite and S-video: 720x480 NTSC / 720x576 PAL, 640 × 480,
  480 × 360; HDMI and component: the sizes the source allows, as the
  driver lists them, source size first), bitrate, stretch of SD input
  (composite and S-video), "Allow 50/60 fps" (HDMI and component), HDMI
  color range (Standard, Expanded), brightness, contrast, saturation, hue,
  audio gain (analog inputs, −12 … +12 dB). The H.264 profile and level are automatic (the
  driver's default). They are the driver's settings, shared with the
  settings window and other programs: the properties show the device's
  values when they open and follow changes made elsewhere (checked every
  1.5 s). A change in the dialog is applied to the device 0.5 s after the
  last edit, so the preview shows it while the dialog is open (a moved
  slider is applied once it rests for 0.5 s; OBS does not tell a plugin
  when a slider is released); Cancel sets the values from before the
  dialog again. Changes are remembered by the driver. Only values that
  differ from the device are sent, so values the dialog still shows from
  before never overwrite a change made elsewhere. A change of input
  shows the output size the driver keeps for it; a change of output size,
  input, source mode or "Allow 50/60 fps" shows the bitrate the driver
  keeps for that output mode (the picture encoded; read from its module
  parameters, the source mode from `VIDIOC_G_DV_TIMINGS`); it is not sent:
  the driver loads it itself.
- **Profiles:** the "Profiles" group of the properties loads, saves and
  deletes profiles (named sets of all settings, shared with the settings
  window and text menu; the driver's `gchd-profile` command, installed
  with the core). In the Profile field choose a saved profile or type a
  new name; Load applies the profile at once, Save saves the current
  settings under the name, Delete removes the profile. Replacing a saved
  profile and deleting ask first; an error is shown in a message box.
- **Bitrate:** one slider in Mbit/s: below 10.00 variable (VBR, shown as
  the average, half the bitrate), from 10.00 constant (CBR, the bitrate),
  up to the highest bitrate of the output mode (480 × 360 15, 640 × 480
  and 480i/576i 11.25, 480p/576p 15, 720p at 60 26.25, 720p at 30 and
  1280 × 720 15, 1080 30 Mbit/s); the lowest is 8/15 of the highest as
  the average. 480 × 360 is VBR only, 2.00–7.50. The mode follows the
  slider and is sent with the bitrate.
- **No signal:** when no pictures arrive for 0.5 s and the driver reports
  no signal on the selected input (another input chosen, the source
  switched off), the source shows a black picture of the last size
  instead of the last picture. A setting change (encoder restart) keeps
  the last picture.
- **Stretch SD input** (for games that squash a 16:9 picture into 4:3):
  the device keeps the picture size and stores a 16:9 sample aspect ratio
  in the stream, as the files the Windows software saves do; players that
  follow it show the picture stretched. OBS does not use the aspect ratio,
  so the source widens a stretched picture itself (720 × 480 → 872 × 480,
  720 × 576 → 1048 × 576). Other pictures are passed as they are.
- **Log:** the picture size, the stream's sample aspect ratio and the size
  shown are logged when they change (`[gchd] picture ...`).
- **Capture device on/off:** the device captures while the source is shown
  anywhere in OBS: in the program or preview, in a projector or multiview,
  in another scene that is shown, or in the preview of its properties
  window. It switches off when none of them shows it.

The source uses the video device exclusively while it captures; other
programs cannot capture from the device at the same time.

## Build and install

The installer of the repository installs the plugin
(`./install-driver.sh --obs`). By hand: the development files of OBS
(libobs, the frontend API), FFmpeg (libavformat, libavcodec, libavutil)
and Qt 6 Widgets (the profile question boxes, the Qt of OBS), a C and C++
compiler, `cmake` and `pkg-config` (Debian and Ubuntu: `libobs-dev`,
`libavformat-dev`, `libavcodec-dev`, `libavutil-dev`, `qt6-base-dev`,
`g++`; other distributions have equivalent packages).

```sh
cmake -S src/obs-plugin -B build/obs-plugin -DCMAKE_BUILD_TYPE=Release
cmake --build build/obs-plugin
cmake --install build/obs-plugin
```

The plugin is installed for the current user in
`~/.config/obs-studio/plugins/obs-elgato-gchd/bin/64bit/`. Restart OBS and
add the source "Elgato Game Capture HD".
