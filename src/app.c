// SPDX-License-Identifier: GPL-2.0
/*
 * Application device and feed device.
 *
 * The application device ("Elgato Game Capture HD") is a capture device with
 * uncompressed YUYV frames for applications that do not handle the
 * compressed stream. The frames are written into the feed device (an output
 * device) by a userspace program that decodes the stream of the main
 * device; the driver copies each written frame into the next queued buffer
 * of the application device. No decoding takes place in the kernel.
 *
 * The application device has the size and frame rate of the source. When
 * the source size or rate changes while an application uses the device, the
 * device is unregistered and registered again with the new format, as when
 * a USB camera is unplugged and plugged in again. The registration waits
 * until the old device is closed, at most GCHD_APP_REPLUG_MS.
 *
 * The state is published in the sysfs attribute "app" of the USB interface:
 *   STATE APP-NODE WIDTH HEIGHT RATE-NUM RATE-DEN FEED-NODE
 * STATE is "streaming" while an application streams from the application
 * device, "idle" otherwise; "-" for a node that is not registered.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/sysfs.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-vmalloc.h>
#include "elgato_gchd.h"

#define GCHD_APP_REPLUG_MS 2000
#define GCHD_APP_RETRY_MS 100

struct gchd_app_buf {
	struct vb2_v4l2_buffer vb;
	struct list_head list;
};

/* One registration of the application device. */
struct gchd_app {
	struct gchd *d;
	struct video_device vdev;
	struct vb2_queue q;
	struct mutex lock;	/* V4L2 ioctls and the queue */
	struct mutex deliver;	/* frame delivery against stop_streaming */
	spinlock_t qlock;
	struct list_head bufs;
	u32 width, height, sizeimage;
	struct v4l2_fract rate;	/* frames per second */
	u32 sequence;
	bool streaming;
};

static struct gchd_app_buf *to_app_buf(struct vb2_buffer *vb)
{
	return container_of(to_vb2_v4l2_buffer(vb), struct gchd_app_buf, vb);
}

static void gchd_app_notify(struct gchd *d)
{
	if (!READ_ONCE(d->disconnected))
		sysfs_notify(&d->intf->dev.kobj, NULL, "app");
}

/* ------------------------------------------------------------ format */

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

/* ------------------------------------------------------------ app queue */

static int gchd_app_queue_setup(struct vb2_queue *q, unsigned int *nb,
				unsigned int *np, unsigned int sizes[],
				struct device *alloc[])
{
	struct gchd_app *a = vb2_get_drv_priv(q);

	if (*np)
		return sizes[0] < a->sizeimage ? -EINVAL : 0;
	*np = 1;
	sizes[0] = a->sizeimage;
	if (*nb < 2)
		*nb = 2;
	return 0;
}

static int gchd_app_buf_prepare(struct vb2_buffer *vb)
{
	struct gchd_app *a = vb2_get_drv_priv(vb->vb2_queue);

	if (vb2_plane_size(vb, 0) < a->sizeimage)
		return -EINVAL;
	vb2_set_plane_payload(vb, 0, a->sizeimage);
	return 0;
}

static void gchd_app_buf_queue(struct vb2_buffer *vb)
{
	struct gchd_app *a = vb2_get_drv_priv(vb->vb2_queue);
	unsigned long flags;

	spin_lock_irqsave(&a->qlock, flags);
	list_add_tail(&to_app_buf(vb)->list, &a->bufs);
	spin_unlock_irqrestore(&a->qlock, flags);
}

static int gchd_app_start(struct vb2_queue *q, unsigned int count)
{
	struct gchd_app *a = vb2_get_drv_priv(q);

	a->sequence = 0;
	WRITE_ONCE(a->streaming, true);
	gchd_app_notify(a->d);
	return 0;
}

