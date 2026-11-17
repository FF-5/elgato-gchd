#include <linux/module.h>
#include <linux/delay.h>
#include "elgato_gchd.h"
#include "gchd_mailbox.h"

/*
 * Signal-measurement polling policy. A source with recognizable timing gets
 * the longer window; a source without recognizable timing gets the short
 * window. These limits are independent of the hardware setup sequence.
 */
static unsigned int signal_active_retries = 3;
static unsigned int signal_inactive_retries = 1;
static unsigned int signal_poll_delay_ms = 200;
module_param(signal_active_retries, uint, 0644);
MODULE_PARM_DESC(signal_active_retries, "Signal measurement retries when timing is detected");
module_param(signal_inactive_retries, uint, 0644);
MODULE_PARM_DESC(signal_inactive_retries, "Signal measurement retries when timing is not detected");
module_param(signal_poll_delay_ms, uint, 0644);
MODULE_PARM_DESC(signal_poll_delay_ms, "Delay in milliseconds between signal measurement batches");

static bool gchd_hdmi_timing_valid(u32 value)
{
 return (value >= 0xb6cd && value <= 0xb6e1) ||
        (value >= 0xb077 && value <= 0xb08b) ||
        (value >= 0xb052 && value <= 0xb066) ||
        (value >= 0xb0b5 && value <= 0xb0cd);
}

static bool gchd_component_timing_valid(u32 value)
{
 return abs((int)value - 0xbbf4) < 10 ||
        abs((int)value - 0xa03d) < 10 ||
        abs((int)value - 0xbf59) < 10 ||
        abs((int)value - 0xa6b7) < 10 ||
        abs((int)value - 0x9ab9) < 10 ||
        abs((int)value - 0xa150) < 10 ||
        abs((int)value - 0x9576) < 10;
}


static int gchd_mail_seq(struct gchd *d, const struct gchd_mail_cmd *s, size_t n)
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
  GCHD_SEQ_BRIDGE3(0x99, 0x89, 0x8b),
 };
 static const struct gchd_mail_cmd hdmi[] = {
  GCHD_SEQ_BE(0x70, 0xc8),
  GCHD_SEQ_BE(0x90, 0x88), GCHD_SEQ_BE(0x91, 0x77),
  GCHD_SEQ_BE(0x92, 0x77), GCHD_SEQ_BE(0x93, 0x77),
  GCHD_SEQ_BE(0x94, 0x77), GCHD_SEQ_BE(0x95, 0x77),
  GCHD_SEQ_BE(0x96, 0x77), GCHD_SEQ_BE(0x97, 0x77),
  GCHD_SEQ_BE(0x98, 0x77), GCHD_SEQ_BE(0x99, 0x77),
  GCHD_SEQ_BE(0x9a, 0x77), GCHD_SEQ_BE(0x9b, 0x77),
  GCHD_SEQ_BE(0x9c, 0x77), GCHD_SEQ_BE(0x9d, 0x77),
  GCHD_SEQ_BE(0x9e, 0x77), GCHD_SEQ_BE(0x9f, 0x77),
  GCHD_SEQ_BE(0xa0, 0x77), GCHD_SEQ_BE(0xa1, 0x77),
  GCHD_SEQ_BE(0xa2, 0x77), GCHD_SEQ_BE(0xa3, 0x77),
 };
 static const struct gchd_mail_cmd hd1080i[] = {
  GCHD_SEQ_BE(0x70, 0xc0), GCHD_SEQ_BE(0x0f, 0x88),
  GCHD_SEQ_BE(0x90, 0xae), GCHD_SEQ_BE(0x91, 0xc2),
  GCHD_SEQ_BE(0x95, 0xde), GCHD_SEQ_BE(0x96, 0x0a),
  GCHD_SEQ_BE(0x97, 0x88), GCHD_SEQ_BE(0xa0, 0x8a),
  GCHD_SEQ_BE(0xa1, 0xf8), GCHD_SEQ_BE(0xa2, 0xbc),
  GCHD_SEQ_BE(0xa3, 0x1a),
 };
 static const struct gchd_mail_cmd hd1080p[] = {
  GCHD_SEQ_BE(0x70, 0xc0), GCHD_SEQ_BE(0x0f, 0x88),
  GCHD_SEQ_BE(0x90, 0x7e), GCHD_SEQ_BE(0x91, 0x77),
  GCHD_SEQ_BE(0x95, 0xde), GCHD_SEQ_BE(0x96, 0x0a),
  GCHD_SEQ_BE(0x97, 0x88), GCHD_SEQ_BE(0xa0, 0x8c),
  GCHD_SEQ_BE(0xa1, 0x18), GCHD_SEQ_BE(0xa2, 0x77),
  GCHD_SEQ_BE(0xa3, 0x77),
 };
 static const struct gchd_mail_cmd hd720[] = {
  GCHD_SEQ_BE(0x70, 0xc0), GCHD_SEQ_BE(0x0f, 0x88),
  GCHD_SEQ_BE(0x90, 0xfe), GCHD_SEQ_BE(0x91, 0xbb),
  GCHD_SEQ_BE(0x95, 0xe4), GCHD_SEQ_BE(0x96, 0x1c),
  GCHD_SEQ_BE(0x97, 0x88), GCHD_SEQ_BE(0xa0, 0x8d),
  GCHD_SEQ_BE(0xa1, 0x28), GCHD_SEQ_BE(0xa2, 0x77),
  GCHD_SEQ_BE(0xa3, 0x77),
 };
 static const struct gchd_mail_cmd pal_i[] = {
  GCHD_SEQ_BE(0x70, 0xd0), GCHD_SEQ_BE(0x0f, 0x88),
  GCHD_SEQ_BE(0x90, 0xd8), GCHD_SEQ_BE(0x91, 0xbf),
  GCHD_SEQ_BE(0x95, 0x9d), GCHD_SEQ_BE(0x96, 0x1b),
  GCHD_SEQ_BE(0x97, 0x88), GCHD_SEQ_BE(0xa0, 0x8a),
  GCHD_SEQ_BE(0xa1, 0xd8), GCHD_SEQ_BE(0xa2, 0xb2),
  GCHD_SEQ_BE(0xa3, 0x59),
 };
 static const struct gchd_mail_cmd pal_p[] = {
  GCHD_SEQ_BE(0x70, 0xc0), GCHD_SEQ_BE(0x0f, 0x88),
  GCHD_SEQ_BE(0x90, 0x28), GCHD_SEQ_BE(0x91, 0x93),
  GCHD_SEQ_BE(0x95, 0x82), GCHD_SEQ_BE(0x96, 0xc2),
  GCHD_SEQ_BE(0x97, 0x88), GCHD_SEQ_BE(0xa0, 0x8d),
  GCHD_SEQ_BE(0xa1, 0x28), GCHD_SEQ_BE(0xa2, 0x77),
  GCHD_SEQ_BE(0xa3, 0x77),
 };
 static const struct gchd_mail_cmd ntsc_i[] = {
  GCHD_SEQ_BE(0x70, 0xd0), GCHD_SEQ_BE(0x0f, 0x88),
  GCHD_SEQ_BE(0x90, 0x68), GCHD_SEQ_BE(0x91, 0xbf),
  GCHD_SEQ_BE(0x95, 0xab), GCHD_SEQ_BE(0x96, 0x17),
  GCHD_SEQ_BE(0x97, 0x88), GCHD_SEQ_BE(0xa0, 0x8c),
  GCHD_SEQ_BE(0xa1, 0xf8), GCHD_SEQ_BE(0xa2, 0x82),
  GCHD_SEQ_BE(0xa3, 0x59),
 };
 static const struct gchd_mail_cmd ntsc_p[] = {
  GCHD_SEQ_BE(0x70, 0xc0), GCHD_SEQ_BE(0x0f, 0x88),
  GCHD_SEQ_BE(0x90, 0x78), GCHD_SEQ_BE(0x91, 0xb9),
  GCHD_SEQ_BE(0x95, 0x86), GCHD_SEQ_BE(0x96, 0xc4),
  GCHD_SEQ_BE(0x97, 0x88), GCHD_SEQ_BE(0xa0, 0x81),
  GCHD_SEQ_BE(0xa1, 0x78), GCHD_SEQ_BE(0xa2, 0x77),
  GCHD_SEQ_BE(0xa3, 0x77),
 };
 int r = gchd_mail_seq(d, common, ARRAY_SIZE(common));
 u8 status;
 if (r) return r;
 GCHD_BRIDGE_R(d, &status, 1);
 if (d->input == 0)
  return gchd_mail_seq(d, hdmi, ARRAY_SIZE(hdmi));
 if (d->input_width == 1920 && d->input_interlaced)
  return gchd_mail_seq(d, hd1080i, ARRAY_SIZE(hd1080i));
 if (d->input_width == 1920)
  return gchd_mail_seq(d, hd1080p, ARRAY_SIZE(hd1080p));
 if (d->input_width == 1280)
  return gchd_mail_seq(d, hd720, ARRAY_SIZE(hd720));
 if (d->input_height == 576 && d->input_interlaced)
  return gchd_mail_seq(d, pal_i, ARRAY_SIZE(pal_i));
 if (d->input_height == 576)
  return gchd_mail_seq(d, pal_p, ARRAY_SIZE(pal_p));
 if (d->input_interlaced)
  return gchd_mail_seq(d, ntsc_i, ARRAY_SIZE(ntsc_i));
 return gchd_mail_seq(d, ntsc_p, ARRAY_SIZE(ntsc_p));
}

static int gchd_post_encoder_prefix(struct gchd *d);
static int gchd_post_encoder_sweep(struct gchd *d);
static int gchd_post_encoder_state(struct gchd *d);
static int gchd_input_finalize(struct gchd *d);

static int gchd_encoder_start(struct gchd *d)
{
 u8 reply[3];
 int r, i;
 u32 state;

 /*
  * hw_init() has already completed the processor-state machine through the
  * second 0x27f97b.  configureDevice() enters the encoder bring-up here,
  * starting with the 0x5b polling phase.
  */
 for (i = 0; i < 5; ++i) {
  GCHD_BRIDGE_W(d, 0xab, 0xa9, 0x0f, 0xa4, 0x5b);
  GCHD_BRIDGE_R(d, reply, 3);
  r = gchd_enable_analog(d);
  if (r) return r;
  state = ((u32)reply[0] << 16) | ((u32)reply[1] << 8) | reply[2];
  if ((state & GCHD_ENC_READY_MASK) == GCHD_ENC_READY)
   break;
 }

 r = gchd_do_enable(d, GCHD_EB_ENCODER_ENABLE, GCHD_EB_ENCODER_ENABLE);
 if (r) return r;

 /*
  * Turning on the encoder can temporarily return to 0x334455.  Keep polling
  * until the processor is back at 0x27f97b, as configureDevice() does.
  */
 for (i = 0; i < 1000; ++i) {
  GCHD_BRIDGE_W(d, 0xab, 0xa9, 0x0f, 0xa4, 0x55);
  GCHD_BRIDGE_R(d, reply, 3);
  state = ((u32)reply[0] << 16) | ((u32)reply[1] << 8) | reply[2];
  if (state == GCHD_PROC_RUNNING)
   break;
 }
 if (i == 1000)
  return -ETIMEDOUT;

 GCHD_BRIDGE_W(d, 0x28, 0x28);
 GCHD_BRIDGE_W(d, 0x29, 0x89, 0x5b);
 GCHD_BRIDGE_R(d, reply, 1);

 GCHD_BRIDGE_W(d, 0xdd, 0xce, 0x3f, 0xb2);
 GCHD_BRIDGE_R(d, reply, 2);

 r = gchd_do_enable(d, GCHD_EB_ENCODER_TRIGGER, GCHD_EB_ENCODER_TRIGGER);
 if (r) return r;

 for (i = 0; i < 1000; ++i) {
  GCHD_BRIDGE_W(d, 0x43, 0x23, 0x84);
  GCHD_BRIDGE_R(d, reply, 1);
  if (reply[0] == GCHD_TRIGGER_DONE)
   break;
 }
 if (i == 1000)
  return -ETIMEDOUT;

 r = gchd_do_enable(d, GCHD_EB_ENCODER_TRIGGER, 0);
 if (r) return r;

 GCHD_DEC_R(d, 0xfb, reply[0]);

 r = gchd_post_encoder_prefix(d);
 if (r) return r;

 r = gchd_post_encoder_sweep(d);
 if (r) return r;

 r = gchd_post_encoder_state(d);
 if (r) return r;

 r = gchd_transcoder_setup(d);
 if (r) return r;
 r = gchd_transcoder_output_enable(d, true);
 if (r) return r;

 return 0;
}

