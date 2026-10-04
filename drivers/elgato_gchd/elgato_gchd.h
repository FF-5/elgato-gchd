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

#define SCMD_IDLE 1
#define SCMD_RESET 2

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
 /* Serializes first-open initialization and last-close shutdown. */
 struct mutex users_lock;
 unsigned int open_count;
 struct delayed_work detect_work;
 spinlock_t qlock;
 struct list_head queued;
 struct task_struct *rx_thread;
 struct gchd_ring ring;
 u8 *usb_buf;
 u8 ts_partial[188];
 u16 ts_partial_len;
 bool streaming;
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
