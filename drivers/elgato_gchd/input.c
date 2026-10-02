#include <linux/module.h>
#include "elgato_gchd.h"

struct gchd_mail_cmd {
 u8 port;
 u8 len;
 u8 data[8];
};

static int gchd_seq(struct gchd *d, const struct gchd_mail_cmd *s, size_t n)
{
 size_t i;
 int r;
 for (i = 0; i < n; ++i) {
  r = gchd_mail_write(d, s[i].port, s[i].data, s[i].len);
  if (r)
   return r;
 }
 return 0;
}

static int gchd_setup_subblock(struct gchd *d)
{
 static const struct gchd_mail_cmd common[] = {
  {0x33,3,{0x99,0x89,0x8b}},
 };
 static const struct gchd_mail_cmd hdmi[] = {
  {0x4c,2,{0x70,0xc8}},
  {0x4c,2,{0x90,0x88}}, {0x4c,2,{0x91,0x77}},
  {0x4c,2,{0x92,0x77}}, {0x4c,2,{0x93,0x77}},
  {0x4c,2,{0x94,0x77}}, {0x4c,2,{0x95,0x77}},
  {0x4c,2,{0x96,0x77}}, {0x4c,2,{0x97,0x77}},
  {0x4c,2,{0x98,0x77}}, {0x4c,2,{0x99,0x77}},
  {0x4c,2,{0x9a,0x77}}, {0x4c,2,{0x9b,0x77}},
  {0x4c,2,{0x9c,0x77}}, {0x4c,2,{0x9d,0x77}},
  {0x4c,2,{0x9e,0x77}}, {0x4c,2,{0x9f,0x77}},
  {0x4c,2,{0xa0,0x77}}, {0x4c,2,{0xa1,0x77}},
  {0x4c,2,{0xa2,0x77}}, {0x4c,2,{0xa3,0x77}},
 };
 static const struct gchd_mail_cmd analog[] = {
  {0x4c,2,{0x70,0xc0}}, {0x4c,2,{0x0f,0x88}},
  {0x4c,2,{0x90,0xfe}}, {0x4c,2,{0x91,0xbb}},
  {0x4c,2,{0x95,0xe4}}, {0x4c,2,{0x96,0x1c}},
  {0x4c,2,{0x97,0x88}}, {0x4c,2,{0xa0,0x8d}},
  {0x4c,2,{0xa1,0x28}}, {0x4c,2,{0xa2,0x77}},
  {0x4c,2,{0xa3,0x77}},
 };
 int r = gchd_seq(d, common, ARRAY_SIZE(common));
 if (r) return r;
 if (d->input == 0)
  return gchd_seq(d, hdmi, ARRAY_SIZE(hdmi));
 return gchd_seq(d, analog, ARRAY_SIZE(analog));
}

static int gchd_color_yuv(struct gchd *d)
{
 static const struct gchd_mail_cmd seq[] = {
  {0x4e,2,{0x92,0xaa}}, {0x4e,2,{0x93,0xdc}},
  {0x4e,2,{0x94,0xcc}}, {0x4e,2,{0x95,0xcc}},
  {0x4e,2,{0x96,0xcc}}, {0x4e,2,{0x97,0xcc}},
  {0x4e,2,{0x98,0xcc}}, {0x4e,2,{0x99,0xcc}},
  {0x4e,2,{0x9a,0xcc}}, {0x4e,2,{0x9b,0xdc}},
  {0x4e,2,{0x9c,0xcc}}, {0x4e,2,{0x9d,0xcc}},
  {0x4e,2,{0x9e,0xcc}}, {0x4e,2,{0x9f,0xcc}},
  {0x4e,2,{0xa0,0xcc}}, {0x4e,2,{0xa1,0xcc}},
  {0x4e,2,{0xa2,0xcc}}, {0x4e,2,{0xa3,0xdc}},
  {0x4e,2,{0xa4,0xcc}}, {0x4e,2,{0xa5,0xec}},
  {0x4e,2,{0xa6,0xcc}}, {0x4e,2,{0xa7,0xc8}},
  {0x4e,2,{0xa8,0xcd}}, {0x4e,2,{0xa9,0xec}},
  {0x4e,2,{0xaa,0xcc}},
 };
 return gchd_seq(d, seq, ARRAY_SIZE(seq));
}

