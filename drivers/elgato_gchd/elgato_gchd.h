#ifndef ELGATO_GCHD_H
#define ELGATO_GCHD_H

#include <linux/list.h>
#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/usb.h>
#include <linux/videodev2.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-vmalloc.h>

#define GCHD_VID 0x0fd9
#define GCHD_PID_0 0x0044
#define GCHD_PID_1 0x004e
#define GCHD_PID_2 0x0051
#define GCHD_PID_3 0x005d
#define GCHD_PID GCHD_PID_3
#define GCHD_EP_IN 0x81
#define GCHD_EP_OUT 0x02
#define GCHD_EP_INT 0x83
#define GCHD_USB_BUFSIZE 0x4000
#define GCHD_RING_FRAMES 32
#define GCHD_RING_BYTES (64ULL * 1024ULL * 1024ULL)
#define GCHD_MAX_FRAME (8U * 1024U * 1024U)

/* USB vendor-control protocol identifiers and register indices. */
#define GCHD_REQ_READ                    0xc0
#define GCHD_REQ_WRITE                   0x40
#define GCHD_REQ_REGISTER                0xbc
#define GCHD_REQ_MAILBOX                 0xbd
#define GCHD_REQ_SCMD                    0xb8

#define GCHD_REG_BANK_SELECT             0x0000
#define GCHD_REG_ENABLE                  0x0018
#define GCHD_REG_ENABLE_STATE            0x0014
#define GCHD_REG_MAILBOX_READY           0x001c
#define GCHD_REG_STATE                   0x2008
#define GCHD_REG_STATE_COMPLETE          0x0074
#define GCHD_REG_HDNEW_INTERRUPT_STATUS  0x0016
#define GCHD_REG_HDNEW_SCMD_READBACK     0x0014
#define GCHD_REG_HDNEW_MAILBOX_READ      0x23be
#define GCHD_REG_HDNEW_MAILBOX_WRITE     0x00c0
#define GCHD_REG_HDNEW_MAILBOX_CONFIG    0x0000

/*
 * SCMD protocol commands and state values.
 * Keep command IDs separate from the state values used to confirm them.
 */
#define SCMD_IDLE                 0x01
#define SCMD_RESET                0x02
#define SCMD_INIT                 0x04
#define SCMD_STATE_CHANGE         0x05

#define GCHD_STATE_BOOT           0x00
#define GCHD_STATE_STOP           0x01
#define GCHD_STATE_START          0x02
#define GCHD_STATE_NULL           0x04
#define GCHD_STATE_RESET          0x10
#define GCHD_STATE_IDLE           0x11
#define GCHD_STATE_SHUTDOWN       0x12

#define GCHD_STATE_READ_REQUEST   0x0800
#define GCHD_STATE_STATUS_REQUEST 0x0900
#define GCHD_STATE_COMPLETE_ACK   0x0004
#define GCHD_STATE_COMPLETE_CLEAR_INDEX 0x01b0
 
/* Mailbox ports used by the device command protocol. */
#define GCHD_MAIL_PORT_SYSTEM          0x33
#define GCHD_MAIL_PORT_ENCODER         0x44
#define GCHD_MAIL_PORT_VIDEO           0x4c
#define GCHD_MAIL_PORT_COMPONENT       0x4e

/* Mailbox command opcodes and command-family selectors. */
#define GCHD_SCMD_MODE_DEFAULT         0x00
#define GCHD_SCMD_MODE_HDNEW           0xa0

#define GCHD_CMD_QUERY                 0x99
#define GCHD_CMD_QUERY_EXT             0x89
#define GCHD_CMD_REGISTER_READ         0x9d
#define GCHD_CMD_REGISTER_READ_EXT     0xcd
#define GCHD_CMD_ENCODER_POLL          0xab
#define GCHD_CMD_ENCODER_POLL_EXT      0xa9
#define GCHD_CMD_ENCODER_POLL_MODE     0x0f
#define GCHD_CMD_ENCODER_POLL_ARG      0xa4
#define GCHD_CMD_ENCODER_POLL_INITIAL   0x5b
#define GCHD_CMD_ENCODER_POLL_CONTINUE  0x55
#define GCHD_CMD_ENCODER_STATUS        0x43
#define GCHD_CMD_ENCODER_STATUS_EXT    0x23
#define GCHD_CMD_ENCODER_STATUS_ARG    0x84

