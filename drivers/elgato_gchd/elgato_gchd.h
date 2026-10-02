#ifndef ELGATO_GCHD_H
#define ELGATO_GCHD_H
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/usb.h>
#include <linux/videodev2.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-vmalloc.h>
#define GCHD_VID 0x0fd9
#define GCHD_PID 0x005d
#define GCHD_EP_IN 0x81
#define GCHD_USB_BUFSIZE 0x4000
#define GCHD_RING_FRAMES 32
#define GCHD_RING_BYTES (64ULL * 1024ULL * 1024ULL)
#define GCHD_MAX_FRAME (8U * 1024U * 1024U)
struct gchd_frame { u8 *data; size_t len; u64 sequence; };
struct gchd_ring {
 struct gchd_frame frames[GCHD_RING_FRAMES];
 unsigned int head, count; size_t bytes; u64 sequence; spinlock_t lock;
};
struct gchd_buffer { struct vb2_v4l2_buffer vb; struct list_head list; };
int gchd_hw_init(struct gchd *);\nint gchd_hw_shutdown(struct gchd *);\n\nstruct gchd {
 struct usb_device *udev; struct usb_interface *intf;
 struct video_device vdev; struct v4l2_device v4l2_dev; struct vb2_queue vbq;
 struct v4l2_ctrl_handler ctrls; struct mutex lock; spinlock_t qlock;
 struct list_head queued; struct task_struct *rx_thread; struct gchd_ring ring;
 u8 *usb_buf; bool streaming, disconnected, hw_initialized;\n u16 hw_enable_state; u32 width, height, sizeimage;
};
int gchd_ring_push(struct gchd_ring *, const u8 *, size_t);
struct gchd_frame *gchd_ring_pop(struct gchd_ring *);
void gchd_ring_free(struct gchd_ring *);
int gchd_v4l2_register(struct gchd *);
void gchd_v4l2_unregister(struct gchd *);\nextern const struct v4l2_file_operations vb2_fops;
#endif