static void gchd_app_stop(struct vb2_queue *q)
{
	struct gchd_app *a = vb2_get_drv_priv(q);
	struct gchd_app_buf *b, *n;
	unsigned long flags;

	mutex_lock(&a->deliver);
	WRITE_ONCE(a->streaming, false);
	spin_lock_irqsave(&a->qlock, flags);
	list_for_each_entry_safe(b, n, &a->bufs, list) {
		list_del(&b->list);
		vb2_buffer_done(&b->vb.vb2_buf, VB2_BUF_STATE_ERROR);
	}
	spin_unlock_irqrestore(&a->qlock, flags);
	mutex_unlock(&a->deliver);
	gchd_app_notify(a->d);
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

/* Copy one frame written into the feed device to the application device. */
static void gchd_app_frame(struct gchd *d, const void *src, size_t size)
{
	struct gchd_app *a;
	struct gchd_app_buf *b = NULL;
	unsigned long flags;

	mutex_lock(&d->app_lock);
	a = d->app;
	if (!a || !READ_ONCE(a->streaming) || size != a->sizeimage)
		goto out;
	mutex_lock(&a->deliver);
	if (a->streaming) {
		spin_lock_irqsave(&a->qlock, flags);
		b = list_first_entry_or_null(&a->bufs, struct gchd_app_buf, list);
		if (b)
			list_del(&b->list);
		spin_unlock_irqrestore(&a->qlock, flags);
	}
	if (b) {
		memcpy(vb2_plane_vaddr(&b->vb.vb2_buf, 0), src, size);
		vb2_set_plane_payload(&b->vb.vb2_buf, 0, size);
		b->vb.field = V4L2_FIELD_NONE;
		b->vb.sequence = a->sequence++;
		b->vb.vb2_buf.timestamp = ktime_get_ns();
		vb2_buffer_done(&b->vb.vb2_buf, VB2_BUF_STATE_DONE);
	}
	mutex_unlock(&a->deliver);
out:
	mutex_unlock(&d->app_lock);
}

/* ------------------------------------------------------------ app ioctls */

static int gchd_app_querycap(struct file *f, void *p, struct v4l2_capability *c)
{
	struct gchd_app *a = video_drvdata(f);

	strscpy(c->driver, "elgato-gchd", sizeof(c->driver));
	strscpy(c->card, a->vdev.name, sizeof(c->card));
	usb_make_path(a->d->udev, c->bus_info, sizeof(c->bus_info));
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
	struct gchd_app *a = video_drvdata(f);

	gchd_app_pix(a->width, a->height, &x->fmt.pix);
	return 0;
}

static int gchd_app_framesizes(struct file *f, void *p,
			       struct v4l2_frmsizeenum *s)
{
	struct gchd_app *a = video_drvdata(f);

	if (s->index || s->pixel_format != V4L2_PIX_FMT_YUYV)
		return -EINVAL;
	s->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	s->discrete.width = a->width;
	s->discrete.height = a->height;
	return 0;
}

static int gchd_app_frameintervals(struct file *f, void *p,
				   struct v4l2_frmivalenum *iv)
{
	struct gchd_app *a = video_drvdata(f);

	if (iv->index || iv->pixel_format != V4L2_PIX_FMT_YUYV ||
	    iv->width != a->width || iv->height != a->height)
		return -EINVAL;
	iv->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	iv->discrete.numerator = a->rate.denominator;
	iv->discrete.denominator = a->rate.numerator;
	return 0;
}

static int gchd_app_g_parm(struct file *f, void *p, struct v4l2_streamparm *s)
{
	struct gchd_app *a = video_drvdata(f);

	if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	memset(&s->parm.capture, 0, sizeof(s->parm.capture));
	s->parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
	s->parm.capture.timeperframe.numerator = a->rate.denominator;
	s->parm.capture.timeperframe.denominator = a->rate.numerator;
	s->parm.capture.readbuffers = 2;
	return 0;
}

static int gchd_app_enum_input(struct file *f, void *p, struct v4l2_input *in)
{
	static const char * const names[] = {
		"HDMI", "Component", "Composite", "S-Video",
	};
	struct gchd_app *a = video_drvdata(f);
	unsigned int index = in->index;

	if (index >= ARRAY_SIZE(names))
		return -EINVAL;
	memset(in, 0, sizeof(*in));
	in->index = index;
	strscpy(in->name, names[index], sizeof(in->name));
	in->type = V4L2_INPUT_TYPE_CAMERA;
	if (index == READ_ONCE(a->d->input) && !READ_ONCE(a->d->signal_present) &&
	    READ_ONCE(a->d->hw_initialized))
		in->status = V4L2_IN_ST_NO_SIGNAL;
	return 0;
}

static int gchd_app_g_input(struct file *f, void *p, unsigned int *i)
{
	struct gchd_app *a = video_drvdata(f);

	*i = READ_ONCE(a->d->input);
	return 0;
}

static int gchd_app_s_input(struct file *f, void *p, unsigned int i)
{
	struct gchd_app *a = video_drvdata(f);

	if (i > 3)
		return -EINVAL;
	if (i == READ_ONCE(a->d->input))
		return 0;
	return gchd_select_input(a->d, i);
}

static const struct v4l2_ioctl_ops gchd_app_ioctl = {
	.vidioc_querycap = gchd_app_querycap,
	.vidioc_enum_fmt_vid_cap = gchd_app_enum_fmt,
	.vidioc_g_fmt_vid_cap = gchd_app_g_fmt,
	.vidioc_s_fmt_vid_cap = gchd_app_g_fmt,
	.vidioc_try_fmt_vid_cap = gchd_app_g_fmt,
	.vidioc_enum_framesizes = gchd_app_framesizes,
	.vidioc_enum_frameintervals = gchd_app_frameintervals,
	.vidioc_g_parm = gchd_app_g_parm,
	.vidioc_s_parm = gchd_app_g_parm,
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

/* ------------------------------------------------------------ app lifetime */

static void gchd_app_release(struct video_device *vdev)
{
	struct gchd_app *a = container_of(vdev, struct gchd_app, vdev);

	/* The driver data outlives this call (v4l2_device reference). */
	atomic_dec(&a->d->app_alive);
	kfree(a);
}

/* Register an application device with the current format; app_lock held. */
static int gchd_app_add(struct gchd *d)
{
	struct gchd_app *a;
	int r;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;
	a->d = d;
	mutex_init(&a->lock);
	mutex_init(&a->deliver);
	spin_lock_init(&a->qlock);
	INIT_LIST_HEAD(&a->bufs);
	a->width = d->app_width;
	a->height = d->app_height;
	a->sizeimage = a->width * a->height * 2;
	a->rate = d->app_rate;

	a->q.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->q.io_modes = VB2_MMAP | VB2_USERPTR | VB2_READ;
	a->q.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	a->q.dev = &d->intf->dev;
	a->q.drv_priv = a;
	a->q.buf_struct_size = sizeof(struct gchd_app_buf);
	a->q.ops = &gchd_app_qops;
	a->q.mem_ops = &vb2_vmalloc_memops;
	a->q.lock = &a->lock;
	a->q.min_queued_buffers = 1;
	r = vb2_queue_init(&a->q);
	if (r) {
		kfree(a);
		return r;
	}

	strscpy(a->vdev.name, "Elgato Game Capture HD", sizeof(a->vdev.name));
	a->vdev.v4l2_dev = &d->v4l2_dev;
	a->vdev.fops = &gchd_app_fops;
	a->vdev.ioctl_ops = &gchd_app_ioctl;
	/* Set after a successful registration (see below). */
	a->vdev.release = video_device_release_empty;
	a->vdev.queue = &a->q;
	a->vdev.lock = &a->lock;
	a->vdev.device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING |
			      V4L2_CAP_READWRITE;
	video_set_drvdata(&a->vdev, a);

	atomic_inc(&d->app_alive);
	r = video_register_device(&a->vdev, VFL_TYPE_VIDEO, d->app_nr);
	if (r) {
		atomic_dec(&d->app_alive);
		kfree(a);
		return r;
	}
	/* From now on the instance is freed when its last handle is closed. */
	a->vdev.release = gchd_app_release;
	d->app_nr = a->vdev.num;
	d->app = a;
	dev_info(&d->intf->dev, "application device %s: %ux%u, %u/%u frames/s\n",
		 video_device_node_name(&a->vdev), a->width, a->height,
		 a->rate.numerator, a->rate.denominator);
	return 0;
}

/* Unregister the application device; app_lock held. */
static void gchd_app_remove(struct gchd *d)
{
	struct gchd_app *a = d->app;

	if (!a)
		return;
	d->app = NULL;
	/* Stops streaming and releases the buffers; open handles get ENODEV. */
	vb2_video_unregister_device(&a->vdev);
}

/*
 * Follow the source format: update the application device in place while
 * no application uses it, otherwise unregister it and register it again.
 */
static void gchd_app_workfn(struct work_struct *work)
{
	struct gchd *d = container_of(to_delayed_work(work), struct gchd,
				      app_work);
	struct gchd_app *a;
	u32 w, h;
	struct v4l2_fract rate;
	unsigned int input;
	bool busy, known;

	if (READ_ONCE(d->disconnected))
		return;
	input = READ_ONCE(d->input);
	known = gchd_source_format(d, &w, &h, &rate);

	mutex_lock(&d->app_lock);
	/*
	 * No mode detected (capture starting, no signal): keep the format, the
	 * source is most likely the same. After an input change the nominal
	 * format of the new input applies.
	 */
	if (!known && input == d->app_input) {
		w = d->app_width;
		h = d->app_height;
		rate = d->app_rate;
	}
	d->app_input = input;
	d->app_width = w;
	d->app_height = h;
	d->app_rate = rate;
	a = d->app;
	if (a) {
		if (a->width == w && a->height == h &&
		    a->rate.numerator == rate.numerator &&
		    a->rate.denominator == rate.denominator)
			goto out;
		mutex_lock(&a->lock);
		busy = vb2_is_busy(&a->q);
		if (!busy) {
			a->width = w;
			a->height = h;
			a->sizeimage = w * h * 2;
			a->rate = rate;
		}
		mutex_unlock(&a->lock);
		if (!busy) {
			dev_info(&d->intf->dev,
				 "application device: %ux%u, %u/%u frames/s\n",
				 w, h, rate.numerator, rate.denominator);
			goto notify;
		}
		dev_info(&d->intf->dev,
			 "application device: source now %ux%u, %u/%u frames/s; re-registering\n",
			 w, h, rate.numerator, rate.denominator);
		gchd_app_remove(d);
		d->app_wait_until = jiffies + msecs_to_jiffies(GCHD_APP_REPLUG_MS);
	}
	/* Register again once the old device is closed (same number). */
	if (atomic_read(&d->app_alive) &&
	    time_before(jiffies, d->app_wait_until)) {
		schedule_delayed_work(&d->app_work,
				      msecs_to_jiffies(GCHD_APP_RETRY_MS));
		goto notify;
	}
	if (gchd_app_add(d))
		dev_err(&d->intf->dev, "application device: registration failed\n");
notify:
	gchd_app_notify(d);
out:
	mutex_unlock(&d->app_lock);
}

void gchd_app_update(struct gchd *d)
{
	if (READ_ONCE(d->app_ready) && !READ_ONCE(d->disconnected))
		mod_delayed_work(system_wq, &d->app_work, 0);
}

/* ------------------------------------------------------------ feed device */

static int gchd_feed_queue_setup(struct vb2_queue *q, unsigned int *nb,
				 unsigned int *np, unsigned int sizes[],
				 struct device *alloc[])
{
	struct gchd *d = vb2_get_drv_priv(q);
	u32 size = d->feed_width * d->feed_height * 2;

	if (*np)
		return sizes[0] < size ? -EINVAL : 0;
	*np = 1;
	sizes[0] = size;
	if (*nb < 2)
		*nb = 2;
	return 0;
}

static int gchd_feed_buf_prepare(struct vb2_buffer *vb)
{
	struct gchd *d = vb2_get_drv_priv(vb->vb2_queue);

	if (vb2_get_plane_payload(vb, 0) != d->feed_width * d->feed_height * 2)
		return -EINVAL;
	return 0;
}

static void gchd_feed_process(struct gchd *d, struct vb2_buffer *vb)
{
	gchd_app_frame(d, vb2_plane_vaddr(vb, 0), vb2_get_plane_payload(vb, 0));
	vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
}

static void gchd_feed_buf_queue(struct vb2_buffer *vb)
{
	struct gchd *d = vb2_get_drv_priv(vb->vb2_queue);

	if (!vb2_start_streaming_called(vb->vb2_queue)) {
		list_add_tail(&to_app_buf(vb)->list, &d->feed_pending);
		return;
	}
	gchd_feed_process(d, vb);
}

static int gchd_feed_start(struct vb2_queue *q, unsigned int count)
{
	struct gchd *d = vb2_get_drv_priv(q);
	struct gchd_app_buf *b, *n;

	list_for_each_entry_safe(b, n, &d->feed_pending, list) {
		list_del(&b->list);
		gchd_feed_process(d, &b->vb.vb2_buf);
	}
	return 0;
}

static void gchd_feed_stop(struct vb2_queue *q)
{
	struct gchd *d = vb2_get_drv_priv(q);
	struct gchd_app_buf *b, *n;

	list_for_each_entry_safe(b, n, &d->feed_pending, list) {
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

static int gchd_feed_querycap(struct file *f, void *p, struct v4l2_capability *c)
{
	struct gchd *d = video_drvdata(f);

	strscpy(c->driver, "elgato-gchd", sizeof(c->driver));
	strscpy(c->card, d->feed_vdev.name, sizeof(c->card));
	usb_make_path(d->udev, c->bus_info, sizeof(c->bus_info));
	return 0;
}

static int gchd_feed_enum_fmt(struct file *f, void *p, struct v4l2_fmtdesc *x)
{
	if (x->index)
		return -EINVAL;
	x->pixelformat = V4L2_PIX_FMT_YUYV;
	return 0;
}

/* The feed format is the format of the application device. */
static int gchd_feed_try_fmt(struct file *f, void *p, struct v4l2_format *x)
{
	struct gchd *d = video_drvdata(f);

	mutex_lock(&d->app_lock);
	gchd_app_pix(d->app_width, d->app_height, &x->fmt.pix);
	mutex_unlock(&d->app_lock);
	return 0;
}

static int gchd_feed_g_fmt(struct file *f, void *p, struct v4l2_format *x)
{
	struct gchd *d = video_drvdata(f);

	gchd_app_pix(d->feed_width, d->feed_height, &x->fmt.pix);
	return 0;
}

static int gchd_feed_s_fmt(struct file *f, void *p, struct v4l2_format *x)
{
	struct gchd *d = video_drvdata(f);

	gchd_feed_try_fmt(f, p, x);
	if (x->fmt.pix.width == d->feed_width &&
	    x->fmt.pix.height == d->feed_height)
		return 0;
	if (vb2_is_busy(&d->feed_q))
		return -EBUSY;
	d->feed_width = x->fmt.pix.width;
	d->feed_height = x->fmt.pix.height;
	return 0;
}

static const struct v4l2_ioctl_ops gchd_feed_ioctl = {
	.vidioc_querycap = gchd_feed_querycap,
	.vidioc_enum_fmt_vid_out = gchd_feed_enum_fmt,
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

static ssize_t app_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct gchd *d = usb_get_intfdata(to_usb_interface(dev));
	struct gchd_app *a;
	ssize_t n;

	if (!d || READ_ONCE(d->disconnected) || !READ_ONCE(d->app_ready))
		return -ENODEV;
	mutex_lock(&d->app_lock);
	a = d->app;
	n = sysfs_emit(buf, "%s %s %u %u %u %u %s\n",
		       a && READ_ONCE(a->streaming) ? "streaming" : "idle",
		       a ? video_device_node_name(&a->vdev) : "-",
		       d->app_width, d->app_height,
		       d->app_rate.numerator, d->app_rate.denominator,
		       video_device_node_name(&d->feed_vdev));
	mutex_unlock(&d->app_lock);
	return n;
}
static DEVICE_ATTR_RO(app);

static struct attribute *gchd_attrs[] = {
	&dev_attr_app.attr,
	NULL,
};

static const struct attribute_group gchd_group = {
	.attrs = gchd_attrs,
};

const struct attribute_group *gchd_groups[] = {
	&gchd_group,
	NULL,
};

/* ------------------------------------------------------------ setup */

int gchd_app_init(struct gchd *d)
{
	int r;

	mutex_init(&d->app_lock);
	mutex_init(&d->feed_lock);
	INIT_LIST_HEAD(&d->feed_pending);
	INIT_DELAYED_WORK(&d->app_work, gchd_app_workfn);
	atomic_set(&d->app_alive, 0);
	d->app_nr = -1;
	d->app_input = d->input;
	gchd_source_format(d, &d->app_width, &d->app_height, &d->app_rate);
	d->feed_width = d->app_width;
	d->feed_height = d->app_height;

	d->feed_q.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	d->feed_q.io_modes = VB2_MMAP | VB2_USERPTR | VB2_WRITE;
	d->feed_q.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	d->feed_q.dev = &d->intf->dev;
	d->feed_q.drv_priv = d;
	d->feed_q.buf_struct_size = sizeof(struct gchd_app_buf);
	d->feed_q.ops = &gchd_feed_qops;
	d->feed_q.mem_ops = &vb2_vmalloc_memops;
	d->feed_q.lock = &d->feed_lock;
	r = vb2_queue_init(&d->feed_q);
	if (r)
		return r;

	strscpy(d->feed_vdev.name, "Elgato Game Capture HD (frame input)",
		sizeof(d->feed_vdev.name));
	d->feed_vdev.v4l2_dev = &d->v4l2_dev;
	d->feed_vdev.fops = &gchd_feed_fops;
	d->feed_vdev.ioctl_ops = &gchd_feed_ioctl;
	d->feed_vdev.release = video_device_release_empty;
	d->feed_vdev.queue = &d->feed_q;
	d->feed_vdev.lock = &d->feed_lock;
	d->feed_vdev.vfl_dir = VFL_DIR_TX;
	d->feed_vdev.device_caps = V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_STREAMING |
				   V4L2_CAP_READWRITE;
	video_set_drvdata(&d->feed_vdev, d);
	r = video_register_device(&d->feed_vdev, VFL_TYPE_VIDEO, -1);
	if (r)
		return r;

	mutex_lock(&d->app_lock);
	r = gchd_app_add(d);
	mutex_unlock(&d->app_lock);
	if (r) {
		vb2_video_unregister_device(&d->feed_vdev);
		return r;
	}
	WRITE_ONCE(d->app_ready, true);
	return 0;
}

void gchd_app_cleanup(struct gchd *d)
{
	if (!READ_ONCE(d->app_ready))
		return;
	WRITE_ONCE(d->app_ready, false);
	cancel_delayed_work_sync(&d->app_work);
	mutex_lock(&d->app_lock);
	gchd_app_remove(d);
	mutex_unlock(&d->app_lock);
	vb2_video_unregister_device(&d->feed_vdev);
}
