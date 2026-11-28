// SPDX-License-Identifier: GPL-2.0
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


 /* Continuation after the 0x88:FB read. */
 /* Keep the decoder mode of the selected analog input (S-video: DC). */
 GCHD_DEC_W(d, 0x02, d->input == 3 ? 0xdc : 0xc9);
 d->dec_svideo = d->input == 3;
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

 /* Keep the decoder mode of the selected analog input (S-video: DC). */
 GCHD_DEC_W(d, 0x02, d->input == 3 ? 0xdc : 0xc9);
 d->dec_svideo = d->input == 3;
 GCHD_DEC_W(d, 0x14, 0xd2);
 GCHD_DEC_W(d, 0x3c, 0x6b);
 GCHD_DEC_R(d, GCHD_DEC_REG_FAMILY, v);

 GCHD_DEC_W(d, 0x28, 0x88);
 if (d->input >= 2) {
  /* Composite, S-video: the picture settings. */
  GCHD_TRY(gchd_dec_picture(d));
 } else {
  GCHD_DEC_W(d, 0x10, 0x88);
  GCHD_DEC_W(d, 0x11, 0xd4);
  GCHD_DEC_W(d, 0x12, 0xd0);
  GCHD_DEC_W(d, 0x13, 0x08);
  GCHD_DEC_W(d, 0x14, 0x08);
  GCHD_DEC_W(d, 0x15, 0x88);
 }

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
static int gchd_be_retrigger(struct gchd *d);
static int gchd_encoder_program(struct gchd *d);
static void gchd_analog_note_std(struct gchd *d, u8 family);

/*
 * Status poll of the back end while waiting for an analog source: the back
 * end is retriggered when GCHD_BE_STATUS_FLAG is set (composite and S-video).
 */
static int gchd_be_poll_status(struct gchd *d, u8 *status)
{
 u8 v;

 GCHD_BE_R(d, GCHD_BE_REG_STATUS, v);
 if (status)
  *status = v;
 if (d->input < 2 || !(v & GCHD_BE_STATUS_FLAG))
  return 0;
 gchd_dbg_ratelimited(&d->intf->dev,
                      "back-end status 0x%02x while polling; retrigger\n", v);
 return gchd_be_retrigger(d);
}

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
  * S-video needs its distinct decoder mode before detection. The decoder
  * keeps its setting until the device loses power (also across closing and
  * reopening). After S-video, composite gets the S-video-to-composite
  * sequence instead: 02 = C9, 14 = D2, 3C = 6B, a pause, 28 = 88, 03 = 28.
  */
 if (d->input == 3) {
  GCHD_DEC_W(d, 0x02, 0xdc);
  d->dec_svideo = true;
  GCHD_DEC_W(d, 0x14, 0xd2);
  GCHD_DEC_W(d, 0x28, 0x88);
 } else if (d->input == 2 && d->dec_svideo) {
  GCHD_DEC_W(d, 0x02, 0xc9);
  d->dec_svideo = false;
  GCHD_DEC_W(d, 0x14, 0xd2);
  GCHD_DEC_W(d, 0x3c, 0x6b);
  msleep(220);
  GCHD_DEC_W(d, 0x28, 0x88);
  goto route;
 }
 GCHD_DEC_W(d, 0x06, 0x08);

 GCHD_DEC_R(d, 0xf8, reply);

route:
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
   r = gchd_be_poll_status(d, NULL);
   if (r)
    return r;
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
  };
  r = gchd_mail_seq(d, tune, ARRAY_SIZE(tune));
  if (r) return r;
 }
 /* Third pass: the picture settings (neutral 6F EA D0 1B 1B 88). */
 r = gchd_dec_picture(d);
 if (r) return r;
 r = gchd_audio_put(d);
 if (r)
  return r;

 d->input_detect_prepared = true;
 return 0;
}

/*
 * Component while capturing: read the timing measurement once (the mode
 * worker calls this every 0.75 s). Returns 1 when it shows no
 * signal or another source mode than the configured one, 0 when it matches,
 * a negative error code when the read fails.
 */
int gchd_component_mode_changed(struct gchd *d)
{
 static const struct { u16 value; u16 height; bool interlaced; } modes[] = {
  { 0xbbf4, 1080, false }, { 0xa03d, 1080, true }, { 0xbf59, 720, false },
  { 0xa6b7, 576, false }, { 0x9ab9, 576, true }, { 0xa150, 480, false },
  { 0x9576, 480, true },
 };
 u8 hi, lo;
 u32 v6665, v6867;
 unsigned int i;

 GCHD_FE_R(d, 0x66, hi);
 GCHD_FE_R(d, 0x65, lo);
 v6665 = (u32)hi << 8 | lo;
 GCHD_FE_R(d, 0x68, hi);
 GCHD_FE_R(d, 0x67, lo);
 v6867 = (u32)hi << 8 | lo;
 if (!gchd_component_timing_valid(v6867) || abs((int)v6665 - 0xad4d) < 10)
  return 1;
 for (i = 0; i < ARRAY_SIZE(modes); ++i)
  if (abs((int)v6867 - modes[i].value) < 10)
   return modes[i].height != d->input_height ||
          modes[i].interlaced != d->input_interlaced;
 return 1;
}