/* Recognized mailbox response/status bytes. */
#define GCHD_REPLY_COMMAND_ACK         0x6e
#define GCHD_REPLY_COMMAND_PENDING      0x02
#define GCHD_REPLY_COMMAND_COMPLETE     0x73
#define GCHD_REPLY_ENCODER_READY       0xf7
#define GCHD_REPLY_STATUS_COMPOSITE    0x66
#define GCHD_REPLY_STATUS_SVIDEO       0x7e


/* Named mailbox register indices, grouped by destination port. */
#define GCHD_ENCODER_REG_02 0x02
#define GCHD_ENCODER_REG_03 0x03
#define GCHD_ENCODER_REG_05 0x05
#define GCHD_ENCODER_REG_06 0x06
#define GCHD_ENCODER_REG_07 0x07
#define GCHD_ENCODER_REG_08 0x08
#define GCHD_ENCODER_REG_09 0x09
#define GCHD_ENCODER_REG_0A 0x0a
#define GCHD_ENCODER_REG_14 0x14
#define GCHD_ENCODER_REG_19 0x19
#define GCHD_ENCODER_REG_1A 0x1a
#define GCHD_ENCODER_REG_1B 0x1b
#define GCHD_ENCODER_REG_28 0x28
#define GCHD_ENCODER_REG_29 0x29
#define GCHD_ENCODER_REG_2D 0x2d
#define GCHD_ENCODER_REG_3C 0x3c
#define GCHD_ENCODER_REG_4C 0x4c
#define GCHD_ENCODER_REG_55 0x55
#define GCHD_ENCODER_REG_6B 0x6b
#define GCHD_ENCODER_REG_6C 0x6c
#define GCHD_ENCODER_REG_6D 0x6d
#define GCHD_ENCODER_REG_6E 0x6e
#define GCHD_VIDEO_REG_04 0x04
#define GCHD_VIDEO_REG_05 0x05
#define GCHD_VIDEO_REG_09 0x09
#define GCHD_VIDEO_REG_0A 0x0a
#define GCHD_VIDEO_REG_0B 0x0b
#define GCHD_VIDEO_REG_0C 0x0c
#define GCHD_VIDEO_REG_0E 0x0e
#define GCHD_VIDEO_REG_0F 0x0f
#define GCHD_VIDEO_REG_10 0x10
#define GCHD_VIDEO_REG_11 0x11
#define GCHD_VIDEO_REG_12 0x12
#define GCHD_VIDEO_REG_13 0x13
#define GCHD_VIDEO_REG_14 0x14
#define GCHD_VIDEO_REG_15 0x15
#define GCHD_VIDEO_REG_33 0x33
#define GCHD_VIDEO_REG_34 0x34
#define GCHD_VIDEO_REG_35 0x35
#define GCHD_VIDEO_REG_58 0x58
#define GCHD_VIDEO_REG_59 0x59
#define GCHD_VIDEO_REG_5A 0x5a
#define GCHD_VIDEO_REG_5B 0x5b
#define GCHD_VIDEO_REG_5C 0x5c
#define GCHD_VIDEO_REG_5D 0x5d
#define GCHD_VIDEO_REG_5E 0x5e
#define GCHD_VIDEO_REG_5F 0x5f
#define GCHD_VIDEO_REG_60 0x60
#define GCHD_VIDEO_REG_61 0x61
#define GCHD_VIDEO_REG_62 0x62
#define GCHD_VIDEO_REG_63 0x63
#define GCHD_VIDEO_REG_64 0x64
#define GCHD_VIDEO_REG_65 0x65
#define GCHD_VIDEO_REG_68 0x68
#define GCHD_VIDEO_REG_6B 0x6b
#define GCHD_VIDEO_REG_6C 0x6c
#define GCHD_VIDEO_REG_6D 0x6d
#define GCHD_VIDEO_REG_72 0x72
#define GCHD_VIDEO_REG_91 0x91
#define GCHD_VIDEO_REG_92 0x92
#define GCHD_VIDEO_REG_93 0x93
#define GCHD_VIDEO_REG_94 0x94
#define GCHD_VIDEO_REG_95 0x95
#define GCHD_VIDEO_REG_96 0x96
#define GCHD_VIDEO_REG_97 0x97
#define GCHD_VIDEO_REG_98 0x98
#define GCHD_VIDEO_REG_99 0x99
#define GCHD_VIDEO_REG_C0 0xc0
#define GCHD_VIDEO_REG_C1 0xc1
#define GCHD_VIDEO_REG_C5 0xc5
#define GCHD_VIDEO_REG_C6 0xc6
#define GCHD_VIDEO_REG_C9 0xc9
#define GCHD_VIDEO_REG_CA 0xca
#define GCHD_VIDEO_REG_CB 0xcb
#define GCHD_VIDEO_REG_CC 0xcc
#define GCHD_VIDEO_REG_CD 0xcd
#define GCHD_VIDEO_REG_CE 0xce
#define GCHD_VIDEO_REG_CF 0xcf
#define GCHD_VIDEO_REG_D0 0xd0
#define GCHD_VIDEO_REG_E0 0xe0
#define GCHD_VIDEO_REG_E1 0xe1
#define GCHD_VIDEO_REG_E2 0xe2
#define GCHD_VIDEO_REG_E3 0xe3
#define GCHD_VIDEO_REG_E4 0xe4
#define GCHD_COMPONENT_REG_00 0x00
#define GCHD_COMPONENT_REG_01 0x01
#define GCHD_COMPONENT_REG_02 0x02
#define GCHD_COMPONENT_REG_03 0x03
#define GCHD_COMPONENT_REG_04 0x04
#define GCHD_COMPONENT_REG_05 0x05
#define GCHD_COMPONENT_REG_06 0x06
#define GCHD_COMPONENT_REG_07 0x07
#define GCHD_COMPONENT_REG_08 0x08
#define GCHD_COMPONENT_REG_09 0x09
#define GCHD_COMPONENT_REG_0F 0x0f
#define GCHD_COMPONENT_REG_16 0x16
#define GCHD_COMPONENT_REG_17 0x17
#define GCHD_COMPONENT_REG_18 0x18
#define GCHD_COMPONENT_REG_19 0x19
#define GCHD_COMPONENT_REG_1A 0x1a
#define GCHD_COMPONENT_REG_1B 0x1b
#define GCHD_COMPONENT_REG_1C 0x1c
#define GCHD_COMPONENT_REG_1D 0x1d
#define GCHD_COMPONENT_REG_1E 0x1e
#define GCHD_COMPONENT_REG_1F 0x1f
#define GCHD_COMPONENT_REG_20 0x20
#define GCHD_COMPONENT_REG_21 0x21
#define GCHD_COMPONENT_REG_22 0x22
#define GCHD_COMPONENT_REG_24 0x24
#define GCHD_COMPONENT_REG_25 0x25
#define GCHD_COMPONENT_REG_26 0x26
#define GCHD_COMPONENT_REG_27 0x27
#define GCHD_COMPONENT_REG_2A 0x2a
#define GCHD_COMPONENT_REG_2E 0x2e
#define GCHD_COMPONENT_REG_30 0x30
#define GCHD_COMPONENT_REG_31 0x31
#define GCHD_COMPONENT_REG_32 0x32
#define GCHD_COMPONENT_REG_40 0x40
#define GCHD_COMPONENT_REG_41 0x41
#define GCHD_COMPONENT_REG_51 0x51
#define GCHD_COMPONENT_REG_54 0x54
#define GCHD_COMPONENT_REG_AB 0xab
#define GCHD_COMPONENT_REG_AC 0xac
#define GCHD_COMPONENT_REG_AD 0xad
#define GCHD_COMPONENT_REG_AE 0xae
#define GCHD_COMPONENT_REG_B0 0xb0
#define GCHD_COMPONENT_REG_B1 0xb1
#define GCHD_COMPONENT_REG_B2 0xb2
#define GCHD_COMPONENT_REG_B3 0xb3
#define GCHD_COMPONENT_REG_B4 0xb4
#define GCHD_COMPONENT_REG_B5 0xb5
#define GCHD_COMPONENT_REG_B7 0xb7
#define GCHD_COMPONENT_REG_B8 0xb8
#define GCHD_COMPONENT_REG_CE 0xce
#define GCHD_COMPONENT_REG_CF 0xcf

