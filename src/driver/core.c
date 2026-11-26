// SPDX-License-Identifier: GPL-2.0
#include <linux/kthread.h>
#include <linux/v4l2-dv-timings.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/timekeeping.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include "elgato_gchd.h"

MODULE_DESCRIPTION("Elgato Game Capture HD V4L2 driver");
MODULE_AUTHOR("FF-5 / elgato-gchd contributors");
MODULE_LICENSE("GPL");
MODULE_VERSION(GCHD_VERSION);

/* Private V4L2 control "Stretch SD Input". */
#define V4L2_CID_GCHD_STRETCH (V4L2_CID_USER_BASE + 0x10f1)
#define V4L2_CID_GCHD_ALLOW_5060 (V4L2_CID_USER_BASE + 0x10f2)

/* Preserve the VB2 read/poll/mmap/ioctl operations while adding lifecycle hooks. */
static void gchd_release_ref(struct kref *ref)
{
 struct gchd *d = container_of(ref, struct gchd, refcount);

 kfree(d->usb_buf);
 kfree(d->ts_out);
 usb_put_dev(d->udev);
 kfree(d);
}
static struct v4l2_file_operations gchd_fops;

static unsigned long ring_bytes = GCHD_RING_BYTES;
module_param(ring_bytes, ulong, 0644);
MODULE_PARM_DESC(ring_bytes, "Maximum encoded video bytes retained in RAM");

struct gchd_mode {
 u32 width;
 u32 height;
 u32 fps_num;
 u32 fps_den;
 bool interlaced;
};

/* Modes supported by the capture encoder and exposed through V4L2. */
static const struct gchd_mode gchd_modes[] = {
 {1920, 1080, 60, 1, false},
 {1920, 1080, 60, 1, true},
 {1280, 720, 60, 1, false},
 {720, 576, 50, 1, false},
 {720, 576, 50, 1, true},
 {720, 480, 60, 1, false},
 {720, 480, 60, 1, true},
};

/*
 * Negotiated format seen by applications (G_FMT). It changes only while no
 * buffers are allocated; a source change during capture is reported with
 * V4L2_EVENT_SOURCE_CHANGE and takes effect at the next negotiation.
 */
static void gchd_set_negotiated(struct gchd *d, u32 width, u32 height)
{
 if (!width || !height || vb2_is_busy(&d->vbq))
  return;
 d->width = width;
 d->height = height;
}

/* Analog standards reported for composite and S-video. */
#define GCHD_ANALOG_STDS (V4L2_STD_NTSC_M | V4L2_STD_PAL)

static bool gchd_std_is_50hz(v4l2_std_id std)
{
 return !!(std & V4L2_STD_PAL);
}

/*
 * Standard of the selected analog input: the one detected last on it while
 * the module is loaded, NTSC before the first detection.
 */
static v4l2_std_id gchd_analog_std(const struct gchd *d)
{
 v4l2_std_id std = d->input >= 2 ? d->analog_std[d->input - 2] : 0;

 return std ? std : V4L2_STD_NTSC_M;
}

/*
 * Standards can be enumerated only on composite and S-video. tvnorms is set
 * at registration so that the standard ioctls exist, and follows the input.
 */
static void gchd_update_tvnorms(struct gchd *d)
{
 d->vdev.tvnorms = d->input >= 2 ? GCHD_ANALOG_STDS : 0;
}

/* Output size of composite and S-video for a preset. */
static void gchd_analog_out_size(const struct gchd *d, u8 out,
                                 u32 *width, u32 *height);

/* Output preset for a requested size: 640x480, 480x360, else standard. */
static u8 gchd_analog_out_for(u32 width, u32 height)
{
 if (width == 640 && height == 480)
  return GCHD_OUT_640X480;
 if (width == 480 && height == 360)
  return GCHD_OUT_MOBILE;
 return GCHD_OUT_STANDARD;
}

/* Component: output size of a preset; the source size before detection. */
static void gchd_comp_out_size(const struct gchd *d, u8 out, u32 *width,
                               u32 *height)
{
 if (out == GCHD_OUT_640X480) {
  *width = 640; *height = 480;
 } else if (out == GCHD_OUT_MOBILE) {
  *width = 480; *height = 360;
 } else if (out == GCHD_OUT_1280X720) {
  *width = 1280; *height = 720;
 } else if (d->input_height) {
  *width = d->input_width; *height = d->input_height;
 } else {
  *width = d->width; *height = d->height;
 }
}

/* Component: preset for a requested size; any other size is the source size. */
static u8 gchd_comp_out_for(u32 width, u32 height)
{
 if (width == 1280 && height == 720)
  return GCHD_OUT_1280X720;
 return gchd_analog_out_for(width, height);
}

/*
 * Component: encoded frame rate of an output: the source rate (720p at 30
 * with "Allow 50/60 fps" off), 1280 x 720 of 1080i at 60, other scaled
 * outputs at 30 (25 from 576i).
 */
static void gchd_comp_fps(const struct gchd *d, u8 out, u32 *num, u32 *den)
{
 *den = 1;
 if (out == GCHD_OUT_STANDARD) {
  *num = d->input_fps_num ? d->input_fps_num : 60;
  *den = d->input_fps_den ? d->input_fps_den : 1;
  if (d->input_height == 720 && !d->allow_5060) {
   *num = 30; *den = 1;
  }
 } else if (out == GCHD_OUT_1280X720 && d->input_interlaced) {
  *num = 60;
 } else {
  *num = d->input_height == 576 ? 25 : 30;
 }
}

/* Field order of the encoded analog stream: top field first (both families). */
static enum v4l2_field gchd_field(const struct gchd *d)
{
 if (d->input >= 2)
  return d->analog_out == GCHD_OUT_STANDARD ?
         V4L2_FIELD_INTERLACED_TB : V4L2_FIELD_NONE;
 if (d->input == 1 && gchd_comp_out(d) != GCHD_OUT_STANDARD)
  return V4L2_FIELD_NONE;
 return d->input_interlaced ? V4L2_FIELD_INTERLACED : V4L2_FIELD_NONE;
}

/* Nominal format of an input before a signal has been measured. */
static void gchd_nominal_format(const struct gchd *d, u32 *width, u32 *height)
{
 if (d->input >= 2) {
  *width = 720;
  *height = gchd_std_is_50hz(gchd_analog_std(d)) ? 576 : 480;
 } else {
  *width = 1280;
  *height = 720;
 }
}

static void gchd_analog_out_size(const struct gchd *d, u8 out,
                                 u32 *width, u32 *height)
{
 if (out == GCHD_OUT_640X480) {
  *width = 640;
  *height = 480;
 } else if (out == GCHD_OUT_MOBILE) {
  *width = 480;
  *height = 360;
 } else {
  *width = 720;
  if (d->input_height == 480 || d->input_height == 576)
   *height = d->input_height;
  else
   *height = gchd_std_is_50hz(gchd_analog_std(d)) ? 576 : 480;
 }
}

/*
 * Remembered settings. The module parameters hold the settings in use; on
 * every change the driver updates them and sends a "change" uevent with
 * all values (GCHD_*), from which a udev rule writes them to modprobe.d,
 * so the next load starts with them. HDMI and the defaults on first use.
 */
static DEFINE_MUTEX(gchd_last_input_lock);
static unsigned int gchd_last_input;
module_param_named(input, gchd_last_input, uint, 0644);
MODULE_PARM_DESC(input, "Input: 0 HDMI, 1 component, 2 composite, 3 S-video");
static unsigned int gchd_set_output;
module_param_named(output, gchd_set_output, uint, 0644);
MODULE_PARM_DESC(output, "Composite/S-video output size: 0 source size, 1 640x480, 2 480x360");
static unsigned int gchd_set_format;
module_param_named(format, gchd_set_format, uint, 0644);
MODULE_PARM_DESC(format, "Pixel format: 0 H.264, 1 MPEG transport stream");
static unsigned int gchd_set_bitrate;
module_param_named(bitrate, gchd_set_bitrate, uint, 0644);
MODULE_PARM_DESC(bitrate, "Older settings files: composite/S-video bitrate in kbit/s for the mode in use, read when bitrate_cbr and bitrate_vbr are 0");
static unsigned int gchd_set_bitrate_mode = V4L2_MPEG_VIDEO_BITRATE_MODE_CBR;
module_param_named(bitrate_mode, gchd_set_bitrate_mode, uint, 0644);
MODULE_PARM_DESC(bitrate_mode, "Bitrate mode: 0 VBR, 1 CBR");
static unsigned int gchd_set_stretch;
module_param_named(stretch, gchd_set_stretch, uint, 0644);
MODULE_PARM_DESC(stretch, "Stretch SD input (composite/S-video): 0 off, 1 on");
static unsigned int gchd_set_hd_output;
module_param_named(hd_output, gchd_set_hd_output, uint, 0644);
MODULE_PARM_DESC(hd_output, "Component output size: 0 source size, 1 640x480, 2 480x360, 3 1280x720");
static unsigned int gchd_set_allow_5060 = 1;
module_param_named(allow_5060, gchd_set_allow_5060, uint, 0644);
MODULE_PARM_DESC(allow_5060, "Component: 50/60 frames per second for 576p/480p and 720p (0 off, 1 on)");
static unsigned int gchd_set_profile = V4L2_MPEG_VIDEO_H264_PROFILE_HIGH;
module_param_named(h264_profile, gchd_set_profile, uint, 0644);
MODULE_PARM_DESC(h264_profile, "H.264 profile (V4L2 value): 0 baseline, 2 main, 4 high");
static unsigned int gchd_set_level;
module_param_named(h264_level, gchd_set_level, uint, 0644);
MODULE_PARM_DESC(h264_level, "H.264 level x10 (30-51), 0 automatic");
/*
 * Latency marks for benchmarks: "gchd-timing: <microseconds> <event>" at
 * requests, device commands and their completion, and the first stream
 * data after START.
 */
static bool gchd_timing;
module_param_named(timing, gchd_timing, bool, 0644);
MODULE_PARM_DESC(timing, "Log latency marks (gchd-timing) for benchmarks");

void gchd_mark(struct gchd *d, const char *fmt, ...)
{
 char event[96];
 va_list ap;

 if (!READ_ONCE(gchd_timing))
  return;
 va_start(ap, fmt);
 vsnprintf(event, sizeof(event), fmt, ap);
 va_end(ap);
 dev_info(&d->intf->dev, "gchd-timing: %llu %s\n",
          (unsigned long long)(ktime_get_ns() / 1000), event);
}

/* After START: mark the first stream data and the first buffer. */
void gchd_mark_start(struct gchd *d)
{
 if (!READ_ONCE(gchd_timing))
  return;
 WRITE_ONCE(d->mark_rx, true);
 WRITE_ONCE(d->mark_user, true);
}

static unsigned int gchd_set_std[2];
module_param_named(std_composite, gchd_set_std[0], uint, 0644);
MODULE_PARM_DESC(std_composite, "Last standard on composite: 0 unknown, 1 NTSC (60 Hz), 2 PAL (50 Hz)");
module_param_named(std_svideo, gchd_set_std[1], uint, 0644);
MODULE_PARM_DESC(std_svideo, "Last standard on S-video: 0 unknown, 1 NTSC (60 Hz), 2 PAL (50 Hz)");

/*
 * Settings groups: composite/S-video share one set of encoder settings, HDMI/
 * component another; each remembers a bitrate per bitrate mode (kbit/s, 0
 * automatic). The parameters above (bitrate_mode, h264_profile, h264_level)
 * are the composite/S-video group; "bitrate" is read once for compatibility.
 */
static unsigned int gchd_set_bitrate_cbr;
module_param_named(bitrate_cbr, gchd_set_bitrate_cbr, uint, 0644);
MODULE_PARM_DESC(bitrate_cbr, "Composite/S-video: last CBR bitrate in kbit/s, 0 automatic");
static unsigned int gchd_set_bitrate_vbr;
module_param_named(bitrate_vbr, gchd_set_bitrate_vbr, uint, 0644);
MODULE_PARM_DESC(bitrate_vbr, "Composite/S-video: last VBR bitrate in kbit/s, 0 automatic");
static unsigned int gchd_set_hd_bitrate_mode = V4L2_MPEG_VIDEO_BITRATE_MODE_CBR;
module_param_named(hd_bitrate_mode, gchd_set_hd_bitrate_mode, uint, 0644);
MODULE_PARM_DESC(hd_bitrate_mode, "HDMI/component: bitrate mode: 0 VBR, 1 CBR");
static unsigned int gchd_set_hd_bitrate_cbr;
module_param_named(hd_bitrate_cbr, gchd_set_hd_bitrate_cbr, uint, 0644);
MODULE_PARM_DESC(hd_bitrate_cbr, "HDMI/component: last CBR bitrate in kbit/s, 0 automatic");
static unsigned int gchd_set_bitrate_small;
module_param_named(bitrate_small, gchd_set_bitrate_small, uint, 0644);
MODULE_PARM_DESC(bitrate_small, "Composite/S-video: last 480x360 (VBR) bitrate in kbit/s, 0 automatic");
static unsigned int gchd_set_hd_bitrate_vbr;
module_param_named(hd_bitrate_vbr, gchd_set_hd_bitrate_vbr, uint, 0644);
MODULE_PARM_DESC(hd_bitrate_vbr, "HDMI/component: last VBR bitrate in kbit/s, 0 automatic");
static unsigned int gchd_set_hd_profile = V4L2_MPEG_VIDEO_H264_PROFILE_HIGH;
module_param_named(hd_h264_profile, gchd_set_hd_profile, uint, 0644);
MODULE_PARM_DESC(hd_h264_profile, "HDMI/component: H.264 profile (V4L2 value)");
static unsigned int gchd_set_hd_level;
module_param_named(hd_h264_level, gchd_set_hd_level, uint, 0644);
MODULE_PARM_DESC(hd_h264_level, "HDMI/component: H.264 level x10 (30-51), 0 automatic");

static unsigned int gchd_std_param(v4l2_std_id std)
{
 return !std ? 0 : gchd_std_is_50hz(std) ? 2 : 1;
}

/* V4L2 H.264 level menu values and the level x10 they stand for. */
static const u8 gchd_levels[][2] = {
 { V4L2_MPEG_VIDEO_H264_LEVEL_3_0, 30 }, { V4L2_MPEG_VIDEO_H264_LEVEL_3_1, 31 },
 { V4L2_MPEG_VIDEO_H264_LEVEL_3_2, 32 }, { V4L2_MPEG_VIDEO_H264_LEVEL_4_0, 40 },
 { V4L2_MPEG_VIDEO_H264_LEVEL_4_1, 41 }, { V4L2_MPEG_VIDEO_H264_LEVEL_4_2, 42 },
 { V4L2_MPEG_VIDEO_H264_LEVEL_5_0, 50 }, { V4L2_MPEG_VIDEO_H264_LEVEL_5_1, 51 },
};

/* Settings group of the selected input: 1 composite/S-video, 0 HDMI/component. */
static unsigned int gchd_group(const struct gchd *d)
{
 return d->input >= 2;
}

/* Kept bitrate in use: per mode, and its own for 480 x 360 (VBR only). */
static unsigned int gchd_rate_slot(const struct gchd *d)
{
 if (d->input >= 2 && d->analog_out == GCHD_OUT_MOBILE)
  return 2;
 return d->bitrate_mode ? 1 : 0;
}

static void gchd_rate_store(struct gchd *d)
{
 d->grp[gchd_group(d)].bitrate[gchd_rate_slot(d)] =
  d->bitrate_forced ? d->bitrate : 0;
}

static void gchd_rate_load(struct gchd *d)
{
 u16 b = d->grp[gchd_group(d)].bitrate[gchd_rate_slot(d)];

 d->bitrate_forced = b != 0;
 d->bitrate = b ? b : 40000;
}

/* Keep the encoder settings in use in their group (bitrate per mode). */
static void gchd_group_store(struct gchd *d)
{
 struct gchd_group *g = &d->grp[gchd_group(d)];

 g->bitrate_mode = d->bitrate_mode;
 gchd_rate_store(d);
 g->profile = d->h264_profile;
 g->level = d->h264_level_forced ? d->h264_level : 0;
}

