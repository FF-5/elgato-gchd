// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include "elgato_gchd.h"
#include "gchd_transcoder_fields.h"
#include "gchd_mailbox.h"

/*
 * The Fujitsu transcoder is programmed through two USB vendor requests:
 * SLSI writes a 16-bit register, SPARAM updates a masked bit-field in a
 * 32-bit register.
 */



static int gchd_transcoder_table_write(struct gchd *d, u16 address,
                                       const u8 *data, size_t len)
{
 size_t i;
 int r;

 for (i = 0; i < len; i += 2) {
  u16 value = (u16)data[i] << 8;
  if (i + 1 < len)
   value |= data[i + 1];
  r = gchd_slsi(d, address + i, value);
  if (r)
   return r;
 }
 return 0;
}

/*
 * The PMT is written in this order: the section-length word, the last three
 * words, then the remaining words from the start.
 */
static int gchd_transcoder_pmt_write(struct gchd *d, u16 address,
                                     const u8 *data, size_t len)
{
 int r;

 r = gchd_transcoder_table_write(d, address + 2, data + 2, 2);
 if (r)
  return r;
 r = gchd_transcoder_table_write(d, address + len - 6, data + len - 6, 6);
 if (r)
  return r;
 r = gchd_transcoder_table_write(d, address, data, 2);
 if (r)
  return r;
 return gchd_transcoder_table_write(d, address + 4, data + 4, len - 10);
}

/* The PSI tables are written without CRC; the transcoder adds it. */
static const u8 gchd_pat[] = {
 0x00,0xb0,0x11,0x00,0x00,0xc1,0x00,0x00,
 0x00,0x01,0xe1,0x10,0x00,0x00,0xe0,0x1f
};
static const u8 gchd_sit[] = {
 0x7f,0xf0,0x0b,0xff,0xff,0xc1,0x00,0x00,0xf0,0x00
};
/*
 * Program 1: PCR PID 0x0100, H.264 video on 0x1011 (with registration and
 * AVC descriptors), MPEG-1 audio on 0x010F.
 */
static const u8 gchd_pmt[] = {
 0x02,0xb0,0x37,0x00,0x01,0xc1,0x00,0x00,
 0xe1,0x00,0xf0,0x0c,0x05,0x04,0x48,0x44,0x4d,0x56,
 0x88,0x04,0x0f,0xff,0xfc,0xfc,
 0x1b,0xf0,0x11,0xf0,0x14,0x05,0x08,0x48,0x44,0x4d,0x56,
 0xff,0x1b,0x44,0x3f,0x28,0x04,0x64,0x00,0x28,0x3f,0x2a,0x02,0x7e,0xff,
 0x03,0xe1,0x0f,0xf0,0x00
};

