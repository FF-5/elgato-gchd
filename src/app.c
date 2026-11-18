// SPDX-License-Identifier: GPL-2.0
/*
 * Application device and frame input device.
 *
 * The application device ("Elgato Game Capture HD") is a capture device with
 * uncompressed YUYV frames for applications that do not handle the
 * compressed stream. It is registered when the module is loaded and stays
 * registered while the module is loaded, whether a Game Capture HD is
 * connected or not; the first connected Game Capture HD is attached to it.
 *
 * The frames are written into the frame input device (an output device) by
 * a userspace program that decodes the stream of the attached device and
 * scales it to the format of the application device; the driver copies each
 * written frame into the next queued buffer of the application device. No
 * decoding takes place in the kernel.
 *
 * The application chooses the size and frame rate from a fixed list. While
 * no application has buffers allocated, the format follows the detected
 * source, so that an application sees the source format first. While an
 * application streams, the format does not change; a source change is
 * reported with V4L2_EVENT_SOURCE_CHANGE.
 *
 * The state is published in the sysfs attribute "gchd_state" of the
 * application device:
 *   STATE WIDTH HEIGHT RATE-NUM RATE-DEN FEED-NODE STREAM-NODE
 * STATE is "streaming" while an application streams from the application
 * device, "idle" otherwise; STREAM-NODE is the H.264 device of the attached
 * Game Capture HD, "-" when none is attached.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/sysfs.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-vmalloc.h>
#include "elgato_gchd.h"

struct gchd_app_buf {
	struct vb2_v4l2_buffer vb;
	struct list_head list;
};

static const struct v4l2_frmsize_discrete gchd_app_sizes[] = {
	{ 1920, 1080 }, { 1280, 720 }, { 720, 576 }, { 720, 480 }, { 640, 480 },
};

/* Frames per second. */
static const struct v4l2_fract gchd_app_rates[] = {
	{ 60, 1 }, { 60000, 1001 }, { 50, 1 }, { 30000, 1001 }, { 25, 1 },
};

static struct gchd_app {
	struct v4l2_device v4l2_dev;
	struct video_device vdev;	/* application device */
	struct vb2_queue q;
	struct mutex lock;		/* application device ioctls and queue */
	struct mutex deliver;		/* frame delivery against stop_streaming */
	spinlock_t qlock;
	struct list_head bufs;
	u32 width, height;
	struct v4l2_fract rate;
	u32 sequence;
	bool streaming;

	struct video_device feed;	/* frame input device */
	struct vb2_queue feed_q;
	struct mutex feed_lock;
	struct list_head feed_pending;
	u32 feed_width, feed_height;

	struct mutex attach_lock;	/* d */
	struct gchd *d;			/* attached Game Capture HD, or NULL */
	struct work_struct update_work;
	bool registered;
} app;

static void gchd_app_notify(void)
{
	if (app.registered)
		sysfs_notify(&app.vdev.dev.kobj, NULL, "gchd_state");
}

static void gchd_app_pix(u32 width, u32 height, struct v4l2_pix_format *p)
{
	memset(p, 0, sizeof(*p));
	p->width = width;
	p->height = height;
	p->pixelformat = V4L2_PIX_FMT_YUYV;
	p->field = V4L2_FIELD_NONE;
	p->bytesperline = width * 2;
	p->sizeimage = width * height * 2;
	p->colorspace = height > 576 ? V4L2_COLORSPACE_REC709 :
				       V4L2_COLORSPACE_SMPTE170M;
}

/* The listed size nearest to WIDTH x HEIGHT. */
static const struct v4l2_frmsize_discrete *gchd_app_size(u32 width, u32 height)
{
	const struct v4l2_frmsize_discrete *best = &gchd_app_sizes[0];
	unsigned int i, dist, dbest = UINT_MAX;

	for (i = 0; i < ARRAY_SIZE(gchd_app_sizes); i++) {
		dist = abs((int)gchd_app_sizes[i].width - (int)width) +
		       abs((int)gchd_app_sizes[i].height - (int)height);
		if (dist < dbest) {
			dbest = dist;
			best = &gchd_app_sizes[i];
		}
	}
	return best;
}