static int gchd_fe_read(struct gchd *d, u8 index, u8 *value)
{
 u8 cmd[3] = {GCHD_RD_FE_0, GCHD_RD_FE_1, index};
 int r = gchd_mail_write(d, GCHD_PORT_BRIDGE, cmd, sizeof(cmd));
 if (r)
  return r;
 return gchd_mail_read(d, GCHD_PORT_BRIDGE, value, 1);
}

static int gchd_post_encoder_prefix(struct gchd *d)
{
 u8 v;


 /* Exact continuation of configureDevice() after 0x89 0x89 0xfb. */
 GCHD_DEC_W(d, 0x02, 0xc9);
 GCHD_DEC_W(d, 0x14, 0xd2);
 GCHD_DEC_W(d, 0x3c, 0x6b);
 GCHD_DEC_R(d, GCHD_DEC_REG_FAMILY, v);

 GCHD_DEC_R(d, GCHD_DEC_REG_LOCK, v);
 GCHD_DEC_R(d, 0xe7, v);
 GCHD_DEC_W(d, 0x03, 0x2a);
 GCHD_DEC_W(d, 0x05, 0x89);

 if (d->input == 0) {
  GCHD_DEC_W(d, 0x08, 0x9b);
  GCHD_DEC_W(d, 0x09, 0x7a);
 } else {
  GCHD_DEC_W(d, 0x08, 0x91);
  GCHD_DEC_W(d, 0x09, 0xa8);
 }
 GCHD_DEC_W(d, 0x19, 0xde);
 GCHD_DEC_W(d, 0x1a, 0x87);
 GCHD_DEC_W(d, 0x1b, 0x88);
 GCHD_DEC_W(d, 0x29, 0x8b);
 GCHD_DEC_W(d, 0x2d, 0x8f);
 GCHD_DEC_W(d, 0x4c, 0x89);
 GCHD_DEC_W(d, 0x55, 0x88);
 GCHD_DEC_W(d, 0x6b, 0xae);
 GCHD_DEC_W(d, 0x6c, 0xbe);
 GCHD_DEC_W(d, 0x6d, 0x78);
 GCHD_DEC_W(d, 0x6e, 0xa0);
 GCHD_DEC_W(d, 0x06, 0x08);

 GCHD_DEC_W(d, 0x02, 0xc9);
 GCHD_DEC_W(d, 0x14, 0xd2);
 GCHD_DEC_W(d, 0x3c, 0x6b);
 GCHD_DEC_R(d, GCHD_DEC_REG_FAMILY, v);

 GCHD_DEC_W(d, 0x28, 0x88);
 GCHD_DEC_W(d, 0x10, 0x88);
 GCHD_DEC_W(d, 0x11, 0xd4);
 GCHD_DEC_W(d, 0x12, 0xd0);
 GCHD_DEC_W(d, 0x13, 0x08);
 GCHD_DEC_W(d, 0x14, 0x08);
 GCHD_DEC_W(d, 0x15, 0x88);

 GCHD_AUX_W(d, 0x47, 0xf9);
 GCHD_AUX_W(d, 0x40, 0xf3);
 GCHD_AUX_W(d, 0x43, 0xb7);
 GCHD_AUX_W(d, 0x4e, 0xb7);
 GCHD_AUX_W(d, 0x4f, 0xb7);
 GCHD_AUX_W(d, 0x48, 0xb7);
 GCHD_AUX_W(d, 0x49, 0xb7);
 GCHD_AUX_W(d, 0x58, 0x77);
 GCHD_AUX_W(d, 0x40, 0xf1);
 GCHD_AUX_W(d, 0x4d, 0xf5);
 GCHD_AUX_W(d, 0x4a, 0xaf);
 GCHD_AUX_W(d, 0x4b, 0xaf);
 GCHD_AUX_W(d, 0x5c, 0xb7);
 GCHD_AUX_W(d, 0x46, 0xd7);

 GCHD_FE_R(d, 0x88, v);
 GCHD_FE_W(d, 0xb7, 0xce);
 GCHD_FE_W(d, 0x41, 0xa3);
 GCHD_FE_W(d, 0xb8, 0xcc);
 GCHD_FE_R(d, 0x3f, v);
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcd);
 GCHD_FE_W(d, 0x0f, 0xce);
 GCHD_FE_W(d, 0x16, 0xfc);
 GCHD_FE_W(d, 0x17, 0xcc);
 GCHD_FE_W(d, 0x18, 0xcc);
 GCHD_FE_W(d, 0x19, 0xcc);
 GCHD_FE_W(d, 0x1a, 0x9c);
 GCHD_FE_R(d, 0x15, v);
 GCHD_FE_W(d, 0x2a, 0xcb);
 GCHD_FE_R(d, 0x3f, v);
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xce);
 GCHD_FE_W(d, 0x08, 0xcf);
 GCHD_FE_R(d, 0x3f, v);
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcd);
 GCHD_FE_W(d, 0x24, d->family == GCHD_FAMILY_HDNEW ? 0x8c : 0x8d);
 GCHD_FE_W(d, 0x25, 0xcc);
 GCHD_FE_W(d, 0x30, 0x4c);
 GCHD_FE_W(d, 0x31, 0xcc);
 GCHD_FE_W(d, 0x32, 0xcc);
 GCHD_FE_W(d, 0x25, 0xcc);
 GCHD_FE_W(d, 0x26, 0xcc);
 GCHD_FE_W(d, 0x27, 0xcc);
 GCHD_FE_W(d, 0x27, 0xcc);
 GCHD_FE_W(d, 0x27, 0xcc);
 GCHD_FE_W(d, 0x27, 0xcc);
 GCHD_FE_W(d, 0x27, 0xcc);
 GCHD_FE_R(d, 0x3f, v);
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc);
 GCHD_FE_W(d, 0xb0, 0xe8);
 GCHD_FE_R(d, 0x91, v);
 GCHD_FE_W(d, 0xae, 0xc8);
 GCHD_FE_W(d, 0xb1, 0x0c);
 GCHD_FE_W(d, 0xb2, 0xcc);
 GCHD_FE_W(d, 0xb3, 0xcc);
 GCHD_FE_W(d, 0xb4, 0x99);
 GCHD_FE_R(d, 0x8b, v);
 GCHD_FE_W(d, 0xb4, 0x98);
 GCHD_FE_R(d, 0x3f, v);
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xce);
 GCHD_FE_W(d, 0x01, 0xad);
 GCHD_FE_W(d, 0x02, 0x39);
 GCHD_FE_R(d, 0x3c, v);
 GCHD_FE_W(d, 0x03, 0xce);
 GCHD_FE_W(d, 0x04, 0xcd);
 GCHD_FE_W(d, 0x05, 0xcc);
 GCHD_FE_W(d, 0x06, 0xc4);
 GCHD_FE_W(d, 0x1c, 0xd6);
 GCHD_FE_W(d, 0x1d, 0xcc);
 GCHD_FE_W(d, 0x1e, 0xcc);
 GCHD_FE_W(d, 0x1f, 0xcc);
 GCHD_FE_R(d, 0x1a, v);
 GCHD_FE_W(d, 0x25, 0x6e);
 GCHD_FE_R(d, 0x3d, v);
 GCHD_FE_W(d, 0x02, 0x39);
 GCHD_FE_R(d, 0x38, v);
 GCHD_FE_W(d, 0x07, 0xc8);
 GCHD_FE_W(d, 0x17, 0x0c);
 GCHD_FE_W(d, 0x19, 0x33);
 GCHD_FE_W(d, 0x1a, 0x33);
 GCHD_FE_W(d, 0x1b, 0x30);
 GCHD_FE_W(d, 0x20, 0xcc);
 GCHD_FE_R(d, 0x1e, v);
 GCHD_FE_W(d, 0x21, 0xcc);
 GCHD_FE_W(d, 0x22, 0xea);
 GCHD_FE_W(d, 0x27, 0xcc);
 GCHD_FE_R(d, 0x11, v);
 GCHD_FE_W(d, 0x2e, 0x6d);


 /* Four back-end reads, then the fixed 0x4c tail and the HD-New-only
  * 0x20 block. */
 GCHD_BE_R(d, 0xfa, v);
 GCHD_BE_R(d, 0xf9, v);
 GCHD_BE_R(d, 0xf8, v);
 GCHD_BE_R(d, 0xfe, v);

 GCHD_BE_W(d, 0x05, 0x88);
 GCHD_BE_W(d, 0x04, 0xb5);
 GCHD_BE_W(d, 0x04, 0x95);
 GCHD_BE_W(d, 0x61, 0xb8);
 GCHD_BE_W(d, 0x09, 0x3a);
 GCHD_BE_W(d, 0x0a, 0x70);
 GCHD_BE_W(d, 0x0b, 0xbf);
 GCHD_BE_W(d, 0xc9, 0x88);
 GCHD_BE_W(d, 0xca, 0x88);
 GCHD_BE_W(d, 0xcb, 0x88);
 GCHD_BE_W(d, 0xcc, 0x88);
 GCHD_BE_W(d, 0xcd, 0x88);
 GCHD_BE_W(d, 0xce, 0x88);
 GCHD_BE_W(d, 0xcf, 0x88);
 GCHD_BE_W(d, 0xd0, 0x88);

 if (d->family == GCHD_FAMILY_HDNEW) {
  GCHD_BRIDGE_W(d, 0x21, 0x01, 0x72); GCHD_BRIDGE_R(d, &v, 1);
  GCHD_BRIDGE_W(d, 0x20, 0x02, 0x63);
  GCHD_BRIDGE_W(d, 0x20, 0x03, 0x63);
  GCHD_BRIDGE_W(d, 0x20, 0x04, 0x77);
  GCHD_BRIDGE_W(d, 0x20, 0x05, 0x73);
  GCHD_BRIDGE_W(d, 0x20, 0x06, 0x73);
  GCHD_BRIDGE_W(d, 0x20, 0x07, 0x33);
  GCHD_BRIDGE_W(d, 0x20, 0x08, 0x31);
  GCHD_BRIDGE_W(d, 0x20, 0x09, 0x33);
  GCHD_BRIDGE_W(d, 0x20, 0x0a, 0x57);
  GCHD_BRIDGE_W(d, 0x20, 0x0b, 0x7b);
  GCHD_BRIDGE_W(d, 0x20, 0x0c, 0xf7);
  GCHD_BRIDGE_W(d, 0x20, 0x0d, 0xf7);
  GCHD_BRIDGE_W(d, 0x20, 0x0e, 0x73);
  GCHD_BRIDGE_W(d, 0x20, 0x0f, 0x73);
 }
 GCHD_BRIDGE_W(d, 0xaa, 0x8f, 0x3b);


 return 0;
}