static int gchd_transcoder_defaults(struct gchd *d)
{
 int r;

 /* bank 0 */
 r = gchd_slsi(d, 0x0000, 0x0000);
 if (r) return r;

 GCHD_TC_SET(d, GCHD_TC_V_VINPELCLK, 0); /* v_vinpelclk */
 GCHD_TC_SET(d, GCHD_TC_TBC_MODE, 1); /* tbc_mode */
 /* FIXME: stout_mode is bit 15 of 0x1004; this writes bit 0 (stoutclk). */
 GCHD_TC_SET(d, 0x1004, 0, 1, 0); /* stout_mode */
 GCHD_TC_SET(d, GCHD_TC_EXTRA_INFO_BIT, 0);
 GCHD_TC_SET(d, GCHD_TC_TSOUT_PACKET_SIZE, 0);
 GCHD_TC_SET(d, GCHD_TC_STOUTCLK_HL, 0);
 GCHD_TC_SET(d, GCHD_TC_DMA_SEL_OUT, 1);
 GCHD_TC_SET(d, GCHD_TC_STOUTCLK_IO, 0);
 GCHD_TC_SET(d, GCHD_TC_STOUTCLK, 4);

 GCHD_TC_SET(d, GCHD_TC_STOUT2_MODE, 0);
 GCHD_TC_SET(d, GCHD_TC_STOUT2CLK_HL, 0);
 GCHD_TC_SET(d, GCHD_TC_STOUT2CLK_IO, 0);

 GCHD_TC_SET(d, GCHD_TC_PCR_FIRST, 0);
 GCHD_TC_SET(d, GCHD_TC_PCR_FIRST_TS_OUT2, 0);
 GCHD_TC_SET(d, GCHD_TC_AUTO_NULL, 2);
 GCHD_TC_SET(d, GCHD_TC_AUTO_NULL_TS_OUT2, 2);

 GCHD_TC_SET(d, GCHD_TC_SYSTEM_RATE, 25000);
 GCHD_TC_SET(d, GCHD_TC_SYSTEM_RATE_TS_OUT2, 8000);
 GCHD_TC_SET(d, GCHD_TC_SYSTEM_MIN_RATE, 0);
 GCHD_TC_SET(d, GCHD_TC_SYSTEM_MIN_RATE_TS_OUT2, 0);

 /* Fixed transport/PES identifiers. */
 GCHD_TC_SET(d, GCHD_TC_V_PID_OUT, GCHD_TS_PID_VIDEO);
 GCHD_TC_SET(d, GCHD_TC_V_PID_OUT_TS_OUT2, GCHD_TS_PID_VIDEO);
 GCHD_TC_SET(d, GCHD_TC_A_PID_OUT, GCHD_TS_PID_AUDIO);
 GCHD_TC_SET(d, GCHD_TC_A_PID_OUT_TS_OUT2, GCHD_TS_PID_AUDIO);
 GCHD_TC_SET(d, GCHD_TC_PMT_PID_OUT, GCHD_TS_PID_PMT);
 GCHD_TC_SET(d, GCHD_TC_PMT_PID_OUT_TS_OUT2, GCHD_TS_PID_PMT);
 GCHD_TC_SET(d, GCHD_TC_PCR_PID_OUT, GCHD_TS_PID_PCR);
 GCHD_TC_SET(d, GCHD_TC_PCR_PID_OUT_TS_OUT2, GCHD_TS_PID_PCR);
 GCHD_TC_SET(d, GCHD_TC_SIT_PID_OUT, GCHD_TS_PID_SIT);
 GCHD_TC_SET(d, GCHD_TC_PAT_LENGTH, sizeof(gchd_pat));
 GCHD_TC_SET(d, GCHD_TC_PMT_LENGTH, sizeof(gchd_pmt));
 GCHD_TC_SET(d, GCHD_TC_SIT_LENGTH, sizeof(gchd_sit));
 GCHD_TC_SET(d, GCHD_TC_A_TS_OUT_MODE, 2);
 GCHD_TC_SET(d, GCHD_TC_A_MODE, 3);
 r = gchd_transcoder_table_write(d, 0x1144, gchd_pat, sizeof(gchd_pat)); if (r) return r;
 r = gchd_transcoder_pmt_write(d, 0x1174, gchd_pmt, sizeof(gchd_pmt)); if (r) return r;
 r = gchd_transcoder_table_write(d, 0x1010, gchd_sit, sizeof(gchd_sit)); if (r) return r;
 GCHD_TC_SET(d, GCHD_TC_PAT_LENGTH_TS_OUT2, sizeof(gchd_pat));
 GCHD_TC_SET(d, GCHD_TC_PMT_LENGTH_TS_OUT2, sizeof(gchd_pmt));
 GCHD_TC_SET(d, GCHD_TC_SIT_LENGTH_TS_OUT2, sizeof(gchd_sit));
 r = gchd_transcoder_table_write(d, 0x1284, gchd_pat, sizeof(gchd_pat)); if (r) return r;
 r = gchd_transcoder_pmt_write(d, 0x12b4, gchd_pmt, sizeof(gchd_pmt)); if (r) return r;
 r = gchd_transcoder_table_write(d, 0x1370, gchd_sit, sizeof(gchd_sit)); if (r) return r;
 GCHD_TC_SET(d, GCHD_TC_SIT_PID_OUT_TS_OUT2, GCHD_TS_PID_SIT);
 GCHD_TC_SET(d, GCHD_TC_THUMBNAIL_PID_OUT_TS_OUT2, GCHD_TS_PID_THUMBNAIL);
 GCHD_TC_SET(d, GCHD_TC_V_SID_OUT, GCHD_TS_SID_VIDEO);
 GCHD_TC_SET(d, GCHD_TC_V_SID_OUT_TS_OUT2, GCHD_TS_SID_VIDEO);
 GCHD_TC_SET(d, GCHD_TC_A_SID_OUT, GCHD_TS_SID_AUDIO);
 GCHD_TC_SET(d, GCHD_TC_A_SID_OUT_TS_OUT2, GCHD_TS_SID_AUDIO);

 GCHD_TC_SET(d, GCHD_TC_PAT_CYC, 0);
 GCHD_TC_SET(d, GCHD_TC_PAT_CYC_TS_OUT2, 0);
 GCHD_TC_SET(d, GCHD_TC_PMT_CYC, 0);
 GCHD_TC_SET(d, GCHD_TC_PMT_CYC_TS_OUT2, 0);
 GCHD_TC_SET(d, GCHD_TC_PCR_CYC, 0);
 GCHD_TC_SET(d, GCHD_TC_PCR_CYC_TS_OUT2, 0);
 GCHD_TC_SET(d, GCHD_TC_SIT_CYC, 0);
 GCHD_TC_SET(d, GCHD_TC_SIT_CYC_TS_OUT2, 0);

 /* H.264 encoder defaults. */
 GCHD_TC_SET(d, GCHD_TC_V_V420, 1);
 GCHD_TC_SET(d, GCHD_TC_VIN_SWAP, 1);
 GCHD_TC_SET(d, GCHD_TC_V_FORMAT, 4);
 GCHD_TC_SET(d, GCHD_TC_TFRFF, 2);
 GCHD_TC_SET(d, GCHD_TC_V_EOS, 0);
 GCHD_TC_SET(d, GCHD_TC_V_CL_GOP, 0);
 GCHD_TC_SET(d, GCHD_TC_V_TEMPORAL_DIRECT_ON, 1);
 GCHD_TC_SET(d, GCHD_TC_V_VLC_MODE, 0);
 GCHD_TC_SET(d, GCHD_TC_V_FILLER, 0);
 GCHD_TC_SET(d, GCHD_TC_V_RATE_MODE, 1);
 GCHD_TC_SET(d, GCHD_TC_V_GOP_STRUCT, 0);
 GCHD_TC_SET(d, GCHD_TC_V_GOP_SIZE, 15);
 GCHD_TC_SET(d, GCHD_TC_V_IDR_INTERVAL_H, 0);
 GCHD_TC_SET(d, GCHD_TC_V_IDR_INTERVAL_L, 0);
 GCHD_TC_SET(d, GCHD_TC_V_H264_LEVEL, 41);
 GCHD_TC_SET(d, GCHD_TC_V_H264_PROFILE, 1);
 GCHD_TC_SET(d, GCHD_TC_V_HSIZE_OUT, 1920);
 GCHD_TC_SET(d, GCHD_TC_V_VSIZE_OUT, 1080);
 GCHD_TC_SET(d, GCHD_TC_V_BITRATE, 16000);
 GCHD_TC_SET(d, GCHD_TC_V_MAX_BITRATE, 14000);
 GCHD_TC_SET(d, GCHD_TC_V_AVE_BITRATE, 8000);
 GCHD_TC_SET(d, GCHD_TC_V_MIN_BITRATE, 0);
 GCHD_TC_SET(d, GCHD_TC_V_PIC_ORDER_PRESENT_FLAG, 1);
 GCHD_TC_SET(d, GCHD_TC_INSERT_RECOVERY_POINT_SEI, 1);
 GCHD_TC_SET(d, GCHD_TC_INSERT_PIC_STRUCT, 1);
 GCHD_TC_SET(d, GCHD_TC_INSERT_BUF_CTRL_PARAM, 1);
 GCHD_TC_SET(d, GCHD_TC_INSERT_RESTRICTION, 1);
 GCHD_TC_SET(d, GCHD_TC_OVERSCAN_APPROPRIATE_FLAG, 1);
 GCHD_TC_SET(d, GCHD_TC_OVERSCAN_INFO_PRESENT_FLAG, 1);
 GCHD_TC_SET(d, GCHD_TC_VIDEO_FULL_RANGE_FLAG, 0);
 GCHD_TC_SET(d, GCHD_TC_VIDEO_FORMAT, 5);
 GCHD_TC_SET(d, GCHD_TC_VIDEO_SIGNAL_TYPE_PRESENT_FLAG, 1);
 GCHD_TC_SET(d, GCHD_TC_COLOUR_PRIMARIES, 1);
 GCHD_TC_SET(d, GCHD_TC_COLOUR_DESCRIPTION_PRESENT_FLAG, 1);
 GCHD_TC_SET(d, GCHD_TC_MATRIX_COEFFICIENTS, 1);
 GCHD_TC_SET(d, GCHD_TC_TRANSFER_CHARACTERISTICS, 1);
 GCHD_TC_SET(d, GCHD_TC_TIME_SCALE_H, 0);
 GCHD_TC_SET(d, GCHD_TC_TIME_SCALE_L, 0);
 GCHD_TC_SET(d, GCHD_TC_NUM_UNITS_IN_TICK, 0);

 /* Audio defaults. */
 GCHD_TC_SET(d, GCHD_TC_A_SAMPLE, 0);
 GCHD_TC_SET(d, GCHD_TC_A_BITRATE, 384);
 GCHD_TC_SET(d, GCHD_TC_ACH_SEL, 0);
 GCHD_TC_SET(d, GCHD_TC_AMCK_MODE, 1);
 GCHD_TC_SET(d, GCHD_TC_AIN_DLENGTH, 2);
 GCHD_TC_SET(d, GCHD_TC_AIN_LR_HLSET, 1);
 GCHD_TC_SET(d, GCHD_TC_AIN_I2S, 1);
 GCHD_TC_SET(d, GCHD_TC_E_MPEG_PROTECT, 0);
 GCHD_TC_SET(d, GCHD_TC_E_MPEG_MODE, 0);
 GCHD_TC_SET(d, GCHD_TC_E_MPEG_COPYR, 0);
 GCHD_TC_SET(d, GCHD_TC_E_MPEG_ORIG, 1);
 GCHD_TC_SET(d, GCHD_TC_E_MPEG_EMP, 0);

 /* Disable the secondary output path. */
 GCHD_TC_SET(d, GCHD_TC_V_TS_OUT2_MODE, 0);
 GCHD_TC_SET(d, GCHD_TC_V_TS_OUT_MODE, 1);
 GCHD_TC_SET(d, GCHD_TC_SOUT2_MODE, 0);
 GCHD_TC_SET(d, GCHD_TC_SOUT_MODE, 0);
 GCHD_TC_SET(d, GCHD_TC_A_TS_OUT2_MODE, 0);
 GCHD_TC_SET(d, GCHD_TC_A_TS_OUT_MODE, 2);
 GCHD_TC_SET(d, GCHD_TC_A2_MODE, 0);
 GCHD_TC_SET(d, GCHD_TC_A_MODE, 3);

 return 0;
}