/* Named byte values used in mailbox command and register-write sequences. */
#define GCHD_VALUE_01 0x01
#define GCHD_VALUE_08 0x08
#define GCHD_VALUE_0C 0x0c
#define GCHD_VALUE_0F 0x0f
#define GCHD_VALUE_1C 0x1c
#define GCHD_VALUE_20 0x20
#define GCHD_VALUE_28 0x28
#define GCHD_VALUE_2A 0x2a
#define GCHD_VALUE_2C 0x2c
#define GCHD_VALUE_30 0x30
#define GCHD_VALUE_33 0x33
#define GCHD_VALUE_38 0x38
#define GCHD_VALUE_39 0x39
#define GCHD_VALUE_3A 0x3a
#define GCHD_VALUE_3C 0x3c
#define GCHD_VALUE_45 0x45
#define GCHD_VALUE_48 0x48
#define GCHD_VALUE_49 0x49
#define GCHD_VALUE_4C 0x4c
#define GCHD_VALUE_51 0x51
#define GCHD_VALUE_53 0x53
#define GCHD_VALUE_5C 0x5c
#define GCHD_VALUE_68 0x68
#define GCHD_VALUE_6B 0x6b
#define GCHD_VALUE_6C 0x6c
#define GCHD_VALUE_6D 0x6d
#define GCHD_VALUE_6E 0x6e
#define GCHD_VALUE_70 0x70
#define GCHD_VALUE_78 0x78
#define GCHD_VALUE_7A 0x7a
#define GCHD_VALUE_80 0x80
#define GCHD_VALUE_81 0x81
#define GCHD_VALUE_87 0x87
#define GCHD_VALUE_88 0x88
#define GCHD_VALUE_89 0x89
#define GCHD_VALUE_8A 0x8a
#define GCHD_VALUE_8B 0x8b
#define GCHD_VALUE_8C 0x8c
#define GCHD_VALUE_8D 0x8d
#define GCHD_VALUE_8E 0x8e
#define GCHD_VALUE_8F 0x8f
#define GCHD_VALUE_90 0x90
#define GCHD_VALUE_91 0x91
#define GCHD_VALUE_95 0x95
#define GCHD_VALUE_96 0x96
#define GCHD_VALUE_98 0x98
#define GCHD_VALUE_99 0x99
#define GCHD_VALUE_9B 0x9b
#define GCHD_VALUE_9C 0x9c
#define GCHD_VALUE_9D 0x9d
#define GCHD_VALUE_A0 0xa0
#define GCHD_VALUE_A3 0xa3
#define GCHD_VALUE_A5 0xa5
#define GCHD_VALUE_A8 0xa8
#define GCHD_VALUE_A9 0xa9
#define GCHD_VALUE_AC 0xac
#define GCHD_VALUE_AD 0xad
#define GCHD_VALUE_AE 0xae
#define GCHD_VALUE_B5 0xb5
#define GCHD_VALUE_B8 0xb8
#define GCHD_VALUE_BC 0xbc
#define GCHD_VALUE_BE 0xbe
#define GCHD_VALUE_BF 0xbf
#define GCHD_VALUE_C4 0xc4
#define GCHD_VALUE_C6 0xc6
#define GCHD_VALUE_C7 0xc7
#define GCHD_VALUE_C8 0xc8
#define GCHD_VALUE_C9 0xc9
#define GCHD_VALUE_CB 0xcb
#define GCHD_VALUE_CC 0xcc
#define GCHD_VALUE_CD 0xcd
#define GCHD_VALUE_CE 0xce
#define GCHD_VALUE_CF 0xcf
#define GCHD_VALUE_D0 0xd0
#define GCHD_VALUE_D2 0xd2
#define GCHD_VALUE_D4 0xd4
#define GCHD_VALUE_D6 0xd6
#define GCHD_VALUE_D8 0xd8
#define GCHD_VALUE_D9 0xd9
#define GCHD_VALUE_DC 0xdc
#define GCHD_VALUE_DD 0xdd
#define GCHD_VALUE_DE 0xde
#define GCHD_VALUE_DF 0xdf
#define GCHD_VALUE_E4 0xe4
#define GCHD_VALUE_E7 0xe7
#define GCHD_VALUE_E8 0xe8
#define GCHD_VALUE_E9 0xe9
#define GCHD_VALUE_EA 0xea
#define GCHD_VALUE_EC 0xec
#define GCHD_VALUE_ED 0xed
#define GCHD_VALUE_EF 0xef
#define GCHD_VALUE_F4 0xf4
#define GCHD_VALUE_F8 0xf8
#define GCHD_VALUE_F9 0xf9
#define GCHD_VALUE_FC 0xfc