static int gchd_post_encoder_sweep(struct gchd *d)
{
 static const u8 addr[][2] = {
  {0x03,0x76},{0x3b,0x76},{0x73,0x76},{0xab,0x76},{0xe3,0x76},
  {0x1b,0x77},{0x53,0x77},{0x8b,0x77},{0xc3,0x77},{0xfb,0x77},
  {0x33,0x74},{0x6b,0x74},{0xa3,0x74},{0xdb,0x74},{0x13,0x75},
  {0x4b,0x75},{0x83,0x75},{0xbb,0x75},{0xf3,0x75},{0x2b,0x72},
  {0x63,0x72},{0x9b,0x72},{0xd3,0x72},{0x0b,0x73},{0x43,0x73},
  {0x7b,0x73},{0xb3,0x73},{0xeb,0x73},{0x23,0x70},{0x5b,0x70},
  {0x93,0x70},{0xcb,0x70},{0x03,0x71},{0x3b,0x71},{0x73,0x71},
  {0xab,0x71},{0xe3,0x71},{0x1b,0x7e},{0x53,0x7e},{0x8b,0x7e},
  {0xc3,0x7e},{0xfb,0x7e},{0x33,0x7f},{0x6b,0x7f},{0xa3,0x7f},
  {0xdb,0x7f},{0x13,0x7c},{0x4b,0x7c},{0x83,0x7c},{0xbb,0x7c},
  {0xf3,0x7c},{0x2b,0x7d},{0x63,0x7d},{0x9b,0x7d},{0xd3,0x7d},
  {0x0b,0x7a},{0x43,0x7a},{0x7b,0x7a},{0xb3,0x7a},{0xeb,0x7a},
  {0x23,0x7b},{0x5b,0x7b},{0x93,0x7b},{0xcb,0x7b},{0x03,0x78},
  {0x3b,0x78},{0x73,0x78},{0xab,0x78},{0xe3,0x78},{0x1b,0x79},
  {0x53,0x79},{0x8b,0x79},{0xc3,0x79},{0xfb,0x79},
 };
 u8 cmd[6] = {0xab,0x92,0x3e,0xb4,0,0};
 u8 reply[56];
 int r, i;

 for (i = 0; i < ARRAY_SIZE(addr); ++i) {
  cmd[4] = addr[i][0];
  cmd[5] = addr[i][1];
  r = gchd_mail_write(d, GCHD_PORT_BRIDGE, cmd, sizeof(cmd));
  if (r)
   return r;
  r = gchd_mail_read(d, GCHD_PORT_BRIDGE, reply, sizeof(reply));
  if (r)
   return r;
 }

 cmd[0] = 0xab;
 cmd[1] = 0xa2;
 cmd[2] = 0x3e;
 cmd[3] = 0xb4;
 cmd[4] = 0xfb;
 cmd[5] = 0x79;
 r = gchd_mail_write(d, GCHD_PORT_BRIDGE, cmd, sizeof(cmd));
 if (r)
  return r;

 GCHD_BRIDGE_R(d, reply, 8);

 return gchd_mail_write(d, GCHD_PORT_BRIDGE, (u8[]){0xaa,0x8d,0x35}, 3);
}

static int gchd_post_encoder_state(struct gchd *d)
{
 u8 reply[3];
 u8 magic[5] = {GCHD_BRIDGE_PROBE_0,GCHD_BRIDGE_PROBE_1,GCHD_BRIDGE_PROBE_2,GCHD_BRIDGE_PROBE_3,GCHD_BRIDGE_PROBE_ENCODER};
 int r, i;
 for (i = 0; i < 5; ++i) {
  r = gchd_mail_write(d, GCHD_PORT_BRIDGE, magic, sizeof(magic));
  if (r)
   return r;
  r = gchd_mail_read(d, GCHD_PORT_BRIDGE, reply, sizeof(reply));
  if (r)
   return r;
  r = gchd_do_enable(d, GCHD_EB_ANALOG_INPUT, GCHD_EB_ANALOG_INPUT);
  if (r)
   return r;
  /* byte-wise form of (reply & GCHD_ENC_READY_MASK) == GCHD_ENC_READY */
  if ((reply[0] & 0xf8) == 0x78 &&
      (reply[1] & 0xf0) == 0xe0 &&
      (reply[2] & 0xf0) == 0x40)
   break;
 }
 if (i == 5)
  return -ETIMEDOUT;
 return 0;
}

static int gchd_input_finalize(struct gchd *d)
{
 u8 reply[3];
 int r;

 /*
  * Final source-routing stage. The connector-specific setup above
  * establishes the input path; these writes complete the routing.
  */
 r = gchd_scmd(d, GCHD_SCMD_INIT, GCHD_SCMD_INIT_NO_RELOAD, 0);
 if (r)
  return r;

 if (d->input < 2) {
  GCHD_DEC_W(d, 0x06, 0x86);
 } else {
  GCHD_DEC_R(d, 0xfd, reply[0]);
 }

 /* Exact source-routing enable order from configureDevice(). */
 /*
  * Analog routing is already enabled by general input setup, before source
  * detection. HDMI and Component retain this configure-stage routing.
  */
 if (d->input < 2) {
  r = gchd_do_enable(d, GCHD_EB_COMPOSITE_MUX, 0);
  if (r)
   return r;
  r = gchd_do_enable(d, GCHD_EB_ANALOG_INPUT, d->input != 0 ? GCHD_EB_ANALOG_INPUT : 0);
  if (r)
   return r;
  r = gchd_do_enable(d, GCHD_EB_ANALOG_MUX, d->input != 0 ? GCHD_EB_ANALOG_MUX : 0);
  if (r)
   return r;
 }

 GCHD_DEC_R(d, 0xf8, reply[0]);

 if (d->input < 2) {
  GCHD_DEC_W(d, 0x03, 0x2f);
 }

 r = gchd_fe_read(d, 0x3f, reply);
 if (r)
  return r;

 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc);
 if (d->input < 2) {
  GCHD_FE_W(d, 0xb3, 0xcc);
 }
 r = gchd_fe_read(d, 0x3f, reply);
 if (r)
  return r;

 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xce);
 if (d->input < 2) {
  GCHD_FE_W(d, 0x27, 0xcc);
 }

 if (d->input >= 2) {
  r = gchd_fe_read(d, 0x3f, reply);
  if (r)
   return r;
  GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc);
  r = gchd_fe_read(d, 0x6e, reply);
  if (r)
   return r;
 }

 return 0;
}

static int gchd_hdmi_read_signal(struct gchd *d, u32 *sum6463, u32 *count6463,
                                  u32 *sum6665, u32 *count6665, bool *rgb)
{
 u8 v;
 u16 a, b;
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc);
 GCHD_FE_R(d, 0x66, v); b = (u16)v << 8;
 GCHD_FE_R(d, 0x65, v); b |= v;
 *sum6665 += b; (*count6665)++;
 GCHD_FE_R(d, 0x64, v); a = (u16)v << 8;
 GCHD_FE_R(d, 0x63, v); a |= v;
 *sum6463 += a; (*count6463)++;
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xce);
 GCHD_FE_R(d, 0x34, v);
 *rgb = !!(v & BIT(2));
 return 0;
}

/*
 * One measurement batch for retries after connector preparation. This path
 * never repeats the long encoder-start sequence or timing-dependent setup.
 */
static int gchd_prepare_analog_detection(struct gchd *d);

/*
 * General input setup runs after INPUT_SELECTION and before SOURCE_DETECTION.
 * Analog connectors need their decoder/front-end route established before the
 * FA/F5 idle polling loop. HDMI and Component keep their existing preparation
 * inside their connector-specific detection/configuration paths.
 */
int gchd_prepare_input_detection(struct gchd *d)
{
 if (d->input_detect_prepared)
  return 0;
 if (d->input < 2) {
  d->input_detect_prepared = true;
  return 0;
 }
 return gchd_prepare_analog_detection(d);
}

/*
 * Put the selected analog decoder on its documented route before polling it.
 * Detection runs before encoder configuration by design, so it cannot rely
 * on gchd_input_finalize() having run yet.
 */
static int gchd_prepare_analog_detection(struct gchd *d)
{
 int r;
 u8 reply;

 /*
  * Match the common NTSC/PAL analog selection order. These status reads are
  * present in the captured NTSC setup and do not choose the video family.
  */
 GCHD_DEC_R(d, 0xfd, reply);

 /*
  * Do not write the composite discriminator (0x02=C9) during the shared
  * HDMI/component -> composite detection prefix: the sequence goes from the
  * initial FD poll directly to 0x06=08 and the F8 read.
  * The discriminator is applied in late configuration after detection.
  *
  * S-video needs its distinct decoder mode before detection. When switching
  * from S-video back to composite, C9 + D2 are written as an
  * analog-to-analog transition prefix.
  */
 if (d->input == 3) {
  GCHD_DEC_W(d, 0x02, 0xdc);
  GCHD_DEC_W(d, 0x14, 0xd2);
  GCHD_DEC_W(d, 0x28, 0x88);
 } else if (d->input == 2 && d->previous_input == 3) {
  GCHD_DEC_W(d, 0x02, 0xc9);
  GCHD_DEC_W(d, 0x14, 0xd2);
 }
 GCHD_DEC_W(d, 0x06, 0x08);

 GCHD_DEC_R(d, 0xf8, reply);

 GCHD_DEC_W(d, 0x03, 0x28);
 GCHD_FE_W(d, 0xb3, 0x33);
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xce);
 GCHD_FE_W(d, 0x27, 0x33);
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc);

 if (d->previous_input == 0)
  r = gchd_mail_write(d, GCHD_PORT_FRONTEND,
                      (u8[]){0x51, 0x44}, 2);
 else
  GCHD_FE_W(d, GCHD_FE_REG_INPUT_SEL, GCHD_FE_SEL_ANALOG_ALT);

 /*
  * Apply connector routing before looking for a source. The three masked
  * writes preserve unrelated enable bits and produce the
  * 02 0E -> 03 0E -> 03 0E routing progression for analog inputs.
  */
 r = gchd_do_enable(d, GCHD_EB_COMPOSITE_MUX, GCHD_EB_COMPOSITE_MUX);
 if (r) return r;
 r = gchd_do_enable(d, GCHD_EB_ANALOG_INPUT, GCHD_EB_ANALOG_INPUT);
 if (r) return r;
 r = gchd_do_enable(d, GCHD_EB_ANALOG_MUX, GCHD_EB_ANALOG_MUX);
 if (r) return r;

 /*
  * Pre-tuning idle sequence: two FA/F5 cycles. FA replies (for example 1F,
  * then 3E) are ordinary poll results,
  * not the later CA source-found marker. Always consume both cycles here;
  * SOURCE_DETECTION owns the subsequent CA/E7/F1 polling and retries.
  */
 {
  int cycle;
  for (cycle = 0; cycle < 2; ++cycle) {
   GCHD_DEC_R(d, GCHD_DEC_REG_FAMILY, reply);
   GCHD_BE_R(d, 0xf5, reply);
   if (cycle == 0)
    usleep_range(50000, 70000);
  }
 }

 /*
  * 240p/288p detection is order-dependent: the family nibble only becomes
  * recognizable after the decoder tuning passes and 0x94 prime. Run these
  * before SOURCE_DETECTION instead of waiting for a reply that cannot arrive.
  */
 {
  static const struct gchd_mail_cmd tune[] = {
   GCHD_SEQ_DEC(0x10, 0x6f),
   GCHD_SEQ_DEC(0x11, 0xd4),
   GCHD_SEQ_DEC(0x12, 0xd0),
   GCHD_SEQ_DEC(0x13, 0x08),
   GCHD_SEQ_DEC(0x14, 0x08),
   GCHD_SEQ_DEC(0x15, 0x88),
   GCHD_SEQ_DEC(0x10, 0x6f),
   GCHD_SEQ_DEC(0x11, 0xea),
   GCHD_SEQ_DEC(0x12, 0xd0),
   GCHD_SEQ_DEC(0x13, 0x08),
   GCHD_SEQ_DEC(0x14, 0x08),
   GCHD_SEQ_DEC(0x15, 0x88),
   GCHD_SEQ_DEC(0x10, 0x6f),
   GCHD_SEQ_DEC(0x11, 0xea),
   GCHD_SEQ_DEC(0x12, 0xd0),
   GCHD_SEQ_DEC(0x13, 0x1b),
   GCHD_SEQ_DEC(0x14, 0x1b),
   GCHD_SEQ_DEC(0x15, 0x88),
  };
  r = gchd_mail_seq(d, tune, ARRAY_SIZE(tune));
  if (r) return r;
 }
 GCHD_AUX_W(d, 0x41, 0x37);
 GCHD_AUX_W(d, 0x4a, 0xb7);
 GCHD_AUX_W(d, 0x4b, 0xb7);

 d->input_detect_prepared = true;
 return 0;
}