static bool gchd_hdmi_mode(struct gchd *d, u32 lines, u32 rate);

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
  gchd_dbg(&d->intf->dev,
           "gchd-diag detect HDMI: samples6665=%u avg6665=0x%04x samples6463=%u avg6463=0x%04x rgb=%u\n",
           count6665, value6665, count6463, value6463, d->rgb_input);
  if (!gchd_hdmi_mode(d, value6463, value6665))
   return -ENOLINK;
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
  gchd_dbg(&d->intf->dev,
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
  d->seen[d->input == 1].width = d->input_width;
  d->seen[d->input == 1].height = d->input_height;
  d->seen[d->input == 1].fps = d->input_fps_num;
  d->seen[d->input == 1].interlaced = d->input_interlaced;
 } else {
  u8 v, std, be_status = 0;

  /*
   * After pre-detection tuning and the 0x94 prime, FA reports the analog
   * family in its low nibble (6=NTSC, 7=PAL). Waiting for literal CA here
   * deadlocks the 240p path; CA/E7 polling follows family detection.
   */
  GCHD_DEC_R(d, GCHD_DEC_REG_FAMILY, v);
  r = gchd_be_poll_status(d, &be_status);
  if (r)
   return r;
  gchd_dbg(&d->intf->dev,
           "gchd-diag detect %s: pre-config raw_status=0x%02x mode=0x%x\n",
           d->input == 2 ? "composite" : "s-video", v, v & 0x0f);
  v &= 0x0f;
  if (v != 6 && v != 7) {
   u8 lock;

   d->pal60_since = 0;
   d->pal60_logged = false;

   /* Diagnostic only: decoder lock status while no family is reported. */
   GCHD_DEC_R(d, GCHD_DEC_REG_LOCK, lock);
   gchd_dbg_ratelimited(&d->intf->dev,
                        "gchd-diag no analog family: 0x88:FA=0x%02x 0x88:CA=0x%02x 0x98:F5=0x%02x\n",
                        v, lock, be_status);
   return -ENOLINK;
  }

  /*
   * Family recognized: query decoder status/ready/height in sequence.
   */
  GCHD_DEC_R(d, GCHD_DEC_REG_LOCK, v);
  GCHD_DEC_R(d, 0xe7, std);
  GCHD_DEC_R(d, 0xf1, v);

  /* The 0x94 prime already ran before SOURCE_DETECTION. */
  GCHD_DEC_R(d, GCHD_DEC_REG_FAMILY, v);
  gchd_dbg(&d->intf->dev,
           "gchd-diag detect %s: raw_status=0x%02x masked_mode=0x%01x\n",
           d->input == 2 ? "composite" : "s-video", v, v & 0x0f);
  v &= 0x0f;
  if (v == 6) {
   u8 mode;

   d->input_width=720; d->input_height=480;
   d->input_interlaced=true; d->input_fps_num=60;
   /* 0xCA is the decoder's authoritative 240p/480i lock indicator. */
   GCHD_DEC_R(d, GCHD_DEC_REG_LOCK, mode);
   if (mode != GCHD_DEC_LOCK_480I && mode != GCHD_DEC_LOCK_240P) {
    /* Transitional value: detect again until the decoder has locked. */
    gchd_dbg(&d->intf->dev,
             "analog format status 0x%02x not locked yet\n", mode);
    return -ENOLINK;
   }
   if (!d->pal60_since)
    d->pal60_since = jiffies | 1;
   if (gchd_analog_pal60_hold(d, std, d->pal60_since, &d->pal60_logged))
    return -ENOLINK;
   gchd_analog_note_std(d, 6);
   d->input_interlaced = mode != GCHD_DEC_LOCK_240P;
   gchd_dbg(&d->intf->dev,
            "analog format initial status=0x%02x; configuring %s\n",
            mode, d->input_interlaced ? "480i" : "240p");
  } else if (v == 7) {
   u8 mode;

   d->pal60_since = 0;
   d->pal60_logged = false;
   d->input_width=720; d->input_height=576;
   d->input_interlaced=true; d->input_fps_num=50;
   GCHD_DEC_R(d, GCHD_DEC_REG_LOCK, mode);
   if (mode != GCHD_DEC_LOCK_480I && mode != GCHD_DEC_LOCK_240P) {
    gchd_dbg(&d->intf->dev,
             "analog format status 0x%02x not locked yet\n", mode);
    return -ENOLINK;
   }
   d->input_interlaced = mode != GCHD_DEC_LOCK_240P;
   gchd_analog_note_std(d, 7);
   gchd_dbg(&d->intf->dev,
            "analog format initial status=0x%02x; configuring %s\n",
            mode, d->input_interlaced ? "576i" : "288p");
  } else return -ENOLINK;
  d->input_fps_den=1;
 }
 WRITE_ONCE(d->signal_present,true);
 gchd_dbg(&d->intf->dev,
          "gchd-diag detected input=%u mode=%ux%u fps=%u/%u interlaced=%u signal_present=%u\n",
          d->input, d->input_width, d->input_height, d->input_fps_num,
          d->input_fps_den, d->input_interlaced, d->signal_present);
 return 0;
}

/*
 * Colour conversion of the front end (0x9C:92-AA): a header byte, a 3 x 3
 * matrix of 15-bit two's complement coefficients (4096 = 1.0; word
 * in * 3 + out, outputs Cr, Y, Cb) and three offsets (x 64: Cr, Y, Cb),
 * 16-bit big-endian. YCbCr sources get the identity; RGB sources (HDMI) the
 * BT.601 (standard definition) or BT.709 conversion, "Expanded" range
 * scaled by 255/219.
 */
struct gchd_csc {
 u8 header;
 s16 m[9];
};

static const struct gchd_csc gchd_csc_ycbcr = {
 0x66, { 4096, 0, 0, 0, 4096, 0, 0, 0, 4096 } };
static const struct gchd_csc gchd_csc_rgb[2][2] = {
 { /* BT.601: standard, expanded */
  { 0x40, { 1798, 1053, -606, -1507, 2064, -1192, -291, 401, 1798 } },
  { 0x55, { 2093, 1225, -705, -1753, 2404, -1389, -340, 467, 2093 } } },
 { /* BT.709 */
  { 0x40, { 1798, 750, -414, -1634, 2515, -1384, -164, 254, 1798 } },
  { 0x55, { 2093, 872, -479, -1900, 2928, -1613, -192, 294, 2093 } } },
};

/* Write a conversion table; Y_OFFSET is the luma offset word. */
static int gchd_csc_put(struct gchd *d, const struct gchd_csc *c, u16 y_offset)
{
 u8 b[25];
 u16 off[3] = { 0x2000, y_offset, 0x2000 };
 unsigned int i;

 b[0] = c->header;
 for (i = 0; i < 9; ++i) {
  b[1 + 2 * i] = ((u16)c->m[i] & 0x7fff) >> 8;
  b[2 + 2 * i] = (u16)c->m[i] & 0xff;
 }
 for (i = 0; i < 3; ++i) {
  b[19 + 2 * i] = off[i] >> 8;
  b[20 + 2 * i] = off[i] & 0xff;
 }
 for (i = 0; i < ARRAY_SIZE(b); ++i)
  GCHD_FE_W(d, 0x92 + i, b[i] ^ GCHD_FE_XOR);
 return 0;
}

/* The conversion of the selected input and source in use. */
static const struct gchd_csc *gchd_csc_of(const struct gchd *d)
{
 if (d->input != 0 || !d->rgb_input)
  return &gchd_csc_ycbcr;
 return &gchd_csc_rgb[d->input_height > 576][d->hdmi_range != 0];
}

/* sin(n / 2 degrees) x 4096, n = 0 ... 180. */
static const u16 gchd_sin_half[181] = {
 0, 36, 71, 107, 143, 179, 214, 250, 286, 321, 357, 393,
 428, 464, 499, 535, 570, 605, 641, 676, 711, 746, 782, 817,
 852, 887, 921, 956, 991, 1026, 1060, 1095, 1129, 1163, 1198, 1232,
 1266, 1300, 1334, 1367, 1401, 1434, 1468, 1501, 1534, 1567, 1600, 1633,
 1666, 1699, 1731, 1763, 1796, 1828, 1860, 1891, 1923, 1954, 1986, 2017,
 2048, 2079, 2110, 2140, 2171, 2201, 2231, 2261, 2290, 2320, 2349, 2379,
 2408, 2436, 2465, 2493, 2522, 2550, 2578, 2605, 2633, 2660, 2687, 2714,
 2741, 2767, 2793, 2820, 2845, 2871, 2896, 2921, 2946, 2971, 2996, 3020,
 3044, 3068, 3091, 3115, 3138, 3161, 3183, 3206, 3228, 3250, 3271, 3293,
 3314, 3335, 3355, 3376, 3396, 3416, 3435, 3455, 3474, 3492, 3511, 3529,
 3547, 3565, 3582, 3600, 3617, 3633, 3650, 3666, 3681, 3697, 3712, 3727,
 3742, 3756, 3770, 3784, 3798, 3811, 3824, 3837, 3849, 3861, 3873, 3884,
 3896, 3906, 3917, 3927, 3937, 3947, 3956, 3966, 3974, 3983, 3991, 3999,
 4006, 4014, 4021, 4027, 4034, 4040, 4046, 4051, 4056, 4061, 4065, 4070,
 4074, 4077, 4080, 4083, 4086, 4088, 4090, 4092, 4094, 4095, 4095, 4096,
 4096,
};

/* Picture setting -100 ... +100 as 0 ... 255 (128 neutral). */
static int gchd_pic_v(int setting)
{
 return ((setting + 100) * 255 + 100) / 200;
}

/*
 * The conversion of the source with the picture settings: contrast scales
 * Y (v x 32), saturation Cr and Cb (v x 32), hue rotates Cr/Cb (0.5 degree
 * steps, +/-61.5 degrees at +/-180), all applied after the source's
 * conversion; brightness moves the Y offset (16 + (v - 128) x 1.25).
 */
