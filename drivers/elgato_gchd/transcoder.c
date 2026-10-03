#include <linux/module.h>
#include "elgato_gchd.h"

/*
 * The original driver programs the Fujitsu transcoder through two USB
 * vendor requests: SLSI writes a 16-bit register, while SPARAM updates a
 * masked bit-field in a 32-bit register.  Keep this table-driven here so
 * the kernel driver does not need any userspace configuration program.
 */

#define VPID 0x1011
#define APID 0x010f
#define PMTPID 0x0110
#define SITPID 0x001f
#define PCRPID 0x0100
#define THUMBPID 0x1111
#define VSID 0x00e0
#define ASID 0x00c0

#define BF(addr, lsb, bits, val) gchd_sparam(d, (addr), (lsb), (bits), (val))

static int gchd_transcoder_defaults(struct gchd *d)
{
 int r;

 /* bank 0 */
 r = gchd_slsi(d, 0x0000, 0x0000);
 if (r) return r;

 if ((r = BF(0x1002, 1, 1, 0))) return r; /* v_vinpelclk */
 if ((r = BF(0x1002, 4, 1, 1))) return r; /* tbc_mode */
 if ((r = BF(0x1004, 0, 1, 0))) return r; /* stout_mode */
 if ((r = BF(0x1004, 13, 2, 0))) return r;
 if ((r = BF(0x1004, 11, 2, 0))) return r;
 if ((r = BF(0x1004, 10, 1, 0))) return r;
 if ((r = BF(0x1004, 8, 2, 1))) return r;
 if ((r = BF(0x1004, 7, 1, 0))) return r;
 if ((r = BF(0x1004, 0, 7, 4))) return r;

 if ((r = BF(0x1006, 15, 1, 0))) return r;
 if ((r = BF(0x1006, 10, 1, 0))) return r;
 if ((r = BF(0x1006, 7, 1, 0))) return r;

 if ((r = BF(0x1102, 2, 1, 0))) return r;
 if ((r = BF(0x1242, 2, 1, 0))) return r;
 if ((r = BF(0x1102, 0, 2, 2))) return r;
 if ((r = BF(0x1242, 0, 2, 2))) return r;

 if ((r = BF(0x1104, 0, 16, 25000))) return r;
 if ((r = BF(0x1244, 0, 16, 8000))) return r;
 if ((r = BF(0x1106, 0, 16, 0))) return r;
 if ((r = BF(0x1246, 0, 16, 0))) return r;

 /* Fixed transport/PES identifiers used by the original encoder. */
 if ((r = BF(0x1126, 0, 13, VPID))) return r;
 if ((r = BF(0x1266, 0, 13, VPID))) return r;
 if ((r = BF(0x1128, 0, 13, APID))) return r;
 if ((r = BF(0x1268, 0, 13, APID))) return r;
 if ((r = BF(0x112c, 0, 13, PMTPID))) return r;
 if ((r = BF(0x126c, 0, 13, PMTPID))) return r;
 if ((r = BF(0x112e, 0, 13, PCRPID))) return r;
 if ((r = BF(0x126e, 0, 13, PCRPID))) return r;
 if ((r = BF(0x1134, 0, 13, SITPID))) return r;
 if ((r = BF(0x1274, 0, 13, SITPID))) return r;
 if ((r = BF(0x126a, 0, 13, THUMBPID))) return r;
 if ((r = BF(0x1130, 0, 8, VSID))) return r;
 if ((r = BF(0x1270, 0, 8, VSID))) return r;
 if ((r = BF(0x1132, 0, 8, ASID))) return r;
 if ((r = BF(0x1272, 0, 8, ASID))) return r;

 if ((r = BF(0x113c, 0, 8, 0))) return r;
 if ((r = BF(0x127c, 0, 8, 0))) return r;
 if ((r = BF(0x113e, 0, 8, 0))) return r;
 if ((r = BF(0x127e, 0, 8, 0))) return r;
 if ((r = BF(0x1140, 0, 8, 0))) return r;
 if ((r = BF(0x1280, 0, 8, 0))) return r;
 if ((r = BF(0x1142, 0, 10, 0))) return r;
 if ((r = BF(0x1282, 0, 10, 0))) return r;

 /* H.264 encoder defaults. */
 if ((r = BF(0x1710, 12, 2, 1))) return r;
 if ((r = BF(0x1502, 10, 1, 1))) return r;
 if ((r = BF(0x1502, 0, 8, 4))) return r;
 if ((r = BF(0x1504, 0, 2, 2))) return r;
 if ((r = BF(0x1510, 8, 2, 0))) return r;
 if ((r = BF(0x1510, 5, 1, 0))) return r;
 if ((r = BF(0x1510, 4, 1, 1))) return r;
 if ((r = BF(0x1510, 3, 1, 0))) return r;
 if ((r = BF(0x1510, 2, 1, 0))) return r;
 if ((r = BF(0x1510, 0, 2, 1))) return r;
 if ((r = BF(0x1518, 0, 8, 0))) return r;
 if ((r = BF(0x1518, 8, 8, 15))) return r;
 if ((r = BF(0x151a, 0, 16, 0))) return r;
 if ((r = BF(0x151c, 0, 16, 0))) return r;
 if ((r = BF(0x1526, 0, 8, 41))) return r;
 if ((r = BF(0x1526, 8, 8, 1))) return r;
 if ((r = BF(0x152c, 0, 16, 1920))) return r;
 if ((r = BF(0x152e, 0, 16, 1080))) return r;
 if ((r = BF(0x1532, 0, 16, 16000))) return r;
 if ((r = BF(0x1534, 0, 16, 14000))) return r;
 if ((r = BF(0x1536, 0, 16, 8000))) return r;
 if ((r = BF(0x1538, 0, 16, 0))) return r;
 if ((r = BF(0x1570, 0, 1, 1))) return r;
 if ((r = BF(0x1570, 11, 1, 1))) return r;
 if ((r = BF(0x1570, 14, 1, 1))) return r;
 if ((r = BF(0x1570, 15, 1, 1))) return r;
 if ((r = BF(0x157c, 11, 1, 1))) return r;
 if ((r = BF(0x157c, 14, 1, 1))) return r;
 if ((r = BF(0x157c, 15, 1, 1))) return r;
 if ((r = BF(0x157e, 0, 1, 0))) return r;
 if ((r = BF(0x157e, 4, 3, 5))) return r;
 if ((r = BF(0x157e, 12, 1, 1))) return r;
 if ((r = BF(0x1580, 0, 8, 1))) return r;
 if ((r = BF(0x1580, 12, 1, 1))) return r;
 if ((r = BF(0x1582, 0, 8, 1))) return r;
 if ((r = BF(0x1582, 8, 8, 1))) return r;
 if ((r = BF(0x1584, 0, 16, 0))) return r;
 if ((r = BF(0x1586, 0, 16, 0))) return r;
 if ((r = BF(0x1588, 0, 16, 0))) return r;

 /* Audio defaults. */
 if ((r = BF(0x1a02, 0, 2, 0))) return r;
 if ((r = BF(0x1a04, 0, 16, 384))) return r;
 if ((r = BF(0x1a06, 0, 1, 0))) return r;
 if ((r = BF(0x1a08, 3, 1, 1))) return r;
 if ((r = BF(0x1a08, 4, 2, 2))) return r;
 if ((r = BF(0x1a08, 6, 1, 1))) return r;
 if ((r = BF(0x1a08, 7, 1, 1))) return r;
 if ((r = BF(0x1a16, 0, 1, 0))) return r;
 if ((r = BF(0x1a16, 1, 2, 0))) return r;
 if ((r = BF(0x1a16, 3, 1, 0))) return r;
 if ((r = BF(0x1a16, 4, 1, 1))) return r;
 if ((r = BF(0x1a16, 5, 2, 0))) return r;

 /* Disable the secondary output path. */
 if ((r = BF(0x100a, 0, 4, 0))) return r;
 if ((r = BF(0x100a, 4, 4, 1))) return r;
 if ((r = BF(0x100a, 8, 4, 0))) return r;
 if ((r = BF(0x100a, 12, 4, 0))) return r;
 if ((r = BF(0x100c, 0, 4, 0))) return r;
 if ((r = BF(0x100c, 4, 4, 2))) return r;
 if ((r = BF(0x100c, 8, 4, 0))) return r;
 if ((r = BF(0x100c, 12, 4, 3))) return r;

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
 int r;
 u16 width = d->input_width;
 u16 height = d->input_height;
 u16 fps = d->input_fps_num ? d->input_fps_num : 60;
 u32 auto_bitrate;
 u16 bitrate;
 u16 audio = 320;
 u32 auto_max_bitrate;
 u32 auto_average_bitrate;
 u32 auto_min_bitrate;

 auto_bitrate = min_t(u32, 31250U * width, 40000U);
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
 if (d->h264_level_forced)
  level = d->h264_level;
 else if ((u32)bitrate <= 64) level = 10;
 else if ((u32)bitrate <= 192) level = 11;
 else if ((u32)bitrate <= 384) level = 12;
 else if ((u32)bitrate <= 768) level = 13;
 else if ((u32)bitrate <= 2000) level = 20;
 else if ((u32)bitrate <= 4000) level = 21;
 else if ((u32)bitrate <= 10000) level = 30;
 else if ((u32)bitrate <= 14000) level = 31;
 else if ((u32)bitrate <= 20000) level = 32;
 else if ((u32)bitrate <= 50000) level = 41;
 else level = 52;

 u32 system_rate = (1075U * ((u32)bitrate + audio)) / 1000U + 256U;
 u16 colour = d->input == 0 ? 1 : 6;
 u8 vformat;

 if (height == 1080) vformat = d->input_interlaced ? 0 : 32;
 else if (height == 720) vformat = 2;
 else if (height == 576) vformat = d->input_interlaced ? 5 : 9;
 else if (height == 480) vformat = d->input_interlaced ? 4 : 8;
 else return -EINVAL;
 if (fps == 50) vformat |= 1;

#define P(a,l,b,v) do { r=gchd_sparam(d,(a),(l),(b),(v)); if(r) return r; } while(0)
 if (d->family == GCHD_FAMILY_HDNEW) { P(0x1574,0,5,3); }
 P(0x1104,0,16,system_rate); P(0x1106,0,16,0);
 P(0x1510,5,1,1); P(0x1518,0,8,anchor); P(0x1518,8,8,gop);
 P(0x1510,3,1,0); P(0x1514,8,3,0); P(0x1510,0,2,0);
 P(0x1532,0,16,bitrate); P(0x1534,0,16,auto_max_bitrate); P(0x1536,0,16,auto_average_bitrate); P(0x1538,0,16,auto_min_bitrate);
 P(0x152c,0,16,width); P(0x152e,0,16,height); P(0x1002,1,1,0);
 P(0x1584,0,16,0); P(0x1586,0,16,0); P(0x1588,0,16,0);
 P(0x151a,0,16,0); P(0x151c,0,16,0);
 P(0x1526,0,8,level); P(0x1526,8,8,profile);
 P(0x1a02,0,2,0); P(0x1a16,0,1,0); P(0x1a16,1,2,0); P(0x1a16,3,1,0); P(0x1a16,4,1,1); P(0x1a16,5,2,0); P(0x1a04,0,16,audio);
 P(0x1580,12,1,1); P(0x1580,0,8,colour); P(0x1582,8,8,1); P(0x1582,0,8,colour); P(0x157e,0,1,0); P(0x157e,12,1,1); P(0x157e,4,3,5);
 if (d->family == GCHD_FAMILY_HDNEW) { P(0x1674,0,5,3); }
 P(0x1244,0,16,system_rate); P(0x1246,0,16,0);
 P(0x1610,5,1,1); P(0x1618,0,8,anchor); P(0x1618,8,8,gop); P(0x1610,3,1,0); P(0x1614,8,3,0); P(0x1610,0,2,0);
 P(0x1632,0,16,bitrate); P(0x1634,0,16,auto_max_bitrate); P(0x1636,0,16,auto_average_bitrate); P(0x1638,0,16,auto_min_bitrate); P(0x163a,0,16,0);
 P(0x162c,0,16,width); P(0x162e,0,16,height); P(0x161c,0,16,0); P(0x1626,0,8,31); P(0x1626,8,8,profile);
 P(0x1684,0,16,0); P(0x1686,0,16,0); P(0x1688,0,16,1);
 P(0x1a36,0,1,0); P(0x1a36,1,2,0); P(0x1a36,3,1,0); P(0x1a36,4,1,1); P(0x1a36,5,2,0); P(0x1a24,0,16,audio);
 P(0x1680,12,1,1); P(0x1680,0,8,colour); P(0x1682,8,8,1); P(0x1682,0,8,colour); P(0x167e,0,1,0); P(0x167e,12,1,1); P(0x167e,4,3,5);
 P(0x100a,4,4,1); P(0x100c,12,4,3); P(0x100a,0,4,0); P(0x100c,4,4,2); P(0x100a,8,4,0); P(0x100c,0,4,0); P(0x100c,8,4,0);
 P(0x1504,8,3,(height==576)?3:0); P(0x1502,0,8,vformat); P(0x1512,8,7,d->input_interlaced ? 0 : 2); P(0x1002,4,1,1); P(0x1002,4,1,1);
 P(0x1a02,0,2,0); P(0x1a08,0,1,0); P(0x1a08,1,1,0); P(0x1a08,2,1,0); P(0x1a08,3,1,1); P(0x1a08,4,2,2); P(0x1a08,6,1,1); P(0x1a08,7,1,1); P(0x1a08,8,3,0); P(0x1a08,11,2,0); P(0x1a08,13,1,0); P(0x1a08,14,1,0); P(0x1a08,15,1,0); P(0x1a0c,0,16,0);
 P(0x158a,12,1,0); P(0x168a,12,1,0); P(0x1570,0,1,1); P(0x1670,0,1,1);
#undef P
 return 0;
}