int gchd_probe_video_source(struct gchd *d)
{
 u32 sum6665 = 0, sum6463 = 0, count6665 = 0, count6463 = 0;
 u32 value6665, value6463;
 int r;

 WRITE_ONCE(d->signal_present, false);
 if (d->input == 0) {
  r = gchd_hdmi_read_signal(d, &sum6463, &count6463,
                            &sum6665, &count6665, &d->rgb_input);
  if (r) return r;
  value6665 = sum6665 / count6665;
  value6463 = sum6463 / count6463;
  dev_info(&d->intf->dev,
           "gchd-diag detect HDMI: samples6665=%u avg6665=0x%04x samples6463=%u avg6463=0x%04x rgb=%u\n",
           count6665, value6665, count6463, value6463, d->rgb_input);
  if (!gchd_hdmi_timing_valid(value6463) ||
      abs((int)value6665 - 0xad4d) < 10)
   return -ENOLINK;
  if (value6463 >= 0xb6cd && value6463 <= 0xb6e1) {
   d->input_width=1920; d->input_height=1080; d->input_interlaced=false;
  } else if (value6463 >= 0xb077 && value6463 <= 0xb08b) {
   d->input_width=1920; d->input_height=1080; d->input_interlaced=true;
  } else if (value6463 >= 0xb052 && value6463 <= 0xb066) {
   d->input_width=1280; d->input_height=720; d->input_interlaced=false;
  } else {
   d->input_width=720;
   d->input_height=(value6665 >= 0xbb6b && value6665 <= 0xbb7f) ? 576 : 480;
   d->input_interlaced=false;
  }
  d->input_fps_num=d->input_height==576 ? 50 : 60;
  d->input_fps_den=1;
 } else if (d->input == 1) {
  u8 hi, lo;
  u32 value6867;
  GCHD_FE_R(d, 0x66, hi);
  sum6665=(u32)hi<<8;
  GCHD_FE_R(d, 0x65, lo);
  sum6665|=lo;
  GCHD_FE_R(d, 0x68, hi);
  value6867=(u32)hi<<8;
  GCHD_FE_R(d, 0x67, lo);
  value6867|=lo;
  dev_info(&d->intf->dev,
           "gchd-diag detect component: raw6665=0x%04x raw6867=0x%04x\n",
           (u16)sum6665, (u16)value6867);
  if (!gchd_component_timing_valid(value6867) ||
      abs((int)sum6665-0xad4d)<10)
   return -ENOLINK;
  if (abs((int)value6867-0xbbf4)<10) {
   d->input_width=1920; d->input_height=1080; d->input_interlaced=false; d->input_fps_num=30;
  } else if (abs((int)value6867-0xa03d)<10) {
   d->input_width=1920; d->input_height=1080; d->input_interlaced=true; d->input_fps_num=60;
  } else if (abs((int)value6867-0xbf59)<10) {
   d->input_width=1280; d->input_height=720; d->input_interlaced=false; d->input_fps_num=60;
  } else if (abs((int)value6867-0xa6b7)<10) {
   d->input_width=720; d->input_height=576; d->input_interlaced=false; d->input_fps_num=50;
  } else if (abs((int)value6867-0x9ab9)<10) {
   d->input_width=720; d->input_height=576; d->input_interlaced=true; d->input_fps_num=50;
  } else if (abs((int)value6867-0xa150)<10) {
   d->input_width=720; d->input_height=480; d->input_interlaced=false; d->input_fps_num=60;
  } else {
   d->input_width=720; d->input_height=480; d->input_interlaced=true; d->input_fps_num=60;
  }
  d->input_fps_den=1;
 } else {
  u8 v;

  /*
   * After pre-detection tuning and the 0x94 prime, FA reports the analog
   * family in its low nibble (6=NTSC, 7=PAL). Waiting for literal CA here
   * deadlocks the 240p path; CA/E7 polling follows family detection.
   */
  GCHD_DEC_R(d, GCHD_DEC_REG_FAMILY, v);
  dev_info(&d->intf->dev,
           "gchd-diag detect %s: pre-config raw_status=0x%02x mode=0x%x\n",
           d->input == 2 ? "composite" : "s-video", v, v & 0x0f);
  v &= 0x0f;
  if (v != 6 && v != 7) {
   u8 lock;

   /* Diagnostic only: decoder lock status while no family is reported. */
   GCHD_DEC_R(d, GCHD_DEC_REG_LOCK, lock);
   dev_info_ratelimited(&d->intf->dev,
                        "gchd-diag no analog family: 0x88:FA=0x%02x 0x88:CA=0x%02x\n",
                        v, lock);
   return -ENOLINK;
  }

  /*
   * Family recognized: query decoder status/ready/height in sequence.
   */
  GCHD_DEC_R(d, GCHD_DEC_REG_LOCK, v);
  GCHD_DEC_R(d, 0xe7, v);
  GCHD_DEC_R(d, 0xf1, v);

  /* The 0x94 prime already ran before SOURCE_DETECTION. */
  GCHD_DEC_R(d, GCHD_DEC_REG_FAMILY, v);
  dev_info(&d->intf->dev,
           "gchd-diag detect %s: raw_status=0x%02x masked_mode=0x%01x\n",
           d->input == 2 ? "composite" : "s-video", v, v & 0x0f);
  v &= 0x0f;
  if (v == 6) {
   u8 mode;

   d->input_width=720; d->input_height=480;
   d->input_interlaced=true; d->input_fps_num=60;
   /* 0xCA is the decoder's authoritative 240p/480i lock indicator. */
   GCHD_DEC_R(d, GCHD_DEC_REG_LOCK, mode);
   if (mode != GCHD_DEC_LOCK_480I && mode != GCHD_DEC_LOCK_240P)
    dev_dbg(&d->intf->dev,
            "analog format status not locked yet: 0x%02x; using 480i\n",
            mode);
   /* 0x66 = 240p; anything else is configured as 480i. */
   d->input_interlaced = mode != GCHD_DEC_LOCK_240P;
   dev_info(&d->intf->dev,
            "analog format initial status=0x%02x; configuring %s\n",
            mode, d->input_interlaced ? "480i" : "240p");
  } else if (v == 7) {
   d->input_width=720; d->input_height=576;
   d->input_interlaced=true; d->input_fps_num=50;
  } else return -ENOLINK;
  d->input_fps_den=1;
 }
 WRITE_ONCE(d->signal_present,true);
 dev_info(&d->intf->dev,
          "gchd-diag detected input=%u mode=%ux%u fps=%u/%u interlaced=%u signal_present=%u\n",
          d->input, d->input_width, d->input_height, d->input_fps_num,
          d->input_fps_den, d->input_interlaced, d->signal_present);
 return 0;
}

