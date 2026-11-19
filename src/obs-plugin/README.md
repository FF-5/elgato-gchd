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
- **Properties:** input (HDMI, component, composite, S-video), output size
  of composite and S-video (source size, 640 × 480, 480 × 360), bitrate
  mode and bitrate, H.264 profile, stretch of SD input. They are the
  driver's settings, shared with the settings window and other programs:
  the properties show the device's current values; a change is applied to
  the device at once, also while capturing, and remembered by the driver.
- **Bitrate:** shown in Mbit/s as in the observed settings
  (`docs/composite.md` §10): VBR as the average (half the bitrate),
  3.00–7.23 for the source size and 640 × 480, 2.00–7.50 for 480 × 360
  (VBR only); CBR as the bitrate, 7.00–11.25 (11.25 is the observed CBR
  setting). The slider range follows the mode and the output size.
- **Capture device on/off:** the device captures while the source is shown
  anywhere in OBS: in the program or preview, in a projector or multiview,
  in another scene that is shown, or in the preview of its properties
  window. It switches off when none of them shows it.

The source uses the video device exclusively while it captures; other
programs cannot capture from the device at the same time.

## Build and install

The installer of the repository installs the plugin
(`./install-driver.sh --obs`). By hand, packages (Ubuntu): `libobs-dev`, `libavformat-dev`, `libavcodec-dev`,
`libavutil-dev`, `cmake`, `pkg-config`.

```sh
cmake -S src/obs-plugin -B build/obs-plugin -DCMAKE_BUILD_TYPE=Release
cmake --build build/obs-plugin
cmake --install build/obs-plugin
```

The plugin is installed for the current user in
`~/.config/obs-studio/plugins/obs-elgato-gchd/bin/64bit/`. Restart OBS and
add the source "Elgato Game Capture HD".