/* The listed rate nearest to NUM/DEN frames per second. */
static struct v4l2_fract gchd_app_rate(u32 num, u32 den)
{
	u64 want, have, dist, dbest = U64_MAX;
	unsigned int i, best = 0;

	if (!num || !den)
		return gchd_app_rates[1];
	want = div_u64((u64)num * 1000, den);
	for (i = 0; i < ARRAY_SIZE(gchd_app_rates); i++) {
		have = div_u64((u64)gchd_app_rates[i].numerator * 1000,
			       gchd_app_rates[i].denominator);
		dist = have > want ? have - want : want - have;
		if (dist < dbest) {
			dbest = dist;
			best = i;
		}
	}
	return gchd_app_rates[best];
}

/* ------------------------------------------------------------ app queue */

static struct gchd_app_buf *to_app_buf(struct vb2_buffer *vb)
{
	return container_of(to_vb2_v4l2_buffer(vb), struct gchd_app_buf, vb);
}

static int gchd_app_queue_setup(struct vb2_queue *q, unsigned int *nb,
				unsigned int *np, unsigned int sizes[],
				struct device *alloc[])
{
	u32 size = app.width * app.height * 2;

	if (*np)
		return sizes[0] < size ? -EINVAL : 0;
	*np = 1;
	sizes[0] = size;
	if (*nb < 2)
		*nb = 2;
	return 0;
}

static int gchd_app_buf_prepare(struct vb2_buffer *vb)
{
	u32 size = app.width * app.height * 2;

	if (vb2_plane_size(vb, 0) < size)
		return -EINVAL;
	vb2_set_plane_payload(vb, 0, size);
	return 0;
}

static void gchd_app_buf_queue(struct vb2_buffer *vb)
{
	unsigned long flags;

	spin_lock_irqsave(&app.qlock, flags);
	list_add_tail(&to_app_buf(vb)->list, &app.bufs);
	spin_unlock_irqrestore(&app.qlock, flags);
}

static int gchd_app_start(struct vb2_queue *q, unsigned int count)
{
	app.sequence = 0;
	WRITE_ONCE(app.streaming, true);
	gchd_app_notify();
	return 0;
}

static void gchd_app_stop(struct vb2_queue *q)
{
	struct gchd_app_buf *b, *n;
	unsigned long flags;

	mutex_lock(&app.deliver);
	WRITE_ONCE(app.streaming, false);
	spin_lock_irqsave(&app.qlock, flags);
	list_for_each_entry_safe(b, n, &app.bufs, list) {
		list_del(&b->list);
		vb2_buffer_done(&b->vb.vb2_buf, VB2_BUF_STATE_ERROR);
	}
	spin_unlock_irqrestore(&app.qlock, flags);
	mutex_unlock(&app.deliver);
	gchd_app_notify();
	/* The format follows the source again (after the buffers are freed). */
	schedule_work(&app.update_work);
}

static const struct vb2_ops gchd_app_qops = {
	.queue_setup = gchd_app_queue_setup,
	.buf_prepare = gchd_app_buf_prepare,
	.buf_queue = gchd_app_buf_queue,
	.start_streaming = gchd_app_start,
	.stop_streaming = gchd_app_stop,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
};