static int gchd_configure_hdmi_exact(struct gchd *d)
{
 u8 v, reply;
 u32 sum6665=0, sum6463=0, count6665=0, count6463=0;
 u32 value6665, value6463;
 int r, i, j;


 GCHD_AUX_W(d, 0x41, 0x37); GCHD_AUX_W(d, 0x4a, 0xaf); GCHD_AUX_W(d, 0x4b, 0xaf); GCHD_FE_R(d, 0x3f, v);
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc); GCHD_FE_R(d, 0x94, v); GCHD_FE_W(d, 0xab, 0x4c); GCHD_FE_R(d, 0x3f, v);
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xce); GCHD_FE_W(d, 0x1b, 0x33); GCHD_FE_R(d, 0x3f, v);
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc); GCHD_FE_R(d, 0x88, v); GCHD_FE_W(d, 0xb7, 0xce); GCHD_FE_W(d, 0xb8, 0xdc);
 GCHD_FE_W(d, 0xb8, 0xcc); GCHD_FE_R(d, 0x3f, v); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xce); GCHD_FE_W(d, 0x07, 0x38);
 GCHD_FE_W(d, 0x07, 0xc8); GCHD_FE_R(d, 0x3f, v); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc); GCHD_FE_W(d, GCHD_FE_REG_INPUT_SEL, GCHD_FE_SEL_HDMI);
 GCHD_FE_R(d, 0x88, v); GCHD_FE_W(d, 0xb7, 0xcc);
 r=gchd_do_enable(d,0,0); if(r)return r;
 GCHD_FE_R(d, 0x3f, v); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xce); GCHD_FE_R(d, 0x3e, v); GCHD_FE_W(d, 0x01, 0xad);
 GCHD_FE_R(d, 0x3b, v); GCHD_FE_W(d, 0x04, 0xcd); GCHD_FE_W(d, 0x06, 0xc4); GCHD_FE_R(d, 0x36, v);
 GCHD_FE_W(d, 0x09, 0xe4); GCHD_FE_R(d, 0x3f, v); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc); GCHD_FE_R(d, 0x6b, v);
 GCHD_FE_W(d, 0x54, 0xec); GCHD_FE_R(d, 0x93, v); GCHD_FE_W(d, 0xac, 0x4c); GCHD_FE_R(d, 0x3f, v);
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0x4c); GCHD_FE_R(d, 0x3f, v); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc); GCHD_FE_R(d, 0xf1, v);
 GCHD_FE_W(d, 0xce, 0x4c); GCHD_FE_R(d, 0xf0, v); GCHD_FE_W(d, 0xcf, 0xce); GCHD_FE_R(d, 0x3f, v);

 for (i=0; i<max_t(unsigned int, 1, signal_active_retries); i++) {
  unsigned int retry_limit;

  sum6665=sum6463=count6665=count6463=0;
  for (j=0;j<10;j++) {
   r=gchd_hdmi_read_signal(d,&sum6463,&count6463,&sum6665,&count6665,&d->rgb_input);
   if(r)return r;
  }
  value6665=sum6665/count6665;
  value6463=sum6463/count6463;
  retry_limit = gchd_hdmi_timing_valid(value6463) ?
   max_t(unsigned int, 1, signal_active_retries) :
   max_t(unsigned int, 1, signal_inactive_retries);
  if (i >= 2 && (value6665 < 0xad43 || value6665 > 0xad57))
   break;
  if (i + 1 >= retry_limit)
   break;
  if (signal_poll_delay_ms)
   msleep(signal_poll_delay_ms);
 }
 value6665=sum6665/count6665;
 value6463=sum6463/count6463;
 /*
  * The timing register can retain a plausible value with no source attached.
  * Require both a recognized timing and a non-idle 0x6665 measurement.
  * 0xad4d +/- 10 is the no-signal value.
  */
 d->signal_present = gchd_hdmi_timing_valid(value6463) &&
                     abs((int)value6665 - 0xad4d) >= 10;

 if (d->input_width == 0) {
  value6463=sum6463/count6463;
  if (value6463 >= 0xb6cd && value6463 <= 0xb6e1) { d->input_width=1920; d->input_height=1080; d->input_interlaced=false; }
  else if (value6463 >= 0xb077 && value6463 <= 0xb08b) { d->input_width=1920; d->input_height=1080; d->input_interlaced=true; }
  else if (value6463 >= 0xb052 && value6463 <= 0xb066) { d->input_width=1280; d->input_height=720; d->input_interlaced=false; }
  else if (value6463 >= 0xb0b5 && value6463 <= 0xb0cd) { d->input_width=720; d->input_height=480; d->input_interlaced=false; }
  else {
   /*
    * No recognizable timing is not a setup failure. Use the driver's
    * default HDMI mode so the receiver can be configured before a source
    * is connected; signal status remains false until timing is detected.
    */
   d->input_width=1280;
   d->input_height=720;
   d->input_interlaced=false;
   d->input_fps_num=60;
   d->input_fps_den=1;
  }
  if (d->input_height==480 && value6665 >= 0xbb6b && value6665 <= 0xbb7f)
   d->input_height=576;
 }
 if (!d->signal_present)
  return -ENOLINK;

 if (!d->input_fps_num) {
  d->input_fps_num = (d->input_height==576)?50:60; d->input_fps_den=1;
 }

 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc);
 if (d->input_width==1920) { GCHD_FE_W(d, 0xb2, 0xc4); GCHD_FE_W(d, 0xb5, 0xd0); }
 else if (d->input_width==1280) { GCHD_FE_W(d, 0xb2, 0xcc); GCHD_FE_W(d, 0xb5, 0xcc); }
 else { GCHD_FE_W(d, 0xb2, 0xcf); GCHD_FE_W(d, 0xb5, 0xcc); }
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xce); GCHD_FE_W(d, 0x1b, 0x30);
 GCHD_FE_W(d, 0x1f, 0xdc); GCHD_FE_R(d, 0x29, v); GCHD_FE_W(d, 0x1f, 0xcc); GCHD_FE_R(d, 0x3b, v);
 GCHD_FE_W(d, 0x04, 0xcf); GCHD_FE_W(d, 0x04, 0xcd); GCHD_FE_R(d, 0x3f, v); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc);
 GCHD_FE_W(d, 0x40, 0xcc); GCHD_FE_R(d, 0x3f, v); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcd);
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc); GCHD_FE_W(d, 0xb0, 0xe8); GCHD_FE_W(d, 0xb1, 0x0c); GCHD_FE_W(d, 0xad, 0xc9);
 GCHD_FE_R(d, 0x8f, v); GCHD_FE_W(d, 0xb0, 0xe9); GCHD_FE_R(d, 0x3f, v);
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc); GCHD_FE_W(d, 0xab, 0xcc);
 GCHD_BRIDGE_W(d, 0x99, 0x89, 0xf5); GCHD_BRIDGE_R(d, &reply, 1);
 GCHD_BRIDGE_W(d, 0x99, 0x89, 0xfd); GCHD_BRIDGE_R(d, &reply, 1);
 GCHD_BRIDGE_W(d, 0x99, 0x89, 0xf5); GCHD_BRIDGE_R(d, &reply, 1);
 GCHD_BRIDGE_W(d, 0x99, 0x89, 0xfc); GCHD_BRIDGE_R(d, &reply, 1);
 GCHD_BRIDGE_W(d, 0x99, 0x89, 0xf3); GCHD_BRIDGE_R(d, &reply, 1);
 GCHD_BE_W(d, 0x0c, 0x89);
 GCHD_BRIDGE_W(d, 0x99, 0x89, 0xf5); GCHD_BRIDGE_R(d, &reply, 1);
 GCHD_BE_W(d, GCHD_BE_REG_PULSE, 0x65);
 GCHD_BE_W(d, GCHD_BE_REG_PULSE, 0x64);

 r=gchd_common_block_a(d); if(r)return r;
 r=gchd_setup_subblock_exact(d); if(r)return r;
 GCHD_BRIDGE_W(d, 0x99, 0x89, 0x6b); GCHD_BRIDGE_R(d, &reply, 1);
 GCHD_BE_W(d, 0xc0, 0x80);
 GCHD_BE_W(d, 0xa2, 0x77);
 GCHD_BRIDGE_W(d, 0x99, 0x89, 0x58); GCHD_BRIDGE_R(d, &reply, 1);
 GCHD_BE_W(d, 0xc0, 0x77);

 r=gchd_common_block_b1(d, d->input_width != 1920 && !d->input_interlaced); if(r)return r;
 r=gchd_common_block_b2(d); if(r)return r;
 r=gchd_common_block_b3(d); if(r)return r;
 r=gchd_common_block_c(d); if(r)return r;
 r=gchd_hdmi_read_signal(d,&sum6463,&count6463,&sum6665,&count6665,&d->rgb_input); if(r)return r;

 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc);
 r=gchd_color_space_exact(d); if(r)return r;
 return 0;

}

int gchd_setup_subblock_exact(struct gchd *d)
{
 return gchd_setup_subblock(d);
}

int gchd_color_space_exact(struct gchd *d)
{
 static const struct gchd_mail_cmd prefix[] = {
  GCHD_SEQ_FE(0x0b, 0x4c), GCHD_SEQ_FE(0x0c, 0x4c),
  GCHD_SEQ_FE(0x0d, 0x4c), GCHD_SEQ_FE(0x08, 0x4c),
  GCHD_SEQ_FE(0x09, 0x4c), GCHD_SEQ_FE(0x0a, 0x4c),
  GCHD_SEQ_FE(0x1e, 0x98), GCHD_SEQ_FE(0x1f, 0xc9),
 };
 static const struct gchd_mail_cmd yuv[] = {
  GCHD_SEQ_FE(0x92, 0xaa),GCHD_SEQ_FE(0x93, 0xdc),
  GCHD_SEQ_FE(0x94, 0xcc),GCHD_SEQ_FE(0x95, 0xcc),
  GCHD_SEQ_FE(0x96, 0xcc),GCHD_SEQ_FE(0x97, 0xcc),
  GCHD_SEQ_FE(0x98, 0xcc),GCHD_SEQ_FE(0x99, 0xcc),
  GCHD_SEQ_FE(0x9a, 0xcc),GCHD_SEQ_FE(0x9b, 0xdc),
  GCHD_SEQ_FE(0x9c, 0xcc),GCHD_SEQ_FE(0x9d, 0xcc),
  GCHD_SEQ_FE(0x9e, 0xcc),GCHD_SEQ_FE(0x9f, 0xcc),
  GCHD_SEQ_FE(0xa0, 0xcc),GCHD_SEQ_FE(0xa1, 0xcc),
  GCHD_SEQ_FE(0xa2, 0xcc),GCHD_SEQ_FE(0xa3, 0xdc),
  GCHD_SEQ_FE(0xa4, 0xcc),GCHD_SEQ_FE(0xa5, 0xec),
  GCHD_SEQ_FE(0xa6, 0xcc),GCHD_SEQ_FE(0xa7, 0xc8),
  GCHD_SEQ_FE(0xa8, 0xcd),GCHD_SEQ_FE(0xa9, 0xec),
  GCHD_SEQ_FE(0xaa, 0xcc),
 };
 static const struct gchd_mail_cmd rgb[] __maybe_unused = {
  GCHD_SEQ_FE(0x92, 0x8c),GCHD_SEQ_FE(0x93, 0xcb),
  GCHD_SEQ_FE(0x94, 0xca),GCHD_SEQ_FE(0x95, 0xce),
  GCHD_SEQ_FE(0x96, 0x22),GCHD_SEQ_FE(0x97, 0xb2),
  GCHD_SEQ_FE(0x98, 0xae),GCHD_SEQ_FE(0x99, 0xb5),
  GCHD_SEQ_FE(0x9a, 0x52),GCHD_SEQ_FE(0x9b, 0xc5),
  GCHD_SEQ_FE(0x9c, 0x1f),GCHD_SEQ_FE(0x9d, 0xb6),
  GCHD_SEQ_FE(0x9e, 0x54),GCHD_SEQ_FE(0x9f, 0xb3),
  GCHD_SEQ_FE(0xa0, 0x90),GCHD_SEQ_FE(0xa1, 0xcc),
  GCHD_SEQ_FE(0xa2, 0x32),GCHD_SEQ_FE(0xa3, 0xcb),
  GCHD_SEQ_FE(0xa4, 0xca),GCHD_SEQ_FE(0xa5, 0xec),
  GCHD_SEQ_FE(0xa6, 0xcc),GCHD_SEQ_FE(0xa7, 0xc8),
  GCHD_SEQ_FE(0xa8, 0xcd),GCHD_SEQ_FE(0xa9, 0xec),
  GCHD_SEQ_FE(0xaa, 0xcc),
 };
 int r;
 if (d->input == 1) {
  r=gchd_mail_seq(d,prefix,ARRAY_SIZE(prefix));
  if(r)return r;
 }
 /* HDMI selects RGB/YUV from the front-end status bit (0x9C:34 bit 2).
  * Component and Composite use YUV. */
 if (d->input == 0 && d->rgb_input)
  return gchd_mail_seq(d,rgb,ARRAY_SIZE(rgb));
 return gchd_mail_seq(d,yuv,ARRAY_SIZE(yuv));
}

/*
 * Back-end retrigger: read 0x98:FD/FC/F3, then 0x98:0D = C8 and two writes
 * to 0x98:0E (value, then value with bit 0 cleared).
 */
static int gchd_be_retrigger(struct gchd *d, u8 value)
{
 u8 v;

 GCHD_BE_R(d, 0xfd, v);
 GCHD_BE_R(d, 0xfc, v);
 GCHD_BE_R(d, 0xf3, v);
 GCHD_BE_W(d, GCHD_BE_REG_RETRIGGER, GCHD_BE_RETRIGGER_VALUE);
 GCHD_BE_W(d, GCHD_BE_REG_PULSE, value);
 GCHD_BE_W(d, GCHD_BE_REG_PULSE, value & ~0x01);
 return 0;
}

