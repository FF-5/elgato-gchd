# Transcoder command call-site ledger

**Branch:** `diagnostic-input-baseline`  
**Source:** [`src/gchd/transcoder.cpp`](https://github.com/FF-5/elgato-gchd/blob/diagnostic-input-baseline/src/gchd/transcoder.cpp)  
**Matching lines:** 305.

This ledger records source-ordered transcoder configuration calls and their arguments, including conditional calls. It is not a USB packet transcript: `sparam`, `slsi`, and `transcoderTableWrite` expand into the wire-level operations documented in [SOURCE_AUDIT.md](https://github.com/FF-5/elgato-gchd/blob/diagnostic-input-baseline/SOURCE_AUDIT.md). Dynamic expressions (for example values derived from input/transcoder settings) are deliberately retained as source expressions, not replaced with guessed runtime values.

| Line | Enclosing function | Call |
|---:|---|---|
| 36 | `transcoderWriteVideoAndAudioPids` | `void GCHD::transcoderWriteVideoAndAudioPids()` |
| 38 | `transcoderWriteVideoAndAudioPids` | `sparam( v_pid_out, videoPID );` |
| 39 | `transcoderWriteVideoAndAudioPids` | `sparam( v_pid_out_ts_out2, videoPID );` |
| 40 | `transcoderWriteVideoAndAudioPids` | `sparam( a_pid_out, audioPID );` |
| 41 | `transcoderWriteVideoAndAudioPids` | `sparam( a_pid_out_ts_out2, audioPID );` |
| 46 | `transcoderDefaultsInitialize` | `void GCHD::transcoderDefaultsInitialize()` |
| 49 | `transcoderDefaultsInitialize` | `sparam( v_vinpelclk, 0 );` |
| 50 | `transcoderDefaultsInitialize` | `sparam( tbc_mode, 1 );` |
| 51 | `transcoderDefaultsInitialize` | `sparam( stout_mode, 0 );` |
| 52 | `transcoderDefaultsInitialize` | `sparam( extra_info_bit, 0 );` |
| 53 | `transcoderDefaultsInitialize` | `sparam( tsout_packet_size, 0 );` |
| 54 | `transcoderDefaultsInitialize` | `sparam( stoutclk_hl, 0 );` |
| 55 | `transcoderDefaultsInitialize` | `sparam( dma_sel_out, 1 );` |
| 56 | `transcoderDefaultsInitialize` | `sparam( stoutclk_io, 0 );` |
| 57 | `transcoderDefaultsInitialize` | `sparam( stoutclk, 4 );` |
| 58 | `transcoderDefaultsInitialize` | `sparam( stout2_mode, 0 ); //8 bit parallel` |
| 59 | `transcoderDefaultsInitialize` | `sparam( stout2clk_hl, 0 );` |
| 60 | `transcoderDefaultsInitialize` | `sparam( stout2clk_io, 0 );` |
| 61 | `transcoderDefaultsInitialize` | `sparam( pcr_first, 0 );` |
| 62 | `transcoderDefaultsInitialize` | `sparam( pcr_first_ts_out2, 0 );` |
| 63 | `transcoderDefaultsInitialize` | `sparam( auto_null, 2 );` |
| 64 | `transcoderDefaultsInitialize` | `sparam( auto_null_ts_out2, 2 );` |
| 65 | `transcoderDefaultsInitialize` | `sparam( system_rate, 25000 );` |
| 66 | `transcoderDefaultsInitialize` | `sparam( system_rate_ts_out2, 8000 );` |
| 67 | `transcoderDefaultsInitialize` | `sparam( system_min_rate, 0 );` |
| 68 | `transcoderDefaultsInitialize` | `sparam( system_min_rate_ts_out2, 0 );` |
| 69 | `transcoderDefaultsInitialize` | `sparam( initial_time_stamp_h, 0 );` |
| 70 | `transcoderDefaultsInitialize` | `sparam( initial_time_stamp_l, 0 );` |
| 71 | `transcoderDefaultsInitialize` | `sparam( initial_time_stamp_h_ts_out2, 0 );` |
| 72 | `transcoderDefaultsInitialize` | `sparam( initial_time_stamp_l_ts_out2, 0 );` |
| 73 | `transcoderDefaultsInitialize` | `sparam( initial_stc_h, 0 );` |
| 74 | `transcoderDefaultsInitialize` | `sparam( initial_stc_l, 0 );` |
| 75 | `transcoderDefaultsInitialize` | `sparam( initial_stc_ll, 0 );` |
| 76 | `transcoderDefaultsInitialize` | `sparam( initial_stc_ext, 0 );` |
| 77 | `transcoderDefaultsInitialize` | `sparam( initial_stc_h_ts_out2, 0 );` |
| 78 | `transcoderDefaultsInitialize` | `sparam( initial_stc_l_ts_out2, 0 );` |
| 79 | `transcoderDefaultsInitialize` | `sparam( initial_stc_ll_ts_out2, 0 );` |
| 80 | `transcoderDefaultsInitialize` | `sparam( initial_stc_ext_ts_out2, 0 );` |
| 82 | `transcoderDefaultsInitialize` | `transcoderWriteVideoAndAudioPids();` |
| 84 | `transcoderDefaultsInitialize` | `sparam( sit_pid_out, sitPID );` |
| 85 | `transcoderDefaultsInitialize` | `sparam( sit_pid_out_ts_out2, sitPID );` |
| 86 | `transcoderDefaultsInitialize` | `sparam( pmt_pid_out, pmtPID );` |
| 87 | `transcoderDefaultsInitialize` | `sparam( pmt_pid_out_ts_out2, pmtPID );` |
| 88 | `transcoderDefaultsInitialize` | `sparam( pcr_pid_out, pcrPID );` |
| 89 | `transcoderDefaultsInitialize` | `sparam( pcr_pid_out_ts_out2, pcrPID );` |
| 90 | `transcoderDefaultsInitialize` | `sparam( thumbnail_pid_out_ts_out2, thumbnailPID );` |
| 91 | `transcoderDefaultsInitialize` | `sparam( v_sid_out, videoSID );` |
| 92 | `transcoderDefaultsInitialize` | `sparam( v_sid_out_ts_out2, videoSID );` |
| 93 | `transcoderDefaultsInitialize` | `sparam( pat_cyc, 0 );` |
| 94 | `transcoderDefaultsInitialize` | `sparam( pat_cyc_ts_out2, 0 );` |
| 95 | `transcoderDefaultsInitialize` | `sparam( pmt_cyc, 0 );` |
| 96 | `transcoderDefaultsInitialize` | `sparam( pmt_cyc_ts_out2, 0 );` |
| 97 | `transcoderDefaultsInitialize` | `sparam( pcr_cyc, 0 );` |
| 98 | `transcoderDefaultsInitialize` | `sparam( pcr_cyc_ts_out2, 0 );` |
| 99 | `transcoderDefaultsInitialize` | `sparam( sit_cyc, 0 );` |
| 100 | `transcoderDefaultsInitialize` | `sparam( sit_cyc_ts_out2, 0 );` |
| 113 | `transcoderDefaultsInitialize` | `sparam( pat_length, patVector.size() );` |
| 114 | `transcoderDefaultsInitialize` | `transcoderTableWrite( pat_table_start, patVector );` |
| 117 | `transcoderDefaultsInitialize` | `sparam( pat_length_ts_out2, patVector.size() );` |
| 118 | `transcoderDefaultsInitialize` | `transcoderTableWrite( pat_table_ts_out2_start, patVector );` |
| 126 | `transcoderDefaultsInitialize` | `sparam( sit_length, sitVector.size() );` |
| 127 | `transcoderDefaultsInitialize` | `transcoderTableWrite( sit_table_start, sitVector );` |
| 130 | `transcoderDefaultsInitialize` | `sparam( sit_length_ts_out2, sitVector.size() );` |
| 131 | `transcoderDefaultsInitialize` | `transcoderTableWrite( sit_table_ts_out2_start, sitVector );` |
| 161 | `transcoderDefaultsInitialize` | `sparam( a_ts_out_mode, 2 );` |
| 162 | `transcoderDefaultsInitialize` | `sparam( a_ts_out2_mode, 2 );` |
| 163 | `transcoderDefaultsInitialize` | `sparam( a_mode, 3 );` |
| 164 | `transcoderDefaultsInitialize` | `sparam( a_sid_out, audioSID );` |
| 167 | `transcoderDefaultsInitialize` | `sparam( pmt_length, pmtVector.size() );` |
| 168 | `transcoderDefaultsInitialize` | `transcoderTableWrite( pmt_table_start, pmtVector );` |
| 170 | `transcoderDefaultsInitialize` | `sparam( a_ts_out2_mode, 0 );` |
| 171 | `transcoderDefaultsInitialize` | `sparam( a2_mode, 0 );` |
| 172 | `transcoderDefaultsInitialize` | `sparam( a_sid_out_ts_out2, audioSID );` |
| 174 | `transcoderDefaultsInitialize` | `sparam( pmt_length_ts_out2, pmtVector.size() );` |
| 175 | `transcoderDefaultsInitialize` | `transcoderTableWrite( pmt_table_ts_out2_start, pmtVector );` |
| 177 | `transcoderDefaultsInitialize` | `sparam( v_v420, 1 );` |
| 178 | `transcoderDefaultsInitialize` | `slsi( 0x1710, 0x00fc );` |
| 179 | `transcoderDefaultsInitialize` | `slsi( 0x1712, 0x0024 );` |
| 180 | `transcoderDefaultsInitialize` | `slsi( 0x1714, 0x00f8 );` |
| 181 | `transcoderDefaultsInitialize` | `slsi( 0x1716, 0x00e8 );` |
| 182 | `transcoderDefaultsInitialize` | `slsi( 0x1718, 0x00da );` |
| 183 | `transcoderDefaultsInitialize` | `slsi( 0x171a, 0x00c6 );` |
| 184 | `transcoderDefaultsInitialize` | `slsi( 0x171c, 0x0076 );` |
| 185 | `transcoderDefaultsInitialize` | `slsi( 0x171e, 0x00ea );` |
| 186 | `transcoderDefaultsInitialize` | `sparam( vin_swap, 1 );` |
| 187 | `transcoderDefaultsInitialize` | `sparam( v_format, 4 );` |
| 188 | `transcoderDefaultsInitialize` | `sparam( v_ip_format, 0 );` |
| 189 | `transcoderDefaultsInitialize` | `sparam( tfrff, 2 );` |
| 190 | `transcoderDefaultsInitialize` | `sparam( v_eos, 0 );` |
| 191 | `transcoderDefaultsInitialize` | `sparam( v_eos_bl, 0 );` |
| 192 | `transcoderDefaultsInitialize` | `sparam( v_cl_gop, 0 );` |
| 193 | `transcoderDefaultsInitialize` | `sparam( v_cl_gop_bl, 0 );` |
| 194 | `transcoderDefaultsInitialize` | `sparam( v_temporal_direct_on, 1 );` |
| 195 | `transcoderDefaultsInitialize` | `sparam( v_vlc_mode, 0 );` |
| 196 | `transcoderDefaultsInitialize` | `sparam( v_vlc_mode_bl, 1 );` |
| 197 | `transcoderDefaultsInitialize` | `sparam( v_filler, 0 );` |
| 198 | `transcoderDefaultsInitialize` | `sparam( v_filler_bl, 0 );` |
| 199 | `transcoderDefaultsInitialize` | `sparam( v_rate_mode, 1 );` |
| 200 | `transcoderDefaultsInitialize` | `sparam( v_rate_mode_bl, 1 );` |
| 201 | `transcoderDefaultsInitialize` | `sparam( v_gop_size, 15 );` |
| 202 | `transcoderDefaultsInitialize` | `sparam( v_gop_size_bl, 15 );` |
| 203 | `transcoderDefaultsInitialize` | `sparam( v_gop_struct, 0 );` |
| 204 | `transcoderDefaultsInitialize` | `sparam( v_gop_struct_bl, 1 );` |
| 205 | `transcoderDefaultsInitialize` | `sparam( v_idr_interval_h, 0 );` |
| 206 | `transcoderDefaultsInitialize` | `sparam( v_idr_interval_l, 0 );` |
| 207 | `transcoderDefaultsInitialize` | `sparam( v_idr_interval_h_bl, 0 );` |
| 208 | `transcoderDefaultsInitialize` | `sparam( v_idr_interval_l_bl, 0 );` |
| 209 | `transcoderDefaultsInitialize` | `sparam( v_h264_profile, 1 );` |
| 210 | `transcoderDefaultsInitialize` | `sparam( v_h264_level, 41 );` |
| 211 | `transcoderDefaultsInitialize` | `sparam( v_hsize_out, 720 );` |
| 212 | `transcoderDefaultsInitialize` | `sparam( v_vsize_out, 480 );` |
| 213 | `transcoderDefaultsInitialize` | `sparam( v_h264_profile_bl, 3 );` |
| 214 | `transcoderDefaultsInitialize` | `sparam( v_h264_level_bl, 12 );` |
| 215 | `transcoderDefaultsInitialize` | `sparam( v_hsize_out_bl, 320 );` |
| 216 | `transcoderDefaultsInitialize` | `sparam( v_vsize_out_bl, 240 );` |
| 217 | `transcoderDefaultsInitialize` | `sparam( v_bitrate, 16000 );` |
| 218 | `transcoderDefaultsInitialize` | `sparam( v_max_bitrate, 14000 );` |
| 219 | `transcoderDefaultsInitialize` | `sparam( v_ave_bitrate, 8000 );` |
| 220 | `transcoderDefaultsInitialize` | `sparam( v_min_bitrate, 0 );` |
| 221 | `transcoderDefaultsInitialize` | `sparam( v_bitrate_bl, 10000 );` |
| 222 | `transcoderDefaultsInitialize` | `sparam( v_max_bitrate_bl, 6000 );` |
| 223 | `transcoderDefaultsInitialize` | `sparam( v_ave_bitrate_bl, 1800 );` |
| 224 | `transcoderDefaultsInitialize` | `sparam( v_min_bitrate_bl, 0 );` |
| 225 | `transcoderDefaultsInitialize` | `sparam( error_rate_ipic, 0 );` |
| 226 | `transcoderDefaultsInitialize` | `sparam( error_rate_ppic, 0 );` |
| 227 | `transcoderDefaultsInitialize` | `sparam( error_rate_refpic, 0 );` |
| 228 | `transcoderDefaultsInitialize` | `sparam( error_rate_bpic, 0 );` |
| 229 | `transcoderDefaultsInitialize` | `sparam( v_error_level_th_h, 0 );` |
| 230 | `transcoderDefaultsInitialize` | `sparam( v_error_level_th_l, 0 );` |
| 231 | `transcoderDefaultsInitialize` | `sparam( insert_buf_ctrl_param, 1 );` |
| 232 | `transcoderDefaultsInitialize` | `sparam( insert_buf_ctrl_param_bl, 1 );` |
| 233 | `transcoderDefaultsInitialize` | `sparam( insert_pic_struct, 1 );` |
| 234 | `transcoderDefaultsInitialize` | `sparam( insert_pic_struct_bl, 1 );` |
| 235 | `transcoderDefaultsInitialize` | `sparam( insert_recovery_point_sei, 1 );` |
| 236 | `transcoderDefaultsInitialize` | `sparam( insert_recovery_point_sei_bl, 1 );` |
| 237 | `transcoderDefaultsInitialize` | `sparam( overscan_info_present_flag, 1 );` |
| 238 | `transcoderDefaultsInitialize` | `sparam( overscan_info_present_flag_bl, 1 );` |
| 239 | `transcoderDefaultsInitialize` | `sparam( overscan_appropriate_flag, 1 );` |
| 240 | `transcoderDefaultsInitialize` | `sparam( overscan_appropriate_flag_bl, 1 );` |
| 241 | `transcoderDefaultsInitialize` | `sparam( insert_restriction, 1 );` |
| 242 | `transcoderDefaultsInitialize` | `sparam( insert_restriction_bl, 1 );` |
| 243 | `transcoderDefaultsInitialize` | `sparam( video_signal_type_present_flag, 0 );` |
| 244 | `transcoderDefaultsInitialize` | `sparam( video_signal_type_present_flag_bl, 0 );` |
| 245 | `transcoderDefaultsInitialize` | `sparam( video_format, 0 );` |
| 246 | `transcoderDefaultsInitialize` | `sparam( video_format_bl, 0 );` |
| 247 | `transcoderDefaultsInitialize` | `sparam( video_full_range_flag, 0 );` |
| 248 | `transcoderDefaultsInitialize` | `sparam( video_full_range_flag_bl, 0 );` |
| 249 | `transcoderDefaultsInitialize` | `sparam( colour_description_present_flag, 0 );` |
| 250 | `transcoderDefaultsInitialize` | `sparam( colour_description_present_flag_bl, 0 );` |
| 251 | `transcoderDefaultsInitialize` | `sparam( colour_primaries, 0 );` |
| 252 | `transcoderDefaultsInitialize` | `sparam( colour_primaries_bl, 0 );` |
| 253 | `transcoderDefaultsInitialize` | `sparam( transfer_characteristics, 0 );` |
| 254 | `transcoderDefaultsInitialize` | `sparam( transfer_characteristics_bl, 0 );` |
| 255 | `transcoderDefaultsInitialize` | `sparam( matrix_coefficients, 0 );` |
| 256 | `transcoderDefaultsInitialize` | `sparam( matrix_coefficients_bl, 0 );` |
| 257 | `transcoderDefaultsInitialize` | `sparam( time_scale_h_bl, 0 );` |
| 258 | `transcoderDefaultsInitialize` | `sparam( time_scale_l_bl, 0 );` |
| 259 | `transcoderDefaultsInitialize` | `sparam( num_units_in_tick_bl, 0 );` |
| 260 | `transcoderDefaultsInitialize` | `sparam( a_sample, 0 );` |
| 261 | `transcoderDefaultsInitialize` | `sparam( ain_master, 0 );` |
| 262 | `transcoderDefaultsInitialize` | `sparam( ain_lsb_first, 0 );` |
| 263 | `transcoderDefaultsInitialize` | `sparam( ain_bit_posb, 0 );` |
| 264 | `transcoderDefaultsInitialize` | `sparam( amck_mode, 1 );` |
| 265 | `transcoderDefaultsInitialize` | `sparam( ain_dlength, 2 );` |
| 266 | `transcoderDefaultsInitialize` | `sparam( ain_lr_hlset, 1 );` |
| 267 | `transcoderDefaultsInitialize` | `sparam( ain_i2s, 1 );` |
| 268 | `transcoderDefaultsInitialize` | `sparam( ain_lrclk_sel, 0 );` |
| 269 | `transcoderDefaultsInitialize` | `sparam( ain_sclk_sel, 0 );` |
| 270 | `transcoderDefaultsInitialize` | `sparam( ain_bclk_sel, 0 );` |
| 271 | `transcoderDefaultsInitialize` | `sparam( ain_bclk_hl, 0 );` |
| 272 | `transcoderDefaultsInitialize` | `sparam( ain_offset_mode, 0 );` |
| 273 | `transcoderDefaultsInitialize` | `sparam( ain_offset, 0 );` |
| 274 | `transcoderDefaultsInitialize` | `sparam( lch_scale, 32768 );` |
| 275 | `transcoderDefaultsInitialize` | `sparam( rch_scale, 32768 );` |
| 276 | `transcoderDefaultsInitialize` | `sparam( lsch_scale, 32768 );` |
| 277 | `transcoderDefaultsInitialize` | `sparam( rsch_scale, 32768 );` |
| 278 | `transcoderDefaultsInitialize` | `sparam( cch_scale, 32768 );` |
| 279 | `transcoderDefaultsInitialize` | `sparam( lfech_scale, 32768 );` |
| 280 | `transcoderDefaultsInitialize` | `sparam( a_bitrate, 384 );` |
| 281 | `transcoderDefaultsInitialize` | `sparam( e_mpeg_emp, 0 );` |
| 282 | `transcoderDefaultsInitialize` | `sparam( e_mpeg_orig, 1 );` |
| 283 | `transcoderDefaultsInitialize` | `sparam( e_mpeg_copyr, 0 );` |
| 284 | `transcoderDefaultsInitialize` | `sparam( e_mpeg_mode, 0 );` |
| 285 | `transcoderDefaultsInitialize` | `sparam( e_mpeg_protect, 0 );` |
| 286 | `transcoderDefaultsInitialize` | `sparam( ach_sel, 0 );` |
| 290 | `transcoderSetup` | `void GCHD::transcoderSetup(InputSettings &inputSettings, TranscoderSettings &settings)` |
| 432 | `transcoderSetup` | `sparam( 0x1574, 15, 1, 0 );` |
| 433 | `transcoderSetup` | `sparam( v_max_cpb_delay, 3 );` |
| 443 | `transcoderSetup` | `sparam( system_rate, systemBitRate ); //Always set higher than the bitRate...` |
| 444 | `transcoderSetup` | `sparam( system_min_rate, 0 );` |
| 450 | `transcoderSetup` | `sparam( v_cl_gop, useGroupOfPictures );` |
| 451 | `transcoderSetup` | `sparam( v_gop_struct, distanceBetweenAnchorFrames );` |
| 452 | `transcoderSetup` | `sparam( v_gop_size, gopGroupSize );` |
| 453 | `transcoderSetup` | `sparam( v_vlc_mode, variableLengthCodingMode );` |
| 455 | `transcoderSetup` | `sparam( v_vbr_converge_mode, 0 ); //not sure what it does.` |
| 457 | `transcoderSetup` | `sparam( v_rate_mode, bitRateMode );` |
| 459 | `transcoderSetup` | `sparam( v_bitrate, bitRate );` |
| 460 | `transcoderSetup` | `sparam( v_max_bitrate, maxBitRate );` |
| 461 | `transcoderSetup` | `sparam( v_ave_bitrate, averageBitRate);` |
| 462 | `transcoderSetup` | `sparam( v_min_bitrate, minBitRate );` |
| 465 | `transcoderSetup` | `sparam( v_hsize_out, horizontal );` |
| 466 | `transcoderSetup` | `sparam( v_vsize_out, vertical );` |
| 468 | `transcoderSetup` | `sparam( v_vinpelclk, fixedFrameRate ); //This seems to be set to 1 when doing fixed frame rate....but might be for complicated` |
| 471 | `transcoderSetup` | `sparam( time_scale_h, fpsFraction.num>>16 ); //USED FOR SETTING FIXED FRAME -frame numerator here` |
| 472 | `transcoderSetup` | `sparam( time_scale_l, fpsFraction.num & 0xffff );` |
| 473 | `transcoderSetup` | `sparam( num_units_in_tick, fpsFraction.denom ); //FRAME DENOMINATOR HERE` |
| 475 | `transcoderSetup` | `sparam( v_idr_interval_h, idrInterval>>16 );` |
| 476 | `transcoderSetup` | `sparam( v_idr_interval_l, idrInterval & 0xffff );` |
| 481 | `transcoderSetup` | `sparam( 0x151c, 0, 5, 0 );` |
| 484 | `transcoderSetup` | `sparam( v_h264_profile, h264Profile );` |
| 485 | `transcoderSetup` | `sparam( v_h264_level, h264Level );` |
| 490 | `transcoderSetup` | `sparam( a_sample, 0 ); // 2 bit unknown field.` |
| 492 | `transcoderSetup` | `sparam( e_mpeg_protect, audioProtect );` |
| 493 | `transcoderSetup` | `sparam( e_mpeg_mode, audioMode ); //Set mpeg audio mode?` |
| 494 | `transcoderSetup` | `sparam( e_mpeg_copyr, audioCopyright );` |
| 495 | `transcoderSetup` | `sparam( e_mpeg_orig, audioOriginal );` |
| 496 | `transcoderSetup` | `sparam( e_mpeg_emp, audioMpegEmphasis );` |
| 497 | `transcoderSetup` | `sparam( a_bitrate, audioBitRate );` |
| 502 | `transcoderSetup` | `sparam( colour_description_present_flag, colourDescriptionPresentSetting );` |
| 503 | `transcoderSetup` | `sparam( colour_primaries, colourPrimariesSetting );` |
| 504 | `transcoderSetup` | `sparam( transfer_characteristics, transferCharacteristicsSetting );` |
| 505 | `transcoderSetup` | `sparam( matrix_coefficients, matrixCoefficientsSetting );` |
| 506 | `transcoderSetup` | `sparam( video_full_range_flag, videoFullRangeFlagSetting );` |
| 511 | `transcoderSetup` | `sparam( video_signal_type_present_flag, videoSignalTypePresentFlagSetting );` |
| 514 | `transcoderSetup` | `sparam( video_format, videoFormatSetting );` |
| 515 | `transcoderSetup` | `sparam( video_full_range_flag, videoFullRangeFlagSetting ); //Still set to 0` |
| 521 | `transcoderSetup` | `sparam( 0x1674, 15, 1, 0 );` |
| 522 | `transcoderSetup` | `sparam( v_max_cpb_delay_bl, 3 );` |
| 525 | `transcoderSetup` | `sparam( system_rate_ts_out2, systemBitRate );` |
| 526 | `transcoderSetup` | `sparam( system_min_rate_ts_out2, 0 );` |
| 529 | `transcoderSetup` | `sparam( v_cl_gop_bl, useGroupOfPictures );` |
| 531 | `transcoderSetup` | `sparam( v_gop_struct_bl, distanceBetweenAnchorFrames );` |
| 532 | `transcoderSetup` | `sparam( v_gop_size_bl, gopGroupSize);` |
| 533 | `transcoderSetup` | `sparam( v_vlc_mode_bl, variableLengthCodingMode );` |
| 534 | `transcoderSetup` | `sparam( v_rate_mode_bl, bitRateMode );` |
| 535 | `transcoderSetup` | `sparam( v_vbr_converge_mode_bl, 0 );` |
| 536 | `transcoderSetup` | `sparam( v_bitrate_bl, bitRate );` |
| 537 | `transcoderSetup` | `sparam( v_max_bitrate_bl, maxBitRate );` |
| 538 | `transcoderSetup` | `sparam( v_ave_bitrate_bl, averageBitRate );` |
| 539 | `transcoderSetup` | `sparam( v_min_bitrate_bl, minBitRate );` |
| 540 | `transcoderSetup` | `sparam( v_filler_bitrate_bl, 0 );` |
| 541 | `transcoderSetup` | `sparam( v_hsize_out_bl, horizontal );` |
| 542 | `transcoderSetup` | `sparam( v_vsize_out_bl, vertical );` |
| 544 | `transcoderSetup` | `sparam( tfrff, 2 );` |
| 546 | `transcoderSetup` | `sparam( v_idr_interval_h_bl, idrInterval>>16 );` |
| 547 | `transcoderSetup` | `sparam( v_idr_interval_l_bl, idrInterval & 0xffff );` |
| 552 | `transcoderSetup` | `sparam( 0x161c, 0, 5, 0 );` |
| 555 | `transcoderSetup` | `sparam( v_h264_profile_bl, h264Profile );` |
| 556 | `transcoderSetup` | `sparam( v_h264_level_bl, 31 ); //Always set to 0x31 by driver, rather than calc value.` |
| 558 | `transcoderSetup` | `sparam( time_scale_h_bl, fpsFraction.num>>16 ); //USED FOR SETTING FIXED FRAME -frame numerator here` |
| 559 | `transcoderSetup` | `sparam( time_scale_l_bl, fpsFraction.num & 0xffff );` |
| 560 | `transcoderSetup` | `sparam( num_units_in_tick_bl, fpsFraction.denom ); //FRAME DENOMINATOR HERE` |
| 562 | `transcoderSetup` | `sparam( ach_sel_bl, 0 );` |
| 564 | `transcoderSetup` | `sparam( e_mpeg_protect_bl, audioProtect );` |
| 565 | `transcoderSetup` | `sparam( e_mpeg_mode_bl, audioMode );` |
| 566 | `transcoderSetup` | `sparam( e_mpeg_copyr_bl, audioCopyright );` |
| 567 | `transcoderSetup` | `sparam( e_mpeg_orig_bl, audioOriginal );` |
| 568 | `transcoderSetup` | `sparam( e_mpeg_emp_bl, audioMpegEmphasis );` |
| 569 | `transcoderSetup` | `sparam( a_bitrate_bl, audioBitRate );` |
| 571 | `transcoderSetup` | `sparam( colour_description_present_flag_bl, colourDescriptionPresentSetting );` |
| 572 | `transcoderSetup` | `sparam( colour_primaries_bl, colourPrimariesSetting );` |
| 573 | `transcoderSetup` | `sparam( transfer_characteristics_bl, transferCharacteristicsSetting );` |
| 574 | `transcoderSetup` | `sparam( matrix_coefficients_bl, matrixCoefficientsSetting );` |
| 575 | `transcoderSetup` | `sparam( video_full_range_flag_bl, videoFullRangeFlagSetting );` |
| 576 | `transcoderSetup` | `sparam( video_signal_type_present_flag_bl, videoSignalTypePresentFlagSetting );` |
| 577 | `transcoderSetup` | `sparam( video_format_bl, videoFormatSetting );` |
| 578 | `transcoderSetup` | `sparam( video_full_range_flag_bl, videoFullRangeFlagSetting );` |
| 579 | `transcoderSetup` | `sparam( sout_mode, 0 );` |
| 580 | `transcoderSetup` | `sparam( sout2_mode, 0 );` |
| 581 | `transcoderSetup` | `sparam( v_ts_out_mode, 1 ); //Encode (H.264/MPEG2)` |
| 582 | `transcoderSetup` | `sparam( a_ts_out_mode, 2 ); //Encode audio` |
| 583 | `transcoderSetup` | `sparam( a_mode, 3 ); //Always 3 for MPEG-1 layer 2` |
| 584 | `transcoderSetup` | `sparam( v_ts_out2_mode, 0 ); //1 first time through original driver` |
| 585 | `transcoderSetup` | `sparam( a_ts_out2_mode, 0 ); //2 first time through original driver` |
| 586 | `transcoderSetup` | `sparam( a2_mode, 0 ); //3 first time through original driver` |
| 590 | `transcoderOutputEnable` | `void GCHD::transcoderOutputEnable(bool value)` |
| 592 | `transcoderOutputEnable` | `sparam( amck_mode, value );` |
| 596 | `transcoderFinalConfigure` | `void GCHD::transcoderFinalConfigure(InputSettings &inputSettings, TranscoderSettings &settings)` |
| 598 | `transcoderFinalConfigure` | `transcoderWriteVideoAndAudioPids();` |
| 599 | `transcoderFinalConfigure` | `sparam( dma_sel_out, 1 );` |
| 620 | `transcoderFinalConfigure` | `sparam( v_in_source_type, inSourceType );` |
| 678 | `transcoderFinalConfigure` | `sparam( v_format, videoFormat );` |
| 682 | `transcoderFinalConfigure` | `sparam( v_ip_format, 0);` |
| 684 | `transcoderFinalConfigure` | `sparam( v_ip_format, 2);` |
| 686 | `transcoderFinalConfigure` | `sparam( tfrff, 2 );` |
| 687 | `transcoderFinalConfigure` | `sparam( tbc_mode, 1 );` |
| 688 | `transcoderFinalConfigure` | `sparam( tbc_mode, 1 ); //probably artifact of function calls that this is repeated.` |
| 689 | `transcoderFinalConfigure` | `sparam( a_sample, 0 );` |
| 690 | `transcoderFinalConfigure` | `sparam( ain_master, 0 );` |
| 691 | `transcoderFinalConfigure` | `sparam( ain_lsb_first, 0 );` |
| 692 | `transcoderFinalConfigure` | `sparam( ain_bit_posb, 0 );` |
| 693 | `transcoderFinalConfigure` | `sparam( amck_mode, 1 );` |
| 694 | `transcoderFinalConfigure` | `sparam( ain_dlength, 2 );` |
| 695 | `transcoderFinalConfigure` | `sparam( ain_lr_hlset, 1 );` |
| 696 | `transcoderFinalConfigure` | `sparam( ain_i2s, 1 );` |
| 697 | `transcoderFinalConfigure` | `sparam( ain_lrclk_sel, 0 );` |
| 698 | `transcoderFinalConfigure` | `sparam( ain_sclk_sel, 0 );` |
| 699 | `transcoderFinalConfigure` | `sparam( ain_bclk_sel, 0 );` |
| 700 | `transcoderFinalConfigure` | `sparam( ain_bclk_hl, 0 );` |
| 701 | `transcoderFinalConfigure` | `sparam( ain_offset_mode, 0 );` |
| 702 | `transcoderFinalConfigure` | `sparam( ain_offset, 0 );` |
| 703 | `transcoderFinalConfigure` | `sparam( v_disable_aspect_ratio_info_present_flag, 0 );` |
| 704 | `transcoderFinalConfigure` | `sparam( v_disable_aspect_ratio_info_present_flag_bl, 0 );` |
| 705 | `transcoderFinalConfigure` | `sparam( v_pic_order_present_flag, 1 );` |
| 706 | `transcoderFinalConfigure` | `sparam( v_pic_order_present_flag_bl, 1 );` |