/* Copy one frame written into the frame input device. */
static void gchd_app_frame(const void *src, size_t size)
{
	struct gchd_app_buf *b = NULL;
	unsigned long flags;

	mutex_lock(&app.deliver);
	if (READ_ONCE(app.streaming) && size == app.width * app.height * 2) {
		spin_lock_irqsave(&app.qlock, flags);
		b = list_first_entry_or_null(&app.bufs, struct gchd_app_buf, list);
		if (b)
			list_del(&b->list);
		spin_unlock_irqrestore(&app.qlock, flags);
	}
	if (b) {
		memcpy(vb2_plane_vaddr(&b->vb.vb2_buf, 0), src, size);
		vb2_set_plane_payload(&b->vb.vb2_buf, 0, size);
		b->vb.field = V4L2_FIELD_NONE;
		b->vb.sequence = app.sequence++;
		b->vb.vb2_buf.timestamp = ktime_get_ns();
		vb2_buffer_done(&b->vb.vb2_buf, VB2_BUF_STATE_DONE);
	}
	mutex_unlock(&app.deliver);
}

/* ------------------------------------------------------------ app ioctls */

static int gchd_app_querycap(struct file *f, void *p, struct v4l2_capability *c)
{
	strscpy(c->driver, "elgato-gchd", sizeof(c->driver));
	strscpy(c->card, video_devdata(f)->name, sizeof(c->card));
	strscpy(c->bus_info, "platform:elgato-gchd", sizeof(c->bus_info));
	return 0;
}

static int gchd_app_enum_fmt(struct file *f, void *p, struct v4l2_fmtdesc *x)
{
	if (x->index)
		return -EINVAL;
	x->pixelformat = V4L2_PIX_FMT_YUYV;
	return 0;
}

static int gchd_app_g_fmt(struct file *f, void *p, struct v4l2_format *x)
{
	gchd_app_pix(app.width, app.height, &x->fmt.pix);
	return 0;
}

static int gchd_app_try_fmt(struct file *f, void *p, struct v4l2_format *x)
{
	const struct v4l2_frmsize_discrete *s =
		gchd_app_size(x->fmt.pix.width, x->fmt.pix.height);

	gchd_app_pix(s->width, s->height, &x->fmt.pix);
	return 0;
}

static int gchd_app_s_fmt(struct file *f, void *p, struct v4l2_format *x)
{
	gchd_app_try_fmt(f, p, x);
	if (x->fmt.pix.width == app.width && x->fmt.pix.height == app.height)
		return 0;
	if (vb2_is_busy(&app.q))
		return -EBUSY;
	app.width = x->fmt.pix.width;
	app.height = x->fmt.pix.height;
	gchd_app_notify();
	return 0;
}

static int gchd_app_framesizes(struct file *f, void *p,
			       struct v4l2_frmsizeenum *s)
{
	if (s->index >= ARRAY_SIZE(gchd_app_sizes) ||
	    s->pixel_format != V4L2_PIX_FMT_YUYV)
		return -EINVAL;
	s->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	s->discrete = gchd_app_sizes[s->index];
	return 0;
}

static int gchd_app_frameintervals(struct file *f, void *p,
				   struct v4l2_frmivalenum *iv)
{
	const struct v4l2_frmsize_discrete *s;

	if (iv->index >= ARRAY_SIZE(gchd_app_rates) ||
	    iv->pixel_format != V4L2_PIX_FMT_YUYV)
		return -EINVAL;
	s = gchd_app_size(iv->width, iv->height);
	if (s->width != iv->width || s->height != iv->height)
		return -EINVAL;
	iv->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	iv->discrete.numerator = gchd_app_rates[iv->index].denominator;
	iv->discrete.denominator = gchd_app_rates[iv->index].numerator;
	return 0;
}

static int gchd_app_g_parm(struct file *f, void *p, struct v4l2_streamparm *s)
{
	if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	memset(&s->parm.capture, 0, sizeof(s->parm.capture));
	s->parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
	s->parm.capture.timeperframe.numerator = app.rate.denominator;
	s->parm.capture.timeperframe.denominator = app.rate.numerator;
	s->parm.capture.readbuffers = 2;
	return 0;
}