static int gchd_analog_backend(struct gchd *d, bool pal, bool status_reads)
{
 /* 0x98 per-mode table: NTSC writes all of 90-A3, PAL a sparse set. */
 static const struct gchd_mail_cmd table_ntsc[] = {
  GCHD_SEQ_BE(0x70, 0xd0), GCHD_SEQ_BE(0x0f, 0x88),
  GCHD_SEQ_BE(0x90, 0x01), GCHD_SEQ_BE(0x91, 0xbd), GCHD_SEQ_BE(0x92, 0x9a),
  GCHD_SEQ_BE(0x93, 0x3a), GCHD_SEQ_BE(0x94, 0xe9), GCHD_SEQ_BE(0x95, 0xab),
  GCHD_SEQ_BE(0x96, 0x17), GCHD_SEQ_BE(0x97, 0x88), GCHD_SEQ_BE(0x98, 0x84),
  GCHD_SEQ_BE(0x99, 0x8a), GCHD_SEQ_BE(0x9a, 0x9c), GCHD_SEQ_BE(0x9b, 0x8c),
  GCHD_SEQ_BE(0x9c, 0x98), GCHD_SEQ_BE(0x9d, 0x93), GCHD_SEQ_BE(0x9e, 0x83),
  GCHD_SEQ_BE(0x9f, 0xa9), GCHD_SEQ_BE(0xa0, 0x8b), GCHD_SEQ_BE(0xa1, 0xe8),
  GCHD_SEQ_BE(0xa2, 0x81), GCHD_SEQ_BE(0xa3, 0x49),
 };
 static const struct gchd_mail_cmd table_pal[] = {
  GCHD_SEQ_BE(0x70, 0xd0), GCHD_SEQ_BE(0x0f, 0x88),
  GCHD_SEQ_BE(0x90, 0xd8), GCHD_SEQ_BE(0x91, 0xbf), GCHD_SEQ_BE(0x95, 0x9d),
  GCHD_SEQ_BE(0x96, 0x1b), GCHD_SEQ_BE(0x97, 0x88), GCHD_SEQ_BE(0xa0, 0x8a),
  GCHD_SEQ_BE(0xa1, 0xd8), GCHD_SEQ_BE(0xa2, 0xb2), GCHD_SEQ_BE(0xa3, 0x59),
 };
 u8 v;
 int r;

 if (status_reads) {
  GCHD_DEC_R(d, GCHD_DEC_REG_LOCK, v);
  GCHD_DEC_R(d, 0xe7, v);
  GCHD_DEC_R(d, 0xf1, v);
  GCHD_DEC_R(d, 0xcb, v);
  GCHD_DEC_R(d, GCHD_DEC_REG_LOCK, v);
 }

 GCHD_BE_W(d, 0x04, 0x95);
 GCHD_BE_R(d, 0x8b, v);
 r = pal ? gchd_mail_seq(d, table_pal, ARRAY_SIZE(table_pal)) :
           gchd_mail_seq(d, table_ntsc, ARRAY_SIZE(table_ntsc));
 if (r)
  return r;
 GCHD_BE_R(d, 0x89, v);
 GCHD_BE_W(d, 0x72, 0x88);
 GCHD_BE_W(d, 0xc0, 0x88);
 GCHD_BE_W(d, 0x61, 0x98);
 GCHD_BE_W(d, 0x63, 0x89);
 GCHD_BE_W(d, 0x64, 0x96);
 GCHD_BE_W(d, 0x65, 0x88);
 GCHD_BE_R(d, 0xff, v);
 GCHD_BE_W(d, 0x04, 0x8d);
 GCHD_BE_W(d, 0x62, 0x91);

 r = gchd_be_retrigger(d, 0x05);
 if (r)
  return r;
 r = gchd_be_retrigger(d, 0x15);
 if (r)
  return r;

 GCHD_DEC_R(d, GCHD_DEC_REG_FAMILY, v);
 GCHD_DEC_R(d, GCHD_DEC_REG_LOCK, v);
 GCHD_DEC_R(d, 0xe7, v);
 GCHD_DEC_R(d, 0xf1, v);
 return 0;
}

static int gchd_configure_composite_exact(struct gchd *d)
{
 u8 v;
 int r;

 /* Tuning is performed before source detection so 240p/288p can lock. */

 /* Composite and S-Video share the assumed 0x4E:0x51=0xEC route.
  * Select the decoder's distinct luma/chroma input mode separately. */
 GCHD_DEC_W(d, 0x02, d->input == 3 ? 0xdc : 0xc9);
 GCHD_DEC_W(d, 0x3c, 0x6b);

 GCHD_AUX_W(d, 0x41, 0x37); GCHD_AUX_W(d, 0x4a, 0xb7); GCHD_AUX_W(d, 0x4b, 0xb7);
 GCHD_DEC_R(d, GCHD_DEC_REG_FAMILY, v);
 v &= 0x0f;
 d->signal_present = (v == 6 || v == 7);
 if (v == 6) {
  d->input_width=720; d->input_height=480;
  /* Preserve the 240p/480i decision made by gchd_probe_video_source(). */
  d->input_fps_num=60; d->input_fps_den=1;
 }
 else if (v == 7) { d->input_width=720; d->input_height=576; d->input_interlaced=true; d->input_fps_num=50; d->input_fps_den=1; }
 else if (!d->input_height) {
  u8 x;
  gchd_mail_write(d,GCHD_PORT_BRIDGE,(u8[]){GCHD_RD_FE_0,GCHD_RD_FE_1,0x3f},3);
  gchd_mail_read(d,GCHD_PORT_BRIDGE,&x,1);
  gchd_mail_write(d,GCHD_PORT_FRONTEND,(u8[]){0x00,0xcc},2);
  gchd_mail_write(d,GCHD_PORT_BRIDGE,(u8[]){GCHD_RD_FE_0,GCHD_RD_FE_1,0x94},3);
  gchd_mail_read(d,GCHD_PORT_BRIDGE,&x,1);
  gchd_mail_write(d,GCHD_PORT_FRONTEND,(u8[]){0xab,0x4c},2);
  gchd_mail_write(d,GCHD_PORT_BRIDGE,(u8[]){GCHD_RD_FE_0,GCHD_RD_FE_1,0x3f},3);
  gchd_mail_read(d,GCHD_PORT_BRIDGE,&x,1);
  gchd_mail_write(d,GCHD_PORT_FRONTEND,(u8[]){0x00,0xce},2);
  gchd_mail_write(d,GCHD_PORT_FRONTEND,(u8[]){0x1b,0x33},2);
  gchd_mail_write(d,GCHD_PORT_BRIDGE,(u8[]){GCHD_RD_FE_0,GCHD_RD_FE_1,0x3f},3);
  gchd_mail_read(d,GCHD_PORT_BRIDGE,&x,1);
  gchd_mail_write(d,GCHD_PORT_FRONTEND,(u8[]){0x00,0xcc},2);
  gchd_mail_write(d,GCHD_PORT_BRIDGE,(u8[]){GCHD_RD_FE_0,GCHD_RD_FE_1,0x88},3);
  gchd_mail_read(d,GCHD_PORT_BRIDGE,&x,1);
  gchd_mail_write(d,GCHD_PORT_FRONTEND,(u8[]){0xb7,0xce},2);
  gchd_mail_write(d,GCHD_PORT_FRONTEND,(u8[]){0xb8,0xdc},2);
  gchd_mail_write(d,GCHD_PORT_FRONTEND,(u8[]){0xb8,0xcc},2);
  gchd_mail_write(d,GCHD_PORT_BRIDGE,(u8[]){GCHD_RD_FE_0,GCHD_RD_FE_1,0x3f},3);
  gchd_mail_read(d,GCHD_PORT_BRIDGE,&x,1);
  /*
   * Keep the receiver configured while no composite source is present.
   * The caller tracks signal_present separately and the detection worker
   * will repeat this measurement when the source may have appeared.
   */
  d->input_width = 720;
  d->input_height = 480;
  d->input_interlaced = true;
  d->input_fps_num = 60;
  d->input_fps_den = 1;
 }

 if (!d->signal_present)
  return -ENOLINK;

 if (d->input_height==480 || d->input_height==576) {
  GCHD_DEC_W(d, 0x07, 0x8a); GCHD_DEC_W(d, 0x08, 0x9b); GCHD_DEC_W(d, 0x09, 0x7a);
  GCHD_DEC_W(d, 0x28, d->input_interlaced ? 0x88 : 0x8a);
  GCHD_DEC_R(d, 0xfd, v); GCHD_DEC_W(d, 0x06, 0x08);
 }
 if (!d->input_interlaced && d->input_height == 480) {
  /* 240p: retrigger, wait for the decoder to settle, then 0x88:0A = 98. */
  int i;

  GCHD_DEC_R(d, 0xe7, v);
  r = gchd_be_retrigger(d, 0x15);
  if (r)
   return r;
  for (i = 0; i < 40; ++i) {
   GCHD_DEC_R(d, GCHD_DEC_REG_FAMILY, v);
   GCHD_DEC_R(d, GCHD_DEC_REG_LOCK, v);
   GCHD_DEC_R(d, 0xe7, v);
   if (v == 0x69)
    break;
   msleep(40);
  }
  GCHD_DEC_R(d, 0xf1, v);
  GCHD_DEC_W(d, 0x0a, 0x98);
 }
 /* Back-end programming after analog lock (composite and S-video). */
 r = gchd_analog_backend(d, d->input_height == 576, d->input_interlaced);
 if (r)
  return r;
 return 0;

}


int gchd_input_read_analog_mode(struct gchd *d, bool *interlaced)
{
 u8 status;

 if (d->input < 2 || d->input_height != 480)
  return -EOPNOTSUPP;
 GCHD_DEC_R(d, GCHD_DEC_REG_LOCK, status);

 /* FE is unlocked and EE is transitional: neither is a mode change. */
 if (status == GCHD_DEC_LOCK_240P)
  *interlaced = false;
 else if (status == GCHD_DEC_LOCK_480I)
  *interlaced = true;
 else
  return -EAGAIN;
 return 0;
}