#define GCHD_QUERY_POLL_COMPLETE         0xed

#define GCHD_QUERY_SEQUENCE_READ         0xec

#define GCHD_QUERY_ENCODER_PROBE         0x8b

#define GCHD_VALUE_02 0x02
#define GCHD_VALUE_03 0x03
#define GCHD_VALUE_04 0x04
#define GCHD_VALUE_05 0x05
#define GCHD_VALUE_06 0x06
#define GCHD_VALUE_07 0x07
#define GCHD_VALUE_09 0x09
#define GCHD_VALUE_0A 0x0a
#define GCHD_VALUE_0B 0x0b
#define GCHD_VALUE_0D 0x0d
#define GCHD_VALUE_0E 0x0e
#define GCHD_VALUE_11 0x11
#define GCHD_VALUE_15 0x15
#define GCHD_VALUE_1A 0x1a
#define GCHD_VALUE_1E 0x1e
#define GCHD_VALUE_21 0x21
#define GCHD_VALUE_29 0x29
#define GCHD_VALUE_2F 0x2f
#define GCHD_VALUE_31 0x31
#define GCHD_VALUE_34 0x34
#define GCHD_VALUE_36 0x36
#define GCHD_VALUE_37 0x37
#define GCHD_VALUE_3B 0x3b
#define GCHD_VALUE_3D 0x3d
#define GCHD_VALUE_3E 0x3e
#define GCHD_VALUE_3F 0x3f
#define GCHD_VALUE_40 0x40
#define GCHD_VALUE_41 0x41
#define GCHD_VALUE_46 0x46
#define GCHD_VALUE_47 0x47
#define GCHD_VALUE_4A 0x4a
#define GCHD_VALUE_4B 0x4b
#define GCHD_VALUE_4D 0x4d
#define GCHD_VALUE_4F 0x4f
#define GCHD_VALUE_57 0x57
#define GCHD_VALUE_58 0x58
#define GCHD_VALUE_63 0x63
#define GCHD_VALUE_64 0x64
#define GCHD_VALUE_65 0x65
#define GCHD_VALUE_66 0x66
#define GCHD_VALUE_67 0x67
#define GCHD_VALUE_72 0x72
#define GCHD_VALUE_73 0x73
#define GCHD_VALUE_77 0x77
#define GCHD_VALUE_7B 0x7b
#define GCHD_VALUE_93 0x93
#define GCHD_VALUE_94 0x94
#define GCHD_VALUE_AA 0xaa
#define GCHD_VALUE_AF 0xaf
#define GCHD_VALUE_B7 0xb7
#define GCHD_VALUE_D7 0xd7
#define GCHD_VALUE_F0 0xf0
#define GCHD_VALUE_F1 0xf1
#define GCHD_VALUE_F3 0xf3
#define GCHD_VALUE_F5 0xf5
#define GCHD_VALUE_F7 0xf7