static int gchd_app_s_parm(struct file *f, void *p, struct v4l2_streamparm *s)
{
	struct v4l2_fract *t = &s->parm.capture.timeperframe;
	struct v4l2_fract rate;

	if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	rate = gchd_app_rate(t->denominator, t->numerator);
	if ((rate.numerator != app.rate.numerator ||
	     rate.denominator != app.rate.denominator) && !vb2_is_busy(&app.q)) {
		app.rate = rate;
		gchd_app_notify();
	}
	return gchd_app_g_parm(f, p, s);
}

/* The attached Game Capture HD with a reference, or NULL. */
static struct gchd *gchd_app_get(void)
{
	struct gchd *d;

	mutex_lock(&app.attach_lock);
	d = app.d;
	if (d)
		kref_get(&d->refcount);
	mutex_unlock(&app.attach_lock);
	return d;
}

static int gchd_app_enum_input(struct file *f, void *p, struct v4l2_input *in)
{
	static const char * const names[] = {
		"HDMI", "Component", "Composite", "S-Video",
	};
	unsigned int index = in->index;
	struct gchd *d;

	if (index >= ARRAY_SIZE(names))
		return -EINVAL;
	memset(in, 0, sizeof(*in));
	in->index = index;
	strscpy(in->name, names[index], sizeof(in->name));
	in->type = V4L2_INPUT_TYPE_CAMERA;
	d = gchd_app_get();
	if (!d) {
		in->status = V4L2_IN_ST_NO_POWER;
		return 0;
	}
	if (index == READ_ONCE(d->input) && !READ_ONCE(d->signal_present) &&
	    READ_ONCE(d->hw_initialized))
		in->status = V4L2_IN_ST_NO_SIGNAL;
	gchd_put(d);
	return 0;
}

static int gchd_app_g_input(struct file *f, void *p, unsigned int *i)
{
	struct gchd *d = gchd_app_get();

	*i = d ? READ_ONCE(d->input) : 0;
	if (d)
		gchd_put(d);
	return 0;
}

static int gchd_app_s_input(struct file *f, void *p, unsigned int i)
{
	struct gchd *d;
	int r = 0;

	if (i > 3)
		return -EINVAL;
	d = gchd_app_get();
	if (!d)
		return i ? -ENODEV : 0;
	if (i != READ_ONCE(d->input))
		r = gchd_select_input(d, i);
	gchd_put(d);
	return r;
}

static int gchd_app_subscribe(struct v4l2_fh *fh,
			      const struct v4l2_event_subscription *sub)
{
	if (sub->type == V4L2_EVENT_SOURCE_CHANGE)
		return v4l2_src_change_event_subscribe(fh, sub);
	return -EINVAL;
}