static void gchd_csc_picture(const struct gchd *d, struct gchd_csc *c,
                             u16 *y_offset)
{
 const struct gchd_csc *m = gchd_csc_of(d);
 s32 con = gchd_pic_v(d->contrast) * 32, sat = gchd_pic_v(d->saturation) * 32;
 int n = DIV_ROUND_CLOSEST(d->hue * 123, 180);
 s32 sn = (n < 0 ? -1 : 1) * gchd_sin_half[abs(n)];
 s32 cs = gchd_sin_half[180 - abs(n)];
 s32 a[9] = { 0 };
 int in, out, k;

 /* Adjustment in YCbCr (word in * 3 + out; channels Cr, Y, Cb). */
 a[0] = DIV_ROUND_CLOSEST(sat * cs, 4096);
 a[2] = -DIV_ROUND_CLOSEST(sat * sn, 4096);
 a[6] = DIV_ROUND_CLOSEST(sat * sn, 4096);
 a[8] = a[0];
 a[4] = con;
 c->header = m->header;
 for (in = 0; in < 3; ++in)
  for (out = 0; out < 3; ++out) {
   s32 sum = 0;

   for (k = 0; k < 3; ++k)
    sum += a[k * 3 + out] * m->m[in * 3 + k];
   c->m[in * 3 + out] = clamp_t(s32, DIV_ROUND_CLOSEST(sum, 4096),
                                -16384, 16383);
  }
 *y_offset = (((16 + (gchd_pic_v(d->brightness) - 128) * 5 / 4) & 0x1ff) << 6) | 1;
}

/* Conversion of the source in use, with the picture settings. */
int gchd_csc_apply(struct gchd *d)
{
 struct gchd_csc c;
 u16 y_offset;

 gchd_csc_picture(d, &c, &y_offset);
 return gchd_csc_put(d, &c, y_offset);
}

/*
 * Picture settings of the analog decoder (composite, S-video), device 0x88,
 * values sent XOR 0x88: 10 brightness (signed, neutral -25), 11 contrast
 * (98), 13 and 14 saturation (147), 15 hue (signed, 0); 12 stays D0. Each
 * moves 1.28 per step from neutral (contrast 0.64; hue 128 per 180
 * degrees), limited to the register range.
 */
int gchd_dec_picture(struct gchd *d)
{
 int b = clamp_t(int, -25 + DIV_ROUND_CLOSEST(d->brightness * 128, 100),
                 -128, 127);
 int c = clamp_t(int, 98 + DIV_ROUND_CLOSEST(d->contrast * 64, 100), 0, 255);
 int s = clamp_t(int, 147 + DIV_ROUND_CLOSEST(d->saturation * 128, 100),
                 0, 255);
 int h = clamp_t(int, DIV_ROUND_CLOSEST(d->hue * 128, 180), -128, 127);

 GCHD_DEC_W(d, 0x10, (u8)b ^ 0x88);
 GCHD_DEC_W(d, 0x11, (u8)c ^ 0x88);
 GCHD_DEC_W(d, 0x12, 0xd0);
 GCHD_DEC_W(d, 0x13, (u8)s ^ 0x88);
 GCHD_DEC_W(d, 0x14, (u8)s ^ 0x88);
 GCHD_DEC_W(d, 0x15, (u8)h ^ 0x88);
 return 0;
}

/*
 * Analog audio gain (-12 ... +12 dB, device 0x94): 0x41 = 37, or 57 for a
 * 20 dB boost above 0 dB; 0x4A/0x4B (left, right) attenuation code A7 + 2
 * per dB, sent XOR 0x08.
 */
int gchd_audio_put(struct gchd *d)
{
 int g = clamp_t(int, d->audio_gain, -12, 12);
 u8 code = 0xa7 + 2 * (g > 0 ? 20 - g : -g);

 GCHD_AUX_W(d, 0x41, g > 0 ? 0x57 : 0x37);
 GCHD_AUX_W(d, 0x4a, code ^ 0x08);
 GCHD_AUX_W(d, 0x4b, code ^ 0x08);
 return 0;
}

/* While capturing: the same, in bank CC. */
int gchd_csc_update(struct gchd *d)
{
 u8 v;

 GCHD_FE_R(d, 0x3f, v);
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc);
 return gchd_csc_apply(d);
}

/*
 * HDMI mode from the receiver's measurements: 0x9C:64/63 (lines) and
 * 0x9C:66/65 (field rate: about BA95 60 Hz, BB75 50 Hz, A2F9 30 Hz, A13C
 * 25 Hz; AD4D no signal). False if no mode is recognized.
 */
static bool gchd_hdmi_classify(u32 lines, u32 rate, struct gchd_seen *m)
{
 m->fps = abs((int)rate - 0xbb75) < 10 ? 50 :
          abs((int)rate - 0xa2f9) < 10 ? 30 :
          abs((int)rate - 0xa13c) < 10 ? 25 : 60;
 if (!gchd_hdmi_timing_valid(lines) || abs((int)rate - 0xad4d) < 10)
  return false;
 m->interlaced = false;
 if (lines >= 0xb6cd && lines <= 0xb6e1) {
  m->width = 1920; m->height = 1080;
 } else if (lines >= 0xb077 && lines <= 0xb08b) {
  m->width = 1920; m->height = 1080; m->interlaced = true;
 } else if (lines >= 0xb052 && lines <= 0xb066) {
  m->width = 1280; m->height = 720;
 } else {
  m->width = 720; m->height = m->fps == 50 ? 576 : 480;
 }
 return true;
}

/* Use the measured HDMI mode; false if none is recognized. */
static bool gchd_hdmi_mode(struct gchd *d, u32 lines, u32 rate)
{
 struct gchd_seen m;

 if (!gchd_hdmi_classify(lines, rate, &m))
  return false;
 d->input_width = m.width;
 d->input_height = m.height;
 d->input_interlaced = m.interlaced;
 d->input_fps_num = m.fps;
 d->input_fps_den = 1;
 d->seen[0] = m;
 return true;
}

/*
 * HDMI while capturing: read the timing measurement once (mode worker).
 * Returns 1 when it shows no signal or another mode than the configured
 * one, 0 when it matches, a negative error code when the read fails.
 */
int gchd_hdmi_mode_changed(struct gchd *d)
{
 struct gchd_seen m;
 u8 hi, lo;
 u32 lines, rate;

 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc);
 GCHD_FE_R(d, 0x64, hi);
 GCHD_FE_R(d, 0x63, lo);
 lines = (u32)hi << 8 | lo;
 GCHD_FE_R(d, 0x66, hi);
 GCHD_FE_R(d, 0x65, lo);
 rate = (u32)hi << 8 | lo;
 if (!gchd_hdmi_classify(lines, rate, &m))
  return 1;
 return m.height != d->input_height || m.interlaced != d->input_interlaced ||
        m.fps != d->input_fps_num;
}

/* Status reads of the receiver between the configuration steps. */
static int gchd_hdmi_status(struct gchd *d)
{
 u8 v;

 GCHD_FE_R(d, 0x3f, v); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcd);
 GCHD_FE_R(d, 0x3e, v); GCHD_FE_R(d, 0x0b, v);
 GCHD_FE_R(d, 0x3f, v); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xce);
 GCHD_FE_R(d, 0x34, v); GCHD_FE_R(d, 0x33, v);
 GCHD_FE_R(d, 0x31, v); GCHD_FE_R(d, 0x31, v);
 GCHD_FE_W(d, 0x1b, 0x30);
 GCHD_FE_R(d, 0x2b, v); GCHD_FE_R(d, 0x77, v);
 GCHD_FE_R(d, 0x74, v); GCHD_FE_R(d, 0x76, v);
 return 0;
}