/* Use the encoder settings of the selected input's group. */
static void gchd_group_load(struct gchd *d)
{
 const struct gchd_group *g = &d->grp[gchd_group(d)];

 d->bitrate_mode = g->bitrate_mode;
 gchd_rate_load(d);
 d->h264_profile = g->profile;
 d->h264_level_forced = g->level != 0;
 d->h264_level = g->level ? g->level : 41;
}

static void gchd_ctrls_show(struct gchd *d);

/* Store the device's settings in the parameters and announce them. */
static void gchd_settings_save(struct gchd *d)
{
 char e[19][40];
 char *envp[20];
 const struct gchd_group *a = &d->grp[1], *h = &d->grp[0];
 int i;

 gchd_group_store(d);
 gchd_set_output = d->analog_out;
 gchd_set_format = d->pixfmt == V4L2_PIX_FMT_MPEG;
 gchd_set_bitrate = 0;
 gchd_set_bitrate_mode = a->bitrate_mode;
 gchd_set_bitrate_vbr = a->bitrate[0];
 gchd_set_bitrate_cbr = a->bitrate[1];
 gchd_set_bitrate_small = a->bitrate[2];
 gchd_set_stretch = d->stretch;
 gchd_set_hd_output = d->hd_out;
 gchd_set_allow_5060 = d->allow_5060;
 gchd_set_profile = a->profile;
 gchd_set_level = a->level;
 gchd_set_hd_bitrate_mode = h->bitrate_mode;
 gchd_set_hd_bitrate_vbr = h->bitrate[0];
 gchd_set_hd_bitrate_cbr = h->bitrate[1];
 gchd_set_hd_profile = h->profile;
 gchd_set_hd_level = h->level;
 gchd_set_std[0] = gchd_std_param(d->analog_std[0]);
 gchd_set_std[1] = gchd_std_param(d->analog_std[1]);

 snprintf(e[0], sizeof(e[0]), "GCHD_INPUT=%u", d->input);
 snprintf(e[1], sizeof(e[1]), "GCHD_OUTPUT=%u", gchd_set_output);
 snprintf(e[2], sizeof(e[2]), "GCHD_FORMAT=%u", gchd_set_format);
 snprintf(e[3], sizeof(e[3]), "GCHD_BITRATE_MODE=%u", gchd_set_bitrate_mode);
 snprintf(e[4], sizeof(e[4]), "GCHD_BITRATE_CBR=%u", gchd_set_bitrate_cbr);
 snprintf(e[5], sizeof(e[5]), "GCHD_BITRATE_VBR=%u", gchd_set_bitrate_vbr);
 snprintf(e[6], sizeof(e[6]), "GCHD_STRETCH=%u", gchd_set_stretch);
 snprintf(e[7], sizeof(e[7]), "GCHD_H264_PROFILE=%u", gchd_set_profile);
 snprintf(e[8], sizeof(e[8]), "GCHD_H264_LEVEL=%u", gchd_set_level);
 snprintf(e[9], sizeof(e[9]), "GCHD_STD_COMPOSITE=%u", gchd_set_std[0]);
 snprintf(e[10], sizeof(e[10]), "GCHD_STD_SVIDEO=%u", gchd_set_std[1]);
 snprintf(e[11], sizeof(e[11]), "GCHD_HD_BITRATE_MODE=%u", gchd_set_hd_bitrate_mode);
 snprintf(e[12], sizeof(e[12]), "GCHD_HD_BITRATE_CBR=%u", gchd_set_hd_bitrate_cbr);
 snprintf(e[13], sizeof(e[13]), "GCHD_HD_BITRATE_VBR=%u", gchd_set_hd_bitrate_vbr);
 snprintf(e[14], sizeof(e[14]), "GCHD_HD_H264_PROFILE=%u", gchd_set_hd_profile);
 snprintf(e[15], sizeof(e[15]), "GCHD_HD_H264_LEVEL=%u", gchd_set_hd_level);
 snprintf(e[16], sizeof(e[16]), "GCHD_BITRATE_SMALL=%u", gchd_set_bitrate_small);
 snprintf(e[17], sizeof(e[17]), "GCHD_HD_OUTPUT=%u", gchd_set_hd_output);
 snprintf(e[18], sizeof(e[18]), "GCHD_ALLOW_5060=%u", gchd_set_allow_5060);
 for (i = 0; i < 19; ++i)
  envp[i] = e[i];
 envp[19] = NULL;
 kobject_uevent_env(&d->intf->dev.kobj, KOBJ_CHANGE, envp);
}

/* Remember a newly detected composite/S-video standard (process context). */
void gchd_remember_std(struct gchd *d)
{
 if (READ_ONCE(d->disconnected))
  return;
 if (gchd_set_std[0] != gchd_std_param(d->analog_std[0]) ||
     gchd_set_std[1] != gchd_std_param(d->analog_std[1]))
  gchd_settings_save(d);
}

/* One group from its parameters. */
static void gchd_group_param(struct gchd_group *g, unsigned int mode,
                             unsigned int vbr, unsigned int cbr,
                             unsigned int small, unsigned int profile,
                             unsigned int level)
{
 unsigned int i;

 g->bitrate_mode = mode ? V4L2_MPEG_VIDEO_BITRATE_MODE_CBR :
                          V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;
 g->bitrate[0] = vbr ? clamp_t(unsigned int, vbr, 32, 40000) : 0;
 g->bitrate[1] = cbr ? clamp_t(unsigned int, cbr, 32, 40000) : 0;
 g->bitrate[2] = small ? clamp_t(unsigned int, small, 32, 40000) : 0;
 g->profile = profile == V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE ||
              profile == V4L2_MPEG_VIDEO_H264_PROFILE_MAIN ?
              profile : V4L2_MPEG_VIDEO_H264_PROFILE_HIGH;
 g->level = 0;
 for (i = 0; i < ARRAY_SIZE(gchd_levels); ++i)
  if (gchd_levels[i][1] == level)
   g->level = level;
}

/* Settings from the parameters at probe time. */
static void gchd_settings_load(struct gchd *d)
{
 unsigned int i;

 d->analog_out = gchd_set_output <= GCHD_OUT_MOBILE ? gchd_set_output :
                                                      GCHD_OUT_STANDARD;
 d->pixfmt = gchd_set_format ? V4L2_PIX_FMT_MPEG : V4L2_PIX_FMT_H264;
 d->stretch = !!gchd_set_stretch;
 d->hd_out = gchd_set_hd_output <= GCHD_OUT_1280X720 ? gchd_set_hd_output :
                                                       GCHD_OUT_STANDARD;
 d->allow_5060 = !!gchd_set_allow_5060;
 for (i = 0; i < 2; ++i)
  d->analog_std[i] = gchd_set_std[i] == 2 ? V4L2_STD_PAL :
                     gchd_set_std[i] == 1 ? V4L2_STD_NTSC_M : 0;
 gchd_group_param(&d->grp[1], gchd_set_bitrate_mode, gchd_set_bitrate_vbr,
                  gchd_set_bitrate_cbr, gchd_set_bitrate_small,
                  gchd_set_profile, gchd_set_level);
 /* Older settings files: one bitrate for the mode in use. */
 if (gchd_set_bitrate && !d->grp[1].bitrate[0] && !d->grp[1].bitrate[1])
  d->grp[1].bitrate[d->grp[1].bitrate_mode ? 1 : 0] =
   clamp_t(unsigned int, gchd_set_bitrate, 32, 40000);
 gchd_group_param(&d->grp[0], gchd_set_hd_bitrate_mode, gchd_set_hd_bitrate_vbr,
                  gchd_set_hd_bitrate_cbr, 0, gchd_set_hd_profile,
                  gchd_set_hd_level);
 gchd_group_load(d);
}

/* GCHD_* tracks driver lifecycle; HW_* names device protocol states/commands. */
static void gchd_set_lifecycle(struct gchd *d, enum gchd_state state)
{
 enum gchd_state old = READ_ONCE(d->lifecycle_state);

 if (old == state)
  return;
 WRITE_ONCE(d->lifecycle_state, state);
 dev_dbg(&d->intf->dev, "lifecycle: %u -> %u\n", old, state);
}

/*
 * Enter INPUT_SELECTION only after boot/init has completed. This is the
 * single place that resolves the remembered input (HDMI on first use), then
 * hands control to the independent source-detection state.
 */
static void gchd_input_selection_enter(struct gchd *d)
{
 mutex_lock(&gchd_last_input_lock);
 d->input = gchd_last_input <= 3 ? gchd_last_input : 0;
 gchd_update_tvnorms(d);
 mutex_unlock(&gchd_last_input_lock);

 /* A new open starts from the common HDMI/default route. */
 d->previous_input = 0;
 d->pal60_since = 0;
 d->pal60_logged = false;
 gchd_set_lifecycle(d, GCHD_INPUT_SELECTION);
 WRITE_ONCE(d->detect_requested, true);
 schedule_delayed_work(&d->detect_work, 0);
}

static const struct gchd_mode *gchd_find_mode(u32 width, u32 height,
                                                bool interlaced)
{
 unsigned int i;

 for (i = 0; i < ARRAY_SIZE(gchd_modes); ++i)
  if (gchd_modes[i].width == width &&
      gchd_modes[i].height == height &&
      gchd_modes[i].interlaced == interlaced)
   return &gchd_modes[i];

 return NULL;
}

static bool gchd_mode_allowed(const struct gchd *d,
                              const struct gchd_mode *m)
{
 switch (d->input) {
 case 0:
  return (m->width == 1920 && m->height == 1080) ||
         (m->width == 1280 && m->height == 720) ||
         (m->width == 720 && !m->interlaced &&
          (m->height == 480 || m->height == 576));
 case 1:
  return true;
 case 2:
 case 3:
  /*
   * Composite and S-video also carry 240p/288p. These are represented as
   * progressive 720x480/576 modes by the capture encoder, so do not reject
   * them merely because the analog input is selected.
   */
  return m->width == 720 &&
         (m->height == 480 || m->height == 576);
 default:
  return false;
 }
}

int gchd_ring_push(struct gchd_ring *r, const u8 *data, size_t len, u64 ts90)
{
 unsigned long flags; u8 *copy; struct gchd_frame *f; unsigned int tail;
 if (!len || len > GCHD_MAX_FRAME) return -EMSGSIZE;
 copy = kvmemdup(data, len, GFP_KERNEL); if (!copy) return -ENOMEM;
 spin_lock_irqsave(&r->lock, flags);
 while (r->count && (r->count >= GCHD_RING_FRAMES || r->bytes + len > ring_bytes)) {
  tail=(r->head+GCHD_RING_FRAMES-r->count)%GCHD_RING_FRAMES;
  kvfree(r->frames[tail].data); r->bytes-=r->frames[tail].len; r->dropped++;
  r->frames[tail].data=NULL; r->frames[tail].len=0; r->count--;
 }
 f=&r->frames[r->head]; f->data=copy; f->len=len; f->ts90=ts90; f->sequence=++r->sequence;
 r->bytes+=len; r->head=(r->head+1)%GCHD_RING_FRAMES; r->count++;
 spin_unlock_irqrestore(&r->lock, flags); return 0;
}

struct gchd_frame *gchd_ring_pop(struct gchd_ring *r)
{
 unsigned long flags; struct gchd_frame *f=NULL; unsigned int tail;
 spin_lock_irqsave(&r->lock, flags);
 if (r->count) {
  tail=(r->head+GCHD_RING_FRAMES-r->count)%GCHD_RING_FRAMES;
  f=kmemdup(&r->frames[tail],sizeof(*f),GFP_ATOMIC);
  if (f) { r->frames[tail].data=NULL; r->bytes-=r->frames[tail].len;
   r->frames[tail].len=0; r->count--; }
 }
 spin_unlock_irqrestore(&r->lock, flags); return f;
}
void gchd_ring_free(struct gchd_ring *r)
{
 unsigned long flags; unsigned int i;
 spin_lock_irqsave(&r->lock,flags);
 for(i=0;i<GCHD_RING_FRAMES;i++){ kvfree(r->frames[i].data); r->frames[i].data=NULL; r->frames[i].len=0; }
 r->count=0; r->bytes=0; spin_unlock_irqrestore(&r->lock,flags);
}

/*
 * Decoding timestamp of a PES packet in 90 kHz units: DTS when present,
 * otherwise PTS; GCHD_TS_NONE if the header carries neither.
 */
static u64 gchd_pes_timestamp(const u8 *h, unsigned int header_len)
{
 unsigned int flags = h[7] >> 6;
 const u8 *t;

 if (flags == 3 && header_len >= 19)
  t = h + 14;
 else if (flags == 2 && header_len >= 14)
  t = h + 9;
 else
  return GCHD_TS_NONE;
 return ((u64)(t[0] & 0x0e) << 29) | ((u64)t[1] << 22) |
        ((u64)(t[2] & 0xfe) << 14) | ((u64)t[3] << 7) | (t[4] >> 1);
}

/*
 * Buffer timestamp: the stream's 90 kHz timestamp converted to nanoseconds
 * and anchored to the monotonic clock at the first delivered frame of a
 * stream. Frames without a timestamp get the delivery time.
 */
static u64 gchd_buffer_timestamp(struct gchd *d, u64 ts90)
{
 u64 delta;

 if (ts90 == GCHD_TS_NONE)
  return ktime_get_ns();
 if (!d->ts_base_valid) {
  d->ts_base_ns = ktime_get_ns();
  d->ts_base90 = ts90;
  d->ts_base_valid = true;
 }
 delta = (ts90 - d->ts_base90) & ((1ULL << 33) - 1);
 return d->ts_base_ns + div_u64(delta * 100000ULL, 9);
}

/* True if the H.264 byte stream contains a sequence parameter set. */
static bool gchd_has_sps(const u8 *p, size_t n)
{
 size_t i;

 for (i = 0; i + 3 < n; ++i)
  if (p[i] == 0 && p[i + 1] == 0 && p[i + 2] == 1 && (p[i + 3] & 0x1f) == 7)
   return true;
 return false;
}

/*
 * Queue one video PES payload. After a stream (re)start, payloads are held
 * back until the first one carrying an SPS, so that consumers start at a
 * decodable picture.
 */
static int gchd_queue_pes(struct gchd *d, const u8 *pes, size_t n)
{
 if (READ_ONCE(d->pixfmt) == V4L2_PIX_FMT_MPEG)
  return 0;
 if (READ_ONCE(d->wait_sps)) {
  if (!gchd_has_sps(pes, n))
   return 0;
  WRITE_ONCE(d->wait_sps, false);
 }
 return gchd_ring_push(&d->ring, pes, n, d->pes_ts90);
}

/*
 * Parse the video PID from the device's MPEG-TS stream.
 *
 * USB transfers do not preserve TS/PES boundaries. Keep the PES assembly
 * state across calls, but abandon the current PES after transport errors or
 * continuity-counter gaps and wait for the next payload-unit start. That
 * prevents corrupted bytes from being delivered as valid H.264 data.
 */
/*
 * Transport-stream delivery: whole packets except null packets, starting at
 * a PAT packet after each stream (re)start, collected into buffers of
 * GCHD_TS_BUF_PACKETS packets. Video and audio start with a complete PES
 * packet.
 */
static void gchd_ts_out(struct gchd *d, const u8 *p)
{
 u16 pid;
 int r;

 if (p[0] != GCHD_TS_SYNC)
  return;
 pid = ((p[1] & 0x1f) << 8) | p[2];
 if (pid == GCHD_TS_PID_NULL)
  return;
 if (READ_ONCE(d->wait_pat)) {
  if (pid != GCHD_TS_PID_PAT)
   return;
  WRITE_ONCE(d->wait_pat, false);
  d->ts_v_started = false;
  d->ts_a_started = false;
 }
 /* Drop the tail of a PES packet that started before the PAT. */
 if (pid == GCHD_TS_PID_VIDEO || pid == GCHD_TS_PID_AUDIO) {
  bool *started = pid == GCHD_TS_PID_VIDEO ? &d->ts_v_started :
                                             &d->ts_a_started;

  if (!*started) {
   if (!(p[1] & 0x40))
    return;
   *started = true;
  }
 }
 memcpy(d->ts_out + d->ts_out_len, p, GCHD_TS_PACKET);
 d->ts_out_len += GCHD_TS_PACKET;
 if (d->ts_out_len < GCHD_TS_BUF_SIZE)
  return;
 r = gchd_ring_push(&d->ring, d->ts_out, d->ts_out_len, GCHD_TS_NONE);
 if (r)
  dev_warn_ratelimited(&d->intf->dev, "dropping TS buffer: %d\n", r);
 d->ts_out_len = 0;
}