/*
 * Common transcoder stage only. Input-specific signal-lock sequences run
 * from the V4L2 input/mode paths. Until a mode is selected, the encoder is
 * left stopped.
 */

/* Encoder parameter stage. One encoder profile/rate tuple is exposed;
 * the values are derived from the active capture mode. */
/*
 * Analog inputs (composite, S-video) are always encoded as interlaced
 * 720x480/576 with v_in_source_type 3, also for 240p/288p sources: the
 * progressive source is carried as one picture per field.
 */
static bool gchd_enc_interlaced(const struct gchd *d)
{
 return d->input >= 2 ? true : d->input_interlaced;
}

static u8 gchd_enc_source_type(const struct gchd *d)
{
 /* Composite and S-video: 3, or 2 with "stretch" (composite.md §10). */
 if (d->input >= 2)
  return d->stretch ? 2 : 3;
 /* HDMI/component: 3 for standard-definition sources. */
 return d->input_height <= 576 ? 3 : 0;
}

struct gchd_enc_cfg {
 u16 width, height, bitrate, audio, gop, anchor, colour;
 u32 max_bitrate, ave_bitrate, min_bitrate, system_rate;
 u8 profile, level, vformat;
 u8 out;   /* GCHD_OUT_STANDARD, GCHD_OUT_640X480 or GCHD_OUT_MOBILE */
 bool vbr; /* v_rate_mode 1, v_max_cpb_delay 10 */
 bool vinpelclk;
 /* Stream timing (time_scale / num_units_in_tick; 0: none) of the main
  * and the _bl stream: frames encoded at half the source rate. */
 u16 ts, tick, ts_bl, tick_bl;
};

/*
 * Encoder programming issued before every capture start: 120 field writes
 * in a fixed order. Values that depend on the mode come from @c.
 */