/* Conversion table in the receiver's parameter window (B0 = E8 ... E9). */
static int gchd_hdmi_csc_window(struct gchd *d, const struct gchd_csc *c)
{
 u8 v;
 int r;

 GCHD_FE_W(d, 0xb0, 0xe8); GCHD_FE_W(d, 0xb1, 0x0c);
 r = gchd_csc_put(d, c, 0x0400);
 if (r)
  return r;
 GCHD_FE_W(d, 0xad, 0xc9);
 GCHD_FE_R(d, 0x8f, v);
 GCHD_FE_W(d, 0xb0, 0xe9);
 return 0;
}

/*
 * Receiver setup after selecting HDMI (once per selection): input select,
 * receiver registers and an initial BT.601 conversion in the parameter
 * window. The back end (device 0x98) is not used by HDMI.
 */
static int gchd_hdmi_rx_setup(struct gchd *d)
{
 u8 v;
 int r;

 GCHD_FE_R(d, 0x3f, v);
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc); GCHD_FE_R(d, 0x94, v); GCHD_FE_W(d, 0xab, 0x4c); GCHD_FE_R(d, 0x3f, v);
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xce); GCHD_FE_W(d, 0x1b, 0x33); GCHD_FE_R(d, 0x3f, v);
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc); GCHD_FE_R(d, 0x88, v); GCHD_FE_W(d, 0xb7, 0xce); GCHD_FE_W(d, 0xb8, 0xdc);
 GCHD_FE_W(d, 0xb8, 0xcc); GCHD_FE_R(d, 0x3f, v); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xce); GCHD_FE_W(d, 0x07, 0x38);
 GCHD_FE_W(d, 0x07, 0xc8); GCHD_FE_R(d, 0x3f, v); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc); GCHD_FE_W(d, GCHD_FE_REG_INPUT_SEL, GCHD_FE_SEL_HDMI);
 GCHD_FE_R(d, 0x88, v); GCHD_FE_W(d, 0xb7, 0xcc);
 r = gchd_do_enable(d, 0, 0);
 if (r)
  return r;
 GCHD_FE_R(d, 0x3f, v); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xce); GCHD_FE_R(d, 0x3e, v); GCHD_FE_W(d, 0x01, 0xad);
 GCHD_FE_R(d, 0x3b, v); GCHD_FE_W(d, 0x04, 0xcd); GCHD_FE_W(d, 0x06, 0xc4); GCHD_FE_R(d, 0x36, v);
 GCHD_FE_W(d, 0x09, 0xe4); GCHD_FE_R(d, 0x3f, v); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc); GCHD_FE_R(d, 0x6b, v);
 GCHD_FE_W(d, 0x54, 0xec); GCHD_FE_R(d, 0x93, v); GCHD_FE_W(d, 0xac, 0x4c); GCHD_FE_R(d, 0x3f, v);
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0x4c); GCHD_FE_R(d, 0x3f, v); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc); GCHD_FE_R(d, 0xf1, v);
 GCHD_FE_W(d, 0xce, 0x4c); GCHD_FE_R(d, 0xf0, v); GCHD_FE_W(d, 0xcf, 0xce); GCHD_FE_R(d, 0x3f, v);
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc); GCHD_FE_R(d, 0x6a, v);
 r = gchd_hdmi_csc_window(d, &gchd_csc_rgb[0][0]);
 if (r)
  return r;
 d->hdmi_rx_ready = true;
 return 0;
}

/*
 * HDMI configuration for the measured mode (first start and every source
 * change): timing measurement, per-mode receiver values, conversion of the
 * source (BT.709 in the parameter window, then the source's own).
 */
static int gchd_configure_hdmi_exact(struct gchd *d)
{
 u32 sum6665 = 0, sum6463 = 0, count6665 = 0, count6463 = 0;
 u32 value6665, value6463;
 bool hd, mode, first = !d->hdmi_rx_ready;
 u8 v;
 int r, i, j;

 if (first) {
  r = gchd_hdmi_rx_setup(d);
  if (r)
   return r;
 }
 GCHD_FE_R(d, 0x94, v); GCHD_FE_W(d, 0xab, 0x4c);

 for (i = 0; i < max_t(unsigned int, 1, signal_active_retries); i++) {
  unsigned int retry_limit;

  sum6665 = sum6463 = count6665 = count6463 = 0;
  for (j = 0; j < 10; j++) {
   r = gchd_hdmi_read_signal(d, &sum6463, &count6463, &sum6665,
                             &count6665, &d->rgb_input);
   if (r)
    return r;
  }
  value6665 = sum6665 / count6665;
  value6463 = sum6463 / count6463;
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
 value6665 = sum6665 / count6665;
 value6463 = sum6463 / count6463;
 mode = gchd_hdmi_mode(d, value6463, value6665);
 d->signal_present = mode;
 if (!mode) {
  /* The receiver stays set up; the measurement is repeated later. */
  if (!d->input_width) {
   d->input_width = 1280; d->input_height = 720;
   d->input_interlaced = false; d->input_fps_num = 60; d->input_fps_den = 1;
  }
  return -ENOLINK;
 }

 hd = d->input_height > 576;
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc);
 /* Per mode: 1080 at 50/60 Hz, other HD, standard definition. */
 if (d->input_width == 1920 && d->input_fps_num >= 50) { GCHD_FE_W(d, 0xb2, 0xc4); GCHD_FE_W(d, 0xb5, 0xd0); }
 else if (hd) { GCHD_FE_W(d, 0xb2, 0xcc); GCHD_FE_W(d, 0xb5, 0xcc); }
 else { GCHD_FE_W(d, 0xb2, 0xcf); GCHD_FE_W(d, 0xb5, 0xcc); }
 r = gchd_hdmi_status(d);
 if (r)
  return r;
 /* After the receiver setup, below 1080: equalizer reset pulses. */
 if (first && d->input_width != 1920) {
  GCHD_FE_R(d, 0x20, v);
  GCHD_FE_W(d, 0x1f, 0xdc); GCHD_FE_R(d, 0x29, v); GCHD_FE_W(d, 0x1f, 0xcc);
  GCHD_FE_R(d, 0x3b, v); GCHD_FE_W(d, 0x04, 0xcf); GCHD_FE_W(d, 0x04, 0xcd);
 }
 GCHD_FE_R(d, 0x3f, v); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc); GCHD_FE_W(d, 0x40, 0xcc);
 GCHD_FE_R(d, 0x3f, v); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcd);
 GCHD_FE_R(d, 0x3e, v); GCHD_FE_R(d, 0x0b, v);
 GCHD_FE_R(d, 0x3f, v); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc);
 r = gchd_hdmi_csc_window(d, &gchd_csc_rgb[hd][0]);
 if (r)
  return r;
 GCHD_FE_W(d, 0xab, 0xcc);
 GCHD_FE_R(d, 0x3f, v); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xce);
 GCHD_FE_R(d, 0x6a, v); GCHD_FE_R(d, 0xa7, v); GCHD_FE_R(d, 0x69, v);
 GCHD_FE_R(d, 0x3f, v); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc);
 r = gchd_hdmi_read_signal(d, &sum6463, &count6463, &sum6665, &count6665,
                           &d->rgb_input);
 if (r)
  return r;
 r = gchd_hdmi_status(d);
 if (r)
  return r;
 GCHD_FE_R(d, 0x3f, v); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc);
 r = gchd_csc_apply(d);
 if (r)
  return r;
 GCHD_FE_R(d, 0x8d, v); GCHD_FE_R(d, 0x8a, v);
 GCHD_FE_W(d, 0xb2, hd ? 0xcc : 0xcf);
 GCHD_FE_W(d, 0xb5, 0xc4);
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
 int r;

 if (d->input == 1) {
  r = gchd_mail_seq(d, prefix, ARRAY_SIZE(prefix));
  if (r)
   return r;
 }
 return gchd_csc_apply(d);
}