static int gchd_configure_component_exact(struct gchd *d)
{
 u8 v;
 u32 sum6867=0,sum6665=0,count6867=0,count6665=0;
 u32 value6867,value6665;
 int r,i,j;

 GCHD_AUX_W(d, 0x41, 0x37); GCHD_AUX_W(d, 0x4a, 0xaf); GCHD_AUX_W(d, 0x4b, 0xaf);
 GCHD_FE_R(d, 0x3f, v); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc); GCHD_FE_R(d, 0x94, v); GCHD_FE_W(d, 0xab, 0x4c); GCHD_FE_R(d, 0x3f, v);
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xce); GCHD_FE_W(d, 0x1b, 0x33); GCHD_FE_R(d, 0x3f, v); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc); GCHD_FE_R(d, 0x88, v);
 GCHD_FE_W(d, 0xb7, 0xce); GCHD_FE_W(d, 0xb8, 0xdc); GCHD_FE_W(d, 0xb8, 0xcc); GCHD_FE_R(d, 0x3f, v);
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xce); GCHD_FE_W(d, 0x07, 0x38); GCHD_FE_W(d, 0x07, 0xc8); GCHD_FE_R(d, 0x3f, v);
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc);
 {
  static const struct gchd_mail_cmd select_component[] = {
   GCHD_SEQ_FE(GCHD_FE_REG_INPUT_SEL, GCHD_FE_SEL_COMPONENT),
  };
  r = gchd_mail_seq(d, select_component, ARRAY_SIZE(select_component));
  if (r) return r;
 }
 GCHD_FE_R(d, 0x88, v); GCHD_FE_W(d, 0xb7, 0xce);
 r=gchd_do_enable(d,0,0); if(r)return r;
 GCHD_FE_W(d, 0x24, 0x0c); GCHD_FE_W(d, 0x21, 0xcd); GCHD_FE_W(d, 0x91, 0xc8); GCHD_FE_W(d, 0x0e, 0x8c);
 GCHD_FE_W(d, 0x11, 0xec); GCHD_FE_W(d, 0x71, 0x4c); GCHD_FE_W(d, 0x04, 0xcc);

 for (i=0; i<max_t(unsigned int, 1, signal_active_retries); i++) {
  unsigned int retry_limit;

  sum6867=sum6665=count6867=count6665=0;
  for (j=0;j<10;j++) {
   u8 a,b;
   GCHD_FE_R(d, 0x66, v); a=v;
   GCHD_FE_R(d, 0x65, v); b=v;
   sum6665 += ((u32)a<<8)|b; count6665++;
   GCHD_FE_R(d, 0x68, v); a=v;
   GCHD_FE_R(d, 0x67, v); b=v;
   sum6867 += ((u32)a<<8)|b; count6867++;
  }
  value6665=sum6665/count6665;
  value6867=sum6867/count6867;
  retry_limit = gchd_component_timing_valid(value6867) ?
   max_t(unsigned int, 1, signal_active_retries) :
   max_t(unsigned int, 1, signal_inactive_retries);
  if(i>=2 && abs((int)value6665-0xad4d)>=10) break;
  if (i + 1 >= retry_limit) break;
  if (signal_poll_delay_ms)
   msleep(signal_poll_delay_ms);
 }

 value6867=sum6867/count6867;
 /*
  * A valid-looking timing value alone is insufficient: the receiver can
  * leave the last timing latched after the source disappears.
  */
 d->signal_present = gchd_component_timing_valid(value6867) &&
                     abs((int)value6665 - 0xad4d) >= 10;
 if(!d->input_height) {
  if(abs((int)value6867-0xbbf4)<10) {
   d->input_width=1920; d->input_height=1080; d->input_interlaced=false; d->input_fps_num=30;
  } else if(abs((int)value6867-0xa03d)<10) {
   d->input_width=1920; d->input_height=1080; d->input_interlaced=true; d->input_fps_num=60;
  } else if(abs((int)value6867-0xbf59)<10) {
   d->input_width=1280; d->input_height=720; d->input_interlaced=false; d->input_fps_num=60;
  } else if(abs((int)value6867-0xa6b7)<10) {
   d->input_width=720; d->input_height=576; d->input_interlaced=false; d->input_fps_num=50;
  } else if(abs((int)value6867-0x9ab9)<10) {
   d->input_width=720; d->input_height=576; d->input_interlaced=true; d->input_fps_num=50;
  } else if(abs((int)value6867-0xa150)<10) {
   d->input_width=720; d->input_height=480; d->input_interlaced=false; d->input_fps_num=60;
  } else if(abs((int)value6867-0x9576)<10) {
   d->input_width=720; d->input_height=480; d->input_interlaced=true; d->input_fps_num=60;
  } else {
   /* Keep the advertised fallback aligned with the driver's 720p default. */
   d->input_width=1280; d->input_height=720;
   d->input_interlaced=false; d->input_fps_num=60;
  }
  d->input_fps_den=1;
 }

 if (!d->signal_present)
  return -ENOLINK;

 if(d->input_height==480 || d->input_height==576) GCHD_FE_W(d, 0xb2, 0xcf); else GCHD_FE_W(d, 0xb2, 0xcc);
 GCHD_FE_W(d, 0xb5, 0xcc);

 if(d->input_height==1080) {
  if(d->input_interlaced) {
   GCHD_FE_W(d, 0x03, 0x94); GCHD_FE_W(d, 0x05, 0xf4); GCHD_FE_W(d, 0x06, 0xec); GCHD_FE_W(d, 0x07, 0xfc);
   GCHD_FE_W(d, 0x1e, 0xdd); GCHD_FE_W(d, 0x1f, 0xcd); GCHD_FE_W(d, 0x01, 0x45); GCHD_FE_W(d, 0x02, 0xbc);
  } else {
   GCHD_FE_W(d, 0x03, 0x24); GCHD_FE_W(d, 0x05, 0xf4); GCHD_FE_W(d, 0x06, 0xfc); GCHD_FE_W(d, 0x07, 0xac);
   GCHD_FE_W(d, 0x1e, 0xcc); GCHD_FE_W(d, 0x1f, 0xcc); GCHD_FE_W(d, 0x01, 0x45); GCHD_FE_W(d, 0x02, 0xbc);
  }
 } else if(d->input_height==720) {
  GCHD_FE_W(d, 0x03, 0x94); GCHD_FE_W(d, 0x05, 0xf4); GCHD_FE_W(d, 0x06, 0xec); GCHD_FE_W(d, 0x07, 0xfc);
  GCHD_FE_W(d, 0x1e, 0xdd); GCHD_FE_W(d, 0x1f, 0xcd); GCHD_FE_W(d, 0x01, 0xab); GCHD_FE_W(d, 0x02, 0xdc);
 } else if(d->input_height==576) {
  if(d->input_interlaced) {
   GCHD_FE_W(d, 0x03, 0xc4); GCHD_FE_W(d, 0x05, 0xc4); GCHD_FE_W(d, 0x06, 0xc4); GCHD_FE_W(d, 0x07, 0xd4);
   GCHD_FE_W(d, 0x1e, 0x88); GCHD_FE_W(d, 0x1f, 0xc8); GCHD_FE_W(d, 0x01, 0xf9); GCHD_FE_W(d, 0x02, 0x3c);
  } else {
   GCHD_FE_W(d, 0x03, 0xd4); GCHD_FE_W(d, 0x05, 0xc4); GCHD_FE_W(d, 0x06, 0xc4); GCHD_FE_W(d, 0x07, 0xd4);
   GCHD_FE_W(d, 0x1e, 0xff); GCHD_FE_W(d, 0x1f, 0xcf); GCHD_FE_W(d, 0x01, 0xf9); GCHD_FE_W(d, 0x02, 0x3c);
  }
 } else if(d->input_height==480) {
  if(d->input_interlaced) {
   GCHD_FE_W(d, 0x03, 0xc4); GCHD_FE_W(d, 0x05, 0xc4); GCHD_FE_W(d, 0x06, 0xc4); GCHD_FE_W(d, 0x07, 0xd4);
   GCHD_FE_W(d, 0x1e, 0x88); GCHD_FE_W(d, 0x1f, 0xc8); GCHD_FE_W(d, 0x01, 0xf9); GCHD_FE_W(d, 0x02, 0x5c);
  } else {
   GCHD_FE_W(d, 0x03, 0xec); GCHD_FE_W(d, 0x05, 0xc4); GCHD_FE_W(d, 0x06, 0xc4); GCHD_FE_W(d, 0x07, 0xd4);
   GCHD_FE_W(d, 0x1e, 0xff); GCHD_FE_W(d, 0x1f, 0xcf); GCHD_FE_W(d, 0x01, 0xf9); GCHD_FE_W(d, 0x02, 0x5c);
  }
 } else return -EINVAL;

 GCHD_FE_R(d, 0x2f, v); GCHD_FE_W(d, 0x10, 0x71); GCHD_FE_R(d, 0x30, v); GCHD_FE_W(d, 0x0f, 0xe4); GCHD_FE_W(d, 0x12, 0xc8); GCHD_FE_R(d, 0x28, v);
 GCHD_FE_W(d, 0x17, 0xce);
 if(d->input_height==480||d->input_height==576) GCHD_FE_W(d, 0x12, 0xc8); else GCHD_FE_W(d, 0x12, 0xcc);
 GCHD_FE_W(d, 0x13, 0xd8); GCHD_FE_W(d, 0x0b, 0x4c); GCHD_FE_W(d, 0x0c, 0xac); GCHD_FE_W(d, 0x0d, 0x4c);
 GCHD_FE_W(d, 0x1b, 0xcc); GCHD_FE_W(d, 0x1c, 0xcc); GCHD_FE_W(d, 0x1d, 0xcc); GCHD_FE_W(d, 0x18, 0xdc); GCHD_FE_W(d, 0x19, 0xdc);
 GCHD_FE_W(d, 0x1a, 0xdc); GCHD_FE_W(d, 0x2d, 0xdd); GCHD_FE_W(d, 0x2e, 0xdd); GCHD_FE_W(d, 0x2f, 0xce); GCHD_FE_W(d, 0x3a, 0xc0); GCHD_FE_W(d, 0x3b, 0xc4);
 GCHD_FE_R(d, 0x39, v);

 if(d->input_height==1080) {
  if(d->input_interlaced) {
   GCHD_FE_W(d, 0x39, 0x2c); GCHD_FE_W(d, 0x2c, 0x51); GCHD_FE_W(d, 0x03, 0x8c); GCHD_FE_W(d, 0x12, 0xc8); GCHD_FE_W(d, 0x13, 0xd8); GCHD_FE_W(d, 0x17, 0xc7);
   GCHD_FE_W(d, 0x21, 0xc6); GCHD_FE_W(d, 0x20, 0xcc); GCHD_FE_W(d, 0x40, 0xcc); GCHD_FE_W(d, 0x80, 0xcc); GCHD_FE_W(d, 0x81, 0x73); GCHD_FE_W(d, 0x82, 0xcb);
   GCHD_FE_W(d, 0x83, 0x4c); GCHD_FE_W(d, 0x84, 0xdf); GCHD_FE_W(d, 0x85, 0xee); GCHD_FE_W(d, 0x86, 0xd0);
  } else {
   GCHD_FE_W(d, 0x39, 0x3c); GCHD_FE_W(d, 0x2c, 0x51); GCHD_FE_W(d, 0x03, 0x24); GCHD_FE_W(d, 0x12, 0xcc); GCHD_FE_W(d, 0x13, 0xc6); GCHD_FE_W(d, 0x17, 0xc7);
   GCHD_FE_W(d, 0x21, 0xcb); GCHD_FE_W(d, 0x20, 0xcc); GCHD_FE_W(d, 0x40, 0xcc); GCHD_FE_W(d, 0x80, 0xcc); GCHD_FE_W(d, 0x81, 0x73); GCHD_FE_W(d, 0x82, 0xcb);
   GCHD_FE_W(d, 0x83, 0x4c); GCHD_FE_W(d, 0x84, 0xe4); GCHD_FE_W(d, 0x85, 0xc8); GCHD_FE_W(d, 0x86, 0xf4);
  }
 } else if(d->input_height==720) {
  GCHD_FE_W(d, 0x39, 0x2c); GCHD_FE_W(d, 0x2c, 0x51); GCHD_FE_W(d, 0x03, 0x8c); GCHD_FE_W(d, 0x12, 0xc8); GCHD_FE_W(d, 0x13, 0xd8); GCHD_FE_W(d, 0x17, 0xc7);
  GCHD_FE_W(d, 0x21, 0xc6); GCHD_FE_W(d, 0x20, 0xcc); GCHD_FE_W(d, 0x40, 0xcc); GCHD_FE_W(d, 0x80, 0xcd); GCHD_FE_W(d, 0x81, 0xcf); GCHD_FE_W(d, 0x82, 0xc9);
  GCHD_FE_W(d, 0x83, 0xcc); GCHD_FE_W(d, 0x84, 0xd4); GCHD_FE_W(d, 0x85, 0xce); GCHD_FE_W(d, 0x86, 0x1c);
 } else if(d->input_height==576) {
  if(d->input_interlaced) {
   GCHD_FE_W(d, 0x39, 0x54); GCHD_FE_W(d, 0x2c, 0x51); GCHD_FE_W(d, 0x03, 0xcc); GCHD_FE_W(d, 0x12, 0xc8); GCHD_FE_W(d, 0x13, 0xc2); GCHD_FE_W(d, 0x17, 0xc6);
   GCHD_FE_W(d, 0x21, 0xc6); GCHD_FE_W(d, 0x20, 0xec); GCHD_FE_W(d, 0x40, 0xc4); GCHD_FE_W(d, 0x80, 0xcc); GCHD_FE_W(d, 0x81, 0x4f); GCHD_FE_W(d, 0x82, 0xce);
   GCHD_FE_W(d, 0x83, 0x1c); GCHD_FE_W(d, 0x84, 0xd9); GCHD_FE_W(d, 0x85, 0xed); GCHD_FE_W(d, 0x86, 0xec);
  } else {
   GCHD_FE_W(d, 0x39, 0x54); GCHD_FE_W(d, 0x2c, 0x51); GCHD_FE_W(d, 0x03, 0x8c); GCHD_FE_W(d, 0x12, 0xc8); GCHD_FE_W(d, 0x13, 0xe4); GCHD_FE_W(d, 0x17, 0xc7);
   GCHD_FE_W(d, 0x21, 0xc6); GCHD_FE_W(d, 0x20, 0xcc); GCHD_FE_W(d, 0x40, 0xcc); GCHD_FE_W(d, 0x80, 0xcc); GCHD_FE_W(d, 0x81, 0x8f); GCHD_FE_W(d, 0x82, 0xce);
   GCHD_FE_W(d, 0x83, 0x1c); GCHD_FE_W(d, 0x84, 0xe7); GCHD_FE_W(d, 0x85, 0xce); GCHD_FE_W(d, 0x86, 0x8c);
  }
 } else {
  if(d->input_interlaced) {
   GCHD_FE_W(d, 0x39, 0x54); GCHD_FE_W(d, 0x2c, 0x51); GCHD_FE_W(d, 0x03, 0xcc); GCHD_FE_W(d, 0x12, 0xc8); GCHD_FE_W(d, 0x13, 0xd8); GCHD_FE_W(d, 0x17, 0xc7);
   GCHD_FE_W(d, 0x21, 0xc6); GCHD_FE_W(d, 0x20, 0xec); GCHD_FE_W(d, 0x40, 0xc4); GCHD_FE_W(d, 0x80, 0xcc); GCHD_FE_W(d, 0x81, 0xf4); GCHD_FE_W(d, 0x82, 0xce);
   GCHD_FE_W(d, 0x83, 0x1c); GCHD_FE_W(d, 0x84, 0xdd); GCHD_FE_W(d, 0x85, 0xec); GCHD_FE_W(d, 0x86, 0x3c);
  } else {
   GCHD_FE_W(d, 0x39, 0x54); GCHD_FE_W(d, 0x2c, 0x51); GCHD_FE_W(d, 0x03, 0x8c); GCHD_FE_W(d, 0x12, 0xc8); GCHD_FE_W(d, 0x13, 0xec); GCHD_FE_W(d, 0x17, 0xc7);
   GCHD_FE_W(d, 0x21, 0xc6); GCHD_FE_W(d, 0x20, 0xcc); GCHD_FE_W(d, 0x40, 0xcc); GCHD_FE_W(d, 0x80, 0xcc); GCHD_FE_W(d, 0x81, 0xf7); GCHD_FE_W(d, 0x82, 0xce);
   GCHD_FE_W(d, 0x83, 0x1c); GCHD_FE_W(d, 0x84, 0xef); GCHD_FE_W(d, 0x85, 0xcd); GCHD_FE_W(d, 0x86, 0x2c);
  }
 }

 GCHD_FE_W(d, 0xb0, 0xe8); GCHD_FE_W(d, 0xb1, 0x0c);
 if(d->input_height==576 && d->input_interlaced) { GCHD_FE_W(d, 0xad, 0xcc); GCHD_FE_W(d, 0xb0, 0xf9); GCHD_FE_W(d, 0xb1, 0xcc); }
 else if(d->input_height==480 && d->input_interlaced) { GCHD_FE_W(d, 0xad, 0xcc); GCHD_FE_W(d, 0xb0, 0xf9); GCHD_FE_W(d, 0xb1, 0x4c); }
 else { GCHD_FE_W(d, 0xad, 0xc9); GCHD_FE_W(d, 0xb0, 0xe9); }

 r=gchd_color_space_exact(d); if(r)return r;
 if(d->input_height==480 && d->input_interlaced) { GCHD_FE_W(d, 0xb0, 0xf9); GCHD_FE_W(d, 0xb1, 0xcc); }
 GCHD_FE_W(d, 0xab, 0xcc); GCHD_BE_W(d, 0x04, 0x95);
 r=gchd_setup_subblock_exact(d); if(r)return r;
 r=gchd_common_block_b1(d,false); if(r)return r;

 GCHD_BE_R(d, 0xf5, v); GCHD_BE_R(d, 0xfd, v);
 GCHD_BE_R(d, 0xf5, v); GCHD_BE_R(d, 0xfc, v); GCHD_BE_R(d, 0xf3, v);
 GCHD_BE_W(d, 0x0c, 0x89); GCHD_BE_W(d, GCHD_BE_REG_RETRIGGER, GCHD_BE_RETRIGGER_VALUE); GCHD_BE_R(d, 0xf5, v);
 GCHD_BE_W(d, GCHD_BE_REG_PULSE, (u8)((v&0x10)|0x65)); GCHD_BE_W(d, GCHD_BE_REG_PULSE, (u8)((v&0x10)|0x64));

 r=gchd_common_block_a(d); if(r)return r;
 r=gchd_setup_subblock_exact(d); if(r)return r;
 r=gchd_common_block_b1(d,d->input_height==1080 && !d->input_interlaced); if(r)return r;
 r=gchd_common_block_b2(d); if(r)return r;
 r=gchd_common_block_b3(d); if(r)return r;
 GCHD_FE_W(d, 0x0f, 0xe4);
 r=gchd_common_block_c(d); if(r)return r;

 return 0;

}

