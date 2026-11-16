#include <linux/module.h>
#include "elgato_gchd.h"
#include "gchd_transcoder_fields.h"
#include "gchd_mailbox.h"

/*
 * The original driver programs the Fujitsu transcoder through two USB
 * vendor requests: SLSI writes a 16-bit register, while SPARAM updates a
 * masked bit-field in a 32-bit register.  Keep this table-driven here so
 * the kernel driver does not need any userspace configuration program.
 */

#define GCHD_TS_PID_VIDEO 0x1011
#define GCHD_TS_PID_AUDIO 0x010f
#define GCHD_TS_PID_PMT 0x0110
#define GCHD_TS_PID_SIT 0x001f
#define GCHD_TS_PID_PCR 0x0100
#define GCHD_TS_PID_THUMBNAIL 0x1111
#define GCHD_TS_SID_VIDEO 0x00e0
#define GCHD_TS_SID_AUDIO 0x00c0


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

/* The userspace PSI builders leave the CRC to the transcoder hardware. */
static const u8 gchd_pat[] = {
 0x00,0xb0,0x11,0x00,0x00,0x01,0x00,0x00,
 0x00,0x01,0xe1,0x10,0x00,0x00,0xe0,0x1f
};
static const u8 gchd_sit[] = {
 0x7f,0xf0,0x0b,0xff,0xff,0x01,0x00,0x00,0xf0,0x00
};
static const u8 gchd_pmt[] = {
 0x02,0xb0,0x33,0x00,0x01,0x01,0x00,0x00,
 0xe0,0x00,0xf0,0x0c,0x05,0x04,0x48,0x44,0x4d,0x56,
 0x88,0x04,0x0f,0xff,0xfc,0xfc,
 0x1b,0xe0,0x11,0xf0,0x14,0x05,0x08,0x48,0x44,0x4d,0x56,
 0xff,0x1b,0x44,0x3f,0x28,0x04,0x2a,0x02,0x7e,0xff,
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
 /* FIXME (T-001 finding): C++ stout_mode is 0x1004 bit 15; this writes bit 0
  * (part of stoutclk). Kept as ported; verify against a Windows trace. */
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

 /* Fixed transport/PES identifiers used by the original encoder. */
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
 r = gchd_transcoder_table_write(d, 0x1174, gchd_pmt, sizeof(gchd_pmt)); if (r) return r;
 r = gchd_transcoder_table_write(d, 0x1010, gchd_sit, sizeof(gchd_sit)); if (r) return r;
 GCHD_TC_SET(d, GCHD_TC_PAT_LENGTH_TS_OUT2, sizeof(gchd_pat));
 GCHD_TC_SET(d, GCHD_TC_PMT_LENGTH_TS_OUT2, sizeof(gchd_pmt));
 GCHD_TC_SET(d, GCHD_TC_SIT_LENGTH_TS_OUT2, sizeof(gchd_sit));
 r = gchd_transcoder_table_write(d, 0x1284, gchd_pat, sizeof(gchd_pat)); if (r) return r;
 r = gchd_transcoder_table_write(d, 0x12b4, gchd_pmt, sizeof(gchd_pmt)); if (r) return r;
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
 * This deliberately performs only the common transcoder stage. The original
 * program has large analog/HDMI signal-lock scripts; those are being moved
 * behind the V4L2 input/mode controls rather than executed by a userspace
 * process. Until a mode is selected, the encoder is left stopped.
 */

/* Exact configure.cpp transcoderSetup() parameter stage.  The V4L2 driver
 * currently exposes one encoder profile/rate tuple, so the corresponding
 * source values are derived from the active capture mode. */
int gchd_transcoder_setup(struct gchd *d)
{
 u16 width = d->input_width;
 u16 height = d->input_height;
 u16 fps = d->input_fps_num ? d->input_fps_num : 60;
 u32 auto_bitrate;
 u16 bitrate;
 u16 audio = 320;
 u32 auto_max_bitrate;
 u32 auto_average_bitrate;
 u32 auto_min_bitrate;

 auto_bitrate = min_t(u32, (31250U * width) / 1000U, 40000U);
 auto_max_bitrate = min_t(u32, (auto_bitrate * 9U) / 10U, 40000U);
 auto_average_bitrate = min_t(u32, auto_bitrate / 2U, 40000U);
 auto_min_bitrate = min_t(u32, (auto_bitrate * 35U) / 100U, 40000U);
 bitrate = d->bitrate_forced ? d->bitrate : (u16)auto_bitrate;
 u16 gop = (fps + 10) / 5;
 u16 anchor = 3;
 u8 profile;
 u8 level;

 /*
  * V4L2 profile values are API enum values; the Fujitsu encoder uses
  * 1=High and 2=Main.  The userspace driver maps Baseline to Main too.
  */
 if (d->h264_profile == V4L2_MPEG_VIDEO_H264_PROFILE_HIGH)
  profile = 1;
 else
  profile = 2;

 if (d->h264_profile == V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE)
  anchor = 1;
 if (d->h264_level_forced) {
  level = d->h264_level;
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
 u8 vformat;

 if (height == 1080) vformat = d->input_interlaced ? 0 : 32;
 else if (height == 720) vformat = 2;
 else if (height == 576) vformat = d->input_interlaced ? 5 : 9;
 else if (height == 480) vformat = d->input_interlaced ? 4 : 8;
 else return -EINVAL;
 if (fps == 50) vformat |= 1;

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
 GCHD_TC_SET(d, GCHD_TC_V_TS_OUT_MODE,1); GCHD_TC_SET(d, GCHD_TC_A_MODE,3); GCHD_TC_SET(d, GCHD_TC_V_TS_OUT2_MODE,0); GCHD_TC_SET(d, GCHD_TC_A_TS_OUT_MODE,2); GCHD_TC_SET(d, GCHD_TC_SOUT2_MODE,0); GCHD_TC_SET(d, GCHD_TC_A_TS_OUT2_MODE,0); GCHD_TC_SET(d, GCHD_TC_A2_MODE,0);
 GCHD_TC_SET(d, GCHD_TC_V_IN_SOURCE_TYPE,(height==576)?3:0); GCHD_TC_SET(d, GCHD_TC_V_FORMAT,vformat); GCHD_TC_SET(d, GCHD_TC_V_IP_FORMAT,d->input_interlaced ? 0 : 2); GCHD_TC_SET(d, GCHD_TC_TBC_MODE,1); GCHD_TC_SET(d, GCHD_TC_TBC_MODE,1);
 GCHD_TC_SET(d, GCHD_TC_A_SAMPLE,0); GCHD_TC_SET(d, GCHD_TC_AIN_MASTER,0); GCHD_TC_SET(d, GCHD_TC_AIN_LSB_FIRST,0); GCHD_TC_SET(d, GCHD_TC_AIN_BIT_POSB,0); GCHD_TC_SET(d, GCHD_TC_AMCK_MODE,1); GCHD_TC_SET(d, GCHD_TC_AIN_DLENGTH,2); GCHD_TC_SET(d, GCHD_TC_AIN_LR_HLSET,1); GCHD_TC_SET(d, GCHD_TC_AIN_I2S,1); GCHD_TC_SET(d, GCHD_TC_AIN_LRCLK_SEL,0); GCHD_TC_SET(d, GCHD_TC_AIN_SCLK_SEL,0); GCHD_TC_SET(d, GCHD_TC_AIN_BCLK_SEL,0); GCHD_TC_SET(d, GCHD_TC_AIN_BCLK_HL,0); GCHD_TC_SET(d, GCHD_TC_AIN_OFFSET_MODE,0); GCHD_TC_SET(d, GCHD_TC_AIN_OFFSET,0);
 GCHD_TC_SET(d, GCHD_TC_V_DISABLE_ASPECT_RATIO_INFO_PRESENT_FLAG,0); GCHD_TC_SET(d, GCHD_TC_V_DISABLE_ASPECT_RATIO_INFO_PRESENT_FLAG_BL,0); GCHD_TC_SET(d, GCHD_TC_V_PIC_ORDER_PRESENT_FLAG,1); GCHD_TC_SET(d, GCHD_TC_V_PIC_ORDER_PRESENT_FLAG_BL,1);
 return 0;
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

int gchd_transcoder_final_configure(struct gchd *d)
{
 u8 video_format;
 u8 source_type = 0;

 /*
  * Match the reference transcoderFinalConfigure() mapping:
  * PAL 4:3 = 3, NTSC 4:3 = 0, and HD/other sources = 0.
  * The kernel driver does not currently expose SD stretch modes.
  */
 if (d->input_height == 576)
  source_type = 3;
 else if (d->input_height == 480)
  source_type = 0;

 /*
  * v_format:
  *   0 = 1080i60
  *   2 = 720p60
  *   4 = 480i60
  *   5 = 576i50
  *   8 = 480p60
  *   9 = 576p50
  *  32 = 1080p60
  *
  * As in the reference, bit 0 selects the 50 Hz variant.
  */
 if (d->input_height == 1080) {
  video_format = d->input_interlaced ? 0 : 32;
 } else if (d->input_height == 720) {
  if (d->input_interlaced)
   return -EINVAL;
  video_format = 2;
 } else if (d->input_height == 576) {
  video_format = d->input_interlaced ? 5 : 9;
 } else if (d->input_height == 480) {
  video_format = d->input_interlaced ? 4 : 8;
 } else {
  return -EINVAL;
 }

 if (d->input_fps_num == 50)
  video_format |= 1;
 else if (d->input_fps_num != 30 && d->input_fps_num != 60)
  return -EINVAL;

 GCHD_TC_SET(d, GCHD_TC_V_PID_OUT, GCHD_TS_PID_VIDEO);
 GCHD_TC_SET(d, GCHD_TC_V_PID_OUT_TS_OUT2, GCHD_TS_PID_VIDEO);
 GCHD_TC_SET(d, GCHD_TC_A_PID_OUT, GCHD_TS_PID_AUDIO);
 GCHD_TC_SET(d, GCHD_TC_A_PID_OUT_TS_OUT2, GCHD_TS_PID_AUDIO);
 GCHD_TC_SET(d, GCHD_TC_V_SID_OUT, GCHD_TS_SID_VIDEO);
 GCHD_TC_SET(d, GCHD_TC_V_SID_OUT_TS_OUT2, GCHD_TS_SID_VIDEO);
 GCHD_TC_SET(d, GCHD_TC_A_SID_OUT, GCHD_TS_SID_AUDIO);
 GCHD_TC_SET(d, GCHD_TC_A_SID_OUT_TS_OUT2, GCHD_TS_SID_AUDIO);
 GCHD_TC_SET(d, GCHD_TC_V_IN_SOURCE_TYPE, source_type);
 GCHD_TC_SET(d, GCHD_TC_V_FORMAT, video_format);
 GCHD_TC_SET(d, GCHD_TC_V_IP_FORMAT,
                 d->input_interlaced ? 0 : 2);
 GCHD_TC_SET(d, GCHD_TC_V_VINPELCLK, 0);
 GCHD_TC_SET(d, GCHD_TC_TBC_MODE, 1);
 GCHD_TC_SET(d, GCHD_TC_A_SAMPLE, 0);
 GCHD_TC_SET(d, GCHD_TC_AMCK_MODE, 1);
 GCHD_TC_SET(d, GCHD_TC_AIN_DLENGTH, 2);
 GCHD_TC_SET(d, GCHD_TC_AIN_LR_HLSET, 1);
 GCHD_TC_SET(d, GCHD_TC_AIN_I2S, 1);
 GCHD_TC_SET(d, GCHD_TC_TFRFF, 2);

 /* These are emitted twice by the reference implementation. */
 GCHD_TC_SET(d, GCHD_TC_TBC_MODE, 1);
 GCHD_TC_SET(d, GCHD_TC_TBC_MODE, 1);

 return 0;
}