/*
 * Back-end retrigger: read 0x98:FD/FC/F3, then 0x98:0D = C8 and two writes
 * to 0x98:0E (value, then value with bit 0 cleared).
 */
static int gchd_be_retrigger(struct gchd *d)
{
 u8 v;
 u8 pulse;

 GCHD_BE_R(d, GCHD_BE_REG_STATUS, v);
 GCHD_BE_R(d, 0xfd, v);
 GCHD_BE_R(d, 0xfc, v);
 GCHD_BE_R(d, 0xf3, v);
 GCHD_BE_R(d, GCHD_BE_REG_STATUS, v);
 GCHD_BE_W(d, GCHD_BE_REG_RETRIGGER, GCHD_BE_RETRIGGER_VALUE);
 GCHD_BE_R(d, GCHD_BE_REG_STATUS, v);
 /* Bit 4 of the pulse follows bit 4 of the status read just before it. */
 pulse = GCHD_BE_PULSE_BASE | (v & GCHD_BE_STATUS_PULSE_BIT);
 GCHD_BE_W(d, GCHD_BE_REG_PULSE, pulse);
 GCHD_BE_W(d, GCHD_BE_REG_PULSE, pulse & ~0x01);
 return 0;
}
/*
 * Poll the back-end status about every 4 ms until it reads
 * GCHD_BE_STATUS_SETTLED or has GCHD_BE_STATUS_FLAG set; *status gets the
 * last value. A timeout is reported but not treated as an error.
 */
static int gchd_be_wait_status(struct gchd *d, unsigned int timeout_ms, u8 *status)
{
 unsigned long deadline = jiffies + msecs_to_jiffies(timeout_ms);
 unsigned int polls = 0;
 u8 v;

 for (;;) {
  GCHD_BE_R(d, GCHD_BE_REG_STATUS, v);
  polls++;
  if (v == GCHD_BE_STATUS_SETTLED || (v & GCHD_BE_STATUS_FLAG))
   break;
  if (time_after(jiffies, deadline)) {
   dev_warn(&d->intf->dev,
            "back-end status 0x%02x after %u polls\n", v, polls);
   break;
  }
  usleep_range(3000, 4000);
 }
 dev_dbg(&d->intf->dev, "back-end status 0x%02x after %u polls\n", v, polls);
 *status = v;
 return 0;
}

/*
 * Retrigger the back end; when the status then shows the flag instead of
 * the settled value, retrigger once more and wait again.
 */
static int gchd_be_retrigger_settle(struct gchd *d)
{
 u8 v;
 int r;

 r = gchd_be_retrigger(d);
 if (r)
  return r;
 r = gchd_be_wait_status(d, 1500, &v);
 if (r || v == GCHD_BE_STATUS_SETTLED)
  return r;
 r = gchd_be_retrigger(d);
 if (r)
  return r;
 r = gchd_be_wait_status(d, 1000, &v);
 if (!r && v != GCHD_BE_STATUS_SETTLED)
  dev_warn(&d->intf->dev, "back-end not settled: status 0x%02x\n", v);
 return r;
}

/*
 * Back-end per-mode table and the following block, then retrigger and
 * settle.
 */
static int gchd_analog_backend_table(struct gchd *d, bool pal)
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

 return gchd_be_retrigger_settle(d);
}

static int gchd_analog_backend(struct gchd *d, bool pal, bool status_reads)
{
 u8 v;
 int r;

 if (status_reads) {
  GCHD_DEC_R(d, GCHD_DEC_REG_LOCK, v);
  GCHD_DEC_R(d, 0xe7, v);
  GCHD_DEC_R(d, 0xf1, v);
  GCHD_DEC_R(d, 0xcb, v);
  GCHD_DEC_R(d, GCHD_DEC_REG_LOCK, v);
 }

 r = gchd_analog_backend_table(d, pal);
 if (r)
  return r;

 GCHD_DEC_R(d, GCHD_DEC_REG_FAMILY, v);
 GCHD_DEC_R(d, GCHD_DEC_REG_LOCK, v);
 GCHD_DEC_R(d, 0xe7, v);
 GCHD_DEC_R(d, 0xf1, v);
 return 0;
}

/*
 * Remember the analog standard of the selected input: PAL for the 50 Hz
 * family, NTSC for the 60 Hz family (also when 0x88:E7 reports 59).
 */
static void gchd_analog_note_std(struct gchd *d, u8 family)
{
 if (d->input < 2)
  return;
 d->analog_std[d->input - 2] = family == 7 ? V4L2_STD_PAL : V4L2_STD_NTSC_M;
 /* Remembered across module loads (module parameters, udev rule). */
 gchd_remember_std(d);
}

/* 0x88:0A for the decoder standard reported in 0x88:E7; 0 if unknown. */
static u8 gchd_dec_0a_value(u8 e7)
{
 switch (e7) {
 case 0x59: return 0x8f;
 case 0x69: return 0x98;
 case 0x79: return 0x85;
 }
 return 0;
}

/*
 * Wait until 0x88:E7 reports a known standard (at most about 1.6 s), read
 * 0x88:F1 and write the matching 0x88:0A.
 */
static int gchd_dec_write_standard(struct gchd *d)
{
 u8 v, val = 0;
 int i;

 for (i = 0; i < 40; ++i) {
  GCHD_DEC_R(d, 0xe7, v);
  val = gchd_dec_0a_value(v);
  if (val)
   break;
  msleep(40);
 }
 GCHD_DEC_R(d, 0xf1, v);
 if (!val) {
  gchd_dbg(&d->intf->dev, "decoder standard not reported; 0x88:0A kept\n");
  return 0;
 }
 GCHD_DEC_W(d, 0x0a, val);
 return 0;
}

/*
 * Decoder apply block for a family (6 = 60 Hz, 7 = 50 Hz); 0x28 follows bit 3
 * of 0x88:CA (set: 88, clear: 8A).
 */
static u16 gchd_dec_apply_key(u8 family, u8 lock)
{
 return family << 8 | ((lock & 0x08) ? 0x88 : 0x8a);
}

static int gchd_dec_apply_family(struct gchd *d, u8 family, u8 lock)
{
 u8 v;

 d->dec_applied = gchd_dec_apply_key(family, lock);

 if (family == 7) {
  GCHD_DEC_W(d, 0x07, 0x9a); GCHD_DEC_W(d, 0x08, 0x90); GCHD_DEC_W(d, 0x09, 0xa8);
 } else {
  GCHD_DEC_W(d, 0x07, 0x8a); GCHD_DEC_W(d, 0x08, 0x9b); GCHD_DEC_W(d, 0x09, 0x7a);
 }
 GCHD_DEC_W(d, 0x28, (lock & 0x08) ? 0x88 : 0x8a);
 GCHD_DEC_R(d, 0xfd, v); GCHD_DEC_W(d, 0x06, 0x08);
 return 0;
}

/* Decoder apply block for a 50 Hz source: 576i (0x28 = 88) or 288p (8A). */
static int gchd_pal_apply(struct gchd *d, bool interlaced)
{
 u8 v;

 GCHD_DEC_W(d, 0x07, 0x9a); GCHD_DEC_W(d, 0x08, 0x90); GCHD_DEC_W(d, 0x09, 0xa8);
 GCHD_DEC_W(d, 0x28, interlaced ? 0x88 : 0x8a);
 GCHD_DEC_R(d, 0xfd, v); GCHD_DEC_W(d, 0x06, 0x08);
 return 0;
}

