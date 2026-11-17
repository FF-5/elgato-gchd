/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Transcoder (Fujitsu MB86H57/H58/M01 encoder) register fields.
 *
 * Field names follow the encoder register naming; addresses, least
 * significant bit and width are given for each field.
 *
 * Each GCHD_TC_<name> expands to "address, lsb, bits" for gchd_sparam():
 *     GCHD_TC_SET(d, GCHD_TC_V_HSIZE_OUT, 720);
 * Field descriptions: docs/commands.md §5.
 */
#ifndef GCHD_TRANSCODER_FIELDS_H
#define GCHD_TRANSCODER_FIELDS_H

#define GCHD_TC_V_VINPELCLK                              0x1002,  1,  1
#define GCHD_TC_TBC_MODE                                 0x1002,  4,  1
#define GCHD_TC_STOUTCLK                                 0x1004,  0,  7
#define GCHD_TC_STOUTCLK_IO                              0x1004,  7,  1
#define GCHD_TC_DMA_SEL_OUT                              0x1004,  8,  2
#define GCHD_TC_STOUTCLK_HL                              0x1004, 10,  1
#define GCHD_TC_TSOUT_PACKET_SIZE                        0x1004, 11,  2
#define GCHD_TC_EXTRA_INFO_BIT                           0x1004, 13,  2
#define GCHD_TC_STOUT_MODE                               0x1004, 15,  1
#define GCHD_TC_STOUT2CLK_IO                             0x1006,  7,  1
#define GCHD_TC_STOUT2CLK_HL                             0x1006, 10,  1
#define GCHD_TC_STOUT2_MODE                              0x1006, 15,  1
#define GCHD_TC_V_TS_OUT2_MODE                           0x100a,  0,  4
#define GCHD_TC_V_TS_OUT_MODE                            0x100a,  4,  4
#define GCHD_TC_SOUT2_MODE                               0x100a,  8,  4
#define GCHD_TC_SOUT_MODE                                0x100a, 12,  4
#define GCHD_TC_A_TS_OUT2_MODE                           0x100c,  0,  4
#define GCHD_TC_A_TS_OUT_MODE                            0x100c,  4,  4
#define GCHD_TC_A2_MODE                                  0x100c,  8,  4
#define GCHD_TC_A_MODE                                   0x100c, 12,  4
#define GCHD_TC_AUTO_NULL                                0x1102,  0,  2
#define GCHD_TC_PCR_FIRST                                0x1102,  2,  1
#define GCHD_TC_SYSTEM_RATE                              0x1104,  0, 16
#define GCHD_TC_SYSTEM_MIN_RATE                          0x1106,  0, 16
#define GCHD_TC_INITIAL_TIME_STAMP_H                     0x1108,  0, 16
#define GCHD_TC_INITIAL_TIME_STAMP_L                     0x110a,  0, 16
#define GCHD_TC_INITIAL_STC_H                            0x110c,  0, 16
#define GCHD_TC_INITIAL_STC_L                            0x110e,  0, 16
#define GCHD_TC_INITIAL_STC_EXT                          0x1110,  0,  9
#define GCHD_TC_INITIAL_STC_LL                           0x1110, 15,  1
#define GCHD_TC_V_PID_OUT                                0x1126,  0, 13
#define GCHD_TC_A_PID_OUT                                0x1128,  0, 13
#define GCHD_TC_PMT_PID_OUT                              0x112c,  0, 13
#define GCHD_TC_PCR_PID_OUT                              0x112e,  0, 13
#define GCHD_TC_V_SID_OUT                                0x1130,  0,  8
#define GCHD_TC_A_SID_OUT                                0x1132,  0,  8
#define GCHD_TC_SIT_PID_OUT                              0x1134,  0, 13
#define GCHD_TC_PMT_LENGTH                               0x1138,  0,  8
#define GCHD_TC_PAT_LENGTH                               0x1138,  8,  8
#define GCHD_TC_PAT_CYC                                  0x113c,  0,  8
#define GCHD_TC_PMT_CYC                                  0x113e,  0,  8
#define GCHD_TC_SIT_LENGTH                               0x113e,  8,  8
#define GCHD_TC_PCR_CYC                                  0x1140,  0,  8
#define GCHD_TC_SIT_CYC                                  0x1142,  0, 10
#define GCHD_TC_AUTO_NULL_TS_OUT2                        0x1242,  0,  2
#define GCHD_TC_PCR_FIRST_TS_OUT2                        0x1242,  2,  1
#define GCHD_TC_SYSTEM_RATE_TS_OUT2                      0x1244,  0, 16
#define GCHD_TC_SYSTEM_MIN_RATE_TS_OUT2                  0x1246,  0, 16
#define GCHD_TC_INITIAL_TIME_STAMP_H_TS_OUT2             0x1248,  0, 16
#define GCHD_TC_INITIAL_TIME_STAMP_L_TS_OUT2             0x124a,  0, 16
#define GCHD_TC_INITIAL_STC_H_TS_OUT2                    0x124c,  0, 16
#define GCHD_TC_INITIAL_STC_L_TS_OUT2                    0x124e,  0, 16
#define GCHD_TC_INITIAL_STC_EXT_TS_OUT2                  0x1250,  0,  9
#define GCHD_TC_INITIAL_STC_LL_TS_OUT2                   0x1250, 15,  1
#define GCHD_TC_V_PID_OUT_TS_OUT2                        0x1266,  0, 13
#define GCHD_TC_A_PID_OUT_TS_OUT2                        0x1268,  0, 13
#define GCHD_TC_THUMBNAIL_PID_OUT_TS_OUT2                0x126a,  0, 13
#define GCHD_TC_PMT_PID_OUT_TS_OUT2                      0x126c,  0, 13
#define GCHD_TC_PCR_PID_OUT_TS_OUT2                      0x126e,  0, 13
#define GCHD_TC_V_SID_OUT_TS_OUT2                        0x1270,  0,  8
#define GCHD_TC_A_SID_OUT_TS_OUT2                        0x1272,  0,  8
#define GCHD_TC_SIT_PID_OUT_TS_OUT2                      0x1274,  0, 13
#define GCHD_TC_PMT_LENGTH_TS_OUT2                       0x1278,  0,  8
#define GCHD_TC_PAT_LENGTH_TS_OUT2                       0x1278,  8,  8
#define GCHD_TC_PAT_CYC_TS_OUT2                          0x127c,  0,  8
#define GCHD_TC_PMT_CYC_TS_OUT2                          0x127e,  0,  8
#define GCHD_TC_SIT_LENGTH_TS_OUT2                       0x127e,  8,  8
#define GCHD_TC_PCR_CYC_TS_OUT2                          0x1280,  0,  8
#define GCHD_TC_SIT_CYC_TS_OUT2                          0x1282,  0, 10
#define GCHD_TC_V_FORMAT                                 0x1502,  0,  8
#define GCHD_TC_VIN_SWAP                                 0x1502, 10,  1
#define GCHD_TC_TFRFF                                    0x1504,  0,  2
#define GCHD_TC_V_IN_SOURCE_TYPE                         0x1504,  8,  3
#define GCHD_TC_V_RATE_MODE                              0x1510,  0,  2
#define GCHD_TC_V_FILLER                                 0x1510,  2,  1
#define GCHD_TC_V_VLC_MODE                               0x1510,  3,  1
#define GCHD_TC_V_TEMPORAL_DIRECT_ON                     0x1510,  4,  1
#define GCHD_TC_V_CL_GOP                                 0x1510,  5,  1
#define GCHD_TC_V_EOS                                    0x1510,  8,  2
#define GCHD_TC_V_IP_FORMAT                              0x1512,  8,  7
#define GCHD_TC_V_VBR_CONVERGE_MODE                      0x1514,  8,  3
#define GCHD_TC_V_GOP_STRUCT                             0x1518,  0,  8
#define GCHD_TC_V_GOP_SIZE                               0x1518,  8,  8
#define GCHD_TC_V_IDR_INTERVAL_H                         0x151a,  0, 16
#define GCHD_TC_V_IDR_INTERVAL_L                         0x151c,  0, 16
#define GCHD_TC_V_H264_LEVEL                             0x1526,  0,  8
#define GCHD_TC_V_H264_PROFILE                           0x1526,  8,  8
#define GCHD_TC_V_HSIZE_OUT                              0x152c,  0, 16
#define GCHD_TC_V_VSIZE_OUT                              0x152e,  0, 16
#define GCHD_TC_V_BITRATE                                0x1532,  0, 16
#define GCHD_TC_V_MAX_BITRATE                            0x1534,  0, 16
#define GCHD_TC_V_AVE_BITRATE                            0x1536,  0, 16
#define GCHD_TC_V_MIN_BITRATE                            0x1538,  0, 16
#define GCHD_TC_V_FILLER_BITRATE                         0x153a,  0, 16
#define GCHD_TC_ERROR_RATE_PPIC                          0x1552,  0,  8
#define GCHD_TC_ERROR_RATE_IPIC                          0x1552,  8,  8
#define GCHD_TC_ERROR_RATE_BPIC                          0x1554,  0,  8
#define GCHD_TC_ERROR_RATE_REFPIC                        0x1554,  8,  8
#define GCHD_TC_V_ERROR_LEVEL_TH_H                       0x1556,  0, 16
#define GCHD_TC_V_ERROR_LEVEL_TH_L                       0x1558,  0, 16
#define GCHD_TC_V_PIC_ORDER_PRESENT_FLAG                 0x1570,  0,  1
#define GCHD_TC_INSERT_RECOVERY_POINT_SEI                0x1570, 11,  1
#define GCHD_TC_INSERT_PIC_STRUCT                        0x1570, 14,  1
#define GCHD_TC_INSERT_BUF_CTRL_PARAM                    0x1570, 15,  1
#define GCHD_TC_V_MAX_CPB_DELAY                          0x1574,  0,  5
#define GCHD_TC_INSERT_RESTRICTION                       0x157c, 11,  1
#define GCHD_TC_OVERSCAN_APPROPRIATE_FLAG                0x157c, 14,  1
#define GCHD_TC_OVERSCAN_INFO_PRESENT_FLAG               0x157c, 15,  1
#define GCHD_TC_VIDEO_FULL_RANGE_FLAG                    0x157e,  0,  1
#define GCHD_TC_VIDEO_FORMAT                             0x157e,  4,  3
#define GCHD_TC_VIDEO_SIGNAL_TYPE_PRESENT_FLAG           0x157e, 12,  1
#define GCHD_TC_COLOUR_PRIMARIES                         0x1580,  0,  8
#define GCHD_TC_COLOUR_DESCRIPTION_PRESENT_FLAG          0x1580, 12,  1
#define GCHD_TC_MATRIX_COEFFICIENTS                      0x1582,  0,  8
#define GCHD_TC_TRANSFER_CHARACTERISTICS                 0x1582,  8,  8
#define GCHD_TC_TIME_SCALE_H                             0x1584,  0, 16
#define GCHD_TC_TIME_SCALE_L                             0x1586,  0, 16
#define GCHD_TC_NUM_UNITS_IN_TICK                        0x1588,  0, 16
#define GCHD_TC_V_DISABLE_ASPECT_RATIO_INFO_PRESENT_FLAG 0x158a, 12,  1
#define GCHD_TC_V_RATE_MODE_BL                           0x1610,  0,  2
#define GCHD_TC_V_FILLER_BL                              0x1610,  2,  1
#define GCHD_TC_V_VLC_MODE_BL                            0x1610,  3,  1
#define GCHD_TC_V_CL_GOP_BL                              0x1610,  5,  1
#define GCHD_TC_V_EOS_BL                                 0x1610,  8,  2
#define GCHD_TC_V_IP_FORMAT_BL                           0x1612,  8,  7
#define GCHD_TC_V_VBR_CONVERGE_MODE_BL                   0x1614,  8,  3
#define GCHD_TC_V_GOP_STRUCT_BL                          0x1618,  0,  8
#define GCHD_TC_V_GOP_SIZE_BL                            0x1618,  8,  8
#define GCHD_TC_V_IDR_INTERVAL_H_BL                      0x161a,  0, 16
#define GCHD_TC_V_IDR_INTERVAL_L_BL                      0x161c,  0, 16
#define GCHD_TC_V_H264_LEVEL_BL                          0x1626,  0,  8
#define GCHD_TC_V_H264_PROFILE_BL                        0x1626,  8,  8
#define GCHD_TC_V_HSIZE_OUT_BL                           0x162c,  0, 16
#define GCHD_TC_V_VSIZE_OUT_BL                           0x162e,  0, 16
#define GCHD_TC_V_BITRATE_BL                             0x1632,  0, 16
#define GCHD_TC_V_MAX_BITRATE_BL                         0x1634,  0, 16
#define GCHD_TC_V_AVE_BITRATE_BL                         0x1636,  0, 16
#define GCHD_TC_V_MIN_BITRATE_BL                         0x1638,  0, 16
#define GCHD_TC_V_FILLER_BITRATE_BL                      0x163a,  0, 16
#define GCHD_TC_V_PIC_ORDER_PRESENT_FLAG_BL              0x1670,  0,  1
#define GCHD_TC_INSERT_RECOVERY_POINT_SEI_BL             0x1670, 11,  1
#define GCHD_TC_INSERT_PIC_STRUCT_BL                     0x1670, 14,  1
#define GCHD_TC_INSERT_BUF_CTRL_PARAM_BL                 0x1670, 15,  1
#define GCHD_TC_V_MAX_CPB_DELAY_BL                       0x1674,  0,  5
#define GCHD_TC_INSERT_RESTRICTION_BL                    0x167c, 11,  1
#define GCHD_TC_OVERSCAN_APPROPRIATE_FLAG_BL             0x167c, 14,  1
#define GCHD_TC_OVERSCAN_INFO_PRESENT_FLAG_BL            0x167c, 15,  1
#define GCHD_TC_VIDEO_FULL_RANGE_FLAG_BL                 0x167e,  0,  1
#define GCHD_TC_VIDEO_FORMAT_BL                          0x167e,  4,  3
#define GCHD_TC_VIDEO_SIGNAL_TYPE_PRESENT_FLAG_BL        0x167e, 12,  1
#define GCHD_TC_COLOUR_PRIMARIES_BL                      0x1680,  0,  8
#define GCHD_TC_COLOUR_DESCRIPTION_PRESENT_FLAG_BL       0x1680, 12,  1
#define GCHD_TC_MATRIX_COEFFICIENTS_BL                   0x1682,  0,  8
#define GCHD_TC_TRANSFER_CHARACTERISTICS_BL              0x1682,  8,  8
#define GCHD_TC_TIME_SCALE_H_BL                          0x1684,  0, 16
#define GCHD_TC_TIME_SCALE_L_BL                          0x1686,  0, 16
#define GCHD_TC_NUM_UNITS_IN_TICK_BL                     0x1688,  0, 16
#define GCHD_TC_V_DISABLE_ASPECT_RATIO_INFO_PRESENT_FLAG_BL 0x168a, 12,  1
#define GCHD_TC_V_V420                                   0x1710, 12,  2
#define GCHD_TC_A_SAMPLE                                 0x1a02,  0,  2
#define GCHD_TC_A_BITRATE                                0x1a04,  0, 16
#define GCHD_TC_ACH_SEL                                  0x1a06,  0,  1
#define GCHD_TC_AIN_MASTER                               0x1a08,  0,  1
#define GCHD_TC_AIN_LSB_FIRST                            0x1a08,  1,  1
#define GCHD_TC_AIN_BIT_POSB                             0x1a08,  2,  1
#define GCHD_TC_AMCK_MODE                                0x1a08,  3,  1
#define GCHD_TC_AIN_DLENGTH                              0x1a08,  4,  2
#define GCHD_TC_AIN_LR_HLSET                             0x1a08,  6,  1
#define GCHD_TC_AIN_I2S                                  0x1a08,  7,  1
#define GCHD_TC_AIN_LRCLK_SEL                            0x1a08,  8,  3
#define GCHD_TC_AIN_SCLK_SEL                             0x1a08, 11,  2
#define GCHD_TC_AIN_BCLK_SEL                             0x1a08, 13,  1
#define GCHD_TC_AIN_BCLK_HL                              0x1a08, 14,  1
#define GCHD_TC_AIN_OFFSET_MODE                          0x1a08, 15,  1
#define GCHD_TC_AIN_OFFSET                               0x1a0c,  0, 16
#define GCHD_TC_E_MPEG_PROTECT                           0x1a16,  0,  1
#define GCHD_TC_E_MPEG_MODE                              0x1a16,  1,  2
#define GCHD_TC_E_MPEG_COPYR                             0x1a16,  3,  1
#define GCHD_TC_E_MPEG_ORIG                              0x1a16,  4,  1
#define GCHD_TC_E_MPEG_EMP                               0x1a16,  5,  2
#define GCHD_TC_A_BITRATE_BL                             0x1a24,  0, 16
#define GCHD_TC_ACH_SEL_BL                               0x1a26,  0,  1
#define GCHD_TC_E_MPEG_PROTECT_BL                        0x1a36,  0,  1
#define GCHD_TC_E_MPEG_MODE_BL                           0x1a36,  1,  2
#define GCHD_TC_E_MPEG_COPYR_BL                          0x1a36,  3,  1
#define GCHD_TC_E_MPEG_ORIG_BL                           0x1a36,  4,  1
#define GCHD_TC_E_MPEG_EMP_BL                            0x1a36,  5,  2
#define GCHD_TC_E_AAC_PROTECT_ABS                        0x1a1c,  0,  1
#define GCHD_TC_E_AAC_PROFILE                            0x1a1c,  1,  2
#define GCHD_TC_E_AAC_CH_CONFIG                          0x1a1c,  3,  2
#define GCHD_TC_E_AAC_ORIG                               0x1a1c,  5,  1
#define GCHD_TC_E_AAC_PCE                                0x1a1c,  8,  2
#define GCHD_TC_E_AAC_HEADER_FORMAT                      0x1a1c, 11,  2
#define GCHD_TC_E_AAC_MPEG4                              0x1a1c, 13,  1
#define GCHD_TC_E_AAC_MAX_BITRATE                        0x1a1e,  0, 16
#define GCHD_TC_E2_AAC_PROTECT_ABS                       0x1a3c,  0,  1
#define GCHD_TC_E2_AAC_PROFILE                           0x1a3c,  1,  2
#define GCHD_TC_E2_AAC_CH_CONFIG                         0x1a3c,  3,  2
#define GCHD_TC_E2_AAC_ORIG                              0x1a3c,  5,  1
#define GCHD_TC_E2_AAC_PCE                               0x1a3c,  8,  2
#define GCHD_TC_E2_AAC_HEADER_FORMAT                     0x1a3c, 11,  2
#define GCHD_TC_E2_AAC_MPEG4                             0x1a3c, 13,  1
#define GCHD_TC_LCH_SCALE                                0x1a64,  0, 16
#define GCHD_TC_RCH_SCALE                                0x1a66,  0, 16
#define GCHD_TC_LSCH_SCALE                               0x1a68,  0, 16
#define GCHD_TC_RSCH_SCALE                               0x1a6a,  0, 16
#define GCHD_TC_CCH_SCALE                                0x1a6c,  0, 16
#define GCHD_TC_LFECH_SCALE                              0x1a6e,  0, 16

/* Start addresses of PSI tables written with slsi() */
#define GCHD_TC_SIT_TABLE_START                          0x1010
#define GCHD_TC_SIT_TABLE_TS_OUT2_START                  0x1370
#define GCHD_TC_PAT_TABLE_START                          0x1144
#define GCHD_TC_PAT_TABLE_TS_OUT2_START                  0x1284
#define GCHD_TC_PMT_TABLE_START                          0x1174
#define GCHD_TC_PMT_TABLE_TS_OUT2_START                  0x12b4

#endif /* GCHD_TRANSCODER_FIELDS_H */