static void gchd_ts(struct gchd *d, const u8 *p, u8 *pes, size_t *n,
                    bool *pes_active, bool *cc_valid, u8 *last_cc)
{
 u16 pid;
 unsigned int off = 4, afc;
 bool start, discontinuity = false, has_payload;
 u8 cc;

 if (p[0] != GCHD_TS_SYNC)
  return;

 pid = ((p[1] & 0x1f) << 8) | p[2];
 if (pid != GCHD_TS_PID_VIDEO)
  return;

 /* A transport error or scrambled video payload invalidates this PES. */
 if ((p[1] & 0x80) || (p[3] & 0xc0)) {
  *n = 0;
  *pes_active = false;
  *cc_valid = false;
  return;
 }

 afc = (p[3] >> 4) & 3;
 if (!afc)
  goto reset_pes;

 if (afc == 2 || afc == 3) {
  unsigned int adaptation_len = p[4];

  if (adaptation_len > 183 || 5 + adaptation_len > GCHD_TS_PACKET)
   goto reset_pes;
  if (adaptation_len)
   discontinuity = !!(p[5] & 0x80);
  off = 5 + adaptation_len;
 }

 has_payload = (afc == 1 || afc == 3) && off < GCHD_TS_PACKET;
 start = !!(p[1] & 0x40);
 cc = p[3] & 0x0f;

 if (discontinuity) {
  *n = 0;
  *pes_active = false;
  *cc_valid = false;
 }

 /* Continuity counters advance only for packets carrying payload. */
 if (has_payload) {
  if (*cc_valid && !discontinuity &&
      cc != ((*last_cc + 1) & 0x0f)) {
   dev_warn_ratelimited(&d->intf->dev,
                        "video TS continuity gap: expected=%u got=%u; dropping partial PES\n",
                        (*last_cc + 1) & 0x0f, cc);
   *n = 0;
   *pes_active = false;
   /* Only a fresh PES start is safe to resume from after a gap. */
   if (!start) {
    *last_cc = cc;
    *cc_valid = true;
    return;
   }
  }
  *last_cc = cc;
  *cc_valid = true;
 }

 if (!has_payload)
  return;

 d->last_video_jiffies = jiffies;
 if (!READ_ONCE(d->signal_present)) {
  struct v4l2_event ev = { .type = V4L2_EVENT_SOURCE_CHANGE };

  ev.id = d->input;
  ev.u.src_change.changes = V4L2_EVENT_SRC_CH_RESOLUTION;
  v4l2_event_queue(&d->vdev, &ev);
  WRITE_ONCE(d->signal_present, true);
  dev_info(&d->intf->dev,
           "signal present: input=%u; first video TS packet on PID 0x1011\n",
           READ_ONCE(d->input));
 }

 if (start) {
  if (*pes_active && *n) {
   int r = gchd_queue_pes(d, pes, *n);

   if (r)
    dev_warn_ratelimited(&d->intf->dev,
                         "dropping PES payload: bytes=%zu queue_error=%d\n",
                         *n, r);
   else
    dev_dbg(&d->intf->dev, "queued PES payload: bytes=%zu\n", *n);
  }
  *n = 0;
  *pes_active = false;

  /* A new PES must begin with a complete MPEG-2 PES header here. */
  if (GCHD_TS_PACKET - off < 9 ||
      p[off] != 0 || p[off + 1] != 0 || p[off + 2] != 1 ||
      (p[off + 6] & 0xc0) != 0x80)
   return;

  {
   unsigned int header_len = 9 + p[off + 8];

   if (header_len > GCHD_TS_PACKET - off)
    return;
   d->pes_ts90 = gchd_pes_timestamp(p + off, header_len);
   off += header_len;
  }
  *pes_active = true;
 } else if (!*pes_active) {
  /* Ignore continuation payload until the next complete PES start. */
  return;
 }

 if (off < GCHD_TS_PACKET) {
  size_t remaining = GCHD_MAX_FRAME - *n;
  size_t payload = GCHD_TS_PACKET - off;

  if (payload > remaining) {
   dev_warn_ratelimited(&d->intf->dev,
                        "video PES exceeds %u bytes; dropping until next PES\n",
                        GCHD_MAX_FRAME);
   *n = 0;
   *pes_active = false;
   return;
  }
  memcpy(pes + *n, p + off, payload);
  *n += payload;
 }
 return;

reset_pes:
 *n = 0;
 *pes_active = false;
 *cc_valid = false;
}

static void gchd_signal_check(struct gchd *d)
{
 /* START and STOP can take longer than the signal-loss debounce. */
 if (READ_ONCE(d->start_pending) || READ_ONCE(d->stop_pending))
  return;
 /*
  * After START the first video can take longer than the debounce (0.6 s
  * with 480x360 at 4000 kbit/s): no signal loss before GCHD_START_GRACE_MS.
  */
 if (time_before(jiffies, READ_ONCE(d->start_jiffies) +
                 msecs_to_jiffies(GCHD_START_GRACE_MS)))
  return;

 if (!READ_ONCE(d->signal_present) &&
     time_after(jiffies, READ_ONCE(d->last_video_jiffies) +
                msecs_to_jiffies(500)))
  return;

 if (READ_ONCE(d->signal_present) &&
     time_after(jiffies, READ_ONCE(d->last_video_jiffies) +
                msecs_to_jiffies(500))) {
  struct v4l2_event ev = { .type = V4L2_EVENT_SOURCE_CHANGE };

  ev.id = d->input;
  ev.u.src_change.changes = V4L2_EVENT_SRC_CH_RESOLUTION;
  v4l2_event_queue(&d->vdev, &ev);
  WRITE_ONCE(d->signal_present, false);
  /* RX must not issue control transfers or attempt to stop itself here. */
  if (READ_ONCE(d->capture_active) && READ_ONCE(d->detect_requested))
   schedule_delayed_work(&d->detect_work, 0);
  pr_info("elgato_gchd: input %u signal lost; scheduling redetection\n",
          d->input);
 }
}

/* Number of encoded frames waiting in the ring. */
static unsigned int gchd_ring_count(struct gchd_ring *r)
{
 unsigned long flags;
 unsigned int n;

 spin_lock_irqsave(&r->lock, flags);
 n = r->count;
 spin_unlock_irqrestore(&r->lock, flags);
 return n;
}

/*
 * Move encoded frames from the ring into queued V4L2 buffers, in order.
 * A frame is taken from the ring only when a buffer is available, so frames
 * wait in the ring instead of being dropped. Serialized by deliver_lock
 * because both the receive thread and buf_queue call it.
 */
static void gchd_deliver(struct gchd *d)
{
 struct gchd_frame *f; struct gchd_buffer *b; unsigned long flags; void *dst;

 mutex_lock(&d->deliver_lock);
 while (READ_ONCE(d->streaming) && gchd_ring_count(&d->ring)) {
  spin_lock_irqsave(&d->qlock,flags);
  if (list_empty(&d->queued)) {
   spin_unlock_irqrestore(&d->qlock,flags);
   break;
  }
  b=list_first_entry(&d->queued,struct gchd_buffer,list);list_del(&b->list);
  spin_unlock_irqrestore(&d->qlock,flags);

  f=gchd_ring_pop(&d->ring);
  if (!f) {
   spin_lock_irqsave(&d->qlock,flags);
   list_add(&b->list,&d->queued);
   spin_unlock_irqrestore(&d->qlock,flags);
   break;
  }
  dst=vb2_plane_vaddr(&b->vb.vb2_buf,0);
  if(!dst || f->len>d->sizeimage) {
   dev_warn_ratelimited(&d->intf->dev,
                        "V4L2 buffer error: dst=%p payload=%zu sizeimage=%u\n",
                        dst, f->len, d->sizeimage);
   vb2_buffer_done(&b->vb.vb2_buf,VB2_BUF_STATE_ERROR);
  } else {
   memcpy(dst,f->data,f->len);vb2_set_plane_payload(&b->vb.vb2_buf,0,f->len);
   b->vb.sequence=f->sequence;b->vb.vb2_buf.timestamp=gchd_buffer_timestamp(d, f->ts90);
   b->vb.field=gchd_field(d);
   dev_dbg(&d->intf->dev, "delivered V4L2 buffer: sequence=%llu bytes=%zu sizeimage=%u\n",
           (unsigned long long)f->sequence, f->len, d->sizeimage);
   if (READ_ONCE(d->mark_user)) {
    WRITE_ONCE(d->mark_user, false);
    gchd_mark(d, "stream first-buffer");
   }
   vb2_buffer_done(&b->vb.vb2_buf,VB2_BUF_STATE_DONE);
  }
  kvfree(f->data);kfree(f);
 }
 mutex_unlock(&d->deliver_lock);
}

#define GCHD_RX_URB_COUNT (GCHD_RX_URBS_EP_IN + GCHD_RX_URBS_AUX_IN)

struct gchd_rx_urb {
 struct urb *urb;
 struct gchd *dev;
 struct list_head done;
 u8 *buf;
 u8 endpoint;
};

static bool gchd_rx_urb_stopped(int status)
{
 return status == -ENOENT || status == -ECONNRESET ||
        status == -ESHUTDOWN;
}

/*
 * Endpoint 0x81 carries the MPEG-TS stream of transport output 1. Receive
 * transfers are also kept queued on endpoint 0x84; transfers that carry
 * data there are passed to the TS parser as well (transport output 2 is
 * expected on one of the two endpoints).
 */
static void gchd_rx_complete(struct urb *urb)
{
 struct gchd_rx_urb *rx = urb->context;
 struct gchd *d = rx->dev;
 unsigned long flags;
 int r;

 if (!READ_ONCE(d->rx_active) || gchd_rx_urb_stopped(urb->status))
  return;

 if (urb->status) {
  atomic_inc(&d->rx_urb_errors);
  WRITE_ONCE(d->rx_urb_last_status, urb->status);
  r = usb_submit_urb(urb, GFP_ATOMIC);
  if (r && READ_ONCE(d->rx_active))
   dev_err_ratelimited(&d->intf->dev,
                       "bulk-IN resubmit failed: ep=0x%02x error=%d\n",
                       rx->endpoint, r);
  return;
 }

 if (rx->endpoint == GCHD_EP_AUX_IN && !urb->actual_length) {
  /* Keep the 0x84 receive queue running. */
  r = usb_submit_urb(urb, GFP_ATOMIC);
  if (r && READ_ONCE(d->rx_active))
   dev_err_ratelimited(&d->intf->dev,
                       "auxiliary bulk-IN resubmit failed: error=%d\n", r);
  return;
 }

 spin_lock_irqsave(&d->rx_lock, flags);
 if (READ_ONCE(d->rx_active))
  list_add_tail(&rx->done, &d->rx_done);
 spin_unlock_irqrestore(&d->rx_lock, flags);
 wake_up_interruptible(&d->rx_wait);
}

