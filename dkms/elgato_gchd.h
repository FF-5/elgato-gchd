#ifndef ELGATO_GCHD_H
#define ELGATO_GCHD_H

#include <linux/list.h>
#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/usb.h>
#include <linux/wait.h>
#include <linux/completion.h>
#include <linux/videodev2.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-vmalloc.h>

#include "gchd_protocol.h"

/* Driver configuration (not protocol). */
#define GCHD_RX_URBS_EP_IN 32	/* URBs queued on GCHD_EP_TS_IN */
#define GCHD_RX_URBS_AUX_IN 8	/* URBs queued on GCHD_EP_AUX_IN */
#define GCHD_USB_BUFSIZE 0x10000	/* bulk-IN URB size */
#define GCHD_RING_FRAMES 32
#define GCHD_RING_BYTES (64ULL * 1024ULL * 1024ULL)
#define GCHD_MAX_FRAME (8U * 1024U * 1024U)
/* Transport-stream buffers: 348 packets of 188 bytes (65 424 bytes). */
#define GCHD_TS_BUF_PACKETS 348
#define GCHD_TS_BUF_SIZE (GCHD_TS_BUF_PACKETS * 188U)

struct gchd_rx_urb;
#define GCHD_TS_NONE U64_MAX
struct gchd_frame { u8 *data; size_t len; u64 sequence; u64 ts90; };
struct gchd_ring {
 struct gchd_frame frames[GCHD_RING_FRAMES];
 unsigned int head, count;
 size_t bytes;
 u64 sequence;
 u64 dropped; /* frames discarded because the ring was full */
 spinlock_t lock;
};
struct gchd_buffer {
 struct vb2_v4l2_buffer vb;
 struct list_head list;
};
/* GCHD_* tracks driver lifecycle; HW_* names device protocol states/commands. */
enum gchd_state {
 GCHD_OFFLINE,       /* Driver is waiting for an attached USB device. */
 GCHD_ONLINE,        /* USB device attached and V4L2 device registered. */
 GCHD_POWER_OFF,     /* Hardware has been cleanly shut down. */
 GCHD_INITIALIZATION,
 GCHD_INPUT_SELECTION,
 GCHD_GENERAL_INPUT_SETUP,
 GCHD_SOURCE_DETECTION,
 GCHD_SOURCE_FORMAT_CONFIGURATION,
 GCHD_CAPTURE_START,
 GCHD_STREAMING,
 GCHD_CAPTURE_STOP,
 GCHD_ERROR,
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
 enum gchd_state lifecycle_state;
 /* Serializes V4L2 open/close reference counting and cleanup. */
 struct mutex users_lock;
 unsigned int open_count;
 struct delayed_work detect_work;
 struct delayed_work mode_work;
 spinlock_t qlock;
 struct list_head queued;
 struct task_struct *rx_thread;
 /* Asynchronous bulk-IN receive queue (URBs are owned by the RX thread). */
 spinlock_t rx_lock;
 struct list_head rx_done;
 wait_queue_head_t rx_wait;
 struct completion rx_ready;
 int rx_setup_status;
 bool rx_active;
 atomic_t rx_urb_errors;  /* failed receive transfers since RX start */
 int rx_urb_last_status;
 struct gchd_ring ring;
 struct mutex deliver_lock; /* serializes gchd_deliver() */
 bool wait_sps; /* drop video until the first SPS after a (re)start */
 u64 pes_ts90; /* timestamp of the PES being assembled */
 bool ts_base_valid; /* buffer timestamp anchor set */
 u64 ts_base_ns, ts_base90;
 u8 *usb_buf;
 u8 ts_partial[GCHD_TS_PACKET];
 u32 pixfmt;          /* V4L2_PIX_FMT_H264 or V4L2_PIX_FMT_MPEG */
 u8 *ts_out;          /* transport-stream buffer being filled */
 size_t ts_out_len;
 bool wait_pat;       /* TS delivery starts at the next PAT packet */
 u16 ts_partial_len;
 bool streaming; /* V4L2 queue delivery only. */
 bool capture_active; /* Hardware capture is active while a source is present. */
 bool start_pending;
 bool stop_pending;
 bool mode_retriggered;
 bool detect_requested;
 bool disconnected;
 bool hw_initialized;
 bool hw_init_failed;
 bool input_configured;
 bool input_routed;
 bool input_detect_prepared;
 bool encoder_started;
 bool input_prepared;
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
 u32 previous_input; /* Connector selected before the current input. */
 bool dec_svideo; /* decoder 0x88:02 set for S-video; kept while plugged in */
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
int gchd_transcoder_start_block(struct gchd *);
int gchd_transcoder_output_enable(struct gchd *, bool);
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
int gchd_input_check_mode(struct gchd *);
int gchd_input_configure_idle(struct gchd *);
int gchd_prepare_input_detection(struct gchd *);
int gchd_probe_video_source(struct gchd *);
int gchd_input_read_analog_mode(struct gchd *, bool *);
int gchd_analog_mode_pre_stop(struct gchd *, bool);
int gchd_analog_mode_reconfigure(struct gchd *);
int gchd_analog_pal_mode_change(struct gchd *, bool);
int gchd_analog_poll_backend(struct gchd *);
int gchd_common_block_a(struct gchd *);
int gchd_common_block_b1(struct gchd *, bool);
int gchd_common_block_b2(struct gchd *);
int gchd_common_block_b3(struct gchd *);
int gchd_common_block_c(struct gchd *);
int gchd_color_space_exact(struct gchd *);
int gchd_setup_subblock_exact(struct gchd *);
void gchd_input_stop(struct gchd *);
int gchd_stream_stop(struct gchd *);
int gchd_return_to_stop(struct gchd *);

int gchd_ring_push(struct gchd_ring *, const u8 *, size_t, u64);
struct gchd_frame *gchd_ring_pop(struct gchd_ring *);
void gchd_ring_free(struct gchd_ring *);
int gchd_v4l2_register(struct gchd *);
void gchd_v4l2_unregister(struct gchd *);
extern const struct v4l2_file_operations vb2_fops;

#endif