/*
 * 576i <-> 288p while capturing: decoder apply block, then one back-end
 * retrigger. The encoder keeps running.
 */
int gchd_analog_poll_backend(struct gchd *d)
{
 u8 v;

 GCHD_BE_R(d, GCHD_BE_REG_STATUS, v);
 if (!(v & GCHD_BE_STATUS_FLAG))
  return 0;
 return gchd_be_retrigger(d);
}

int gchd_analog_pal_mode_change(struct gchd *d, bool interlaced)
{
 u8 v;
 int r;

 r = gchd_pal_apply(d, interlaced);
 if (r)
  return r;
 msleep(430);
 GCHD_DEC_R(d, 0xe7, v);
 GCHD_DEC_R(d, 0xf1, v);
 return gchd_be_retrigger(d);
}

static int gchd_configure_composite_exact(struct gchd *d)
{
 u8 v;
 int r;

 /* Tuning is performed before source detection so 240p/288p can lock. */

 /* Composite and S-Video share the assumed 0x4E:0x51=0xEC route.
  * Select the decoder's distinct luma/chroma input mode separately. */
 GCHD_DEC_W(d, 0x02, d->input == 3 ? 0xdc : 0xc9);
 d->dec_svideo = d->input == 3;
 GCHD_DEC_W(d, 0x3c, 0x6b);

 r = gchd_audio_put(d); if (r) return r;
 GCHD_DEC_R(d, GCHD_DEC_REG_FAMILY, v);
 v &= 0x0f;
 d->signal_present = (v == 6 || v == 7);
 if (v == 6) {
  d->input_width=720; d->input_height=480;
  /* Preserve the 240p/480i decision made by gchd_probe_video_source(). */
  d->input_fps_num=60; d->input_fps_den=1;
 }
 else if (v == 7) {
  d->input_width=720; d->input_height=576;
  /* Preserve the 288p/576i decision made by gchd_probe_video_source(). */
  d->input_fps_num=50; d->input_fps_den=1;
 }
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

 if (d->input_height == 480) {
  GCHD_DEC_W(d, 0x07, 0x8a); GCHD_DEC_W(d, 0x08, 0x9b); GCHD_DEC_W(d, 0x09, 0x7a);
  GCHD_DEC_W(d, 0x28, d->input_interlaced ? 0x88 : 0x8a);
  GCHD_DEC_R(d, 0xfd, v); GCHD_DEC_W(d, 0x06, 0x08);
 } else if (d->input_height == 576 && !d->input_interlaced) {
  /*
   * 288p: apply block with 0x28 = 8A, about 0.4 s, then 0x88:E7 and F1
   * before the back-end table. A 576i source needs no apply block.
   */
  r = gchd_pal_apply(d, false);
  if (r)
   return r;
  msleep(400);
  GCHD_DEC_R(d, 0xe7, v);
  GCHD_DEC_R(d, 0xf1, v);
 }
 if (!d->input_interlaced && d->input_height == 480) {
  /* 240p: retrigger, wait for the decoder standard, then 0x88:0A. */
  int i;

  GCHD_DEC_R(d, 0xe7, v);
  r = gchd_be_retrigger(d);
  if (r)
   return r;
  for (i = 0; i < 40; ++i) {
   GCHD_DEC_R(d, GCHD_DEC_REG_FAMILY, v);
   GCHD_DEC_R(d, GCHD_DEC_REG_LOCK, v);
   GCHD_DEC_R(d, 0xe7, v);
   if (gchd_dec_0a_value(v))
    break;
   msleep(40);
  }
  r = gchd_dec_write_standard(d);
  if (r)
   return r;
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

 if (d->input < 2 || (d->input_height != 480 && d->input_height != 576))
  return -EOPNOTSUPP;
 GCHD_DEC_R(d, GCHD_DEC_REG_LOCK, status);

 switch (status) {
 case GCHD_DEC_LOCK_240P:
  *interlaced = false;
  break;
 case GCHD_DEC_LOCK_480I:
 case GCHD_DEC_LOCK_TO_480I:
  *interlaced = true;
  break;
 case GCHD_DEC_LOCK_TO_240P:
  /* Leaving 480i: pulse the back end once and wait for 240p lock. */
  if (d->input_interlaced && !d->mode_retriggered) {
   int r = gchd_be_retrigger(d);

   if (r)
    return r;
   d->mode_retriggered = true;
  }
  return -EAGAIN;
 default:
  /* FE is unlocked; EE/E6 are transitional. */
  return -EAGAIN;
 }
 d->mode_retriggered = false;
 return 0;
}

/*
 * Mode change while capturing, first part (encoder still in START): apply
 * the decoder setting for the new mode and load the neutral back-end table.
 */
/* Neutral back-end table, the common block, then one retrigger. */
static int gchd_be_neutral(struct gchd *d)
{
 static const struct gchd_mail_cmd table_neutral[] = {
  GCHD_SEQ_BE(0x70, 0xc8),
  GCHD_SEQ_BE(0x90, 0x88), GCHD_SEQ_BE(0x91, 0x77), GCHD_SEQ_BE(0x92, 0x77),
  GCHD_SEQ_BE(0x93, 0x77), GCHD_SEQ_BE(0x94, 0x77), GCHD_SEQ_BE(0x95, 0x77),
  GCHD_SEQ_BE(0x96, 0x77), GCHD_SEQ_BE(0x97, 0x77), GCHD_SEQ_BE(0x98, 0x77),
  GCHD_SEQ_BE(0x99, 0x77), GCHD_SEQ_BE(0x9a, 0x77), GCHD_SEQ_BE(0x9b, 0x77),
  GCHD_SEQ_BE(0x9c, 0x77), GCHD_SEQ_BE(0x9d, 0x77), GCHD_SEQ_BE(0x9e, 0x77),
  GCHD_SEQ_BE(0x9f, 0x77), GCHD_SEQ_BE(0xa0, 0x77), GCHD_SEQ_BE(0xa1, 0x77),
  GCHD_SEQ_BE(0xa2, 0x77), GCHD_SEQ_BE(0xa3, 0x77),
 };
 u8 v;
 int r;

 GCHD_BE_W(d, 0x04, 0x95);
 GCHD_BE_R(d, 0x8b, v);
 r = gchd_mail_seq(d, table_neutral, ARRAY_SIZE(table_neutral));
 if (r)
  return r;
 GCHD_BE_R(d, 0x6b, v);
 GCHD_BE_W(d, 0xc0, 0x80);
 GCHD_BE_W(d, 0xa2, 0x77);
 GCHD_BE_R(d, 0x58, v);
 GCHD_BE_W(d, 0xc0, 0x77);
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
 return gchd_be_retrigger(d);
}

int gchd_analog_mode_pre_stop(struct gchd *d, bool interlaced)
{
 u8 v;

 GCHD_DEC_W(d, 0x07, 0x8a); GCHD_DEC_W(d, 0x08, 0x9b); GCHD_DEC_W(d, 0x09, 0x7a);
 GCHD_DEC_W(d, 0x28, interlaced ? 0x88 : 0x8a);
 GCHD_DEC_R(d, 0xfd, v); GCHD_DEC_W(d, 0x06, 0x08);
 msleep(400);
 GCHD_DEC_R(d, 0xe7, v);
 return gchd_be_neutral(d);
}

/*
 * With a 50 Hz source the decoder can report the 60 Hz family with
 * 0x88:E7 = 59, or a standard still being evaluated (bit 7), for a few
 * seconds after the signal appears, before it reports 50 Hz. Returns true
 * while such a state should be waited out: at most GCHD_PAL60_HOLD_MS after
 * since; a source that stays at 60 Hz is used after that.
 */
bool gchd_analog_pal60_hold(struct gchd *d, u8 e7, unsigned long since,
                            bool *logged)
{
 if (e7 != 0x59 && !(e7 & 0x80))
  return false;
 if (time_after(jiffies, since + msecs_to_jiffies(GCHD_PAL60_HOLD_MS)))
  return false;
 if (!*logged)
  gchd_dbg(&d->intf->dev,
           "%s (0x88:E7=%02x); waiting up to %u ms\n",
           e7 == 0x59 ? "decoder reports 60 Hz, standard 59" :
                        "decoder standard not settled",
           e7, GCHD_PAL60_HOLD_MS);
 *logged = true;
 return true;
}

/*
 * Family change while capturing, first part (encoder still in START).
 * family/lock are the 0x88:FA low nibble and 0x88:CA just read. When the
 * decoder reports the other family, load its apply block and retrigger the
 * back end; otherwise (0x88:E7 re-evaluating) load the neutral back-end
 * table.
 */
int gchd_analog_family_pre_stop(struct gchd *d, u8 family, u8 lock)
{
 u8 v;
 int r;

 if ((family == 7 && d->input_height == 480) ||
     (family == 6 && d->input_height == 576)) {
  r = gchd_dec_apply_family(d, family, lock);
  if (r)
   return r;
  msleep(430);
  GCHD_DEC_R(d, 0xe7, v);
  GCHD_DEC_R(d, 0xf1, v);
  return gchd_be_retrigger(d);
 }
 d->dec_applied = 0;
 return gchd_be_neutral(d);
}

/*
 * Family change, second part (encoder in STOP): wait for the decoder to
 * lock, loading the apply block of the family it reports every 0.5 s while
 * it does not; with hold (the signal had dropped), wait out the 60 Hz
 * state with 0x88:E7 = 59 (gchd_analog_pal60_hold()). Then set the mode, load the apply block of the locked
 * family if needed, write 0x88:0A, load the back-end table and program the
 * encoder. START follows. -ENOLINK when no lock within about 4 s.
 */
int gchd_analog_family_reconfigure(struct gchd *d, bool hold)
{
 unsigned long start = jiffies;
 bool held = false;
 u8 fa, ca, family = 0, v;
 int r;

 for (;;) {
  GCHD_DEC_R(d, GCHD_DEC_REG_FAMILY, fa);
  GCHD_DEC_R(d, GCHD_DEC_REG_LOCK, ca);
  family = fa & 0x0f;
  if ((family == 6 || family == 7) && (ca == 0x7e || ca == 0x66)) {
   if (family != 6 || !hold)
    break;
   GCHD_DEC_R(d, 0xe7, v);
   if (!gchd_analog_pal60_hold(d, v, start, &held))
    break;
   msleep(250);
   continue;
  }
  if (time_after(jiffies, start + msecs_to_jiffies(4000))) {
   gchd_dbg(&d->intf->dev,
            "family change: decoder not locked (FA=%02x CA=%02x)\n", fa, ca);
   return -ENOLINK;
  }
  if (family == 6 || family == 7) {
   r = gchd_dec_apply_family(d, family, ca);
   if (r)
    return r;
   msleep(430);
   GCHD_DEC_R(d, 0xe7, v);
   r = gchd_be_retrigger(d);
   if (r)
    return r;
  } else {
   msleep(500);
  }
 }

 d->input_width = 720;
 d->input_height = family == 7 ? 576 : 480;
 d->input_fps_num = family == 7 ? 50 : 60;
 d->input_fps_den = 1;
 d->input_interlaced = ca == 0x7e;
 gchd_analog_note_std(d, family);

 /* Apply block of the locked family unless it is already loaded. */
 if (d->dec_applied != gchd_dec_apply_key(family, ca)) {
  r = gchd_dec_apply_family(d, family, ca);
  if (r)
   return r;
  msleep(430);
 }
 r = gchd_dec_write_standard(d);
 if (r)
  return r;
 r = gchd_analog_backend_table(d, family == 7);
 if (r) {
  dev_err(&d->intf->dev, "family change: back-end setup failed: %d\n", r);
  return r;
 }
 r = gchd_encoder_program(d);
 if (r)
  return r;
 d->input_prepared = false;
 d->input_configured = true;
 return 0;
}

static int gchd_configure_component_exact(struct gchd *d)
{
 u8 v;
 u32 sum6867=0,sum6665=0,count6867=0,count6665=0;
 u32 value6867,value6665;
 int r,i,j;

 r = gchd_audio_put(d); if (r) return r;
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
 r=gchd_do_enable(d,0,0); if(r)return r;
 GCHD_FE_W(d, GCHD_FE_REG_BANK, 0x4c); GCHD_FE_R(d, 0x3f, v); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc);
 GCHD_FE_W(d, 0x24, 0x0c); GCHD_FE_W(d, GCHD_FE_REG_BANK, 0xcc); GCHD_FE_R(d, 0x1e, v);
 GCHD_FE_W(d, 0x21, 0xc5); GCHD_FE_R(d, 0xae, v); GCHD_FE_W(d, 0x91, 0xc8); GCHD_FE_W(d, 0x0e, 0x8c);
 GCHD_FE_W(d, 0x11, 0xec); GCHD_FE_R(d, 0x6b, v); GCHD_FE_W(d, 0x54, 0xfc); GCHD_FE_R(d, 0x4e, v);
 GCHD_FE_W(d, 0x71, 0x4c); GCHD_FE_W(d, 0x04, 0xcc);

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
 GCHD_FE_W(d, 0x17, 0xc6);
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
 if(d->input_height==576 && d->input_interlaced) {
  GCHD_FE_W(d, 0xad, 0xcc); GCHD_FE_R(d, 0x8f, v); GCHD_FE_W(d, 0xb0, 0xf9); GCHD_FE_W(d, 0xb0, 0xf9); GCHD_FE_W(d, 0xb1, 0xcc);
 } else if(d->input_height==480 && d->input_interlaced) {
  GCHD_FE_W(d, 0xad, 0xcc); GCHD_FE_R(d, 0x8f, v); GCHD_FE_W(d, 0xb0, 0xf9); GCHD_FE_W(d, 0xb1, 0x4c);
 } else {
  GCHD_FE_W(d, 0xad, 0xc9); GCHD_FE_R(d, 0x8f, v); GCHD_FE_W(d, 0xb0, 0xe9);
 }

 /*
  * Back end for the mode (docs/component.md §5): mode table, the common
  * back-end values, then pulses until the status settles.
  */
 GCHD_FE_W(d, 0xab, 0xcc); GCHD_BE_W(d, 0x04, 0x95);
 r=gchd_setup_subblock_exact(d); if(r)return r;
 GCHD_BE_RX(d, 0x89); GCHD_BE_W(d, 0x72, 0x88); GCHD_BE_W(d, 0xc0, 0x88);
 GCHD_BE_W(d, 0x61, 0x98); GCHD_BE_W(d, 0x63, 0x89); GCHD_BE_W(d, 0x64, 0x96);
 GCHD_BE_W(d, 0x65, 0x88); GCHD_BE_RX(d, 0xff); GCHD_BE_W(d, 0x04, 0x8d);
 GCHD_BE_W(d, 0x62, 0x91);
 r=gchd_be_retrigger_settle(d); if(r)return r;
 GCHD_FE_W(d, 0x0f, 0xe4);

 r=gchd_color_space_exact(d); if(r)return r;
 if(d->input_height==480 && d->input_interlaced) { GCHD_FE_W(d, 0xb0, 0xf9); GCHD_FE_W(d, 0xb1, 0xcc); }

 /* Final front-end values per mode, then wait for the back end again. */
 GCHD_FE_R(d, 0x8d, v); GCHD_FE_R(d, 0x8a, v);
 {
  bool sd = d->input_height <= 576;
  u8 b5 = (d->input_height == 720 || (d->input_height == 1080 && d->input_interlaced)) ? 0xcc : 0xc4;
  u8 r03 = d->input_height == 1080 ? (d->input_interlaced ? 0x0c : 0x04) :
           d->input_height == 720 ? 0x8c : d->input_interlaced ? 0x8c : 0x84;
  u8 st;

  GCHD_FE_W(d, 0xb2, sd ? 0xcf : 0xcc); GCHD_FE_W(d, 0xb5, b5);
  GCHD_FE_W(d, 0x03, r03); GCHD_FE_W(d, 0x04, 0xcc); GCHD_FE_W(d, 0xe0, 0xcc);
  r = gchd_be_wait_status(d, 500, &st); if (r) return r;
  if (st != GCHD_BE_STATUS_SETTLED) {
   r = gchd_be_retrigger_settle(d); if (r) return r;
  }
 }
 GCHD_FE_W(d, 0x0f, 0xe4);

 return 0;

}