static int gchd_input_configure_mode(struct gchd *d)
{
 int r;

 if (d->input_configured)
  return 0;

 dev_info(&d->intf->dev,
          "capture setup: input=%u mode=%ux%u fps=%u/%u interlaced=%u\n",
          d->input, d->input_width, d->input_height, d->input_fps_num,
          d->input_fps_den, d->input_interlaced);

 if (!d->encoder_started) {
  r = gchd_encoder_start(d);
  if (r) {
   dev_err(&d->intf->dev, "capture setup: encoder start failed: %d\n", r);
   return r;
  }
  d->encoder_started = true;
  dev_info(&d->intf->dev, "capture setup: encoder start completed\n");
 }

 /*
  * Connector routing is input-selection state, not encoder-firmware
  * initialization. Reapply it on every connector change; otherwise later
  * Composite/S-video selections skip selector 0x51 and retain the old route.
  */
 if (!d->input_routed) {
  r = gchd_input_finalize(d);
  if (r) {
   dev_err(&d->intf->dev,
           "capture setup: connector routing failed: %d\n", r);
   return r;
  }
  d->input_routed = true;
 }

 if (d->input == 0)
  r = gchd_configure_hdmi_exact(d);
 else if (d->input >= 2)
  r = gchd_configure_composite_exact(d);
 else
  r = gchd_configure_component_exact(d);
 if (r) {
  if (r == -ENOLINK) {
   /* Keep the prepared receiver stopped; the worker now polls timing only. */
   d->input_prepared = true;
   return r;
  }
  /*
   * Connector setup failed after the encoder firmware was already running.
   * Return to the stopped state, but do not tear down the encoder: input changes reuse the
   * initialized encoder and retry only the connector-specific configuration.
   */
  gchd_return_to_stop(d);
  d->input_prepared = false;
  dev_err(&d->intf->dev, "capture setup: input configuration failed: %d\n", r);
  return r;
 }
 dev_info(&d->intf->dev, "capture setup: input configuration completed\n");

 /* Encoder configuration common to HDMI, Component and Composite. */
 r = gchd_transcoder_final_configure(d);
 if (r) {
  dev_err(&d->intf->dev, "capture setup: transcoder final configuration failed: %d\n", r);
  return r;
 }
 dev_info(&d->intf->dev, "capture setup: transcoder final configuration completed\n");

 r = gchd_transcoder_setup(d);
 if (r) {
  dev_err(&d->intf->dev, "capture setup: transcoder setup failed: %d\n", r);
  return r;
 }
 dev_info(&d->intf->dev, "capture setup: transcoder setup completed\n");

 /*
  * Every input finishes with: encoder final configuration, encoder setup,
  * SCMD INIT (mode 0xa0), state readback, then SCMD STATE_CHANGE (START).
  * The second INIT is issued here
  * (after the final transcoder programming); the earlier INIT in
  * gchd_input_finalize() is a separate stage before connector configuration.
  * Mode 0xa0 suppresses the INIT interrupt/readback handshake.
  */
 r = gchd_scmd(d, GCHD_SCMD_INIT, GCHD_SCMD_INIT_NO_RELOAD, 0);
 if (r) {
  dev_err(&d->intf->dev, "capture setup: final SCMD_INIT failed: %d\n", r);
  return r;
 }

 /* Read the encoder state after the mode 0xa0 INIT. */
 {
  u16 state;

  r = gchd_raw_read16(d, GCHD_BANK_SYS, GCHD_REG_SCMD_STATE, &state);
  if (r) {
   dev_err(&d->intf->dev,
           "capture setup: state readback after final SCMD_INIT failed: %d\n", r);
   return r;
  }
  dev_info(&d->intf->dev,
           "capture setup: state after final SCMD_INIT=0x%04x\n", state);
 }

 /* A successfully configured input stays configured while hardware is idle. */
 d->input_prepared = false;
 d->input_configured = true;
 return 0;
}

int gchd_input_configure(struct gchd *d)
{
 return gchd_input_configure_mode(d);
}

int gchd_input_configure_idle(struct gchd *d)
{
 return gchd_input_configure_mode(d);
}

int gchd_input_start(struct gchd *d)
{
 int r;

 if (!d->input_configured)
  return -EINVAL;

 /*
  * The timing-detection pass in input configuration is authoritative for
  * now. Do not put the device into START when it only has fallback mode
  * dimensions and no recognized signal. The open-driven detector retries
  * until a valid source is found, then starts capture automatically.
  */
 if (!d->signal_present) {
  dev_info(&d->intf->dev,
           "gchd-diag refusing START: no signal; input=%u mode=%ux%u interlaced=%u\n",
           d->input, d->input_width, d->input_height, d->input_interlaced);
  return -ENOLINK;
 }

 dev_info(&d->intf->dev,
          "gchd-diag issuing START: input=%u mode=%ux%u fps=%u/%u interlaced=%u\n",
          d->input, d->input_width, d->input_height, d->input_fps_num,
          d->input_fps_den, d->input_interlaced);
 r = gchd_state_cmd(d, GCHD_SCMD_STATE_CHANGE, 0, GCHD_STATE_START, GCHD_STATE_START);
 if (r) {
  dev_err(&d->intf->dev, "capture start transition failed: %d\n", r);
  return r;
 }
 dev_info(&d->intf->dev, "gchd-diag START transition completed\n");
 return 0;
}

void gchd_input_stop(struct gchd *d)
{
 if (d->input_configured || d->encoder_started)
  gchd_do_enable(d, GCHD_EB_ENCODER_TRIGGER | GCHD_EB_ENCODER_ENABLE, 0);
 d->input_configured = false;
 d->encoder_started = false;
 d->input_prepared = false;
 d->signal_present = false;
}

MODULE_DESCRIPTION("Elgato Game Capture HD input configuration");
MODULE_LICENSE("GPL");