int gchd_transcoder_output_enable(struct gchd *d, bool value)
{
 return gchd_sparam(d, 0x1a08, 3, 1, value ? 1 : 0);
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
 int r;
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

 r = gchd_sparam(d, 0x1126, 0, 13, VPID); if (r) return r;
 r = gchd_sparam(d, 0x1266, 0, 13, VPID); if (r) return r;
 r = gchd_sparam(d, 0x1128, 0, 13, APID); if (r) return r;
 r = gchd_sparam(d, 0x1268, 0, 13, APID); if (r) return r;
 r = gchd_sparam(d, 0x1130, 0, 8, VSID); if (r) return r;
 r = gchd_sparam(d, 0x1270, 0, 8, VSID); if (r) return r;
 r = gchd_sparam(d, 0x1132, 0, 8, ASID); if (r) return r;
 r = gchd_sparam(d, 0x1272, 0, 8, ASID); if (r) return r;
 r = gchd_sparam(d, 0x1504, 8, 3, source_type); if (r) return r;
 r = gchd_sparam(d, 0x1502, 0, 8, video_format); if (r) return r;
 r = gchd_sparam(d, 0x1512, 8, 7,
                 d->input_interlaced ? 0 : 2); if (r) return r;
 r = gchd_sparam(d, 0x1002, 1, 1, 0); if (r) return r;
 r = gchd_sparam(d, 0x1002, 4, 1, 1); if (r) return r;
 r = gchd_sparam(d, 0x1a02, 0, 2, 0); if (r) return r;
 r = gchd_sparam(d, 0x1a08, 3, 1, 1); if (r) return r;
 r = gchd_sparam(d, 0x1a08, 4, 2, 2); if (r) return r;
 r = gchd_sparam(d, 0x1a08, 6, 1, 1); if (r) return r;
 r = gchd_sparam(d, 0x1a08, 7, 1, 1); if (r) return r;
 r = gchd_sparam(d, 0x1504, 0, 2, 2); if (r) return r;

 /* These are emitted twice by the reference implementation. */
 r = gchd_sparam(d, 0x1002, 4, 1, 1); if (r) return r;
 r = gchd_sparam(d, 0x1002, 4, 1, 1); if (r) return r;

 return 0;
}