static int gchd_mode_regs(struct gchd *d)
{
 int r;
 u16 w = d->input_width, h = d->input_height;
 u16 profile = V4L2_MPEG_VIDEO_H264_PROFILE_HIGH;

 if (w >= 1920) {
  if ((r = gchd_sparam(d, 0x152c, 0, 16, w))) return r;
  if ((r = gchd_sparam(d, 0x152e, 0, 16, h))) return r;
 } else if (w >= 1280) {
  if ((r = gchd_sparam(d, 0x152c, 0, 16, 1280))) return r;
  if ((r = gchd_sparam(d, 0x152e, 0, 16, 720))) return r;
 } else {
  if ((r = gchd_sparam(d, 0x152c, 0, 16, 720))) return r;
  if ((r = gchd_sparam(d, 0x152e, 0, 16, 480))) return r;
 }
 if ((r = gchd_sparam(d, 0x1526, 0, 8, 41))) return r;
 if ((r = gchd_sparam(d, 0x1526, 8, 8, profile))) return r;
 return 0;
}

static int gchd_encoder_start(struct gchd *d)
{
 static const u8 magic[] = {0xab,0xa9,0x0f,0xa4,0x55};
 u8 reply[3];
 int r, i;

 r = gchd_do_enable(d, BIT(2) | BIT(1), BIT(2) | BIT(1));
 if (r) return r;
 r = gchd_scmd(d, 4, 0, 0);
 if (r) return r;
 r = gchd_load_encoder_firmware(d);
 if (r) return r;

 r = gchd_do_enable(d, BIT(3), BIT(3));
 if (r) return r;

 r = gchd_mail_write(d, 0x33, magic, sizeof(magic));
 if (r) return r;
 for (i = 0; i < 50; ++i) {
  r = gchd_mail_read(d, 0x33, reply, sizeof(reply));
  if (r) return r;
  if (reply[0] == 0x27 && reply[1] == 0xf9 && reply[2] == 0x7b)
   break;
  usleep_range(20000, 30000);
 }
 if (i == 50)
  return -ETIMEDOUT;

 return gchd_do_enable(d, BIT(4), 0);
}

int gchd_input_configure(struct gchd *d)
{
 int r;

 if (d->input_configured)
  return 0;

 /*
  * The original driver performs extensive analog front-end calibration and
  * signal probing here. These writes establish the corresponding input
  * routing and encoder dimensions without depending on a userspace helper.
  * Signal-specific calibration remains isolated so it can be extended
  * without changing the V4L2 interface.
  */
 if (d->input == 0) {
  static const struct gchd_mail_cmd seq[] = {
   {0x33,3,{0x94,0x41,0x37}},
   {0x33,3,{0x94,0x4a,0xaf}},
   {0x33,3,{0x94,0x4b,0xaf}},
   {0x4e,2,{0x00,0xcc}},
   {0x4e,2,{0xb2,0xc4}},
   {0x4e,2,{0xb5,0xd0}},
   {0x4e,2,{0x00,0xce}},
   {0x4e,2,{0x1b,0x30}},
  };
  r = gchd_seq(d, seq, ARRAY_SIZE(seq));
 } else if (d->input == 1) {
  static const struct gchd_mail_cmd seq[] = {
   {0x33,3,{0x94,0x41,0x37}},
   {0x33,3,{0x94,0x4a,0xaf}},
   {0x33,3,{0x94,0x4b,0xaf}},
   {0x4e,2,{0x00,0xcc}},
   {0x4e,2,{0xb2,0xcc}},
   {0x4e,2,{0xb5,0xc4}},
   {0x4e,2,{0x03,0x0c}},
  };
  r = gchd_seq(d, seq, ARRAY_SIZE(seq));
 } else {
  static const struct gchd_mail_cmd seq[] = {
   {0x33,3,{0x94,0x41,0x37}},
   {0x33,3,{0x94,0x4a,0xaf}},
   {0x33,3,{0x94,0x4b,0xaf}},
   {0x33,3,{0x89,0x89,0xfa}},
   {0x44,2,{0x07,0x8a}},
   {0x44,2,{0x08,0x9b}},
   {0x44,2,{0x09,0x7a}},
  };
  r = gchd_seq(d, seq, ARRAY_SIZE(seq));
 }
 if (r)
  return r;

 r = gchd_setup_subblock(d);
 if (r) return r;
 r = gchd_color_yuv(d);
 if (r) return r;
 r = gchd_mode_regs(d);
 if (r) return r;

 r = gchd_encoder_start(d);
 if (r)
  return r;

 d->input_configured = true;
 dev_info(&d->intf->dev, "input %u configured: %ux%u @ %u/%u\n",
          d->input, d->input_width, d->input_height,
          d->input_fps_num, d->input_fps_den);
 return 0;
}

void gchd_input_stop(struct gchd *d)
{
 if (!d->input_configured)
  return;
 gchd_do_enable(d, BIT(4) | BIT(3), 0);
 d->input_configured = false;
}

MODULE_DESCRIPTION("Elgato Game Capture HD input configuration");
MODULE_LICENSE("GPL");