static int gchd_rx(void *arg)
{
 struct gchd *d = arg;
 struct gchd_rx_urb *rx;
 struct usb_host_endpoint *ep81, *ep84;
 u8 *pes;
 size_t n = 0;
 bool pes_active = false, cc_valid = false;
 u8 last_cc = 0;
 u64 rx_reads = 0, rx_bytes = 0, rx_empty = 0, rx_errors = 0, rx_aux_bytes = 0;
 unsigned long report_jiffies = jiffies;
 unsigned int i;
 int r = 0;

 rx = kcalloc(GCHD_RX_URB_COUNT, sizeof(*rx), GFP_KERNEL);
 if (!rx) {
  WRITE_ONCE(d->rx_setup_status, -ENOMEM);
  complete(&d->rx_ready);
  return -ENOMEM;
 }
 for (i = 0; i < GCHD_RX_URB_COUNT; ++i)
  INIT_LIST_HEAD(&rx[i].done);
 pes = kvmalloc(GCHD_MAX_FRAME, GFP_KERNEL);
 if (!pes) {
  WRITE_ONCE(d->rx_setup_status, -ENOMEM);
  complete(&d->rx_ready);
  kfree(rx);
  return -ENOMEM;
 }

 ep81 = usb_pipe_endpoint(d->udev,
                          usb_rcvbulkpipe(d->udev, GCHD_EP_TS_IN));
 ep84 = usb_pipe_endpoint(d->udev,
                          usb_rcvbulkpipe(d->udev, GCHD_EP_AUX_IN));
 if (!ep81 || !usb_endpoint_is_bulk_in(&ep81->desc)) {
  dev_err(&d->intf->dev, "required bulk-IN endpoint 0x81 is missing\n");
  r = -ENODEV;
  goto out_free;
 }
 if (!ep84 || !usb_endpoint_is_bulk_in(&ep84->desc)) {
  dev_err(&d->intf->dev,
          "observed auxiliary bulk-IN endpoint 0x84 is missing; cannot reproduce receive queue\n");
  r = -ENODEV;
  goto out_free;
 }

 /*
  * Allocate all buffers first, then submit in the exact order seen in the
  * capture: 32 receive URBs on 0x81 followed by 8 on 0x84.
  */
 for (i = 0; i < GCHD_RX_URB_COUNT; ++i) {
  rx[i].dev = d;
  rx[i].endpoint = i < GCHD_RX_URBS_EP_IN ?
                   GCHD_EP_TS_IN : GCHD_EP_AUX_IN;
  rx[i].buf = kmalloc(GCHD_USB_BUFSIZE, GFP_KERNEL);
  rx[i].urb = usb_alloc_urb(0, GFP_KERNEL);
  if (!rx[i].buf || !rx[i].urb) {
   r = -ENOMEM;
   goto out_kill;
  }
  usb_fill_bulk_urb(rx[i].urb, d->udev,
                    usb_rcvbulkpipe(d->udev, rx[i].endpoint),
                    rx[i].buf, GCHD_USB_BUFSIZE,
                    gchd_rx_complete, &rx[i]);
 }

 atomic_set(&d->rx_urb_errors, 0);
 WRITE_ONCE(d->rx_urb_last_status, 0);
 WRITE_ONCE(d->rx_active, true);
 dev_info(&d->intf->dev,
          "async bulk-IN starting: ep=0x81 URBs=%u maxpacket=%u; ep=0x84 URBs=%u maxpacket=%u speed=%u alt=%u\n",
          GCHD_RX_URBS_EP_IN, usb_endpoint_maxp(&ep81->desc),
          GCHD_RX_URBS_AUX_IN, usb_endpoint_maxp(&ep84->desc),
          d->udev->speed,
          d->intf->cur_altsetting->desc.bAlternateSetting);

 for (i = 0; i < GCHD_RX_URB_COUNT; ++i) {
  r = usb_submit_urb(rx[i].urb, GFP_KERNEL);
  if (r) {
   dev_err(&d->intf->dev,
           "initial bulk-IN submit failed: index=%u ep=0x%02x error=%d\n",
           i, rx[i].endpoint, r);
   goto out_kill;
  }
 }

 /* Do not issue START until all 40 receive URBs are queued. */
 WRITE_ONCE(d->rx_setup_status, 0);
 complete(&d->rx_ready);

 while (!kthread_should_stop() && !d->disconnected &&
        READ_ONCE(d->rx_active)) {
  struct gchd_rx_urb *done = NULL;
  unsigned long flags;
  int wait_r;

  wait_r = wait_event_interruptible_timeout(
      d->rx_wait,
      !list_empty_careful(&d->rx_done) ||
      kthread_should_stop() || !READ_ONCE(d->rx_active),
      msecs_to_jiffies(100));
  (void)wait_r;

  spin_lock_irqsave(&d->rx_lock, flags);
  if (!list_empty(&d->rx_done)) {
   done = list_first_entry(&d->rx_done, struct gchd_rx_urb, done);
   list_del_init(&done->done);
  }
  spin_unlock_irqrestore(&d->rx_lock, flags);

  if (!done) {
   gchd_signal_check(d);
   gchd_deliver(d);
   if (time_after_eq(jiffies, report_jiffies + 5 * HZ)) {
    dev_info(&d->intf->dev,
             "async bulk-IN stats: reads=%llu bytes=%llu ep84_bytes=%llu empty=%llu errors=%llu urb_errors=%d last_status=%d queued=%u start_pending=%u ring_dropped=%llu frames_waiting=%u\n",
             (unsigned long long)rx_reads,
             (unsigned long long)rx_bytes,
             (unsigned long long)rx_aux_bytes,
             (unsigned long long)rx_empty,
             (unsigned long long)rx_errors,
             atomic_read(&d->rx_urb_errors),
             READ_ONCE(d->rx_urb_last_status),
             !list_empty_careful(&d->rx_done),
             READ_ONCE(d->start_pending),
             (unsigned long long)d->ring.dropped,
             gchd_ring_count(&d->ring));
    report_jiffies = jiffies;
   }
   continue;
  }

  rx_reads++;
  if (done->endpoint == GCHD_EP_AUX_IN && done->urb->actual_length > 0) {
   if (!rx_aux_bytes)
    dev_info(&d->intf->dev,
             "bulk-IN: first data on ep=0x84 (%d bytes)\n",
             done->urb->actual_length);
   rx_aux_bytes += done->urb->actual_length;
  }
  if (done->urb->actual_length > 0) {
   rx_bytes += done->urb->actual_length;
   if (READ_ONCE(d->mark_rx)) {
    WRITE_ONCE(d->mark_rx, false);
    gchd_mark(d, "stream first-data ep=0x%02x", done->endpoint);
   }
  }
  else
   rx_empty++;

  /*
   * Before a scaled output has been switched to, the stream of the
   * standard output is not delivered.
   */
  /*
   * Transport output 1 arrives on 0x81, output 2 (scaled outputs) on 0x84;
   * only the endpoint of the running output is parsed.
   */
  if (done->endpoint != (d->input >= 1 && gchd_out_ts2(READ_ONCE(d->out_active)) ?
                         GCHD_EP_AUX_IN : GCHD_EP_TS_IN)) {
   /* Other endpoint: not parsed. */
  } else if (d->input >= 2 && READ_ONCE(d->out_active) != READ_ONCE(d->analog_out)) {
   /* Data flows: keep the signal-loss check satisfied. */
   if (done->urb->actual_length > 0)
    d->last_video_jiffies = jiffies;
  } else {
   int pos;

   for (pos = 0; pos < done->urb->actual_length;) {
    size_t take;

    /* USB transfers are arbitrary byte chunks; recover MPEG-TS alignment. */
    if (!d->ts_partial_len) {
     while (pos < done->urb->actual_length && done->buf[pos] != GCHD_TS_SYNC)
      pos++;
     if (pos == done->urb->actual_length)
      break;
    }

    take = min_t(size_t, GCHD_TS_PACKET - d->ts_partial_len,
                 done->urb->actual_length - pos);
    memcpy(d->ts_partial + d->ts_partial_len, done->buf + pos, take);
    d->ts_partial_len += take;
    pos += take;

    if (d->ts_partial_len == GCHD_TS_PACKET) {
     gchd_ts(d, d->ts_partial, pes, &n, &pes_active,
             &cc_valid, &last_cc);
     if (READ_ONCE(d->pixfmt) == V4L2_PIX_FMT_MPEG)
      gchd_ts_out(d, d->ts_partial);
     d->ts_partial_len = 0;
    }
   }
  }

  if (rx_reads <= 5)
   dev_info(&d->intf->dev,
            "bulk-IN first reads: read=%llu ep=0x81 actual=%d total_bytes=%llu\n",
            (unsigned long long)rx_reads, done->urb->actual_length,
            (unsigned long long)rx_bytes);

  gchd_signal_check(d);
  gchd_deliver(d);

  if (READ_ONCE(d->rx_active) && !kthread_should_stop()) {
   r = usb_submit_urb(done->urb, GFP_KERNEL);
   if (r) {
    rx_errors++;
    dev_err_ratelimited(&d->intf->dev,
                        "bulk-IN resubmit failed: ep=0x81 error=%d\n", r);
   }
  }

  if (time_after_eq(jiffies, report_jiffies + 5 * HZ)) {
   dev_info(&d->intf->dev,
            "async bulk-IN stats: reads=%llu bytes=%llu ep84_bytes=%llu empty=%llu errors=%llu urb_errors=%d last_status=%d queued=%u start_pending=%u ring_dropped=%llu frames_waiting=%u\n",
            (unsigned long long)rx_reads,
            (unsigned long long)rx_bytes,
            (unsigned long long)rx_aux_bytes,
            (unsigned long long)rx_empty,
            (unsigned long long)rx_errors,
            atomic_read(&d->rx_urb_errors),
            READ_ONCE(d->rx_urb_last_status),
            !list_empty_careful(&d->rx_done),
            READ_ONCE(d->start_pending),
            (unsigned long long)d->ring.dropped,
            gchd_ring_count(&d->ring));
   report_jiffies = jiffies;
  }
 }

out_kill:
 WRITE_ONCE(d->rx_active, false);
 wake_up_interruptible(&d->rx_wait);
 for (i = 0; i < GCHD_RX_URB_COUNT; ++i)
  if (rx[i].urb)
   usb_kill_urb(rx[i].urb);

 /* No callback can touch the queue after all URBs have been killed. */
 {
  unsigned long flags;
  spin_lock_irqsave(&d->rx_lock, flags);
  for (i = 0; i < GCHD_RX_URB_COUNT; ++i)
   if (!list_empty(&rx[i].done))
    list_del_init(&rx[i].done);
  spin_unlock_irqrestore(&d->rx_lock, flags);
 }
 if (pes_active && n)
  gchd_queue_pes(d, pes, n);
out_free:
 if (READ_ONCE(d->rx_setup_status) == -EINPROGRESS) {
  WRITE_ONCE(d->rx_setup_status, r ? r : -EIO);
  complete(&d->rx_ready);
 }
 for (i = 0; i < GCHD_RX_URB_COUNT; ++i) {
  usb_free_urb(rx[i].urb);
  kfree(rx[i].buf);
 }
 kvfree(pes);
 kfree(rx);
 return r;
}

/*
 * Leave START with the bulk-IN receive queue still running, so the device can
 * flush its output during NULL and STOP, then join the RX thread.
 */
static int gchd_capture_stop(struct gchd *d)
{
 int r;

 WRITE_ONCE(d->stop_pending, true);
 r = gchd_stream_stop(d);
 if (d->rx_thread) {
  kthread_stop(d->rx_thread);
  d->rx_thread = NULL;
 }
 WRITE_ONCE(d->stop_pending, false);
 return r;
}

static void gchd_detection_worker(struct work_struct *work)
{
 struct gchd *d = container_of(to_delayed_work(work), struct gchd,
                               detect_work);
 int r = 0;

 mutex_lock(&d->lifecycle_lock);
 if (d->disconnected || !READ_ONCE(d->hw_initialized) ||
     !READ_ONCE(d->detect_requested))
  goto out;

 gchd_set_lifecycle(d, GCHD_SOURCE_DETECTION);
 dev_dbg(&d->intf->dev, "source detection started: input=%u streaming=%u\n",
          READ_ONCE(d->input), READ_ONCE(d->streaming));

 /* Only the RX thread may own bulk-IN while START is active. */
 if (d->rx_thread || READ_ONCE(d->capture_active)) {
  /*
   * When the analog source changed mode, the encoder gets no pictures and
   * NULL does not complete; prepare the new mode first, as on a mode change.
   */
  if (READ_ONCE(d->capture_active) && d->input >= 2 &&
      d->input_height == 480 && d->input_configured) {
   bool interlaced;

   if (!gchd_input_read_analog_mode(d, &interlaced) &&
       interlaced != d->input_interlaced) {
    dev_info(&d->intf->dev,
             "redetection: source is now %s; preparing before stop\n",
             interlaced ? "480i" : "240p");
    gchd_analog_mode_pre_stop(d, interlaced);
   }
  }
  r = gchd_capture_stop(d);
  if (r) {
   /*
    * Keep capture_active set so the next pass retries the stop even though
    * the RX thread has already been joined. Do not probe/configure while the
    * device may still be in START.
    */
   dev_warn(&d->intf->dev,
            "redetection: stream stop failed: %d; retrying later\n", r);
   goto retry;
  }
  WRITE_ONCE(d->capture_active, false);

  /*
   * Stop transport and return to the stopped state without re-running
   * hardware init.
   * Keep the encoder firmware running: restarting it after an input change
   * fails with -EINVAL.
   */
  gchd_ring_free(&d->ring); WRITE_ONCE(d->wait_sps, true); d->ts_out_len = 0; WRITE_ONCE(d->wait_pat, true); d->ts_base_valid = false;
  d->ts_partial_len = 0;
  r = gchd_return_to_stop(d);
  if (r) {
   WRITE_ONCE(d->signal_present, false);
   dev_warn(&d->intf->dev,
            "redetection: stop failed: %d\n", r);
   goto retry;
  }
  d->input_configured = false;
  d->input_prepared = false;
 }

 if (d->input_configured && !READ_ONCE(d->signal_present)) {
  /*
   * Signal-loss notification can race with detector work. Invalidate the old
   * timing and return to the stopped state without resetting/reloading the device. Keep
   * the configured marker until the stop succeeds so a failed transition is
   * retried before any new source probe.
   */
  r = gchd_return_to_stop(d);
  if (r) {
   dev_warn(&d->intf->dev,
            "redetection: stop after signal loss failed: %d\n", r);
   goto retry;
  }
  d->input_configured = false;
  d->input_prepared = false;
 }
 if (!d->input_configured) {
  /*
   * Run connector preparation only after any previous capture has been
   * stopped and the device is back in the stopped state. The setup helper is idempotent
   * for the current input and stays complete across no-source retries.
   */
  if (!d->input_detect_prepared) {
   gchd_set_lifecycle(d, GCHD_GENERAL_INPUT_SETUP);
   r = gchd_prepare_input_detection(d);
   if (r) {
    dev_warn(&d->intf->dev,
             "general input setup failed: input=%u error=%d\n",
             READ_ONCE(d->input), r);
    goto retry;
   }
  }
  gchd_set_lifecycle(d, GCHD_SOURCE_DETECTION);
  /*
   * Detection must precede configuration. Do not run the encoder-start
   * handshake against a no-signal input: on HDNew this repeatedly fails
   * with -EINVAL and leaves retries in a bad lifecycle state.
   */
  r = gchd_probe_video_source(d);
  if (r == -ENOLINK) {
   dev_info_ratelimited(&d->intf->dev, "no signal on input=%u; retrying detection\n",
            READ_ONCE(d->input));
   d->input_prepared = false;
   goto retry;
  }
  if (r) {
   dev_warn(&d->intf->dev, "signal probe failed: %d\n", r);
   d->input_prepared = false;
   goto retry;
  }

  d->input_prepared = true;
  gchd_mark(d, "source detected");
  dev_info(&d->intf->dev,
           "signal detected: input=%u mode=%ux%u fps=%u/%u interlaced=%u\n",
           d->input, d->input_width, d->input_height, d->input_fps_num,
           d->input_fps_den, d->input_interlaced);

  gchd_set_lifecycle(d, GCHD_SOURCE_FORMAT_CONFIGURATION);
  r = gchd_input_configure_idle(d);
  if (r) {
   if (r != -ENOLINK) {
    dev_warn(&d->intf->dev,
             "redetection: input configuration failed: %d\n", r);
    /*
     * Configuration began from the stopped state and has not issued START.
     * Re-probe and retry setup.
     */
    d->input_configured = false;
    d->input_prepared = false;
    WRITE_ONCE(d->signal_present, false);
   }
   goto retry;
  }
 }

 if (READ_ONCE(d->signal_present)) {
  struct v4l2_event ev = { .type = V4L2_EVENT_SOURCE_CHANGE };

  gchd_set_negotiated(d, d->input_width, d->input_height);
  ev.id = d->input;
  ev.u.src_change.changes = V4L2_EVENT_SRC_CH_RESOLUTION;
  v4l2_event_queue(&d->vdev, &ev);

  if (!READ_ONCE(d->capture_active)) {
   /*
    * Capture starts automatically once a source is configured. V4L2
    * STREAMON only controls delivery into queued buffers.
    */
   r = gchd_input_check_mode(d);
   if (r) {
    d->input_configured = false;
    d->input_prepared = false;
    WRITE_ONCE(d->signal_present, false);
    gchd_set_lifecycle(d, GCHD_SOURCE_DETECTION);
    schedule_delayed_work(&d->detect_work, msecs_to_jiffies(50));
    goto out;
   }
   gchd_set_lifecycle(d, GCHD_CAPTURE_START);
   d->last_video_jiffies = jiffies;
   WRITE_ONCE(d->start_pending, true);
   reinit_completion(&d->rx_ready);
   WRITE_ONCE(d->rx_setup_status, -EINPROGRESS);
   d->rx_thread = kthread_run(gchd_rx, d, "gchd-rx");
   if (IS_ERR(d->rx_thread)) {
    r = PTR_ERR(d->rx_thread);
    d->rx_thread = NULL;
    WRITE_ONCE(d->start_pending, false);
    goto retry;
   }
   if (!wait_for_completion_timeout(&d->rx_ready, msecs_to_jiffies(2000))) {
    r = -ETIMEDOUT;
    kthread_stop(d->rx_thread);
    d->rx_thread = NULL;
    WRITE_ONCE(d->start_pending, false);
    dev_err(&d->intf->dev, "timed out waiting for bulk-IN queue setup\n");
    goto retry;
   }
   r = READ_ONCE(d->rx_setup_status);
   if (r) {
    kthread_stop(d->rx_thread);
    d->rx_thread = NULL;
    WRITE_ONCE(d->start_pending, false);
    dev_err(&d->intf->dev, "bulk-IN queue setup failed: %d\n", r);
    goto retry;
   }

   r = gchd_input_start(d);
   d->last_video_jiffies = jiffies;
   WRITE_ONCE(d->start_pending, false);
   if (r) {
    int start_error = r;
    int idle_error;

    kthread_stop(d->rx_thread);
    d->rx_thread = NULL;

    /*
     * A failed START must not leave the cached "configured + signal present"
     * flags set: that would make the next detection pass skip configuration
     * and never attempt capture again. Return to the stopped state before allowing the
     * detector to probe or configure the input again.
     */
    idle_error = gchd_return_to_stop(d);
    WRITE_ONCE(d->capture_active, false);
    if (idle_error) {
     gchd_set_lifecycle(d, GCHD_ERROR);
     dev_err(&d->intf->dev,
             "capture start failed: %d; recovery to stopped state failed: %d\n",
             start_error, idle_error);
     goto out;
    }

    d->input_configured = false;
    d->input_prepared = false;
    WRITE_ONCE(d->signal_present, false);
    gchd_set_lifecycle(d, GCHD_SOURCE_DETECTION);
    dev_warn(&d->intf->dev,
             "capture start failed: %d; stopped, retrying detection\n",
             start_error);
    /* Avoid hammering START if the state transition timed out. */
    schedule_delayed_work(&d->detect_work, msecs_to_jiffies(1000));
    goto out;
   }
   WRITE_ONCE(d->capture_active, true);
   gchd_set_lifecycle(d, GCHD_STREAMING);
   if ((d->input >= 2 &&
        (d->input_height == 480 || d->input_height == 576)) ||
       d->input == 1) {
    d->comp_mode_mismatch = 0;
    schedule_delayed_work(&d->mode_work, msecs_to_jiffies(250));
   }
  }
 }

retry:
 if ((r || !READ_ONCE(d->signal_present)) &&
     !d->disconnected && READ_ONCE(d->detect_requested))
  /*
   * The documented composite/S-video FA/F5 idle cycle is approximately
   * 50 ms. Keep analog source detection responsive without changing the
   * HDMI/component retry cadence.
   */
  schedule_delayed_work(&d->detect_work,
                        msecs_to_jiffies(d->input >= 2 ? 50 : 250));

out:
 mutex_unlock(&d->lifecycle_lock);
}