static const struct v4l2_ioctl_ops gchd_app_ioctl = {
	.vidioc_querycap = gchd_app_querycap,
	.vidioc_enum_fmt_vid_cap = gchd_app_enum_fmt,
	.vidioc_g_fmt_vid_cap = gchd_app_g_fmt,
	.vidioc_s_fmt_vid_cap = gchd_app_s_fmt,
	.vidioc_try_fmt_vid_cap = gchd_app_try_fmt,
	.vidioc_enum_framesizes = gchd_app_framesizes,
	.vidioc_enum_frameintervals = gchd_app_frameintervals,
	.vidioc_g_parm = gchd_app_g_parm,
	.vidioc_s_parm = gchd_app_s_parm,
	.vidioc_enum_input = gchd_app_enum_input,
	.vidioc_g_input = gchd_app_g_input,
	.vidioc_s_input = gchd_app_s_input,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_subscribe_event = gchd_app_subscribe,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

static const struct v4l2_file_operations gchd_app_fops = {
	.owner = THIS_MODULE,
	.open = v4l2_fh_open,
	.release = vb2_fop_release,
	.read = vb2_fop_read,
	.poll = vb2_fop_poll,
	.mmap = vb2_fop_mmap,
	.unlocked_ioctl = video_ioctl2,
};

/* ------------------------------------------------------------ source */

/*
 * The source of the attached device changed: while no application has
 * buffers, the application format follows it; a change is reported with
 * V4L2_EVENT_SOURCE_CHANGE in any case.
 */
static void gchd_app_update_workfn(struct work_struct *work)
{
	static const struct v4l2_event ev = {
		.type = V4L2_EVENT_SOURCE_CHANGE,
		.u.src_change.changes = V4L2_EVENT_SRC_CH_RESOLUTION,
	};
	const struct v4l2_frmsize_discrete *s;
	struct v4l2_fract src_rate, rate;
	struct gchd *d;
	u32 w, h;
	bool known;

	d = gchd_app_get();
	if (!d)
		return;
	known = gchd_source_format(d, &w, &h, &src_rate);
	gchd_put(d);
	if (!known)
		return;
	s = gchd_app_size(w, h);
	rate = gchd_app_rate(src_rate.numerator, src_rate.denominator);

	mutex_lock(&app.lock);
	if (s->width != app.width || s->height != app.height ||
	    rate.numerator != app.rate.numerator ||
	    rate.denominator != app.rate.denominator) {
		if (!vb2_is_busy(&app.q)) {
			app.width = s->width;
			app.height = s->height;
			app.rate = rate;
		}
		v4l2_event_queue(&app.vdev, &ev);
	}
	mutex_unlock(&app.lock);
	gchd_app_notify();
}

void gchd_app_update(struct gchd *d)
{
	if (READ_ONCE(app.d) == d)
		schedule_work(&app.update_work);
}

void gchd_app_attach(struct gchd *d)
{
	mutex_lock(&app.attach_lock);
	if (!app.d) {
		kref_get(&d->refcount);
		app.d = d;
		dev_info(&d->intf->dev, "attached to the application device %s\n",
			 video_device_node_name(&app.vdev));
	}
	mutex_unlock(&app.attach_lock);
	gchd_app_notify();
	gchd_app_update(d);
}

void gchd_app_detach(struct gchd *d)
{
	bool was = false;

	mutex_lock(&app.attach_lock);
	if (app.d == d) {
		app.d = NULL;
		was = true;
	}
	mutex_unlock(&app.attach_lock);
	if (!was)
		return;
	cancel_work_sync(&app.update_work);
	gchd_put(d);
	gchd_app_notify();
}

/* ------------------------------------------------------------ feed device */

static int gchd_feed_queue_setup(struct vb2_queue *q, unsigned int *nb,
				 unsigned int *np, unsigned int sizes[],
				 struct device *alloc[])
{
	u32 size = app.feed_width * app.feed_height * 2;

	if (*np)
		return sizes[0] < size ? -EINVAL : 0;
	*np = 1;
	sizes[0] = size;
	if (*nb < 2)
		*nb = 2;
	return 0;
}

/*
 * One frame per buffer. A payload of 0 means the whole buffer: write()
 * queues its buffers with 0 (the write emulation does not pass its byte
 * count to the driver).
 */
static int gchd_feed_buf_prepare(struct vb2_buffer *vb)
{
	unsigned long size = app.feed_width * app.feed_height * 2;
	unsigned long used = vb2_get_plane_payload(vb, 0);

	if (!used) {
		used = size;
		vb2_set_plane_payload(vb, 0, size);
	}
	if (used != size)
		return -EINVAL;
	return 0;
}

static void gchd_feed_process(struct vb2_buffer *vb)
{
	gchd_app_frame(vb2_plane_vaddr(vb, 0), vb2_get_plane_payload(vb, 0));
	vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
}

static void gchd_feed_buf_queue(struct vb2_buffer *vb)
{
	if (!vb2_start_streaming_called(vb->vb2_queue)) {
		list_add_tail(&to_app_buf(vb)->list, &app.feed_pending);
		return;
	}
	gchd_feed_process(vb);
}

static int gchd_feed_start(struct vb2_queue *q, unsigned int count)
{
	struct gchd_app_buf *b, *n;

	list_for_each_entry_safe(b, n, &app.feed_pending, list) {
		list_del(&b->list);
		gchd_feed_process(&b->vb.vb2_buf);
	}
	return 0;
}

static void gchd_feed_stop(struct vb2_queue *q)
{
	struct gchd_app_buf *b, *n;

	list_for_each_entry_safe(b, n, &app.feed_pending, list) {
		list_del(&b->list);
		vb2_buffer_done(&b->vb.vb2_buf, VB2_BUF_STATE_ERROR);
	}
}

static const struct vb2_ops gchd_feed_qops = {
	.queue_setup = gchd_feed_queue_setup,
	.buf_prepare = gchd_feed_buf_prepare,
	.buf_queue = gchd_feed_buf_queue,
	.start_streaming = gchd_feed_start,
	.stop_streaming = gchd_feed_stop,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
};

/* The frame input format is the format of the application device. */
static int gchd_feed_try_fmt(struct file *f, void *p, struct v4l2_format *x)
{
	gchd_app_pix(app.width, app.height, &x->fmt.pix);
	return 0;
}

static int gchd_feed_g_fmt(struct file *f, void *p, struct v4l2_format *x)
{
	gchd_app_pix(app.feed_width, app.feed_height, &x->fmt.pix);
	return 0;
}

static int gchd_feed_s_fmt(struct file *f, void *p, struct v4l2_format *x)
{
	gchd_feed_try_fmt(f, p, x);
	if (x->fmt.pix.width == app.feed_width &&
	    x->fmt.pix.height == app.feed_height)
		return 0;
	if (vb2_is_busy(&app.feed_q))
		return -EBUSY;
	app.feed_width = x->fmt.pix.width;
	app.feed_height = x->fmt.pix.height;
	return 0;
}

static const struct v4l2_ioctl_ops gchd_feed_ioctl = {
	.vidioc_querycap = gchd_app_querycap,
	.vidioc_enum_fmt_vid_out = gchd_app_enum_fmt,
	.vidioc_g_fmt_vid_out = gchd_feed_g_fmt,
	.vidioc_s_fmt_vid_out = gchd_feed_s_fmt,
	.vidioc_try_fmt_vid_out = gchd_feed_try_fmt,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
};

static const struct v4l2_file_operations gchd_feed_fops = {
	.owner = THIS_MODULE,
	.open = v4l2_fh_open,
	.release = vb2_fop_release,
	.write = vb2_fop_write,
	.poll = vb2_fop_poll,
	.mmap = vb2_fop_mmap,
	.unlocked_ioctl = video_ioctl2,
};

/* ------------------------------------------------------------ sysfs */

static ssize_t gchd_state_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct gchd *d = gchd_app_get();
	ssize_t n;

	n = sysfs_emit(buf, "%s %u %u %u %u %s %s\n",
		       READ_ONCE(app.streaming) ? "streaming" : "idle",
		       app.width, app.height,
		       app.rate.numerator, app.rate.denominator,
		       video_device_node_name(&app.feed),
		       d ? video_device_node_name(&d->vdev) : "-");
	if (d)
		gchd_put(d);
	return n;
}
static DEVICE_ATTR_RO(gchd_state);