#define GCHD_COMPONENT_REG_0A            0x0a
#define GCHD_COMPONENT_REG_0B            0x0b
#define GCHD_COMPONENT_REG_0C            0x0c
#define GCHD_COMPONENT_REG_0D            0x0d
#define GCHD_COMPONENT_REG_92            0x92
#define GCHD_COMPONENT_REG_93            0x93
#define GCHD_COMPONENT_REG_94            0x94
#define GCHD_COMPONENT_REG_95            0x95
#define GCHD_COMPONENT_REG_96            0x96
#define GCHD_COMPONENT_REG_97            0x97
#define GCHD_COMPONENT_REG_98            0x98
#define GCHD_COMPONENT_REG_99            0x99
#define GCHD_COMPONENT_REG_9A            0x9a
#define GCHD_COMPONENT_REG_9B            0x9b
#define GCHD_COMPONENT_REG_9C            0x9c
#define GCHD_COMPONENT_REG_9D            0x9d
#define GCHD_COMPONENT_REG_9E            0x9e
#define GCHD_COMPONENT_REG_9F            0x9f
#define GCHD_COMPONENT_REG_A0            0xa0
#define GCHD_COMPONENT_REG_A1            0xa1
#define GCHD_COMPONENT_REG_A2            0xa2
#define GCHD_COMPONENT_REG_A3            0xa3
#define GCHD_COMPONENT_REG_A4            0xa4
#define GCHD_COMPONENT_REG_A5            0xa5
#define GCHD_COMPONENT_REG_A6            0xa6
#define GCHD_COMPONENT_REG_A7            0xa7
#define GCHD_COMPONENT_REG_A8            0xa8
#define GCHD_COMPONENT_REG_A9            0xa9
#define GCHD_COMPONENT_REG_AA            0xaa
#define GCHD_ENCODER_REG_10              0x10
#define GCHD_ENCODER_REG_11              0x11
#define GCHD_ENCODER_REG_12              0x12
#define GCHD_ENCODER_REG_13              0x13
#define GCHD_ENCODER_REG_15              0x15
#define GCHD_VALUE_17                    0x17
#define GCHD_VALUE_18                    0x18
#define GCHD_VALUE_1B                    0x1b
#define GCHD_VALUE_1F                    0x1f
#define GCHD_VALUE_22                    0x22
#define GCHD_VALUE_32                    0x32
#define GCHD_VALUE_52                    0x52
#define GCHD_VALUE_54                    0x54
#define GCHD_VALUE_59                    0x59
#define GCHD_VALUE_7E                    0x7e
#define GCHD_VALUE_82                    0x82
#define GCHD_VALUE_84                    0x84
#define GCHD_VALUE_86                    0x86
#define GCHD_VALUE_AB                    0xab
#define GCHD_VALUE_B2                    0xb2
#define GCHD_VALUE_B3                    0xb3
#define GCHD_VALUE_B6                    0xb6
#define GCHD_VALUE_B9                    0xb9
#define GCHD_VALUE_BB                    0xbb
#define GCHD_VALUE_C0                    0xc0
#define GCHD_VALUE_C2                    0xc2
#define GCHD_VALUE_C5                    0xc5
#define GCHD_VALUE_CA                    0xca
#define GCHD_VALUE_FE                    0xfe
#define GCHD_VIDEO_REG_70                0x70
#define GCHD_VIDEO_REG_90                0x90
#define GCHD_VIDEO_REG_9A                0x9a
#define GCHD_VIDEO_REG_9B                0x9b
#define GCHD_VIDEO_REG_9C                0x9c
#define GCHD_VIDEO_REG_9D                0x9d
#define GCHD_VIDEO_REG_9E                0x9e
#define GCHD_VIDEO_REG_9F                0x9f
#define GCHD_VIDEO_REG_A0                0xa0
#define GCHD_VIDEO_REG_A1                0xa1
#define GCHD_VIDEO_REG_A2                0xa2
#define GCHD_VIDEO_REG_A3                0xa3