static int gchd_transcoder_write_start(struct gchd *d,
                                       const struct gchd_enc_cfg *c)
{
 bool hdnew = d->family == GCHD_FAMILY_HDNEW;
 /* The small output sizes are encoded progressive on transport output 2. */
 bool out2 = gchd_out_ts2(c->out);
 bool vbr = c->vbr;
 /* Scaled outputs are encoded progressive. */
 u8 ip_format = c->out != GCHD_OUT_STANDARD || !gchd_enc_interlaced(d) ? 2 : 0;
 u8 cpb_delay = vbr ? 10 : 3;

 GCHD_TC_SET(d, GCHD_TC_V_PID_OUT, GCHD_TS_PID_VIDEO);
 GCHD_TC_SET(d, GCHD_TC_V_PID_OUT_TS_OUT2, GCHD_TS_PID_VIDEO);
 GCHD_TC_SET(d, GCHD_TC_A_PID_OUT, GCHD_TS_PID_AUDIO);
 GCHD_TC_SET(d, GCHD_TC_A_PID_OUT_TS_OUT2, GCHD_TS_PID_AUDIO);
 GCHD_TC_SET(d, GCHD_TC_DMA_SEL_OUT, 1);
 GCHD_TC_SET(d, GCHD_TC_V_IN_SOURCE_TYPE, gchd_enc_source_type(d));
 GCHD_TC_SET(d, GCHD_TC_V_FORMAT, c->vformat);
 GCHD_TC_SET(d, GCHD_TC_V_IP_FORMAT, ip_format);
 GCHD_TC_SET(d, GCHD_TC_TFRFF, 2);
 GCHD_TC_SET(d, GCHD_TC_TBC_MODE, 1);
 GCHD_TC_SET(d, GCHD_TC_TBC_MODE, 1);
 GCHD_TC_SET(d, GCHD_TC_A_SAMPLE, 0);
 GCHD_TC_SET(d, GCHD_TC_AIN_MASTER, 0);
 GCHD_TC_SET(d, GCHD_TC_AIN_LSB_FIRST, 0);
 GCHD_TC_SET(d, GCHD_TC_AIN_BIT_POSB, 0);
 GCHD_TC_SET(d, GCHD_TC_AMCK_MODE, 1);
 GCHD_TC_SET(d, GCHD_TC_AIN_DLENGTH, 2);
 GCHD_TC_SET(d, GCHD_TC_AIN_LR_HLSET, 1);
 GCHD_TC_SET(d, GCHD_TC_AIN_I2S, 1);
 GCHD_TC_SET(d, GCHD_TC_AIN_LRCLK_SEL, 0);
 GCHD_TC_SET(d, GCHD_TC_AIN_SCLK_SEL, 0);
 GCHD_TC_SET(d, GCHD_TC_AIN_BCLK_SEL, 0);
 GCHD_TC_SET(d, GCHD_TC_AIN_BCLK_HL, 0);
 GCHD_TC_SET(d, GCHD_TC_AIN_OFFSET_MODE, 0);
 GCHD_TC_SET(d, GCHD_TC_AIN_OFFSET, 0);
 GCHD_TC_SET(d, GCHD_TC_V_DISABLE_ASPECT_RATIO_INFO_PRESENT_FLAG, 0);
 GCHD_TC_SET(d, GCHD_TC_V_DISABLE_ASPECT_RATIO_INFO_PRESENT_FLAG_BL, 0);
 GCHD_TC_SET(d, GCHD_TC_V_PIC_ORDER_PRESENT_FLAG, 1);
 GCHD_TC_SET(d, GCHD_TC_V_PIC_ORDER_PRESENT_FLAG_BL, 1);

 if (hdnew)
  GCHD_TC_SET(d, 0x1574, 15, 1, 0);
 GCHD_TC_SET(d, GCHD_TC_V_MAX_CPB_DELAY, cpb_delay);
 GCHD_TC_SET(d, GCHD_TC_SYSTEM_RATE, c->system_rate);
 GCHD_TC_SET(d, GCHD_TC_SYSTEM_MIN_RATE, 0);
 GCHD_TC_SET(d, GCHD_TC_V_CL_GOP, 1);
 GCHD_TC_SET(d, GCHD_TC_V_GOP_STRUCT, c->anchor);
 GCHD_TC_SET(d, GCHD_TC_V_GOP_SIZE, c->gop);
 GCHD_TC_SET(d, GCHD_TC_V_VLC_MODE, 0);
 GCHD_TC_SET(d, GCHD_TC_V_VBR_CONVERGE_MODE, 0);
 GCHD_TC_SET(d, GCHD_TC_V_RATE_MODE, vbr);
 GCHD_TC_SET(d, GCHD_TC_V_BITRATE, c->bitrate);
 GCHD_TC_SET(d, GCHD_TC_V_MAX_BITRATE, c->max_bitrate);
 GCHD_TC_SET(d, GCHD_TC_V_AVE_BITRATE, c->ave_bitrate);
 GCHD_TC_SET(d, GCHD_TC_V_MIN_BITRATE, c->min_bitrate);
 GCHD_TC_SET(d, GCHD_TC_V_HSIZE_OUT, c->width);
 GCHD_TC_SET(d, GCHD_TC_V_VSIZE_OUT, c->height);
 GCHD_TC_SET(d, GCHD_TC_V_VINPELCLK, c->vinpelclk);
 GCHD_TC_SET(d, GCHD_TC_TIME_SCALE_H, 0);
 GCHD_TC_SET(d, GCHD_TC_TIME_SCALE_L, c->ts);
 GCHD_TC_SET(d, GCHD_TC_NUM_UNITS_IN_TICK, c->tick);
 GCHD_TC_SET(d, GCHD_TC_V_IDR_INTERVAL_H, 0);
 GCHD_TC_SET(d, GCHD_TC_V_IDR_INTERVAL_L, 0);
 GCHD_TC_SET(d, 0x151c, 0, 5, 0);
 GCHD_TC_SET(d, GCHD_TC_V_H264_PROFILE, c->profile);
 GCHD_TC_SET(d, GCHD_TC_V_H264_LEVEL, c->level);
 GCHD_TC_SET(d, GCHD_TC_A_SAMPLE, 0);
 GCHD_TC_SET(d, GCHD_TC_E_MPEG_PROTECT, 0);
 GCHD_TC_SET(d, GCHD_TC_E_MPEG_MODE, 0);
 GCHD_TC_SET(d, GCHD_TC_E_MPEG_COPYR, 0);
 GCHD_TC_SET(d, GCHD_TC_E_MPEG_ORIG, 1);
 GCHD_TC_SET(d, GCHD_TC_E_MPEG_EMP, 0);
 GCHD_TC_SET(d, GCHD_TC_A_BITRATE, c->audio);
 GCHD_TC_SET(d, GCHD_TC_COLOUR_DESCRIPTION_PRESENT_FLAG, 1);
 GCHD_TC_SET(d, GCHD_TC_COLOUR_PRIMARIES, c->colour);
 GCHD_TC_SET(d, GCHD_TC_TRANSFER_CHARACTERISTICS, 1);
 GCHD_TC_SET(d, GCHD_TC_MATRIX_COEFFICIENTS, c->colour);
 GCHD_TC_SET(d, GCHD_TC_VIDEO_FULL_RANGE_FLAG, 0);
 GCHD_TC_SET(d, GCHD_TC_VIDEO_SIGNAL_TYPE_PRESENT_FLAG, 1);
 GCHD_TC_SET(d, GCHD_TC_VIDEO_FORMAT, 5);
 GCHD_TC_SET(d, GCHD_TC_VIDEO_FULL_RANGE_FLAG, 0);

 if (hdnew)
  GCHD_TC_SET(d, 0x1674, 15, 1, 0);
 GCHD_TC_SET(d, GCHD_TC_V_MAX_CPB_DELAY_BL, cpb_delay);
 GCHD_TC_SET(d, GCHD_TC_SYSTEM_RATE_TS_OUT2, c->system_rate);
 GCHD_TC_SET(d, GCHD_TC_SYSTEM_MIN_RATE_TS_OUT2, 0);
 GCHD_TC_SET(d, GCHD_TC_V_CL_GOP_BL, 1);
 GCHD_TC_SET(d, GCHD_TC_V_IP_FORMAT_BL, 2);
 GCHD_TC_SET(d, GCHD_TC_V_GOP_STRUCT_BL, c->anchor);
 GCHD_TC_SET(d, GCHD_TC_V_GOP_SIZE_BL, c->gop);
 GCHD_TC_SET(d, GCHD_TC_V_VLC_MODE_BL, 0);
 GCHD_TC_SET(d, GCHD_TC_V_RATE_MODE_BL, vbr);
 GCHD_TC_SET(d, GCHD_TC_V_VBR_CONVERGE_MODE_BL, 0);
 GCHD_TC_SET(d, GCHD_TC_V_BITRATE_BL, c->bitrate);
 GCHD_TC_SET(d, GCHD_TC_V_MAX_BITRATE_BL, c->max_bitrate);
 GCHD_TC_SET(d, GCHD_TC_V_AVE_BITRATE_BL, c->ave_bitrate);
 GCHD_TC_SET(d, GCHD_TC_V_MIN_BITRATE_BL, c->min_bitrate);
 GCHD_TC_SET(d, GCHD_TC_V_FILLER_BITRATE_BL, 0);
 GCHD_TC_SET(d, GCHD_TC_V_HSIZE_OUT_BL, c->width);
 GCHD_TC_SET(d, GCHD_TC_V_VSIZE_OUT_BL, c->height);
 GCHD_TC_SET(d, GCHD_TC_V_IP_FORMAT_BL, ip_format);
 GCHD_TC_SET(d, GCHD_TC_TFRFF, 2);
 GCHD_TC_SET(d, GCHD_TC_V_IDR_INTERVAL_H_BL, 0);
 GCHD_TC_SET(d, GCHD_TC_V_IDR_INTERVAL_L_BL, 0);
 GCHD_TC_SET(d, 0x161c, 0, 5, 0);
 GCHD_TC_SET(d, GCHD_TC_V_H264_PROFILE_BL, c->profile);
 GCHD_TC_SET(d, GCHD_TC_V_H264_LEVEL_BL, min_t(u8, c->level, 31));
 GCHD_TC_SET(d, GCHD_TC_TIME_SCALE_H_BL, 0);
 GCHD_TC_SET(d, GCHD_TC_TIME_SCALE_L_BL, c->ts_bl);
 GCHD_TC_SET(d, GCHD_TC_NUM_UNITS_IN_TICK_BL, c->tick_bl);
 GCHD_TC_SET(d, GCHD_TC_ACH_SEL_BL, 0);
 GCHD_TC_SET(d, GCHD_TC_E_MPEG_PROTECT_BL, 0);
 GCHD_TC_SET(d, GCHD_TC_E_MPEG_MODE_BL, 0);
 GCHD_TC_SET(d, GCHD_TC_E_MPEG_COPYR_BL, 0);
 GCHD_TC_SET(d, GCHD_TC_E_MPEG_ORIG_BL, 1);
 GCHD_TC_SET(d, GCHD_TC_E_MPEG_EMP_BL, 0);
 GCHD_TC_SET(d, GCHD_TC_A_BITRATE_BL, c->audio);
 GCHD_TC_SET(d, GCHD_TC_COLOUR_DESCRIPTION_PRESENT_FLAG_BL, 1);
 GCHD_TC_SET(d, GCHD_TC_COLOUR_PRIMARIES_BL, c->colour);
 GCHD_TC_SET(d, GCHD_TC_TRANSFER_CHARACTERISTICS_BL, 1);
 GCHD_TC_SET(d, GCHD_TC_MATRIX_COEFFICIENTS_BL, c->colour);
 GCHD_TC_SET(d, GCHD_TC_VIDEO_FULL_RANGE_FLAG_BL, 0);
 GCHD_TC_SET(d, GCHD_TC_VIDEO_SIGNAL_TYPE_PRESENT_FLAG_BL, 1);
 GCHD_TC_SET(d, GCHD_TC_VIDEO_FORMAT_BL, 5);
 GCHD_TC_SET(d, GCHD_TC_VIDEO_FULL_RANGE_FLAG_BL, 0);

 GCHD_TC_SET(d, GCHD_TC_SOUT_MODE, 0);
 GCHD_TC_SET(d, GCHD_TC_SOUT2_MODE, 0);
 GCHD_TC_SET(d, GCHD_TC_V_TS_OUT_MODE, out2 ? 0 : 1);
 GCHD_TC_SET(d, GCHD_TC_A_TS_OUT_MODE, out2 ? 0 : 2);
 GCHD_TC_SET(d, GCHD_TC_A_MODE, out2 ? 0 : 3);
 GCHD_TC_SET(d, GCHD_TC_V_TS_OUT2_MODE, out2 ? 1 : 0);
 GCHD_TC_SET(d, GCHD_TC_A_TS_OUT2_MODE, out2 ? 2 : 0);
 GCHD_TC_SET(d, GCHD_TC_A2_MODE, out2 ? 3 : 0);
 return 0;
}