/* ------------------------------------------------------------ setup */

static int gchd_app_init_queue(struct vb2_queue *q, enum v4l2_buf_type type,
			       unsigned int modes, const struct vb2_ops *ops,
			       struct mutex *lock, u32 ts)
{
	q->type = type;
	q->io_modes = modes;
	q->timestamp_flags = ts;
	q->drv_priv = &app;
	q->buf_struct_size = sizeof(struct gchd_app_buf);
	q->ops = ops;
	q->mem_ops = &vb2_vmalloc_memops;
	q->lock = lock;
	return vb2_queue_init(q);
}

static void gchd_app_init_vdev(struct video_device *v, const char *name,
			       const struct v4l2_file_operations *fops,
			       const struct v4l2_ioctl_ops *ioctl,
			       struct vb2_queue *q, struct mutex *lock, u32 caps)
{
	strscpy(v->name, name, sizeof(v->name));
	v->v4l2_dev = &app.v4l2_dev;
	v->fops = fops;
	v->ioctl_ops = ioctl;
	v->release = video_device_release_empty;
	v->queue = q;
	v->lock = lock;
	v->device_caps = caps;
}

int gchd_app_register(void)
{
	int r;

	mutex_init(&app.lock);
	mutex_init(&app.deliver);
	spin_lock_init(&app.qlock);
	INIT_LIST_HEAD(&app.bufs);
	mutex_init(&app.feed_lock);
	INIT_LIST_HEAD(&app.feed_pending);
	mutex_init(&app.attach_lock);
	INIT_WORK(&app.update_work, gchd_app_update_workfn);
	app.width = 720;
	app.height = 480;
	app.rate = gchd_app_rates[1];
	app.feed_width = app.width;
	app.feed_height = app.height;

	strscpy(app.v4l2_dev.name, "elgato-gchd", sizeof(app.v4l2_dev.name));
	r = v4l2_device_register(NULL, &app.v4l2_dev);
	if (r)
		return r;

	r = gchd_app_init_queue(&app.q, V4L2_BUF_TYPE_VIDEO_CAPTURE,
				VB2_MMAP | VB2_USERPTR | VB2_READ,
				&gchd_app_qops, &app.lock,
				V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC);
	if (!r)
		r = gchd_app_init_queue(&app.feed_q, V4L2_BUF_TYPE_VIDEO_OUTPUT,
					VB2_MMAP | VB2_USERPTR | VB2_WRITE,
					&gchd_feed_qops, &app.feed_lock,
					V4L2_BUF_FLAG_TIMESTAMP_COPY);
	if (r)
		goto err_v4l2;

	gchd_app_init_vdev(&app.vdev, "Elgato Game Capture HD",
			   &gchd_app_fops, &gchd_app_ioctl, &app.q, &app.lock,
			   V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING |
			   V4L2_CAP_READWRITE);
	gchd_app_init_vdev(&app.feed, "Elgato Game Capture HD (frame input)",
			   &gchd_feed_fops, &gchd_feed_ioctl, &app.feed_q,
			   &app.feed_lock,
			   V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_STREAMING |
			   V4L2_CAP_READWRITE);
	app.feed.vfl_dir = VFL_DIR_TX;

	r = video_register_device(&app.vdev, VFL_TYPE_VIDEO, -1);
	if (r)
		goto err_v4l2;
	r = video_register_device(&app.feed, VFL_TYPE_VIDEO, -1);
	if (r)
		goto err_app;
	r = device_create_file(&app.vdev.dev, &dev_attr_gchd_state);
	if (r)
		goto err_feed;
	app.registered = true;
	pr_info("elgato_gchd: application device %s, frame input %s\n",
		video_device_node_name(&app.vdev),
		video_device_node_name(&app.feed));
	return 0;

err_feed:
	vb2_video_unregister_device(&app.feed);
err_app:
	vb2_video_unregister_device(&app.vdev);
err_v4l2:
	v4l2_device_unregister(&app.v4l2_dev);
	return r;
}

/* Module exit: all USB devices are disconnected (detached) by now. */
void gchd_app_unregister(void)
{
	if (!app.registered)
		return;
	app.registered = false;
	cancel_work_sync(&app.update_work);
	device_remove_file(&app.vdev.dev, &dev_attr_gchd_state);
	vb2_video_unregister_device(&app.feed);
	vb2_video_unregister_device(&app.vdev);
	v4l2_device_unregister(&app.v4l2_dev);
}