/*
 * Encoder programming common to all inputs, ending with SCMD INIT (mode
 * 0xa0) and a state readback; START follows separately.
 */
static int gchd_encoder_program(struct gchd *d)
{
 int r;

 /* Encoder programming issued before every capture start. */
 r = gchd_transcoder_start_block(d);
 if (r) {
  dev_err(&d->intf->dev, "capture setup: encoder programming failed: %d\n", r);
  return r;
 }
 gchd_dbg(&d->intf->dev, "capture setup: encoder programming completed\n");

 /*
  * Every input finishes with: encoder programming,
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
  gchd_dbg(&d->intf->dev,
           "capture setup: state after final SCMD_INIT=0x%04x\n", state);
 }
 return 0;
}

static int gchd_input_configure_mode(struct gchd *d)
{
 int r;

 if (d->input_configured)
  return 0;

 /*
  * A capture starts with the standard output; a scaled output is switched
  * to from the running capture by the mode worker.
  */
 d->out_active = GCHD_OUT_STANDARD;
 gchd_dbg(&d->intf->dev,
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
  gchd_dbg(&d->intf->dev, "capture setup: encoder start completed\n");
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
 gchd_dbg(&d->intf->dev, "capture setup: input configuration completed\n");

 /* HDMI/component: the output size the detected source allows, from the start. */
 if (gchd_hd(d)) {
  d->out_active = gchd_out_target(d);
  d->enc_dirty = false;
 }
 /* The bitrate settings of the output mode of the detected source. */
 gchd_bitrate_range(d);
 r = gchd_encoder_program(d);
 if (r)
  return r;

 /* A successfully configured input stays configured while hardware is idle. */
 d->input_prepared = false;
 d->input_configured = true;
 return 0;
}

/*
 * Mode change while capturing, second part (encoder in STOP): load the
 * back-end table for the new mode and program the encoder. START follows.
 */
int gchd_analog_mode_reconfigure(struct gchd *d)
{
 int r;

 r = gchd_analog_backend_table(d, d->input_height == 576);
 if (r) {
  dev_err(&d->intf->dev, "mode change: back-end setup failed: %d\n", r);
  return r;
 }
 r = gchd_encoder_program(d);
 if (r)
  return r;
 d->input_prepared = false;
 d->input_configured = true;
 return 0;
}

/*
 * Output size change or new encoder settings while capturing (encoder in
 * STOP), composite/S-video and component: program the encoder for the
 * output size in effect; no decoder, front-end or back-end writes. START
 * follows.
 */
int gchd_analog_output_reconfigure(struct gchd *d)
{
 int r;

 d->out_active = gchd_out_target(d);
 d->enc_dirty = false;
 r = gchd_encoder_program(d);
 if (r)
  return r;
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

/*
 * Analog sources can change between 480i and 240p while the input is being
 * configured. START does not complete while the back end is set up for the
 * other mode; returns -EAGAIN so that the caller configures again.
 */
/* Low nibble of 0x88:FA: 6 = 60 Hz, 7 = 50 Hz family; other values: none. */
int gchd_input_read_family(struct gchd *d, u8 *family)
{
 u8 v;

 GCHD_DEC_R(d, GCHD_DEC_REG_FAMILY, v);
 *family = v & 0x0f;
 return 0;
}

/* Log the analog decoder status registers. */
static int gchd_input_read_status(struct gchd *d, u8 *v)
{
 GCHD_DEC_R(d, GCHD_DEC_REG_FAMILY, v[0]);
 GCHD_DEC_R(d, GCHD_DEC_REG_LOCK, v[1]);
 GCHD_DEC_R(d, 0xe7, v[2]);
 GCHD_DEC_R(d, 0xf1, v[3]);
 return 0;
}

void gchd_input_log_status(struct gchd *d, const char *why)
{
 u8 v[4];

 if (gchd_input_read_status(d, v))
  return;
 gchd_dbg(&d->intf->dev, "%s: 0x88:FA=%02x CA=%02x E7=%02x F1=%02x\n",
          why, v[0], v[1], v[2], v[3]);
}

/* 0x88:CA (lock) and 0x88:E7 (standard). */
int gchd_input_read_lock(struct gchd *d, u8 *lock, u8 *std)
{
 GCHD_DEC_R(d, GCHD_DEC_REG_LOCK, *lock);
 GCHD_DEC_R(d, 0xe7, *std);
 return 0;
}

int gchd_input_check_mode(struct gchd *d)
{
 bool interlaced;
 u8 family;

 if (d->input < 2 || (d->input_height != 480 && d->input_height != 576))
  return 0;
 GCHD_DEC_R(d, GCHD_DEC_REG_FAMILY, family);
 family &= 0x0f;
 if ((family == 6 && d->input_height == 576) ||
     (family == 7 && d->input_height == 480)) {
  gchd_dbg(&d->intf->dev, "source changed to %s during setup; configuring again\n",
           family == 7 ? "50 Hz" : "60 Hz");
  return -EAGAIN;
 }
 if (gchd_input_read_analog_mode(d, &interlaced) ||
     interlaced == d->input_interlaced)
  return 0;
 gchd_dbg(&d->intf->dev,
          "source changed to %s during setup; configuring again\n",
          d->input_height == 576 ? (interlaced ? "576i" : "288p") :
          (interlaced ? "480i" : "240p"));
 return -EAGAIN;
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
  gchd_dbg(&d->intf->dev,
           "gchd-diag refusing START: no signal; input=%u mode=%ux%u interlaced=%u\n",
           d->input, d->input_width, d->input_height, d->input_interlaced);
  return -ENOLINK;
 }

 gchd_dbg(&d->intf->dev,
          "gchd-diag issuing START: input=%u mode=%ux%u fps=%u/%u interlaced=%u\n",
          d->input, d->input_width, d->input_height, d->input_fps_num,
          d->input_fps_den, d->input_interlaced);
 r = gchd_state_cmd(d, GCHD_SCMD_STATE_CHANGE, 0, GCHD_STATE_START, GCHD_STATE_START);
 if (r) {
  dev_err(&d->intf->dev, "capture start transition failed: %d\n", r);
  return r;
 }
 d->std_hold_until = jiffies + msecs_to_jiffies(1000);
 WRITE_ONCE(d->start_jiffies, jiffies);
 gchd_dbg(&d->intf->dev, "gchd-diag START transition completed\n");
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