static int gchd_transcoder_program(struct gchd *d, bool start)
{
 u8 out = d->out_active;
 u16 width = out == GCHD_OUT_640X480 ? 640 :
             out == GCHD_OUT_MOBILE ? 480 :
             out == GCHD_OUT_1280X720 ? 1280 : d->input_width;
 u16 height = out == GCHD_OUT_640X480 ? 480 :
              out == GCHD_OUT_MOBILE ? 360 :
              out == GCHD_OUT_1280X720 ? 720 : d->input_height;
 u16 src_height = d->input_height;
 u16 fps = d->input_fps_num ? d->input_fps_num : 60;
 /* 50 Hz family (also 25 frames per second sources). */
 bool hz50 = fps == 50 || fps == 25;
 u32 auto_bitrate;
 u16 bitrate;
 u16 audio = 320;
 u32 auto_max_bitrate;
 u32 auto_average_bitrate;
 u32 auto_min_bitrate;

 /* Default rate: 15625 kbit/s per 1000 pixels of width (11250 kbit/s at
  * 720), maximum 90 %, average 50 %, minimum 25 %. */
 auto_bitrate = min_t(u32, (15625U * width) / 1000U, 40000U);
 auto_max_bitrate = min_t(u32, (auto_bitrate * 9U) / 10U, 40000U);
 auto_average_bitrate = min_t(u32, auto_bitrate / 2U, 40000U);
 auto_min_bitrate = min_t(u32, auto_bitrate / 4U, 40000U);
 bitrate = d->bitrate_forced ? d->bitrate : (u16)auto_bitrate;

 /*
  * Composite and S-video: bitrate values as the capture application sets
  * them (composite.md §10): default 11250 kbit/s CBR (15000 VBR for the
  * mobile size), maximum 90 %, average 50 %, minimum 0 up to 4000, 10 %
  * below 10000, 25 % above; system_rate floor(1.0728 * B + 462.1) at
  * 50 Hz, + 470.6 at 60 Hz.
  */
 bool vbr = false;
 u32 analog_system_rate = 0;

 if (d->input >= 2) {
  bitrate = gchd_bitrate_eff(d);
  vbr = gchd_bitrate_vbr(d);
  auto_max_bitrate = bitrate * 9U / 10U;
  auto_average_bitrate = bitrate / 2U;
  auto_min_bitrate = bitrate <= 4000 ? 0 :
                     bitrate < 10000 ? bitrate / 10U : bitrate / 4U;
  analog_system_rate = ((u32)bitrate * 10728U +
                        (hz50 ? 4621000U : 4706000U)) / 10000U;
 }
 /* Scaled outputs: GOP 6 at 50 Hz, 8 at 60 Hz; else 12 / 14. */
 u16 gop = out != GCHD_OUT_STANDARD ? (hz50 ? 6 : 8) : (hz50 ? 12 : 14);
 /*
  * Component, per source mode and output size (docs/component.md §4).
  * Source size: 11250 kbit/s (480i/576i), 15000 (480p/576p), 26250 (720p
  * at 60 frames per second), 15000 (720p at 30, "Allow 50/60 fps" off),
  * 30000 (1080i/1080p); 1280 x 720 15000, 640 x 480 11250, 480 x 360
  * 15000. Maximum 90 %, average 50 %, minimum 0 up to 4000, 10 % below
  * 10000, 25 % above, 35 % from 26250. 60 Hz progressive sources encoded
  * at 30 frames per second (1080p30, 720p with the option off, 720p and
  * 1080p scaled outputs) are signalled 60000/1001.
  */
 bool comp = gchd_hd(d);
 bool hdmi = d->input == 0;
 bool comp_sd = comp && src_height <= 576;
 bool comp_sd_p = comp_sd && !d->input_interlaced;
 bool comp_half = false;	/* 60 Hz progressive source at 30 frames/s */
 bool comp_fast = false;	/* progressive output at 50/60 frames/s */
 u32 comp_system_rate = 0;
 u8 comp_level = 0;

 if (comp) {
  if (out == GCHD_OUT_STANDARD) {
   /* Component 1080p30 is signalled as half of 60 Hz; HDMI 1080p30/25
    * as a source of that rate. */
   comp_half = (src_height == 1080 && !d->input_interlaced &&
                (!hdmi || fps >= 50)) ||
               (src_height == 720 && !d->allow_5060);
   comp_fast = comp_sd_p || (src_height == 720 && !comp_half);
   if (comp_sd_p)
    /* "Allow 50/60 fps" off: GOP 12 (level 31); the stream stays at
     * 50/60 frames per second. */
    gop = d->allow_5060 ? (hz50 ? 6 : 8) : (hz50 ? 12 : 14);
   else if (comp_half || (src_height == 1080 && !d->input_interlaced))
    gop = hz50 ? 6 : 8;
   comp_level = src_height > 576 ? (comp_half && src_height == 720 ? 31 : 40) :
                comp_sd_p && d->allow_5060 ? 30 : 31;
  } else {
   /*
    * Scaled outputs: 60 Hz progressive sources (480p, 720p, 1080p30)
    * at 30 frames per second; for 480p/576p "Allow 50/60 fps" off
    * gives the long GOP (and level 31 at 640 x 480).
    */
   bool slow = comp_sd_p && !d->allow_5060;

   comp_half = !d->input_interlaced && (hdmi ? fps >= 50 : fps != 50);
   gop = comp_sd ? (hz50 ? (slow ? 12 : 6) : (slow ? 14 : 8)) : hz50 ? 6 : 8;
   comp_level = out == GCHD_OUT_640X480 ? (slow ? 31 : 30) :
                out == GCHD_OUT_MOBILE ? 22 : 31;
  }
  /*
   * Bitrate mode and bitrate as for composite: the mode set (480 x 360
   * VBR only), the bitrate kept for it when inside the output's range
   * (lowest 8/15 of the highest), else the highest.
   */
  bitrate = gchd_bitrate_eff(d);
  vbr = gchd_bitrate_vbr(d);
  auto_max_bitrate = bitrate * 9U / 10U;
  auto_average_bitrate = bitrate / 2U;
  auto_min_bitrate = bitrate <= 4000 ? 0 : bitrate < 10000 ? bitrate / 10U :
                     bitrate < 26250 ? bitrate / 4U : bitrate * 35U / 100U;
  /* floor(1.07285 * B + c): c 461.7 at 50 Hz, 470.2 at 60 Hz; progressive
   * output at 50/60 frames per second 504.7 / 521.5. */
  comp_system_rate = ((u32)bitrate * 21457U +
                      (hz50 ? (comp_fast ? (hdmi ? 10090000U : 10094000U) :
                                           (hdmi ? 9220000U : 9234000U)) :
                              (comp_fast ? (hdmi ? 10440000U : 10430000U) :
                                           9404000U))) / 20000U;
 }
 u16 anchor = 3;
 u8 profile;
 u8 level;

 /* High profile (encoder value 1); the level per input and output. */
 profile = 1;
 if (comp) {
  level = comp_level;
 } else if (out == GCHD_OUT_640X480) {
  level = 30;
 } else if (out == GCHD_OUT_MOBILE) {
  level = 22;
 } else if (d->input >= 2) {
  level = 31;
 } else {
  u32 frame_mb = (width * height) / (16U * 16U);
  u32 mbps = frame_mb * fps;
  if (mbps <= 1485 && frame_mb <= 99 && bitrate <= 64) level = 10;
  else if (mbps <= 3000 && frame_mb <= 396 && bitrate <= 192) level = 11;
  else if (mbps <= 6000 && frame_mb <= 396 && bitrate <= 384) level = 12;
  else if (mbps <= 11880 && frame_mb <= 396 && bitrate <= 768) level = 13;
  else if (mbps <= 11880 && frame_mb <= 396 && bitrate <= 2000) level = 20;
  else if (mbps <= 19800 && frame_mb <= 792 && bitrate <= 4000) level = 21;
  else if (mbps <= 20250 && frame_mb <= 1620 && bitrate <= 4000) level = 22;
  else if (mbps <= 40500 && frame_mb <= 1620 && bitrate <= 10000) level = 30;
  else if (mbps <= 108000 && frame_mb <= 3600 && bitrate <= 14000) level = 31;
  else if (mbps <= 216000 && frame_mb <= 5120 && bitrate <= 20000) level = 32;
  else if (mbps <= 245760 && frame_mb <= 8192 && bitrate <= 20000) level = 40;
  else if (mbps <= 245760 && frame_mb <= 8192 && bitrate <= 50000) level = 41;
  else if (mbps <= 522240 && frame_mb <= 8704 && bitrate <= 50000) level = 42;
  else if (mbps <= 589824 && frame_mb <= 22080 && bitrate <= 135000) level = 50;
  else if (mbps <= 983040 && frame_mb <= 36864 && bitrate <= 240000) level = 51;
  else if (mbps <= 2073600 && frame_mb <= 36864 && bitrate <= 240000) level = 52;
  else return -EINVAL;
 }

 u32 system_rate = (1075U * ((u32)bitrate + audio)) / 1000U + 256U;
 u16 colour = d->input == 0 ? 1 : 6;

 if (d->input >= 2)
  system_rate = analog_system_rate;
 if (comp) {
  system_rate = comp_system_rate;
  colour = src_height > 576 ? 1 : 6; /* BT.709 / SMPTE 170M */
 }
 u8 vformat;

 /* The input format code follows the source, also for scaled outputs. */
 if (src_height == 1080) vformat = gchd_enc_interlaced(d) ? 0 : 32;
 else if (src_height == 720) vformat = 2;
 else if (src_height == 576) vformat = gchd_enc_interlaced(d) ? 5 : 9;
 else if (src_height == 480) vformat = gchd_enc_interlaced(d) ? 4 : 8;
 else return -EINVAL;
 if (hz50) vformat |= 1;

 if (start) {
  struct gchd_enc_cfg c = {
   .width = width, .height = height, .bitrate = bitrate,
   .audio = audio, .gop = gop, .anchor = anchor, .colour = colour,
   .max_bitrate = auto_max_bitrate, .ave_bitrate = auto_average_bitrate,
   .min_bitrate = auto_min_bitrate, .system_rate = system_rate,
   .profile = profile, .level = level, .vformat = vformat,
   .out = out, .vbr = vbr,
   /* HDMI: always 1; composite/S-video and component with a 50 Hz
    * source. */
   .vinpelclk = hdmi || fps == 50,
   /* Half rate: component 60000/1001 on the _bl stream, HDMI the source
    * rate on both. */
   .ts = hdmi && comp_half ? (hz50 ? 50 : 60) : 0,
   .tick = hdmi && comp_half ? 1 : 0,
   .ts_bl = !comp_half ? 0 : hdmi ? (hz50 ? 50 : 60) : 60000,
   .tick_bl = !comp_half ? 0 : hdmi ? 1 : 1001,
  };

  return gchd_transcoder_write_start(d, &c);
 }

 if (d->family == GCHD_FAMILY_HDNEW) { GCHD_TC_SET(d, 0x1574, 15, 1,0); GCHD_TC_SET(d, GCHD_TC_V_MAX_CPB_DELAY,3); }
 GCHD_TC_SET(d, GCHD_TC_SYSTEM_RATE,system_rate); GCHD_TC_SET(d, GCHD_TC_SYSTEM_MIN_RATE,0);
 GCHD_TC_SET(d, GCHD_TC_V_CL_GOP,1); GCHD_TC_SET(d, GCHD_TC_V_GOP_STRUCT,anchor); GCHD_TC_SET(d, GCHD_TC_V_GOP_SIZE,gop);
 GCHD_TC_SET(d, GCHD_TC_V_VLC_MODE,0); GCHD_TC_SET(d, GCHD_TC_V_VBR_CONVERGE_MODE,0); GCHD_TC_SET(d, GCHD_TC_V_RATE_MODE,0);
 GCHD_TC_SET(d, GCHD_TC_V_BITRATE,bitrate); GCHD_TC_SET(d, GCHD_TC_V_MAX_BITRATE,auto_max_bitrate); GCHD_TC_SET(d, GCHD_TC_V_AVE_BITRATE,auto_average_bitrate); GCHD_TC_SET(d, GCHD_TC_V_MIN_BITRATE,auto_min_bitrate);
 GCHD_TC_SET(d, GCHD_TC_V_HSIZE_OUT,width); GCHD_TC_SET(d, GCHD_TC_V_VSIZE_OUT,height); GCHD_TC_SET(d, GCHD_TC_V_VINPELCLK,0);
 GCHD_TC_SET(d, GCHD_TC_TIME_SCALE_H,0); GCHD_TC_SET(d, GCHD_TC_TIME_SCALE_L,0); GCHD_TC_SET(d, GCHD_TC_NUM_UNITS_IN_TICK,0);
 GCHD_TC_SET(d, GCHD_TC_V_IDR_INTERVAL_H,0); GCHD_TC_SET(d, GCHD_TC_V_IDR_INTERVAL_L,0);
 GCHD_TC_SET(d, GCHD_TC_V_H264_LEVEL,level); GCHD_TC_SET(d, GCHD_TC_V_H264_PROFILE,profile);
 GCHD_TC_SET(d, GCHD_TC_A_SAMPLE,0); GCHD_TC_SET(d, GCHD_TC_E_MPEG_PROTECT,0); GCHD_TC_SET(d, GCHD_TC_E_MPEG_MODE,0); GCHD_TC_SET(d, GCHD_TC_E_MPEG_COPYR,0); GCHD_TC_SET(d, GCHD_TC_E_MPEG_ORIG,1); GCHD_TC_SET(d, GCHD_TC_E_MPEG_EMP,0); GCHD_TC_SET(d, GCHD_TC_A_BITRATE,audio);
 GCHD_TC_SET(d, GCHD_TC_COLOUR_DESCRIPTION_PRESENT_FLAG,1); GCHD_TC_SET(d, GCHD_TC_COLOUR_PRIMARIES,colour); GCHD_TC_SET(d, GCHD_TC_TRANSFER_CHARACTERISTICS,1); GCHD_TC_SET(d, GCHD_TC_MATRIX_COEFFICIENTS,colour); GCHD_TC_SET(d, GCHD_TC_VIDEO_FULL_RANGE_FLAG,0); GCHD_TC_SET(d, GCHD_TC_VIDEO_SIGNAL_TYPE_PRESENT_FLAG,1); GCHD_TC_SET(d, GCHD_TC_VIDEO_FORMAT,5);
 if (d->family == GCHD_FAMILY_HDNEW) { GCHD_TC_SET(d, 0x1674, 15, 1,0); GCHD_TC_SET(d, GCHD_TC_V_MAX_CPB_DELAY_BL,3); }
 GCHD_TC_SET(d, GCHD_TC_SYSTEM_RATE_TS_OUT2,system_rate); GCHD_TC_SET(d, GCHD_TC_SYSTEM_MIN_RATE_TS_OUT2,0);
 GCHD_TC_SET(d, GCHD_TC_V_CL_GOP_BL,1); GCHD_TC_SET(d, GCHD_TC_V_GOP_STRUCT_BL,anchor); GCHD_TC_SET(d, GCHD_TC_V_GOP_SIZE_BL,gop); GCHD_TC_SET(d, GCHD_TC_V_VLC_MODE_BL,0); GCHD_TC_SET(d, GCHD_TC_V_VBR_CONVERGE_MODE_BL,0); GCHD_TC_SET(d, GCHD_TC_V_RATE_MODE_BL,0);
 GCHD_TC_SET(d, GCHD_TC_V_BITRATE_BL,bitrate); GCHD_TC_SET(d, GCHD_TC_V_MAX_BITRATE_BL,auto_max_bitrate); GCHD_TC_SET(d, GCHD_TC_V_AVE_BITRATE_BL,auto_average_bitrate); GCHD_TC_SET(d, GCHD_TC_V_MIN_BITRATE_BL,auto_min_bitrate); GCHD_TC_SET(d, GCHD_TC_V_FILLER_BITRATE_BL,0);
 GCHD_TC_SET(d, GCHD_TC_V_HSIZE_OUT_BL,width); GCHD_TC_SET(d, GCHD_TC_V_VSIZE_OUT_BL,height); GCHD_TC_SET(d, GCHD_TC_V_IDR_INTERVAL_L_BL,0); GCHD_TC_SET(d, GCHD_TC_V_H264_LEVEL_BL,31); GCHD_TC_SET(d, GCHD_TC_V_H264_PROFILE_BL,profile);
 GCHD_TC_SET(d, GCHD_TC_TIME_SCALE_H_BL,0); GCHD_TC_SET(d, GCHD_TC_TIME_SCALE_L_BL,0); GCHD_TC_SET(d, GCHD_TC_NUM_UNITS_IN_TICK_BL,1);
 GCHD_TC_SET(d, GCHD_TC_E_MPEG_PROTECT_BL,0); GCHD_TC_SET(d, GCHD_TC_E_MPEG_MODE_BL,0); GCHD_TC_SET(d, GCHD_TC_E_MPEG_COPYR_BL,0); GCHD_TC_SET(d, GCHD_TC_E_MPEG_ORIG_BL,1); GCHD_TC_SET(d, GCHD_TC_E_MPEG_EMP_BL,0); GCHD_TC_SET(d, GCHD_TC_A_BITRATE_BL,audio);
 GCHD_TC_SET(d, GCHD_TC_COLOUR_DESCRIPTION_PRESENT_FLAG_BL,1); GCHD_TC_SET(d, GCHD_TC_COLOUR_PRIMARIES_BL,colour); GCHD_TC_SET(d, GCHD_TC_TRANSFER_CHARACTERISTICS_BL,1); GCHD_TC_SET(d, GCHD_TC_MATRIX_COEFFICIENTS_BL,colour); GCHD_TC_SET(d, GCHD_TC_VIDEO_FULL_RANGE_FLAG_BL,0); GCHD_TC_SET(d, GCHD_TC_VIDEO_SIGNAL_TYPE_PRESENT_FLAG_BL,1); GCHD_TC_SET(d, GCHD_TC_VIDEO_FORMAT_BL,5);
 /*
  * Both transport outputs enabled in the setup after the firmware INIT, as
  * recorded; a later START with only output 2 enabled does not complete
  * otherwise.
  */
 GCHD_TC_SET(d, GCHD_TC_V_TS_OUT_MODE,1); GCHD_TC_SET(d, GCHD_TC_A_MODE,3); GCHD_TC_SET(d, GCHD_TC_V_TS_OUT2_MODE,1); GCHD_TC_SET(d, GCHD_TC_A_TS_OUT_MODE,2); GCHD_TC_SET(d, GCHD_TC_SOUT2_MODE,0); GCHD_TC_SET(d, GCHD_TC_A_TS_OUT2_MODE,2); GCHD_TC_SET(d, GCHD_TC_A2_MODE,3);
 GCHD_TC_SET(d, GCHD_TC_V_IN_SOURCE_TYPE, gchd_enc_source_type(d)); GCHD_TC_SET(d, GCHD_TC_V_FORMAT,vformat); GCHD_TC_SET(d, GCHD_TC_V_IP_FORMAT, gchd_enc_interlaced(d) ? 0 : 2); GCHD_TC_SET(d, GCHD_TC_TBC_MODE,1); GCHD_TC_SET(d, GCHD_TC_TBC_MODE,1);
 GCHD_TC_SET(d, GCHD_TC_A_SAMPLE,0); GCHD_TC_SET(d, GCHD_TC_AIN_MASTER,0); GCHD_TC_SET(d, GCHD_TC_AIN_LSB_FIRST,0); GCHD_TC_SET(d, GCHD_TC_AIN_BIT_POSB,0); GCHD_TC_SET(d, GCHD_TC_AMCK_MODE,1); GCHD_TC_SET(d, GCHD_TC_AIN_DLENGTH,2); GCHD_TC_SET(d, GCHD_TC_AIN_LR_HLSET,1); GCHD_TC_SET(d, GCHD_TC_AIN_I2S,1); GCHD_TC_SET(d, GCHD_TC_AIN_LRCLK_SEL,0); GCHD_TC_SET(d, GCHD_TC_AIN_SCLK_SEL,0); GCHD_TC_SET(d, GCHD_TC_AIN_BCLK_SEL,0); GCHD_TC_SET(d, GCHD_TC_AIN_BCLK_HL,0); GCHD_TC_SET(d, GCHD_TC_AIN_OFFSET_MODE,0); GCHD_TC_SET(d, GCHD_TC_AIN_OFFSET,0);
 GCHD_TC_SET(d, GCHD_TC_V_DISABLE_ASPECT_RATIO_INFO_PRESENT_FLAG,0); GCHD_TC_SET(d, GCHD_TC_V_DISABLE_ASPECT_RATIO_INFO_PRESENT_FLAG_BL,0); GCHD_TC_SET(d, GCHD_TC_V_PIC_ORDER_PRESENT_FLAG,1); GCHD_TC_SET(d, GCHD_TC_V_PIC_ORDER_PRESENT_FLAG_BL,1);
 return 0;
}

int gchd_transcoder_setup(struct gchd *d)
{
 return gchd_transcoder_program(d, false);
}

int gchd_transcoder_start_block(struct gchd *d)
{
 return gchd_transcoder_program(d, true);
}

int gchd_transcoder_output_enable(struct gchd *d, bool value)
{
 return gchd_sparam(d, GCHD_TC_AMCK_MODE, value ? 1 : 0);
}

int gchd_transcoder_init(struct gchd *d)
{
 int r;

 r = gchd_transcoder_defaults(d);
 if (r)
  return r;

 dev_info(&d->intf->dev, "transcoder defaults loaded\n");
 return 0;
}

MODULE_DESCRIPTION("Elgato Game Capture HD transcoder setup");
MODULE_LICENSE("GPL");