static void gchd_mode_workfn(struct work_struct *work)
{
 struct gchd *d = container_of(to_delayed_work(work), struct gchd,
                               mode_work);
 struct v4l2_event ev = { .type = V4L2_EVENT_SOURCE_CHANGE };
 bool target_interlaced = false;
 bool family_change = false;
 bool output_change = false;
 bool family_hold = false;
 int r = 0;

 mutex_lock(&d->lifecycle_lock);
 /*
  * Component: a new output size or changed encoder settings restart the
  * encoder; the source mode itself is followed by the detection.
  */
 if (d->input == 1) {
  if (d->disconnected || !READ_ONCE(d->capture_active) ||
      !d->input_configured || !READ_ONCE(d->signal_present))
   goto out;
  if (d->out_active == gchd_out_target(d) && !READ_ONCE(d->enc_dirty)) {
   /*
    * Source mode: another timing on two polls in a row (or no signal)
    * stops the capture; the detection configures the new mode and starts
    * again.
    */
   r = gchd_component_mode_changed(d);
   if (r > 0 && ++d->comp_mode_mismatch >= 2) {
    d->comp_mode_mismatch = 0;
    gchd_mark(d, "source mode-change");
    dev_info(&d->intf->dev,
             "component source mode changed while capturing; detecting again\n");
    WRITE_ONCE(d->signal_present, false);
    WRITE_ONCE(d->detect_requested, true);
    schedule_delayed_work(&d->detect_work, 0);
    goto out;
   }
   if (r <= 0)
    d->comp_mode_mismatch = 0;
   schedule_delayed_work(&d->mode_work, msecs_to_jiffies(750));
   goto out;
  }
  dev_info(&d->intf->dev, "encoder restart: output %u -> %u%s\n",
           d->out_active, gchd_out_target(d),
           READ_ONCE(d->enc_dirty) ? ", settings changed" : "");
  gchd_mark(d, "restart encoder");
  output_change = true;
  goto stop_and_restart;
 }
 if (d->disconnected || !READ_ONCE(d->capture_active) ||
     d->input < 2 ||
     (d->input_height != 480 && d->input_height != 576) ||
     !d->input_configured || !READ_ONCE(d->signal_present))
  goto out;

 /*
  * Selected output size differs from the running one, or encoder settings
  * changed: restart with the new encoder block.
  */
 if (d->out_active != d->analog_out || READ_ONCE(d->enc_dirty)) {
  dev_info(&d->intf->dev, "encoder restart: output %u -> %u%s\n",
           d->out_active, d->analog_out,
           READ_ONCE(d->enc_dirty) ? ", settings changed" : "");
  gchd_mark(d, "restart encoder");
  output_change = true;
  goto stop_and_restart;
 }

 /*
  * Family check. When 0x88:E7 has bit 7 set (decoder re-evaluating the
  * standard; ignored for 1 s after START and after a decoder-only mode
  * change) or 0x88:FA reports the other family on two consecutive polls,
  * stop and configure for the family the decoder reports.
  */
 {
  u8 family, lock, std;

  if (!gchd_input_read_family(d, &family) &&
      !gchd_input_read_lock(d, &lock, &std)) {
   bool other = (family == 6 && d->input_height == 576) ||
                (family == 7 && d->input_height == 480);

   if (other)
    ++d->family_mismatch;
   else
    d->family_mismatch = 0;
   if ((std & 0x80) && time_before(jiffies, d->std_hold_until))
    std = 0;
   if ((std & 0x80) || d->family_mismatch >= 2) {
    d->family_mismatch = 0;
    gchd_mark(d, "source family-change");
    dev_info(&d->intf->dev,
             "decoder status changed while capturing (FA=%x CA=%02x E7=%02x); configuring again\n",
             family, lock, std);
    family_change = true;
    /*
     * The signal had dropped (reset, power cycle) or the decoder is not
     * locked (console swapped without a detected drop): wait out a 60 Hz
     * start before configuring.
     */
    family_hold = (family != 6 && family != 7) ||
                  (lock != 0x7e && lock != 0x66);
    r = gchd_analog_family_pre_stop(d, family, lock);
    if (r)
     goto restart_failed;
    goto stop_and_restart;
   }
  }
 }

 /* 50 Hz sources: retrigger the back end when its status flag is set. */
 if (d->input_height == 576) {
  r = gchd_analog_poll_backend(d);
  if (r)
   dev_dbg(&d->intf->dev, "back-end poll failed: %d\n", r);
 }

 r = gchd_input_read_analog_mode(d, &target_interlaced);
 if (r == -EAGAIN)
  goto reschedule;
 if (r) {
  dev_dbg(&d->intf->dev, "analog mode poll failed: %d\n", r);
  goto reschedule;
 }
 if (target_interlaced == d->input_interlaced)
  goto reschedule;

 if (d->input_height == 576) {
  /* 576i <-> 288p: decoder only, the encoder keeps running. */
  gchd_mark(d, "source mode-change");
  dev_info(&d->intf->dev, "analog mode change detected: %s -> %s\n",
           d->input_interlaced ? "576i" : "288p",
           target_interlaced ? "576i" : "288p");
  WRITE_ONCE(d->stop_pending, true);
  r = gchd_analog_pal_mode_change(d, target_interlaced);
  d->last_video_jiffies = jiffies;
  WRITE_ONCE(d->stop_pending, false);
  if (r) {
   dev_warn(&d->intf->dev, "analog mode change failed: %d\n", r);
   goto reschedule;
  }
  d->input_interlaced = target_interlaced;
  d->std_hold_until = jiffies + msecs_to_jiffies(1000);
  ev.id = d->input;
  ev.u.src_change.changes = V4L2_EVENT_SRC_CH_RESOLUTION;
  v4l2_event_queue(&d->vdev, &ev);
  dev_info(&d->intf->dev, "analog mode now %s\n",
           d->input_interlaced ? "576i" : "288p");
  goto reschedule;
 }

 gchd_mark(d, "source mode-change");
  dev_info(&d->intf->dev, "analog mode change detected: %s -> %s\n",
          d->input_interlaced ? "480i" : "240p",
          target_interlaced ? "480i" : "240p");

 /*
  * Either direction: decoder and neutral back-end setup while still in
  * START, then NULL/STOP, the new back-end table, encoder programming and
  * START. Input routing and decoder tuning are kept.
  */
 r = gchd_analog_mode_pre_stop(d, target_interlaced);
 if (r)
  goto restart_failed;
stop_and_restart:
 r = gchd_capture_stop(d);
 if (r)
  goto restart_failed;

 gchd_ring_free(&d->ring); WRITE_ONCE(d->wait_sps, true); d->ts_out_len = 0; WRITE_ONCE(d->wait_pat, true); d->ts_base_valid = false;
 d->ts_partial_len = 0;
 d->input_configured = false;
 gchd_set_lifecycle(d, GCHD_SOURCE_FORMAT_CONFIGURATION);
 if (output_change) {
  r = gchd_analog_output_reconfigure(d);
 } else if (family_change) {
  r = gchd_analog_family_reconfigure(d, family_hold);
 } else {
  d->input_interlaced = target_interlaced;
  r = gchd_analog_mode_reconfigure(d);
 }
 if (r)
  goto restart_failed;
 /* The source can change again during the setup: detect again. */
 r = gchd_input_check_mode(d);
 if (r)
  goto restart_failed;

 gchd_set_lifecycle(d, GCHD_CAPTURE_START);
 d->last_video_jiffies = jiffies;
 WRITE_ONCE(d->start_pending, true);
 reinit_completion(&d->rx_ready);
 WRITE_ONCE(d->rx_setup_status, -EINPROGRESS);
 d->rx_thread = kthread_run(gchd_rx, d, "gchd-rx");
 if (IS_ERR(d->rx_thread)) {
  r = PTR_ERR(d->rx_thread);
  d->rx_thread = NULL;
  WRITE_ONCE(d->start_pending, false);
  goto restart_failed;
 }
 if (!wait_for_completion_timeout(&d->rx_ready, msecs_to_jiffies(2000))) {
  r = -ETIMEDOUT;
  kthread_stop(d->rx_thread);
  d->rx_thread = NULL;
  WRITE_ONCE(d->start_pending, false);
  dev_err(&d->intf->dev, "timed out waiting for bulk-IN queue setup\n");
  goto restart_failed;
 }
 r = READ_ONCE(d->rx_setup_status);
 if (r) {
  kthread_stop(d->rx_thread);
  d->rx_thread = NULL;
  WRITE_ONCE(d->start_pending, false);
  dev_err(&d->intf->dev, "bulk-IN queue setup failed: %d\n", r);
  goto restart_failed;
 }
 r = gchd_input_start(d);
 d->last_video_jiffies = jiffies;
 WRITE_ONCE(d->start_pending, false);
 if (r) {
  kthread_stop(d->rx_thread);
  d->rx_thread = NULL;
  goto restart_failed;
 }

 gchd_set_lifecycle(d, GCHD_STREAMING);
 gchd_set_negotiated(d, d->input_width, d->input_height);
 ev.id = d->input;
 ev.u.src_change.changes = V4L2_EVENT_SRC_CH_RESOLUTION;
 v4l2_event_queue(&d->vdev, &ev);
 if (d->input == 1) {
  dev_info(&d->intf->dev, "component output now %u\n", d->out_active);
  goto reschedule;
 }
 dev_info(&d->intf->dev, "analog mode now %s, output %u\n",
          d->input_height == 576 ? (d->input_interlaced ? "576i" : "288p") :
          (d->input_interlaced ? "480i" : "240p"), d->out_active);
 goto reschedule;

restart_failed:
 WRITE_ONCE(d->capture_active, false);
 gchd_set_lifecycle(d, GCHD_ERROR);
 dev_warn(&d->intf->dev,
          "analog mode change failed: %d\n", r);
 WRITE_ONCE(d->signal_present, false);
 WRITE_ONCE(d->detect_requested, true);
 gchd_set_lifecycle(d, GCHD_SOURCE_DETECTION);
 schedule_delayed_work(&d->detect_work, msecs_to_jiffies(250));

reschedule:
 if (!d->disconnected && READ_ONCE(d->capture_active))
  schedule_delayed_work(&d->mode_work, msecs_to_jiffies(250));
out:
 mutex_unlock(&d->lifecycle_lock);
}

static int gchd_queue_setup(struct vb2_queue *q, unsigned int *nb,
                            unsigned int *np, unsigned int sizes[],
                            struct device *alloc[])
{
 struct gchd *d = vb2_get_drv_priv(q);

 if (*np)
  return sizes[0] >= d->sizeimage ? 0 : -EINVAL;
 *np = 1;
 sizes[0] = d->sizeimage;
 return 0;
}

static void gchd_buf_queue(struct vb2_buffer *vb)
{
 struct gchd *d = vb2_get_drv_priv(vb->vb2_queue);
 struct gchd_buffer *b = container_of(to_vb2_v4l2_buffer(vb),
                                      struct gchd_buffer, vb);
 unsigned long flags;

 spin_lock_irqsave(&d->qlock, flags);
 list_add_tail(&b->list, &d->queued);
 spin_unlock_irqrestore(&d->qlock, flags);
 gchd_deliver(d);
}

static void gchd_return_queued(struct gchd *d, enum vb2_buffer_state state)
{
 struct gchd_buffer *b, *tmp;
 unsigned long flags;

 spin_lock_irqsave(&d->qlock, flags);
 list_for_each_entry_safe(b, tmp, &d->queued, list) {
  list_del(&b->list);
  vb2_buffer_done(&b->vb.vb2_buf, state);
 }
 spin_unlock_irqrestore(&d->qlock, flags);
}

static int gchd_start(struct vb2_queue *q, unsigned int count)
{
 struct gchd *d = vb2_get_drv_priv(q);
 int r = 0;

 gchd_mark(d, "request capture-start");
 mutex_lock(&d->lifecycle_lock);
 if (d->disconnected) {
  r = -ENODEV;
  goto err;
 }
 /* Deliver only from the first SPS of this stream onwards. */
 WRITE_ONCE(d->wait_sps, true); d->ts_base_valid = false;
 d->ts_out_len = 0; WRITE_ONCE(d->wait_pat, true);

 /*
  * A file open is not proof that an application intends to capture:
  * udev's v4l_id and desktop media services open nodes to inspect them.
  * Defer all device boot/init and state-machine activity until STREAMON,
  * when a client has actually requested capture buffers to be streamed.
  */
 if (!READ_ONCE(d->hw_initialized)) {
  gchd_set_lifecycle(d, GCHD_INITIALIZATION);
  WRITE_ONCE(d->capture_active, false);
  d->input_configured = false;
  d->input_routed = false;
  d->input_detect_prepared = false;
  d->input_prepared = false;
  d->encoder_started = false;
  d->signal_present = false;
  d->input_width = 0;
  d->input_height = 0;
  d->input_fps_num = 0;
  d->input_fps_den = 0;

  dev_info(&d->intf->dev,
           "GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON\n");
  /*
   * A boot directly after the shutdown (0.1 s) left the decoder without a
   * signal for the whole session; 1.6 s later it worked.
   */
  if (d->shutdown_done &&
      time_before(jiffies, d->shutdown_done + msecs_to_jiffies(GCHD_REBOOT_GAP_MS))) {
   unsigned int ms = jiffies_to_msecs(d->shutdown_done +
                                      msecs_to_jiffies(GCHD_REBOOT_GAP_MS) - jiffies);

   dev_info(&d->intf->dev, "waiting %u ms after the previous shutdown\n", ms);
   msleep(ms);
  }
  r = gchd_hw_init(d);
  if (r) {
   d->hw_initialized = false;
   d->hw_init_failed = true;
   gchd_set_lifecycle(d, GCHD_ERROR);
   goto err;
  }

  d->hw_initialized = true;
  d->hw_init_failed = false;
 }

 /* First actual capture request begins input selection and source detection. */
 if (!READ_ONCE(d->capture_active) &&
     !READ_ONCE(d->detect_requested)) {
  gchd_input_selection_enter(d);
 }
 WRITE_ONCE(d->streaming, true);
 if (!READ_ONCE(d->capture_active)) {
  WRITE_ONCE(d->detect_requested, true);
  gchd_set_lifecycle(d, GCHD_SOURCE_DETECTION);
  schedule_delayed_work(&d->detect_work, 0);
 }
 mutex_unlock(&d->lifecycle_lock);
 return 0;

err:
 mutex_unlock(&d->lifecycle_lock);
 gchd_return_queued(d, VB2_BUF_STATE_QUEUED);
 return r;
}

static void gchd_stop(struct vb2_queue *q)
{
 struct gchd *d = vb2_get_drv_priv(q);

 gchd_mark(d, "request capture-stop");
 /* STREAMOFF stops buffer delivery, not hardware capture or source monitoring. */
 WRITE_ONCE(d->streaming, false);
 gchd_return_queued(d, VB2_BUF_STATE_ERROR);
}

static const struct vb2_ops gchd_vb2_ops={
 .queue_setup=gchd_queue_setup,.buf_queue=gchd_buf_queue,.start_streaming=gchd_start,
 .stop_streaming=gchd_stop,.wait_prepare=vb2_ops_wait_prepare,.wait_finish=vb2_ops_wait_finish
};

static int gchd_enuminput(struct file *f, void *p, struct v4l2_input *in)
{
 struct gchd *d = video_drvdata(f);
 unsigned int index = in->index;

 if (index > 3) return -EINVAL;
 memset(in, 0, sizeof(*in));
 in->index = index;
 if (index == 0) {
  strscpy(in->name, "HDMI", sizeof(in->name));
 } else if (index == 1) {
  strscpy(in->name, "Component", sizeof(in->name));
 } else if (index == 2) {
  strscpy(in->name, "Composite", sizeof(in->name));
 } else {
  strscpy(in->name, "S-Video", sizeof(in->name));
 }
 in->type = V4L2_INPUT_TYPE_CAMERA;

 if (index == d->input && !READ_ONCE(d->signal_present)) {
  in->status = V4L2_IN_ST_NO_SIGNAL;
  /* Enumeration is often only GUI discovery; don't initialize hardware. */
  if (READ_ONCE(d->hw_initialized)) {
   WRITE_ONCE(d->detect_requested, true);
   schedule_delayed_work(&d->detect_work, 0);
  }
 }
 if (index < 2) {
  in->capabilities = V4L2_IN_CAP_DV_TIMINGS;
 } else {
  in->capabilities = V4L2_IN_CAP_STD;
  in->std = GCHD_ANALOG_STDS;
 }
 return 0;
}