/* Decoder tuning register indices and observed tuning-pass bytes. */
#define GCHD_DECODER_REG_TUNE_10       0x10
#define GCHD_DECODER_REG_TUNE_11       0x11
#define GCHD_DECODER_REG_TUNE_12       0x12
#define GCHD_DECODER_REG_TUNE_13       0x13
#define GCHD_DECODER_REG_TUNE_14       0x14
#define GCHD_DECODER_REG_TUNE_15       0x15
#define GCHD_DECODER_TUNE_6F           0x6f
#define GCHD_DECODER_TUNE_D4           0xd4
#define GCHD_DECODER_TUNE_EA           0xea
#define GCHD_DECODER_TUNE_D0           0xd0
#define GCHD_DECODER_TUNE_08           0x08
#define GCHD_DECODER_TUNE_1B           0x1b
#define GCHD_DECODER_TUNE_88           0x88

/* Mailbox query subcommands documented in the input traces. */
#define GCHD_QUERY_ENCODER_POLL        0xf5
#define GCHD_QUERY_ENCODER_POLL_FD     0xfd
#define GCHD_QUERY_ENCODER_POLL_FC     0xfc
#define GCHD_QUERY_ENCODER_POLL_F3     0xf3
#define GCHD_QUERY_ENCODER_POLL_FF     0xff
#define GCHD_QUERY_ENCODER_POLL_A3     0xa3

