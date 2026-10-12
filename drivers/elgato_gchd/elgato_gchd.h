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
#define GCHD_REPLY_COMMAND_PENDING     0x02

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