/*
 * Analog standard. The source decides it; S_STD accepts any supported
 * standard and changes nothing. Digital inputs have no standard.
 */
static int gchd_g_std(struct file *f, void *p, v4l2_std_id *std)
{
 struct gchd *d = video_drvdata(f);

 if (d->input < 2)
  return -ENODATA;
 *std = gchd_analog_std(d);
 return 0;
}

static int gchd_s_std(struct file *f, void *p, v4l2_std_id std)
{
 struct gchd *d = video_drvdata(f);

 if (d->input < 2)
  return -ENODATA;
 return (std & GCHD_ANALOG_STDS) ? 0 : -EINVAL;
}

static int gchd_querystd(struct file *f, void *p, v4l2_std_id *std)
{
 struct gchd *d = video_drvdata(f);

 if (d->input < 2)
  return -ENODATA;
 *std = READ_ONCE(d->signal_present) ? gchd_analog_std(d) : V4L2_STD_UNKNOWN;
 return 0;
}

static int gchd_ginput(struct file *f, void *p, unsigned int *i)
{
 *i = ((struct gchd *)video_drvdata(f))->input;
 return 0;
}

static int gchd_set_input(struct gchd *d, unsigned int i)
{
 bool group_changed;
 int r;

 if (i > 3)
  return -EINVAL;
 dev_info(&d->intf->dev,
          "input switch requested: old=%u new=%u hw_initialized=%u streaming=%u\n",
          READ_ONCE(d->input), i, READ_ONCE(d->hw_initialized),
          READ_ONCE(d->streaming));
 if (i == d->input) {
  dev_dbg(&d->intf->dev, "input switch ignored: input %u already selected\n", i);
  return 0;
 }

 /*
  * Input selection is safe while buffers are allocated but the queue is not
  * streaming: sizeimage is a fixed maximum-frame allocation. Rejecting every
  * busy VB2 queue here prevents GUI applications from switching inputs after
  * they have prepared their capture buffers.
  */
 WRITE_ONCE(d->detect_requested, false);
 cancel_delayed_work_sync(&d->detect_work);
 cancel_delayed_work_sync(&d->mode_work);
 mutex_lock(&d->lifecycle_lock);
 if (d->disconnected) {
  mutex_unlock(&d->lifecycle_lock);
  return -ENODEV;
 }

 /*
  * Stop/join RX before taking over bulk-IN. Stop the transport even if the
  * V4L2 streaming flag is already clear: the device may still be in START
  * after a signal-loss or STREAMOFF race. The stop helper is a no-op in STOP
  * or IDLE and performs the NULL/STOP drain sequence otherwise.
  */
 if (!READ_ONCE(d->hw_initialized) && d->rx_thread) {
  kthread_stop(d->rx_thread);
  d->rx_thread = NULL;
 }
 if (READ_ONCE(d->hw_initialized)) {
  r = gchd_capture_stop(d);
  if (r) {
   dev_err(&d->intf->dev, "input switch: hardware stop failed: %d\n", r);
   WRITE_ONCE(d->streaming, false);
   WRITE_ONCE(d->detect_requested, true);
   gchd_set_lifecycle(d, GCHD_SOURCE_DETECTION);
   schedule_delayed_work(&d->detect_work, msecs_to_jiffies(250));
   mutex_unlock(&d->lifecycle_lock);
   return r;
  }
  WRITE_ONCE(d->capture_active, false);

  /*
   * Keep encoder firmware initialized; do not run full hardware shutdown or
   * input-specific teardown; a running stream is stopped (NULL -> STOP).
   */
  r = gchd_return_to_stop(d);
  if (r) {
   dev_err(&d->intf->dev,
           "input switch: stop failed: %d\n", r);
   WRITE_ONCE(d->signal_present, false);
   WRITE_ONCE(d->streaming, false);
   WRITE_ONCE(d->detect_requested, true);
   gchd_set_lifecycle(d, GCHD_SOURCE_DETECTION);
   schedule_delayed_work(&d->detect_work, msecs_to_jiffies(250));
   mutex_unlock(&d->lifecycle_lock);
   return r;
  }
 } else {
  WRITE_ONCE(d->capture_active, false);
 }

 /* Clear connector-specific cached state only after the stream is stopped. */
 d->input_configured = false;
 d->input_routed = false;
 d->input_detect_prepared = false;
 d->input_prepared = false;
 WRITE_ONCE(d->signal_present, false);
 gchd_ring_free(&d->ring); WRITE_ONCE(d->wait_sps, true); d->ts_out_len = 0; WRITE_ONCE(d->wait_pat, true); d->ts_base_valid = false;
 d->ts_partial_len = 0;
 /* The other settings group (composite/S-video, HDMI/component). */
 gchd_group_store(d);
 group_changed = gchd_group(d) != (i >= 2);
 d->previous_input = d->input;
 d->input = i;
 if (group_changed)
  gchd_group_load(d);
 gchd_update_tvnorms(d);
 gchd_set_lifecycle(d, GCHD_INPUT_SELECTION);
 mutex_lock(&gchd_last_input_lock);
 gchd_last_input = i;
 mutex_unlock(&gchd_last_input_lock);
 gchd_settings_save(d);
 d->mode_forced = false;
 /* encoder_started is deliberately preserved across connector changes. */
 d->last_video_jiffies = jiffies;
 d->input_width = 0;
 d->input_height = 0;
 d->input_fps_num = 0;
 d->input_fps_den = 0;
 d->input_interlaced = false;
 {
  u32 w, h;

  gchd_nominal_format(d, &w, &h);
  gchd_set_negotiated(d, w, h);
 }

 /*
  * Before first open, only remember the selection; hardware remains
  * untouched until the V4L2 client opens the device.
  */
 if (!READ_ONCE(d->hw_initialized)) {
  dev_info(&d->intf->dev,
           "input selected: input=%u; detection deferred until device open\n", i);
  mutex_unlock(&d->lifecycle_lock);
  if (group_changed)
   gchd_ctrls_show(d);
  return 0;
 }

 /* Detect/configure asynchronously so VIDIOC_S_INPUT stays non-blocking. */
 WRITE_ONCE(d->detect_requested, true);
 gchd_set_lifecycle(d, GCHD_SOURCE_DETECTION);
 schedule_delayed_work(&d->detect_work, 0);
 dev_info(&d->intf->dev,
          "input selected: input=%u; detection scheduled\n", i);
 mutex_unlock(&d->lifecycle_lock);
 if (group_changed)
  gchd_ctrls_show(d);
 return 0;
}

/*
 * Any file handle may switch the input, also while another one captures:
 * the capture continues with the new input (settings tools next to a
 * capturing application).
 */
static int gchd_sinput(struct file *f, void *p, unsigned int i)
{
 struct gchd *d = video_drvdata(f);

 gchd_mark(d, "request input %u", i);
 return gchd_set_input(d, i);
}

/*
 * Standard timings of the HDMI/component modes, in the order of
 * gchd_modes[] (same index).
 */
static const struct v4l2_dv_timings gchd_dv_presets[] = {
 V4L2_DV_BT_CEA_1920X1080P60,
 V4L2_DV_BT_CEA_1920X1080I60,
 V4L2_DV_BT_CEA_1280X720P60,
 V4L2_DV_BT_CEA_720X576P50,
 V4L2_DV_BT_CEA_720X576I50,
 V4L2_DV_BT_CEA_720X480P59_94,
 V4L2_DV_BT_CEA_720X480I59_94,
};

/* Index of a mode in gchd_modes[] / gchd_dv_presets[], or -1. */
static int gchd_dv_index(u32 width, u32 height, bool interlaced)
{
 const struct gchd_mode *m = gchd_find_mode(width, height, interlaced);

 return m ? (int)(m - gchd_modes) : -1;
}

/* Current timings of HDMI/component: the configured mode, else the last set. */
static int gchd_dv_current(const struct gchd *d)
{
 int i = gchd_dv_index(d->input_width, d->input_height, d->input_interlaced);

 return i >= 0 ? i : d->dv_index;
}

static int gchd_g_dv_timings(struct file *f, void *p, struct v4l2_dv_timings *t)
{
 struct gchd *d = video_drvdata(f);

 if (d->input >= 2)
  return -ENODATA;
 *t = gchd_dv_presets[gchd_dv_current(d)];
 return 0;
}

static int gchd_s_dv_timings(struct file *f, void *p, struct v4l2_dv_timings *t)
{
 struct gchd *d = video_drvdata(f);
 const struct gchd_mode *m;
 int i;

 if (d->input >= 2)
  return -ENODATA;
 if (t->type != V4L2_DV_BT_656_1120)
  return -EINVAL;
 i = gchd_dv_index(t->bt.width, t->bt.height, t->bt.interlaced);
 if (i < 0 || !gchd_mode_allowed(d, &gchd_modes[i]))
  return -EINVAL;
 if (vb2_is_busy(&d->vbq))
  return i == gchd_dv_current(d) ? 0 : -EBUSY;
 m = &gchd_modes[i];
 d->dv_index = i;
 d->input_width = m->width;
 d->input_height = m->height;
 d->input_fps_num = m->fps_num;
 d->input_fps_den = m->fps_den;
 d->input_interlaced = m->interlaced;
 d->mode_forced = true;
 d->input_configured = false;
 d->width = m->width;
 d->height = m->height;
 *t = gchd_dv_presets[i];
 return 0;
}

static int gchd_query_dv_timings(struct file *f, void *p,
				     struct v4l2_dv_timings *t)
{
 struct gchd *d = video_drvdata(f);
 int i;

 if (d->input >= 2)
  return -ENODATA;
 if (!READ_ONCE(d->signal_present)) {
  if (READ_ONCE(d->hw_initialized)) {
   WRITE_ONCE(d->detect_requested, true);
   schedule_delayed_work(&d->detect_work, 0);
  }
  return -ENOLINK;
 }
 i = gchd_dv_index(d->input_width, d->input_height, d->input_interlaced);
 if (i < 0)
  return -ERANGE;
 *t = gchd_dv_presets[i];
 return 0;
}

static int gchd_enum_dv_timings(struct file *f, void *p,
				 struct v4l2_enum_dv_timings *e)
{
 struct gchd *d = video_drvdata(f);
 unsigned int n = 0, i;

 if (d->input >= 2)
  return -ENODATA;
 if (e->pad)
  return -EINVAL;
 for (i = 0; i < ARRAY_SIZE(gchd_modes); ++i) {
  if (!gchd_mode_allowed(d, &gchd_modes[i]))
   continue;
  if (n++ != e->index)
   continue;
  e->timings = gchd_dv_presets[i];
  return 0;
 }
 return -EINVAL;
}

static int gchd_dv_timings_cap(struct file *f, void *p,
			       struct v4l2_dv_timings_cap *cap)
{
 struct gchd *d = video_drvdata(f);

 if (d->input >= 2)
  return -ENODATA;
 if (cap->pad)
  return -EINVAL;
 memset(cap, 0, sizeof(*cap));
 cap->type = V4L2_DV_BT_656_1120;
 cap->bt.min_width = 720;
 cap->bt.max_width = 1920;
 cap->bt.min_height = 480;
 cap->bt.max_height = 1080;
 cap->bt.min_pixelclock = 13500000;
 cap->bt.max_pixelclock = 148500000;
 cap->bt.standards = V4L2_DV_BT_STD_CEA861;
 cap->bt.capabilities = V4L2_DV_BT_CAP_INTERLACED | V4L2_DV_BT_CAP_PROGRESSIVE;
 return 0;
}

static int gchd_querycap(struct file*f,void*p,struct v4l2_capability*c)
{struct gchd *d = video_drvdata(f);
 strscpy(c->driver,"elgato-gchd",sizeof(c->driver));strscpy(c->card,"Elgato Game Capture HD",sizeof(c->card));
 usb_make_path(d->udev,c->bus_info,sizeof(c->bus_info));c->device_caps=V4L2_CAP_VIDEO_CAPTURE|V4L2_CAP_STREAMING|V4L2_CAP_READWRITE;c->capabilities=c->device_caps|V4L2_CAP_DEVICE_CAPS;return 0;}

static int gchd_enum(struct file*f,void*p,struct v4l2_fmtdesc*x)
{
 switch (x->index) {
 case 0:
  x->pixelformat = V4L2_PIX_FMT_H264;
  x->flags = V4L2_FMT_FLAG_COMPRESSED | V4L2_FMT_FLAG_CONTINUOUS_BYTESTREAM;
  strscpy(x->description, "H.264", sizeof(x->description));
  return 0;
 case 1:
  x->pixelformat = V4L2_PIX_FMT_MPEG;
  x->flags = V4L2_FMT_FLAG_COMPRESSED;
  strscpy(x->description, "MPEG-2 TS (H.264, MP2 audio)",
          sizeof(x->description));
  return 0;
 default:
  return -EINVAL;
 }
}

static bool gchd_pixfmt_ok(u32 f)
{
 return f == V4L2_PIX_FMT_H264 || f == V4L2_PIX_FMT_MPEG;
}

static u32 gchd_fmt_sizeimage(u32 pixfmt)
{
 return pixfmt == V4L2_PIX_FMT_MPEG ? GCHD_TS_BUF_SIZE : GCHD_MAX_FRAME;
}

static int gchd_enum_framesizes(struct file *f, void *p,
                                struct v4l2_frmsizeenum *s)
{
 struct gchd *d = video_drvdata(f);
 unsigned int i, n = 0;

 if (!gchd_pixfmt_ok(s->pixel_format))
  return -EINVAL;

 if (d->input >= 2) {
  /* Source size, 640x480, 480x360 (GCHD_OUT_* order). */
  if (s->index > GCHD_OUT_MOBILE)
   return -EINVAL;
  s->type = V4L2_FRMSIZE_TYPE_DISCRETE;
  gchd_analog_out_size(d, s->index, &s->discrete.width,
                       &s->discrete.height);
  return 0;
 }

 if (d->input == 1) {
  /* Source size, then the scaled sizes the source allows. */
  static const u8 order[] = { GCHD_OUT_STANDARD, GCHD_OUT_1280X720,
                              GCHD_OUT_640X480, GCHD_OUT_MOBILE };

  u32 sw, sh;

  /* Before detection the source size is the nominal one: not twice. */
  gchd_comp_out_size(d, GCHD_OUT_STANDARD, &sw, &sh);
  for (i = 0; i < ARRAY_SIZE(order); ++i) {
   u32 w, h;

   if (i && gchd_comp_out_of(d, order[i]) != order[i])
    continue;
   gchd_comp_out_size(d, order[i], &w, &h);
   if (i && w == sw && h == sh)
    continue;
   if (n++ == s->index) {
    s->type = V4L2_FRMSIZE_TYPE_DISCRETE;
    s->discrete.width = w;
    s->discrete.height = h;
    return 0;
   }
  }
  return -EINVAL;
 }

 for (i = 0; i < ARRAY_SIZE(gchd_modes); ++i) {
  if (!gchd_mode_allowed(d, &gchd_modes[i]))
   continue;
  if (n++ == s->index) {
   s->type = V4L2_FRMSIZE_TYPE_DISCRETE;
   s->discrete.width = gchd_modes[i].width;
   s->discrete.height = gchd_modes[i].height;
   return 0;
  }
 }
 return -EINVAL;
}

static int gchd_enum_frameintervals(struct file *f, void *p,
                                    struct v4l2_frmivalenum *v)
{
 struct gchd *d = video_drvdata(f);
 const struct gchd_mode *m;
 unsigned int i;

 if (!gchd_pixfmt_ok(v->pixel_format))
  return -EINVAL;

 if (d->input >= 2) {
  /* Encoded frame rate: 30000/1001 (60 Hz family) or 25 (50 Hz). */
  bool pal = gchd_std_is_50hz(gchd_analog_std(d));

  if (v->index != 0 ||
      !((v->width == 720 && (v->height == 480 || v->height == 576)) ||
        (v->width == 640 && v->height == 480) ||
        (v->width == 480 && v->height == 360)))
   return -EINVAL;
  if (v->width == 720)
   pal = v->height == 576;
  v->type = V4L2_FRMIVAL_TYPE_DISCRETE;
  v->discrete.numerator = pal ? 1 : 1001;
  v->discrete.denominator = pal ? 25 : 30000;
  return 0;
 }