/* Backend retrigger/apply pulse values observed during analog setup. */
#define GCHD_BACKEND_REG_RETRIGGER     0x0d
#define GCHD_BACKEND_REG_APPLY         0x0e
#define GCHD_BACKEND_RETRIGGER_VALUE   0xc8
#define GCHD_BACKEND_APPLY_PREPARE     0x15
#define GCHD_BACKEND_APPLY_COMMIT      0x14
#define GCHD_BACKEND_APPLY_PREPARE_ALT 0x05
#define GCHD_BACKEND_APPLY_COMMIT_ALT  0x04

/* Processor state signatures returned by the 0x33 mailbox. */
#define GCHD_REPLY_PROCESSOR_ACTIVE    0x27f97b
#define GCHD_REPLY_PROCESSOR_TRANSIENT 0x334455
#define GCHD_REPLY_ENCODER_ACTIVE      0x78e040
#define GCHD_REPLY_ENCODER_MASK        0xf8f0f0
/* Subcommands for the 0x89/0x89 mailbox query family. */
#define GCHD_QUERY_ENCODER_INIT        0xfb
#define GCHD_QUERY_ENCODER_STATUS      0xfa
#define GCHD_QUERY_ENCODER_CONFIG      0xca
#define GCHD_QUERY_ENCODER_READY        0xe7
#define GCHD_QUERY_ENCODER_FORMAT      0xfd
#define GCHD_QUERY_ENCODER_VARIANT     0xf8
#define GCHD_QUERY_ENCODER_HEIGHT      0xf1
#define GCHD_QUERY_ENCODER_FINALIZE    0xcb

/* Other named command triplets used by the input bring-up sequence. */
#define GCHD_CMD_PROCESSOR_START       0x29
#define GCHD_CMD_PROCESSOR_START_ARG   0x5b
#define GCHD_CMD_COMPONENT_QUERY      0x94
#define GCHD_CMD_COMPONENT_QUERY_EXT  0x43
#define GCHD_CMD_COMPONENT_QUERY_ARG  0xb7
#define GCHD_CMD_COMPONENT_QUERY_ALT  0x4e

/* Additional observed one-byte command responses. */
#define GCHD_REPLY_COMMAND_EC          0xec
#define GCHD_REPLY_COMMAND_E1          0xe1
#define GCHD_REPLY_COMMAND_6B          0x6b
#define GCHD_REPLY_COMMAND_7F          0x7f
#define GCHD_REPLY_COMMAND_6A          0x6a
#define GCHD_REPLY_COMMAND_7C          0x7c
#define GCHD_REPLY_COMMAND_2A          0x2a
#define GCHD_REPLY_COMMAND_6C          0x6c
#define GCHD_REPLY_COMMAND_76          0x76
#define GCHD_REPLY_COMMAND_73          0x73

/* Bit masks and signatures used to recognize encoder status replies. */
#define GCHD_REPLY_ENCODER_HIGH_MASK   0xf8
#define GCHD_REPLY_ENCODER_HIGH_VALUE  0x78
#define GCHD_REPLY_ENCODER_MID_MASK    0xf0
#define GCHD_REPLY_ENCODER_MID_VALUE   0xe0
#define GCHD_REPLY_ENCODER_LOW_MASK    0xf0
#define GCHD_REPLY_ENCODER_LOW_VALUE   0x40




struct gchd_frame { u8 *data; size_t len; u64 sequence; };
struct gchd_ring {
 struct gchd_frame frames[GCHD_RING_FRAMES];
 unsigned int head, count;
 size_t bytes;
 u64 sequence;
 spinlock_t lock;
};
struct gchd_buffer {
 struct vb2_v4l2_buffer vb;
 struct list_head list;
};
enum gchd_family { GCHD_FAMILY_OLD, GCHD_FAMILY_HDNEW };

struct gchd {
 struct kref refcount;
 struct usb_device *udev;
 enum gchd_family family;
 struct usb_interface *intf;
 struct video_device vdev;
 struct v4l2_device v4l2_dev;
 struct vb2_queue vbq;
 struct v4l2_ctrl_handler ctrls;
 struct mutex lock;
 /* Serializes hardware lifecycle changes against asynchronous detection. */
 struct mutex lifecycle_lock;
 /* Serializes V4L2 open/close reference counting and cleanup. */
 struct mutex users_lock;
 unsigned int open_count;
 struct delayed_work detect_work;
 struct delayed_work mode_work;
 spinlock_t qlock;
 struct list_head queued;
 struct task_struct *rx_thread;
 struct gchd_ring ring;
 u8 *usb_buf;
 u8 ts_partial[188];
 u16 ts_partial_len;
 bool streaming;
 bool start_pending;
 bool detect_requested;
 bool disconnected;
 bool hw_initialized;
 bool hw_init_failed;
 bool input_configured;
 bool encoder_started;
 bool input_prepared;
 bool input_forced;
 bool mode_forced;
 bool bitrate_forced;
 bool h264_level_forced;
 bool signal_present;
 bool rgb_input;
 unsigned long last_video_jiffies;
 u16 saved_enable_state;
 u16 hw_enable_state;
 u16 hw_enable_register;
 u16 special_detect_mask;
 u32 width, height, sizeimage;
 u32 input;
 u32 input_width, input_height;
 u32 input_fps_num, input_fps_den;
 bool input_interlaced;
 u32 bitrate;
 u8 h264_profile;
 u8 h264_level;
};

int gchd_enable_analog(struct gchd *);
int gchd_load_encoder_firmware(struct gchd *);
int gchd_hw_init(struct gchd *);
int gchd_hw_shutdown(struct gchd *);
int gchd_transcoder_init(struct gchd *);
int gchd_transcoder_setup(struct gchd *);
int gchd_transcoder_output_enable(struct gchd *, bool);
int gchd_transcoder_final_configure(struct gchd *);
int gchd_mail_write(struct gchd *, u8, const u8 *, u8);
int gchd_mail_read(struct gchd *, u8, u8 *, u8);
int gchd_state_cmd(struct gchd *, u8, u8, u16, u16);
int gchd_scmd(struct gchd *, u8, u8, u16);
int gchd_do_enable(struct gchd *, u16, u16);
int gchd_send_enable_state(struct gchd *);
int gchd_sparam(struct gchd *, u16, u8, u8, u16);
int gchd_slsi(struct gchd *, u16, u16);
int gchd_raw_read16(struct gchd *, u16, u16, u16 *);
int gchd_load_encoder_firmware(struct gchd *);
int gchd_input_configure(struct gchd *);
int gchd_input_start(struct gchd *);
int gchd_input_configure_idle(struct gchd *);
int gchd_input_detect_signal(struct gchd *);
int gchd_input_read_analog_mode(struct gchd *, bool *);
int gchd_input_switch_480i_to_240p(struct gchd *);
int gchd_common_block_a(struct gchd *);
int gchd_common_block_b1(struct gchd *, bool);
int gchd_common_block_b2(struct gchd *);
int gchd_common_block_b3(struct gchd *);
int gchd_common_block_c(struct gchd *);
int gchd_color_space_exact(struct gchd *);
int gchd_setup_subblock_exact(struct gchd *);
void gchd_input_stop(struct gchd *);
int gchd_stream_stop(struct gchd *);

int gchd_ring_push(struct gchd_ring *, const u8 *, size_t);
struct gchd_frame *gchd_ring_pop(struct gchd_ring *);
void gchd_ring_free(struct gchd_ring *);
int gchd_v4l2_register(struct gchd *);
void gchd_v4l2_unregister(struct gchd *);
extern const struct v4l2_file_operations vb2_fops;

#endif