 if (d->input == 1) {
  u8 out = gchd_comp_out_of(d, gchd_comp_out_for(v->width, v->height));
  u32 w, h;

  gchd_comp_out_size(d, out, &w, &h);
  if (v->index != 0 || w != v->width || h != v->height)
   return -EINVAL;
  v->type = V4L2_FRMIVAL_TYPE_DISCRETE;
  gchd_comp_fps(d, out, &v->discrete.denominator, &v->discrete.numerator);
  return 0;
 }

 m = NULL;
 for (i = 0; i < ARRAY_SIZE(gchd_modes); ++i) {
  if (gchd_modes[i].width == v->width &&
      gchd_modes[i].height == v->height &&
      gchd_mode_allowed(d, &gchd_modes[i])) {
   m = &gchd_modes[i];
   break;
  }
 }
 if (!m || v->index != 0)
  return -EINVAL;

 v->type = V4L2_FRMIVAL_TYPE_DISCRETE;
 v->discrete.numerator = m->fps_den;
 v->discrete.denominator = m->fps_num;
 return 0;
}

static int gchd_gfmt(struct file*f,void*p,struct v4l2_format*x)
{
 struct gchd*d=video_drvdata(f);

 /*
  * Composite and S-video: the size follows the source (buffers have a fixed
  * size for both): the configured mode, else the standard detected last. A
  * capture allocates its buffers before detection, so the negotiated size
  * alone stays at the nominal one.
  */
 if (d->input >= 2)
  gchd_analog_out_size(d, d->analog_out, &d->width, &d->height);
 else if (d->input == 1)
  gchd_comp_out_size(d, gchd_comp_out(d), &d->width, &d->height);
 x->fmt.pix.width=d->width;x->fmt.pix.height=d->height;
 x->fmt.pix.pixelformat=d->pixfmt;
 /* Analog inputs are always encoded interlaced (also 240p/288p). */
 x->fmt.pix.field=gchd_field(d);
 x->fmt.pix.sizeimage=d->sizeimage;
 x->fmt.pix.colorspace=V4L2_COLORSPACE_REC709;
 return 0;
}

/*
 * Adjust a requested format to one the current input can deliver.
 * Composite and S-video always deliver interlaced 720x480 or 720x576.
 * For HDMI and component the closest allowed mode is chosen; an unknown
 * size falls back to the negotiated one. Returns the mode for digital
 * inputs, or NULL for analog inputs.
 */
static const struct gchd_mode *gchd_adjust_fmt(struct gchd *d,
                                               struct v4l2_pix_format *pix)
{
 const struct gchd_mode *m = NULL;
 bool interlaced = pix->field == V4L2_FIELD_INTERLACED;

 if (!gchd_pixfmt_ok(pix->pixelformat))
  pix->pixelformat = V4L2_PIX_FMT_H264;
 if (d->input >= 2) {
  /* 640x480 and 480x360 are scaled outputs; any other size is the source size. */
  u8 out = gchd_analog_out_for(pix->width, pix->height);

  gchd_analog_out_size(d, out, &pix->width, &pix->height);
  pix->field = out == GCHD_OUT_STANDARD ? V4L2_FIELD_INTERLACED_TB :
                                          V4L2_FIELD_NONE;
 } else if (d->input == 1) {
  /* Component: an output size the source allows; the mode is detected. */
  u8 out = gchd_comp_out_of(d, gchd_comp_out_for(pix->width, pix->height));
  u32 w, h;

  gchd_comp_out_size(d, out, &w, &h);
  pix->width = w;
  pix->height = h;
  pix->field = out != GCHD_OUT_STANDARD || !d->input_interlaced ?
               V4L2_FIELD_NONE : V4L2_FIELD_INTERLACED;
 } else {
  m = gchd_find_mode(pix->width, pix->height, interlaced);
  if (!m || !gchd_mode_allowed(d, m))
   m = gchd_find_mode(pix->width, pix->height, !interlaced);
  if (!m || !gchd_mode_allowed(d, m))
   m = gchd_find_mode(d->width, d->height, d->input_interlaced);
  if (m && !gchd_mode_allowed(d, m))
   m = NULL;
  if (m) {
   pix->width = m->width;
   pix->height = m->height;
   pix->field = m->interlaced ? V4L2_FIELD_INTERLACED : V4L2_FIELD_NONE;
  } else {
   pix->width = d->width;
   pix->height = d->height;
   pix->field = d->input_interlaced ? V4L2_FIELD_INTERLACED : V4L2_FIELD_NONE;
  }
 }
 pix->bytesperline = 0;
 pix->sizeimage = gchd_fmt_sizeimage(pix->pixelformat);
 pix->colorspace = V4L2_COLORSPACE_REC709;
 return m;
}

/*
 * Composite/S-video output size; 480 x 360 has its own kept bitrate
 * (other range), so the bitrate in use follows (control handler lock: the
 * bitrate is a control value).
 */
static void gchd_set_analog_out(struct gchd *d, u8 out)
{
 if (out == d->analog_out)
  return;
 mutex_lock(d->ctrls.lock);
 gchd_rate_store(d);
 d->analog_out = out;
 gchd_rate_load(d);
 mutex_unlock(d->ctrls.lock);
}

static int gchd_sfmt(struct file*f,void*p,struct v4l2_format*x)
{
 struct gchd*d=video_drvdata(f);
 const struct gchd_mode *m;

 if (vb2_is_busy(&d->vbq)) {
  u8 out;

  /*
   * Composite/S-video while buffers exist (any file handle): another
   * output size is applied to the running capture by an encoder restart
   * (mode worker); the negotiated format of the buffers stays.
   */
  if (d->input == 0 || x->fmt.pix.pixelformat != d->pixfmt)
   return -EBUSY;
  if (d->input == 1) {
   /* Component: the new output size by an encoder restart. */
   out = gchd_comp_out_for(x->fmt.pix.width, x->fmt.pix.height);
   if (out != d->hd_out) {
    gchd_mark(d, "request output %ux%u", x->fmt.pix.width, x->fmt.pix.height);
    d->hd_out = out;
    gchd_settings_save(d);
    if (d->streaming)
     mod_delayed_work(system_wq, &d->mode_work, 0);
   }
   return gchd_gfmt(f, p, x);
  }
  out = gchd_analog_out_for(x->fmt.pix.width, x->fmt.pix.height);
  if (out != d->analog_out) {
   gchd_mark(d, "request output %ux%u", x->fmt.pix.width, x->fmt.pix.height);
   gchd_set_analog_out(d, out);
   gchd_settings_save(d);
   if (d->streaming)
    mod_delayed_work(system_wq, &d->mode_work, 0);
  }
  return gchd_gfmt(f, p, x);
 }

 if (d->input == 1)
  d->hd_out = gchd_comp_out_for(x->fmt.pix.width, x->fmt.pix.height);
 m = gchd_adjust_fmt(d, &x->fmt.pix);
 if (d->input >= 2)
  gchd_set_analog_out(d, gchd_analog_out_for(x->fmt.pix.width,
                                             x->fmt.pix.height));
 d->width = x->fmt.pix.width;
 d->height = x->fmt.pix.height;
 d->pixfmt = x->fmt.pix.pixelformat;
 d->sizeimage = gchd_fmt_sizeimage(d->pixfmt);
 gchd_settings_save(d);
 if (m) {
  /* Digital inputs: the requested mode is programmed as given. */
  d->input_width=m->width;
  d->input_height=m->height;
  d->input_fps_num=m->fps_num;
  d->input_fps_den=m->fps_den;
  d->input_interlaced=m->interlaced;
  d->mode_forced=true;
  d->input_configured=false;
 }
 return gchd_gfmt(f,p,x);
}

static int gchd_tryfmt(struct file*f,void*p,struct v4l2_format*x)
{
 gchd_adjust_fmt(video_drvdata(f), &x->fmt.pix);
 return 0;
}

static int gchd_g_parm(struct file *f, void *p, struct v4l2_streamparm *a)
{
 struct gchd *d = video_drvdata(f);
 struct v4l2_fract *t = &a->parm.capture.timeperframe;

 if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
  return -EINVAL;
 memset(&a->parm.capture, 0, sizeof(a->parm.capture));
 a->parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
 /* Buffers used by read() (vb2 file I/O). */
 a->parm.capture.readbuffers = 2;
 if (d->input >= 2) {
  bool pal = d->input_height ? d->input_height == 576 :
                               gchd_std_is_50hz(gchd_analog_std(d));

  t->numerator = pal ? 1 : 1001;
  t->denominator = pal ? 25 : 30000;
 } else if (d->input == 1) {
  gchd_comp_fps(d, gchd_comp_out(d), &t->denominator, &t->numerator);
 } else if (d->input_fps_num && d->input_fps_den) {
  t->numerator = d->input_fps_den;
  t->denominator = d->input_fps_num;
 } else {
  t->numerator = 1;
  t->denominator = d->height == 576 ? 50 : 60;
 }
 return 0;
}

/* The frame rate follows the source; S_PARM reports the current rate. */
static int gchd_s_parm(struct file *f, void *p, struct v4l2_streamparm *a)
{
 return gchd_g_parm(f, p, a);
}

static int gchd_subscribe_event(struct v4l2_fh *fh,
                                const struct v4l2_event_subscription *sub)
{
 if (sub->type != V4L2_EVENT_SOURCE_CHANGE &&
     sub->type != V4L2_EVENT_CTRL)
  return -EINVAL;
 if (sub->type == V4L2_EVENT_CTRL)
  return v4l2_ctrl_subscribe_event(fh, sub);
 return v4l2_event_subscribe(fh, sub, 8, NULL);
}

static const struct v4l2_ioctl_ops gchd_ioctl={
 .vidioc_querycap=gchd_querycap,
 .vidioc_enum_fmt_vid_cap=gchd_enum,
 .vidioc_enum_framesizes=gchd_enum_framesizes,
 .vidioc_enum_frameintervals=gchd_enum_frameintervals,
 .vidioc_enum_input=gchd_enuminput,
 .vidioc_g_std=gchd_g_std,.vidioc_s_std=gchd_s_std,.vidioc_querystd=gchd_querystd,
 .vidioc_g_input=gchd_ginput,.vidioc_s_input=gchd_sinput,
 .vidioc_g_fmt_vid_cap=gchd_gfmt,.vidioc_s_fmt_vid_cap=gchd_sfmt,
.vidioc_try_fmt_vid_cap=gchd_tryfmt,
 .vidioc_g_parm=gchd_g_parm,.vidioc_s_parm=gchd_s_parm,
 .vidioc_query_dv_timings=gchd_query_dv_timings,
 .vidioc_g_dv_timings=gchd_g_dv_timings,.vidioc_s_dv_timings=gchd_s_dv_timings,
 .vidioc_dv_timings_cap=gchd_dv_timings_cap,
 .vidioc_enum_dv_timings=gchd_enum_dv_timings,
 .vidioc_reqbufs=vb2_ioctl_reqbufs,.vidioc_querybuf=vb2_ioctl_querybuf,
 .vidioc_qbuf=vb2_ioctl_qbuf,.vidioc_dqbuf=vb2_ioctl_dqbuf,
 .vidioc_streamon=vb2_ioctl_streamon,.vidioc_streamoff=vb2_ioctl_streamoff,
 .vidioc_subscribe_event=gchd_subscribe_event,
 .vidioc_unsubscribe_event=v4l2_event_unsubscribe
};

/*
 * Bitrate control value (bit/s) of the settings in use; automatic: the
 * encoder's default (composite/S-video 11.25 Mbit/s, 15 Mbit/s for
 * 480 x 360).
 */
static s32 gchd_ctrl_bitrate_value(const struct gchd *d)
{
 if (d->bitrate_forced)
  return d->bitrate * 1000;
 return d->input >= 2 && d->analog_out == GCHD_OUT_MOBILE ? 15000000 :
                                                           11250000;
}

/* Level menu value of the settings in use (automatic: 4.1). */
static s32 gchd_ctrl_level_value(const struct gchd *d)
{
 unsigned int i;

 for (i = 0; i < ARRAY_SIZE(gchd_levels); ++i)
  if (d->h264_level_forced && gchd_levels[i][1] == d->h264_level)
   return gchd_levels[i][0];
 return V4L2_MPEG_VIDEO_H264_LEVEL_4_1;
}

/*
 * Show the settings in use in the controls (after an input change to the
 * other group). Not called from a control handler (takes the handler lock).
 */
static void gchd_ctrls_show(struct gchd *d)
{
 WRITE_ONCE(d->ctrl_sync, true);
 if (d->ctrl_mode)
  v4l2_ctrl_s_ctrl(d->ctrl_mode, d->bitrate_mode);
 if (d->ctrl_profile)
  v4l2_ctrl_s_ctrl(d->ctrl_profile, d->h264_profile);
 if (d->ctrl_level)
  v4l2_ctrl_s_ctrl(d->ctrl_level, gchd_ctrl_level_value(d));
 WRITE_ONCE(d->ctrl_sync, false);
}

static int gchd_ctrl(struct v4l2_ctrl *c)
{
 struct gchd *d = container_of(c->handler, struct gchd, ctrls);
 u8 level;

 /* Values shown by the driver itself (group change, remembered bitrate). */
 if (READ_ONCE(d->ctrl_sync))
  return 0;

 /*
  * Encoder settings: on composite and S-video a change while capturing
  * restarts the encoder (mode worker); other inputs only while stopped.
  */
 if (d->streaming && d->input == 0)
  return -EBUSY;

 switch (c->id) {
 case V4L2_CID_MPEG_VIDEO_BITRATE_MODE:
  if (c->val != d->bitrate_mode) {
   /*
    * Keep the bitrate of the previous mode and use the one last set
    * in the new mode (the volatile bitrate control shows it).
    */
   gchd_rate_store(d);
   d->bitrate_mode = c->val;
   gchd_rate_load(d);
  }
  break;
 case V4L2_CID_GCHD_STRETCH:
  d->stretch = c->val;
  break;
 case V4L2_CID_GCHD_ALLOW_5060:
  d->allow_5060 = c->val;
  break;
 case V4L2_CID_MPEG_VIDEO_BITRATE:
  d->bitrate = c->val / 1000;
  d->bitrate_forced = true;
  if (d->bitrate < 1)
   d->bitrate = 1;
  gchd_rate_store(d);
  break;
 case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
  if (c->val > V4L2_MPEG_VIDEO_H264_PROFILE_HIGH)
   return -EINVAL;
  d->h264_profile = c->val;
  break;
 case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
  switch (c->val) {
  case V4L2_MPEG_VIDEO_H264_LEVEL_3_0: level = 30; break;
  case V4L2_MPEG_VIDEO_H264_LEVEL_3_1: level = 31; break;
  case V4L2_MPEG_VIDEO_H264_LEVEL_3_2: level = 32; break;
  case V4L2_MPEG_VIDEO_H264_LEVEL_4_0: level = 40; break;
  case V4L2_MPEG_VIDEO_H264_LEVEL_4_1: level = 41; break;
  case V4L2_MPEG_VIDEO_H264_LEVEL_4_2: level = 42; break;
  case V4L2_MPEG_VIDEO_H264_LEVEL_5_0: level = 50; break;
  case V4L2_MPEG_VIDEO_H264_LEVEL_5_1: level = 51; break;
  default: return -EINVAL;
  }
  d->h264_level = level;
  d->h264_level_forced = true;
  break;
 default:
  return -EINVAL;
 }
 gchd_settings_save(d);
 if (d->streaming) {
  WRITE_ONCE(d->enc_dirty, true);
  gchd_mark(d, "request control 0x%x", c->id);
  mod_delayed_work(system_wq, &d->mode_work, 0);
 }
 return 0;
}
/*
 * V4L2_CID_DV_RX_POWER_PRESENT: a signal on the selected HDMI/component
 * input. V4L2_CID_MPEG_VIDEO_BITRATE: the bitrate in use (it follows the
 * bitrate mode, the output size and the settings group).
 */
static int gchd_g_volatile_ctrl(struct v4l2_ctrl *c)
{
 struct gchd *d = container_of(c->handler, struct gchd, ctrls);

 if (c->id == V4L2_CID_MPEG_VIDEO_BITRATE)
  c->val = gchd_ctrl_bitrate_value(d);
 if (c->id == V4L2_CID_DV_RX_POWER_PRESENT)
  c->val = d->input < 2 && READ_ONCE(d->signal_present);
 return 0;
}
static const struct v4l2_ctrl_ops gchd_ctrl_ops = {
 .s_ctrl = gchd_ctrl, .g_volatile_ctrl = gchd_g_volatile_ctrl,
};

static int gchd_fop_open(struct file *file)
{
 struct gchd *d = video_drvdata(file);
 int r = 0;

 /* Protect the embedded video_device/driver state against unplug races. */
 kref_get(&d->refcount);
 if (vb2_fops.open) {
  r = vb2_fops.open(file);
  if (r) {
   kref_put(&d->refcount, gchd_release_ref);
   return r;
  }
 }

 dev_info(&d->intf->dev,
          "GCHD lifecycle: V4L2 open requested by pid=%d comm=%s\n",
          task_pid_nr(current), current->comm);

 mutex_lock(&d->users_lock);
 if (d->disconnected) {
  r = -ENODEV;
 } else {
  /*
   * Opening the node only grants access to its V4L2 metadata and controls.
   * It must not boot hardware or start input detection: udev/v4l_id and
   * desktop media services routinely open nodes just to identify them.
   * The first STREAMON performs the ONLINE/POWER_OFF -> INITIALIZATION transition.
   */
  d->open_count++;
 }
 mutex_unlock(&d->users_lock);

 if (r && vb2_fops.release)
  vb2_fops.release(file);
 if (r)
  kref_put(&d->refcount, gchd_release_ref);
 return r;
}

static int gchd_fop_release(struct file *file)
{
 struct gchd *d = video_drvdata(file);
 int r = 0;

 /* VB2 releases userspace buffers; hardware remains initialized on close. */
 if (vb2_fops.release)
  r = vb2_fops.release(file);

 mutex_lock(&d->users_lock);
 if (d->open_count)
  d->open_count--;

 if (!d->open_count && !d->disconnected) {
  WRITE_ONCE(d->detect_requested, false);
  WRITE_ONCE(d->streaming, false);

  /* Never wait for work while holding lifecycle_lock: the worker takes it. */
  cancel_delayed_work_sync(&d->detect_work);
  cancel_delayed_work_sync(&d->mode_work);
  mutex_lock(&d->lifecycle_lock);

  /* Stop the stream with the receive queue still running, then join RX. */
  if (d->hw_initialized) {
   int stop_r = gchd_capture_stop(d);

   if (stop_r)
    dev_warn(&d->intf->dev, "capture stop on last close failed: %d\n",
             stop_r);
  } else if (d->rx_thread) {
   kthread_stop(d->rx_thread);
   d->rx_thread = NULL;
  }

  /*
   * Closing the last V4L2 handle ends the capture session. Stop the hardware
   * cleanly while USB is still present, then leave the registered device
   * powered off until a real capture request arrives.
   */
  if (d->hw_initialized) {
   int shutdown_r = gchd_hw_shutdown(d);

   if (shutdown_r) {
    dev_warn(&d->intf->dev,
             "hardware shutdown on last close failed: %d; keeping initialized state\n",
             shutdown_r);
    gchd_set_lifecycle(d, GCHD_ERROR);
   } else {
    d->hw_initialized = false;
    d->hw_init_failed = false;
    d->shutdown_done = jiffies | 1;
    gchd_set_lifecycle(d, GCHD_POWER_OFF);
   }
  }

  WRITE_ONCE(d->capture_active, false);
  WRITE_ONCE(d->start_pending, false);
  d->input_configured = false;
  d->input_routed = false;
  d->input_detect_prepared = false;
  d->input_prepared = false;
  d->encoder_started = false;
  d->signal_present = false;
  gchd_ring_free(&d->ring); WRITE_ONCE(d->wait_sps, true); d->ts_out_len = 0; WRITE_ONCE(d->wait_pat, true); d->ts_base_valid = false;
  d->ts_partial_len = 0;
  mutex_unlock(&d->lifecycle_lock);
 }
 mutex_unlock(&d->users_lock);
 kref_put(&d->refcount, gchd_release_ref);
 return r;
}

int gchd_v4l2_register(struct gchd*d)
{
 int r=v4l2_device_register(&d->intf->dev,&d->v4l2_dev);if(r)return r;
 v4l2_ctrl_handler_init(&d->ctrls,7);
 /* Control defaults are the remembered settings (gchd_settings_load()). */
 {
  unsigned int i, level_def = V4L2_MPEG_VIDEO_H264_LEVEL_4_1;

  for (i = 0; i < ARRAY_SIZE(gchd_levels); ++i)
   if (d->h264_level_forced && gchd_levels[i][1] == d->h264_level)
    level_def = gchd_levels[i][0];
  d->ctrl_bitrate = v4l2_ctrl_new_std(&d->ctrls,&gchd_ctrl_ops,V4L2_CID_MPEG_VIDEO_BITRATE,32000,40000000,1000,
                    gchd_ctrl_bitrate_value(d));
  if (d->ctrl_bitrate)
   d->ctrl_bitrate->flags |= V4L2_CTRL_FLAG_VOLATILE |
                             V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;
  d->ctrl_mode = v4l2_ctrl_new_std_menu(&d->ctrls,&gchd_ctrl_ops,V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
   V4L2_MPEG_VIDEO_BITRATE_MODE_CBR,0,d->bitrate_mode);
  d->ctrl_profile = v4l2_ctrl_new_std_menu(&d->ctrls,&gchd_ctrl_ops,V4L2_CID_MPEG_VIDEO_H264_PROFILE,V4L2_MPEG_VIDEO_H264_PROFILE_HIGH,
   ~(BIT(V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE)|BIT(V4L2_MPEG_VIDEO_H264_PROFILE_MAIN)|BIT(V4L2_MPEG_VIDEO_H264_PROFILE_HIGH)),d->h264_profile);
  /* Levels below 3.0 are not offered (0xff skips 1.0 ... 2.2). */
  d->ctrl_level = v4l2_ctrl_new_std_menu(&d->ctrls,&gchd_ctrl_ops,V4L2_CID_MPEG_VIDEO_H264_LEVEL,V4L2_MPEG_VIDEO_H264_LEVEL_5_1,0xff,level_def);
  {
   struct v4l2_ctrl *power = v4l2_ctrl_new_std(&d->ctrls, &gchd_ctrl_ops,
                                               V4L2_CID_DV_RX_POWER_PRESENT, 0, 1, 0, 0);

   if (power)
    power->flags |= V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY;
  }
 }
 {
  static const struct v4l2_ctrl_config stretch_cfg = {
   .ops = &gchd_ctrl_ops,
   .id = V4L2_CID_GCHD_STRETCH,
   .name = "Stretch SD Input",
   .type = V4L2_CTRL_TYPE_BOOLEAN,
   .min = 0, .max = 1, .step = 1,
  };
  static const struct v4l2_ctrl_config allow_cfg = {
   .ops = &gchd_ctrl_ops,
   .id = V4L2_CID_GCHD_ALLOW_5060,
   .name = "Allow 50/60 fps",
   .type = V4L2_CTRL_TYPE_BOOLEAN,
   .min = 0, .max = 1, .step = 1,
  };
  struct v4l2_ctrl_config cfg = stretch_cfg;

  cfg.def = d->stretch;
  v4l2_ctrl_new_custom(&d->ctrls, &cfg, NULL);
  cfg = allow_cfg;
  cfg.def = d->allow_5060;
  v4l2_ctrl_new_custom(&d->ctrls, &cfg, NULL);
 }
 if(d->ctrls.error){r=d->ctrls.error;goto err;}
 memset(&d->vbq,0,sizeof(d->vbq));d->vbq.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;d->vbq.io_modes=VB2_MMAP|VB2_READ;d->vbq.timestamp_flags=V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;d->vbq.dev=&d->intf->dev;
 d->vbq.drv_priv=d;d->vbq.ops=&gchd_vb2_ops;d->vbq.mem_ops=&vb2_vmalloc_memops;d->vbq.lock=&d->lock;
 r=vb2_queue_init(&d->vbq);if(r)goto err;
 strscpy(d->vdev.name,d->family == GCHD_FAMILY_HDNEW ? "Elgato Game Capture HD (HDNew)" : "Elgato Game Capture HD",sizeof(d->vdev.name));d->vdev.v4l2_dev=&d->v4l2_dev;d->vdev.release=video_device_release_empty;
 gchd_fops = vb2_fops;
 gchd_fops.open = gchd_fop_open;
 gchd_fops.release = gchd_fop_release;
 d->vdev.tvnorms=GCHD_ANALOG_STDS;d->vdev.fops=&gchd_fops;d->vdev.ioctl_ops=&gchd_ioctl;d->vdev.queue=&d->vbq;d->vdev.lock=&d->lock;
 d->vdev.ctrl_handler=&d->ctrls;d->vdev.device_caps=V4L2_CAP_VIDEO_CAPTURE|V4L2_CAP_STREAMING|V4L2_CAP_READWRITE;video_set_drvdata(&d->vdev,d);
 r=video_register_device(&d->vdev,VFL_TYPE_VIDEO,-1);if(r)goto err;
 gchd_update_tvnorms(d);
 return 0;
err:v4l2_ctrl_handler_free(&d->ctrls);v4l2_device_unregister(&d->v4l2_dev);return r;
}
void gchd_v4l2_unregister(struct gchd*d){video_unregister_device(&d->vdev);v4l2_ctrl_handler_free(&d->ctrls);v4l2_device_unregister(&d->v4l2_dev);}

static const struct usb_device_id gchd_ids[] = {
 { USB_DEVICE(GCHD_VID, GCHD_PID_HD_0044), .driver_info = GCHD_FAMILY_OLD },
 { USB_DEVICE(GCHD_VID, GCHD_PID_HD_004E), .driver_info = GCHD_FAMILY_OLD },
 { USB_DEVICE(GCHD_VID, GCHD_PID_HD_0051), .driver_info = GCHD_FAMILY_OLD },
 { USB_DEVICE(GCHD_VID, GCHD_PID_HDNEW), .driver_info = GCHD_FAMILY_HDNEW },
 { }
};
MODULE_DEVICE_TABLE(usb, gchd_ids);
static int gchd_probe(struct usb_interface *i,
                      const struct usb_device_id *id)
{
 struct gchd *d;
 int r;

 d = kzalloc(sizeof(*d), GFP_KERNEL);
 if (!d)
  return -ENOMEM;
 kref_init(&d->refcount);
 d->udev = usb_get_dev(interface_to_usbdev(i));
 d->intf = i;
 d->family = (enum gchd_family)id->driver_info;

 dev_info(&i->dev, "GCHD lifecycle: USB probe; registering V4L2 only\\n");
 r = usb_reset_configuration(d->udev);
 if (r)
  goto err;
 mutex_init(&d->lock);
 mutex_init(&d->lifecycle_lock);
 d->lifecycle_state = GCHD_OFFLINE;
 mutex_init(&d->users_lock);
 INIT_DELAYED_WORK(&d->detect_work, gchd_detection_worker);
 INIT_DELAYED_WORK(&d->mode_work, gchd_mode_workfn);
 spin_lock_init(&d->qlock);
 INIT_LIST_HEAD(&d->queued);
 spin_lock_init(&d->rx_lock);
 INIT_LIST_HEAD(&d->rx_done);
 init_waitqueue_head(&d->rx_wait);
 init_completion(&d->rx_ready);
 mutex_init(&d->deliver_lock);
 d->wait_sps = true;
 d->rx_setup_status = -EINPROGRESS;
 spin_lock_init(&d->ring.lock);
 d->width = 1280;
 d->height = 720;
 d->sizeimage = GCHD_MAX_FRAME;
 d->dv_index = 2; /* 1280x720p60 before the first detection or S_DV_TIMINGS */
 /*
  * The remembered input already at probe, so that G_INPUT and G_FMT report
  * it before the first capture; INPUT_SELECTION resolves it again after the
  * hardware initialization.
  */
 d->input = gchd_last_input <= 3 ? gchd_last_input : 0;
 /*
  * The decoder keeps its S-video setting while the device has power, also
  * across module reloads: the first composite start writes the S-video to
  * composite sequence.
  */
 d->dec_svideo = true;
 d->input_width = 0;
 d->input_height = 0;
 d->input_fps_num = 0;
 d->input_fps_den = 0;
 gchd_settings_load(d);
 d->usb_buf = kmalloc(GCHD_USB_BUFSIZE, GFP_KERNEL);
 d->ts_out = kmalloc(GCHD_TS_BUF_SIZE, GFP_KERNEL);
 if (!d->usb_buf || !d->ts_out) {
  r = -ENOMEM;
  goto err;
 }
 d->sizeimage = gchd_fmt_sizeimage(d->pixfmt);
 d->wait_pat = true;

 /*
  * A successful USB probe finds the device and registers its V4L2 node.
  * Mark it ONLINE only after registration succeeds. Hardware initialization
  * remains deferred until STREAMON; POWER_OFF is reserved for clean shutdown.
  */
 usb_set_intfdata(i, d);
 r = gchd_v4l2_register(d);
 if (r) {
  usb_set_intfdata(i, NULL);
  goto err;
 }
 gchd_set_lifecycle(d, GCHD_ONLINE);
 dev_info(&i->dev,
          "GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON\n");
 return 0;

err:
 kref_put(&d->refcount, gchd_release_ref);
 return r;
}

static void gchd_disconnect(struct usb_interface *i)
{
 struct gchd *d = usb_get_intfdata(i);

 if (!d)
  return;
 usb_set_intfdata(i, NULL);
 WRITE_ONCE(d->disconnected, true);
 gchd_set_lifecycle(d, GCHD_OFFLINE);
 cancel_delayed_work_sync(&d->detect_work);
 cancel_delayed_work_sync(&d->mode_work);
 mutex_lock(&d->lifecycle_lock);
 WRITE_ONCE(d->streaming, false);
 WRITE_ONCE(d->capture_active, false);
 if (d->rx_thread) {
  kthread_stop(d->rx_thread);
  d->rx_thread = NULL;
 }
 mutex_unlock(&d->lifecycle_lock);

 /* The interface may already be gone: never issue USB commands here. */
 gchd_v4l2_unregister(d);
 gchd_ring_free(&d->ring); WRITE_ONCE(d->wait_sps, true); d->ts_out_len = 0; WRITE_ONCE(d->wait_pat, true); d->ts_base_valid = false;
 kref_put(&d->refcount, gchd_release_ref);
}

static void gchd_shutdown(struct device *dev)
{
 struct usb_interface *i = to_usb_interface(dev);
 struct gchd *d = usb_get_intfdata(i);
 int r;

 if (!d || d->disconnected || !d->hw_initialized)
  return;

 cancel_delayed_work_sync(&d->detect_work);
 cancel_delayed_work_sync(&d->mode_work);
 mutex_lock(&d->lifecycle_lock);
 WRITE_ONCE(d->streaming, false);
 WRITE_ONCE(d->capture_active, false);
 gchd_capture_stop(d);

 /*
  * Use the same ordered teardown as last-close rather than issuing RESET
  * from an arbitrary active state. System shutdown must continue even if
  * USB communication has already failed.
  */
 r = gchd_hw_shutdown(d);
 if (r) {
  dev_warn(dev, "best-effort hardware shutdown during system shutdown failed: %d\n",
           r);
 } else {
  gchd_set_lifecycle(d, GCHD_POWER_OFF);
  d->hw_initialized = false;
 }
 mutex_unlock(&d->lifecycle_lock);
}

static struct usb_driver gchd_usb = {
 .name = "elgato_gchd",
 .id_table = gchd_ids,
 .probe = gchd_probe,
 .disconnect = gchd_disconnect,
 .driver = {
  .shutdown = gchd_shutdown,
 },
};
module_usb_driver(gchd_usb);
