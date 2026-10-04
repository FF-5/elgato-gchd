#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/timekeeping.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include "elgato_gchd.h"

MODULE_DESCRIPTION("Elgato Game Capture HD V4L2 driver");
MODULE_AUTHOR("FF-5 / elgato-gchd contributors");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.2.0");

/* Private V4L2 menu control so GUI applications can select the connector. */
#define V4L2_CID_GCHD_INPUT_SOURCE (V4L2_CID_USER_BASE + 0x10f0)
static const char * const gchd_input_menu[] = {
 "HDMI", "Component", "Composite", NULL,
};

/* Preserve the VB2 read/poll/mmap/ioctl operations while adding lifecycle hooks. */
static void gchd_release_ref(struct kref *ref)
{
 struct gchd *d = container_of(ref, struct gchd, refcount);

 kfree(d->usb_buf);
 usb_put_dev(d->udev);
 kfree(d);
}
static struct v4l2_file_operations gchd_fops;

static unsigned long ring_bytes = GCHD_RING_BYTES;
module_param(ring_bytes, ulong, 0644);
MODULE_PARM_DESC(ring_bytes, "Maximum encoded video bytes retained in RAM");

struct gchd_mode {
 u32 width;
 u32 height;
 u32 fps_num;
 u32 fps_den;
 bool interlaced;
};

/* The device defaults to HDMI; remember the last selected input across rebinds. */
static DEFINE_MUTEX(gchd_last_input_lock);
static unsigned int gchd_last_input;

static const struct gchd_mode gchd_modes[] = {
 {1920, 1080, 60, 1, false},
 {1920, 1080, 60, 1, true},
 {1280,  720, 60, 1, false},
 { 720,  576, 50, 1, false},
 { 720, 576, 50, 1, true},
 { 720,  480, 60, 1, false},
 { 720, 480, 60, 1, true},
};

static const struct gchd_mode *gchd_find_mode(u32 width, u32 height,
                                                bool interlaced)
{
 unsigned int i;

 for (i = 0; i < ARRAY_SIZE(gchd_modes); ++i)
  if (gchd_modes[i].width == width &&
      gchd_modes[i].height == height &&
      gchd_modes[i].interlaced == interlaced)
   return &gchd_modes[i];

 return NULL;
}

static bool gchd_mode_allowed(const struct gchd *d,
                              const struct gchd_mode *m)
{
 switch (d->input) {
 case 0:
  return (m->width == 1920 && m->height == 1080) ||
         (m->width == 1280 && m->height == 720) ||
         (m->width == 720 && !m->interlaced &&
          (m->height == 480 || m->height == 576));
 case 1:
  return true;
 case 2:
  return m->interlaced && m->width == 720 &&
         (m->height == 480 || m->height == 576);
 default:
  return false;
 }
}

int gchd_ring_push(struct gchd_ring *r, const u8 *data, size_t len)
{
 unsigned long flags; u8 *copy; struct gchd_frame *f; unsigned int tail;
 if (!len || len > GCHD_MAX_FRAME) return -EMSGSIZE;
 copy = kvmemdup(data, len, GFP_KERNEL); if (!copy) return -ENOMEM;
 spin_lock_irqsave(&r->lock, flags);
 while (r->count && (r->count >= GCHD_RING_FRAMES || r->bytes + len > ring_bytes)) {
  tail=(r->head+GCHD_RING_FRAMES-r->count)%GCHD_RING_FRAMES;
  kvfree(r->frames[tail].data); r->bytes-=r->frames[tail].len;
  r->frames[tail].data=NULL; r->frames[tail].len=0; r->count--;
 }
 f=&r->frames[r->head]; f->data=copy; f->len=len; f->sequence=++r->sequence;
 r->bytes+=len; r->head=(r->head+1)%GCHD_RING_FRAMES; r->count++;
 spin_unlock_irqrestore(&r->lock, flags); return 0;
}

struct gchd_frame *gchd_ring_pop(struct gchd_ring *r)
{
 unsigned long flags; struct gchd_frame *f=NULL; unsigned int tail;
 spin_lock_irqsave(&r->lock, flags);
 if (r->count) {
  tail=(r->head+GCHD_RING_FRAMES-r->count)%GCHD_RING_FRAMES;
  f=kmemdup(&r->frames[tail],sizeof(*f),GFP_ATOMIC);
  if (f) { r->frames[tail].data=NULL; r->bytes-=r->frames[tail].len;
   r->frames[tail].len=0; r->count--; }
 }
 spin_unlock_irqrestore(&r->lock, flags); return f;
}
void gchd_ring_free(struct gchd_ring *r)
{
 unsigned long flags; unsigned int i;
 spin_lock_irqsave(&r->lock,flags);
 for(i=0;i<GCHD_RING_FRAMES;i++){ kvfree(r->frames[i].data); r->frames[i].data=NULL; r->frames[i].len=0; }
 r->count=0; r->bytes=0; spin_unlock_irqrestore(&r->lock,flags);
}

/* The device emits MPEG-TS. The existing transcoder assigns video PID 0x1011.
 * Each PES payload is retained as one encoded frame. */
static void gchd_ts(struct gchd *d,const u8 *p,u8 *pes,size_t *n)
{
 u16 pid; unsigned int off=4,afc; bool start;
 if(p[0]!=0x47)return;
 pid=((p[1]&0x1f)<<8)|p[2]; if(pid!=0x1011)return;
 d->last_video_jiffies = jiffies;
 if (!d->signal_present) {
  struct v4l2_event ev = { .type = V4L2_EVENT_SOURCE_CHANGE };
  ev.id = d->input;
  ev.u.src_change.changes = V4L2_EVENT_SRC_CH_RESOLUTION;
  v4l2_event_queue(&d->vdev, &ev);
  d->signal_present = true;
 }
 start=!!(p[1]&0x40); afc=(p[3]>>4)&3; if(!afc||afc==2)return;
 if(afc==3){off+=1+p[4];if(off>=188)return;}
 if(start){
  if (*n)
   gchd_ring_push(&d->ring, pes, *n);
  *n = 0;
  if(188-off>=9 && p[off]==0 && p[off+1]==0 && p[off+2]==1){
   unsigned int h=9+p[off+8]; if(h>=188-off)return; off+=h;
  }
 }
 if(off<188 && *n<GCHD_MAX_FRAME){size_t m=min_t(size_t,188-off,GCHD_MAX_FRAME-*n);
  memcpy(pes+*n,p+off,m);*n+=m;}
}

static void gchd_signal_check(struct gchd *d)
{
 if (!READ_ONCE(d->signal_present) &&
     time_after(jiffies, READ_ONCE(d->last_video_jiffies) +
                msecs_to_jiffies(500)))
  return;

 if (READ_ONCE(d->signal_present) &&
     time_after(jiffies, READ_ONCE(d->last_video_jiffies) +
                msecs_to_jiffies(500))) {
  struct v4l2_event ev = { .type = V4L2_EVENT_SOURCE_CHANGE };

  ev.id = d->input;
  ev.u.src_change.changes = V4L2_EVENT_SRC_CH_RESOLUTION;
  v4l2_event_queue(&d->vdev, &ev);
  WRITE_ONCE(d->signal_present, false);
  /* RX must not issue control transfers or attempt to stop itself here. */
  if (READ_ONCE(d->streaming) && READ_ONCE(d->detect_requested))
   schedule_delayed_work(&d->detect_work, 0);
  pr_info("elgato_gchd: input %u signal lost; scheduling redetection\n",
          d->input);
 }
}

static void gchd_deliver(struct gchd *d)
{
 struct gchd_frame *f; struct gchd_buffer *b; unsigned long flags; void *dst;
 if(!d->streaming)return;
 f=gchd_ring_pop(&d->ring); if(!f)return;
 spin_lock_irqsave(&d->qlock,flags);
 if(list_empty(&d->queued)){spin_unlock_irqrestore(&d->qlock,flags);kvfree(f->data);kfree(f);return;}
 b=list_first_entry(&d->queued,struct gchd_buffer,list);list_del(&b->list);
 spin_unlock_irqrestore(&d->qlock,flags);
 dst=vb2_plane_vaddr(&b->vb.vb2_buf,0);
 if(!dst || f->len>d->sizeimage) vb2_buffer_done(&b->vb.vb2_buf,VB2_BUF_STATE_ERROR);
 else {memcpy(dst,f->data,f->len);vb2_set_plane_payload(&b->vb.vb2_buf,0,f->len);
  b->vb.sequence=f->sequence;b->vb.vb2_buf.timestamp=ktime_get_ns();
  vb2_buffer_done(&b->vb.vb2_buf,VB2_BUF_STATE_DONE);}
 kvfree(f->data);kfree(f);
}

static int gchd_rx(void *arg)
{
 struct gchd *d=arg; u8 *pes; size_t n=0; int ret,actual,pos;
 pes=kvmalloc(GCHD_MAX_FRAME,GFP_KERNEL);if(!pes)return-ENOMEM;
 while(!kthread_should_stop()&&!d->disconnected){
  ret=usb_bulk_msg(d->udev,usb_rcvbulkpipe(d->udev,GCHD_EP_IN),d->usb_buf,
                   GCHD_USB_BUFSIZE,&actual,1000);
  if (ret == -ETIMEDOUT || ret == -EAGAIN) {
   gchd_signal_check(d);
   gchd_deliver(d);
   continue;
  }
  if (ret)
   break;
  for(pos=0;pos<actual;){
   size_t take=min_t(size_t,188-d->ts_partial_len,actual-pos);
   memcpy(d->ts_partial+d->ts_partial_len,d->usb_buf+pos,take);
   d->ts_partial_len+=take;pos+=take;
   if(d->ts_partial_len==188){gchd_ts(d,d->ts_partial,pes,&n);d->ts_partial_len=0;}
  }
  gchd_signal_check(d);
  gchd_deliver(d);
 }
 if (n)
  gchd_ring_push(&d->ring, pes, n);
 kvfree(pes);
 return 0;
}

static void gchd_detect_workfn(struct work_struct *work)
{
 struct gchd *d = container_of(to_delayed_work(work), struct gchd,
                               detect_work);
 int r = 0;

 mutex_lock(&d->lifecycle_lock);
 if (d->disconnected || !READ_ONCE(d->detect_requested))
  goto out;

 /* Only the RX thread may own bulk-IN while START is active. */
 if (d->rx_thread) {
  kthread_stop(d->rx_thread);
  d->rx_thread = NULL;
  r = gchd_stream_stop(d);
  if (r) {
   dev_warn(&d->intf->dev,
            "redetection: stream stop failed: %d; retrying later\n", r);
   goto retry;
  }

  /*
   * Stop transport and return to IDLE without re-running hardware init.
   * hw_init() issues RESET from every nonzero state; on HDNew that RESET
   * transition times out when the device is already IDLE (state 0x11).
   * Keep the encoder firmware alive: restarting it after input changes has
   * been observed to fail with -EINVAL.
   */
  gchd_ring_free(&d->ring);
  d->ts_partial_len = 0;
  d->input_configured = false;
  d->input_prepared = false;
  r = gchd_state_cmd(d, SCMD_IDLE, 0, 0, 0x11);
  if (r) {
   dev_warn(&d->intf->dev,
            "redetection: transition to IDLE failed: %d\n", r);
   goto retry;
  }
 }

 if (d->input_configured && !READ_ONCE(d->signal_present)) {
  /*
   * STREAMOFF can race with signal-loss notification. Invalidate the old
   * timing and return to IDLE without resetting/reloading the device.
   */
  d->input_configured = false;
  d->input_prepared = false;
  r = gchd_state_cmd(d, SCMD_IDLE, 0, 0, 0x11);
  if (r) {
   dev_warn(&d->intf->dev,
            "redetection: transition to IDLE after signal loss failed: %d\n", r);
   goto retry;
  }
 }
 if (d->input_prepared && !d->input_configured) {
  /* Short async probe: one timing-register sample, no encoder restart. */
  r = gchd_input_detect_signal(d);
  if (r == -ENOLINK)
   goto retry;
  if (r) {
   dev_warn(&d->intf->dev, "signal probe failed: %d\n", r);
   goto retry;
  }
 }

 if (!d->input_configured) {
  /*
   * This call runs the userspace-equivalent connector setup only after a
   * signal has been found by the short probe. On the first attempt it also
   * prepares the receiver and may return -ENOLINK, leaving it prepared.
   */
  if (!d->input_prepared) {
   d->input_width = 0;
   d->input_height = 0;
   d->input_fps_num = 0;
   d->input_fps_den = 0;
   d->input_interlaced = false;
   d->mode_forced = false;
  }

  r = gchd_input_configure_idle(d);
  if (r) {
   if (r != -ENOLINK) {
    dev_warn(&d->intf->dev,
             "redetection: input configuration failed: %d\n", r);
    /*
     * Do not call hw_init() from IDLE: it attempts SCMD_RESET and times out
     * because HDNew's IDLE state (0x11) does not transition to RESET (0x10).
     * Keep the encoder instance alive and retry connector setup from IDLE.
     */
    d->input_configured = false;
    d->input_prepared = false;
    r = gchd_state_cmd(d, SCMD_IDLE, 0, 0, 0x11);
    if (r)
     dev_warn(&d->intf->dev,
              "redetection: could not restore IDLE after setup error: %d\n", r);
   }
   goto retry;
  }
 }

 if (READ_ONCE(d->signal_present)) {
  struct v4l2_event ev = { .type = V4L2_EVENT_SOURCE_CHANGE };

  d->width = d->input_width;
  d->height = d->input_height;
  ev.id = d->input;
  ev.u.src_change.changes = V4L2_EVENT_SRC_CH_RESOLUTION;
  v4l2_event_queue(&d->vdev, &ev);

  if (READ_ONCE(d->streaming)) {
   r = gchd_input_start(d);
   if (r) {
    dev_warn(&d->intf->dev, "capture start after detection failed: %d\n", r);
    goto retry;
   }
   d->last_video_jiffies = jiffies;
   d->rx_thread = kthread_run(gchd_rx, d, "gchd-rx");
   if (IS_ERR(d->rx_thread)) {
    r = PTR_ERR(d->rx_thread);
    d->rx_thread = NULL;
    gchd_stream_stop(d);
    goto retry;
   }
  } else {
   /* Configured and ready; later STREAMON uses this known-good setup. */
   WRITE_ONCE(d->detect_requested, false);
  }
 }

retry:
 if ((r || !READ_ONCE(d->signal_present)) &&
     !d->disconnected && READ_ONCE(d->detect_requested))
  schedule_delayed_work(&d->detect_work, msecs_to_jiffies(250));

out:
 mutex_unlock(&d->lifecycle_lock);
}

static int gchd_queue_setup(struct vb2_queue *q, unsigned int *nb,
                            unsigned int *np, unsigned int sizes[],
                            struct device *alloc[])
{
 struct gchd *d = vb2_get_drv_priv(q);

 if (*np)
  return sizes[0] >= d->sizeimage ? 0 : -EINVAL;
 *np = 1;
 sizes[0] = d->sizeimage;
 return 0;
}

static void gchd_buf_queue(struct vb2_buffer *vb)
{
 struct gchd *d = vb2_get_drv_priv(vb->vb2_queue);
 struct gchd_buffer *b = container_of(to_vb2_v4l2_buffer(vb),
                                      struct gchd_buffer, vb);
 unsigned long flags;

 spin_lock_irqsave(&d->qlock, flags);
 list_add_tail(&b->list, &d->queued);
 spin_unlock_irqrestore(&d->qlock, flags);
 gchd_deliver(d);
}

static void gchd_return_queued(struct gchd *d, enum vb2_buffer_state state)
{
 struct gchd_buffer *b, *tmp;
 unsigned long flags;

 spin_lock_irqsave(&d->qlock, flags);
 list_for_each_entry_safe(b, tmp, &d->queued, list) {
  list_del(&b->list);
  vb2_buffer_done(&b->vb.vb2_buf, state);
 }
 spin_unlock_irqrestore(&d->qlock, flags);
}

static int gchd_start(struct vb2_queue *q, unsigned int count)
{
 struct gchd *d = vb2_get_drv_priv(q);
 int r;

 mutex_lock(&d->lifecycle_lock);
 if (d->disconnected) {
  r = -ENODEV;
  goto err;
 }

 /*
  * Keep STREAMON non-blocking when setup is invalidated by S_FMT or when
  * timing is absent. The worker performs configuration/detection while the
  * V4L2 stream request remains pending.
  */
 WRITE_ONCE(d->streaming, true);
 WRITE_ONCE(d->detect_requested, true);
 if (!d->input_configured || !READ_ONCE(d->signal_present)) {
  schedule_delayed_work(&d->detect_work, 0);
  mutex_unlock(&d->lifecycle_lock);
  return 0;
 }

 r = gchd_input_start(d);
 if (r == -ENOLINK) {
  schedule_delayed_work(&d->detect_work, 0);
  mutex_unlock(&d->lifecycle_lock);
  return 0;
 }
 if (r)
  goto err_streaming;

 d->last_video_jiffies = jiffies;
   d->rx_thread = kthread_run(gchd_rx, d, "gchd-rx");
 if (IS_ERR(d->rx_thread)) {
  r = PTR_ERR(d->rx_thread);
  d->rx_thread = NULL;
  if (gchd_stream_stop(d))
   gchd_input_stop(d);
  goto err_streaming;
 }

 mutex_unlock(&d->lifecycle_lock);
 return 0;

err_streaming:
 WRITE_ONCE(d->streaming, false);
err:
 mutex_unlock(&d->lifecycle_lock);
 gchd_return_queued(d, VB2_BUF_STATE_QUEUED);
 return r;
}

static void gchd_stop(struct vb2_queue *q)
{
 struct gchd *d = vb2_get_drv_priv(q);
 struct gchd_buffer *b, *tmp;
 unsigned long flags;
 int r;

 WRITE_ONCE(d->detect_requested, false);
 WRITE_ONCE(d->streaming, false);
 cancel_delayed_work_sync(&d->detect_work);
 mutex_lock(&d->lifecycle_lock);
 if (d->rx_thread) {
  kthread_stop(d->rx_thread);
  d->rx_thread = NULL;
 }
 if (d->disconnected) {
  r = -ENODEV;
 } else {
  r = gchd_stream_stop(d);
 }
 if (r && !d->disconnected) {
  dev_err(&d->intf->dev,
          "userspace-compatible stream stop failed: %d; disabling encoder as fallback\n",
          r);
  gchd_input_stop(d);
 }
 gchd_ring_free(&d->ring);
 d->ts_partial_len = 0;

 spin_lock_irqsave(&d->qlock, flags);
 list_for_each_entry_safe(b, tmp, &d->queued, list) {
  list_del(&b->list);
  vb2_buffer_done(&b->vb.vb2_buf, VB2_BUF_STATE_ERROR);
 }
 spin_unlock_irqrestore(&d->qlock, flags);

 /*
  * STREAMOFF stops capture, not source monitoring. If no source has been
  * found, keep the asynchronous detector alive so a later connection works
  * without requiring another open/input switch.
  */
 if (!d->disconnected && !READ_ONCE(d->signal_present)) {
  WRITE_ONCE(d->detect_requested, true);
  schedule_delayed_work(&d->detect_work, msecs_to_jiffies(1000));
 }
 mutex_unlock(&d->lifecycle_lock);
}

static const struct vb2_ops gchd_vb2_ops={
 .queue_setup=gchd_queue_setup,.buf_queue=gchd_buf_queue,.start_streaming=gchd_start,
 .stop_streaming=gchd_stop,.wait_prepare=vb2_ops_wait_prepare,.wait_finish=vb2_ops_wait_finish
};

static int gchd_enuminput(struct file *f, void *p, struct v4l2_input *in)
{
 struct gchd *d = video_drvdata(f);
 unsigned int index = in->index;

 if (index > 2) return -EINVAL;
 memset(in, 0, sizeof(*in));
 in->index = index;
 if (index == 0) {
  strscpy(in->name, "HDMI", sizeof(in->name));
 } else if (index == 1) {
  strscpy(in->name, "Component", sizeof(in->name));
 } else {
  strscpy(in->name, "Composite", sizeof(in->name));
 }
 in->type = V4L2_INPUT_TYPE_CAMERA;

 if (index == d->input && !READ_ONCE(d->signal_present)) {
  in->status = V4L2_IN_ST_NO_SIGNAL;
  WRITE_ONCE(d->detect_requested, true);
  schedule_delayed_work(&d->detect_work, 0);
 }
 if (index < 2)
  in->capabilities = V4L2_IN_CAP_DV_TIMINGS;
 return 0;
}

static int gchd_ginput(struct file *f, void *p, unsigned int *i)
{
 *i = ((struct gchd *)video_drvdata(f))->input;
 return 0;
}

static int gchd_set_input(struct gchd *d, unsigned int i)
{
 bool resume;
 int r;

 if (i > 2)
  return -EINVAL;
 if (i == d->input)
  return 0;

 resume = READ_ONCE(d->streaming);

 /*
  * Input selection is safe while buffers are allocated but the queue is not
  * streaming: sizeimage is a fixed maximum-frame allocation. Rejecting every
  * busy VB2 queue here prevents GUI applications from switching inputs after
  * they have prepared their capture buffers.
  */
 WRITE_ONCE(d->detect_requested, false);
 cancel_delayed_work_sync(&d->detect_work);
 mutex_lock(&d->lifecycle_lock);
 if (d->disconnected) {
  mutex_unlock(&d->lifecycle_lock);
  return -ENODEV;
 }

 if (resume) {
  if (d->rx_thread) {
   kthread_stop(d->rx_thread);
   d->rx_thread = NULL;
  }
  r = gchd_stream_stop(d);
  if (r) {
   dev_err(&d->intf->dev, "input switch: hardware stop failed: %d\n", r);
   /* Do not advertise an active stream after its RX thread has been joined. */
   WRITE_ONCE(d->streaming, false);
   mutex_unlock(&d->lifecycle_lock);
   return r;
  }
 }
 /*
  * Keep the encoder firmware running across connector changes. It is started
  * during initial setup; after disabling its bits, the encoder-start handshake
  * fails with -EINVAL on this device. Invalidate connector-specific state and
  * reconfigure the selected input from IDLE instead.
  */
 d->input_configured = false;
 d->input_prepared = false;
 d->signal_present = false;

 gchd_ring_free(&d->ring);
 d->ts_partial_len = 0;
 d->input = i;
 mutex_lock(&gchd_last_input_lock);
 gchd_last_input = i;
 mutex_unlock(&gchd_last_input_lock);
 d->input_forced = true;
 d->mode_forced = false;
 d->input_configured = false;
 /* encoder_started is deliberately preserved across connector changes. */
 d->input_prepared = false;
 d->signal_present = false;
 d->last_video_jiffies = jiffies;
 switch (i) {
 case 0:
 case 1:
  d->input_width = 0;
  d->input_height = 0;
  d->input_fps_num = 0;
  d->input_fps_den = 0;
  d->input_interlaced = false;
  break;
 default:
  d->input_width = 0;
  d->input_height = 0;
  d->input_fps_num = 0;
  d->input_fps_den = 0;
  d->input_interlaced = false;
  break;
 }

 /*
  * Input switching is not device shutdown: do not call gchd_hw_init() here.
  * That routine can issue SCMD_RESET for a non-reset state. Capture transport
  * has already been stopped; keep the encoder firmware initialized, move to
  * IDLE, then asynchronously detect timing and configure the selected input.
  */
 r = gchd_state_cmd(d, SCMD_IDLE, 0, 0, 0x11);
 if (r) {
  dev_err(&d->intf->dev,
          "input switch: transition to IDLE failed: %d\n", r);
  WRITE_ONCE(d->streaming, false);
  mutex_unlock(&d->lifecycle_lock);
  return r;
 }
 WRITE_ONCE(d->detect_requested, true);
 schedule_delayed_work(&d->detect_work, 0);
 mutex_unlock(&d->lifecycle_lock);
 return 0;
}

static int gchd_sinput(struct file *f, void *p, unsigned int i)
{
 struct gchd *d = video_drvdata(f);
 struct v4l2_ctrl *ctrl;
 int r;

 r = gchd_set_input(d, i);
 if (r)
  return r;

 /* Keep the GUI-visible menu and standard VIDIOC_S_INPUT API in sync. */
 ctrl = v4l2_ctrl_find(&d->ctrls, V4L2_CID_GCHD_INPUT_SOURCE);
 if (ctrl && ctrl->val != i)
  return v4l2_ctrl_s_ctrl(ctrl, i);
 return 0;
}

static int gchd_query_dv_timings(struct file *f, void *p,
				     struct v4l2_dv_timings *t)
{
 struct gchd *d = video_drvdata(f);
 u64 clock;

 if (d->input == 2)
  return -ENODATA;
 if (!READ_ONCE(d->signal_present)) {
  WRITE_ONCE(d->detect_requested, true);
  schedule_delayed_work(&d->detect_work, 0);
  return -ENOLINK;
 }

 memset(t, 0, sizeof(*t));
 t->type = V4L2_DV_BT_656_1120;
 t->bt.width = d->input_width;
 t->bt.height = d->input_height;
 t->bt.interlaced = d->input_interlaced;
 t->bt.polarities = 0;

 if (d->input_width == 1920)
  clock = d->input_interlaced ? 74250000ULL : 148500000ULL;
 else if (d->input_width == 1280)
  clock = 74250000ULL;
 else if (d->input_height == 576)
  clock = d->input_interlaced ? 13500000ULL : 27000000ULL;
 else
  clock = d->input_interlaced ? 13500000ULL : 27000000ULL;

 t->bt.pixelclock = clock;
 t->bt.hfrontporch = 88;
 t->bt.hsync = 44;
 t->bt.hbackporch = 148;
 t->bt.vfrontporch = d->input_interlaced ? 2 : 4;
 t->bt.vsync = d->input_interlaced ? 5 : 5;
 t->bt.vbackporch = d->input_interlaced ? 15 : 36;

 return 0;
}

static int gchd_enum_dv_timings(struct file *f, void *p,
				 struct v4l2_enum_dv_timings *e)
{
 struct gchd *d = video_drvdata(f);
 static const struct {
  u32 width, height, fps, pixelclock;
  bool interlaced;
 } modes[] = {
  {1920,1080,60,148500000,false},
  {1920,1080,60,74250000,true},
  {1280,720,60,74250000,false},
  {720,576,50,27000000,false},
  {720,576,50,13500000,true},
  {720,480,60,27000000,false},
  {720,480,60,13500000,true},
 };
 unsigned int n = 0, i;

 if (d->input == 2)
  return -ENODATA;

 for (i = 0; i < ARRAY_SIZE(modes); ++i) {
  if (!gchd_mode_allowed(d, &gchd_modes[i]))
   continue;
  if (n++ != e->index)
   continue;
  memset(&e->timings, 0, sizeof(e->timings));
  e->timings.type = V4L2_DV_BT_656_1120;
  e->timings.bt.width = modes[i].width;
  e->timings.bt.height = modes[i].height;
  e->timings.bt.interlaced = modes[i].interlaced;
  e->timings.bt.pixelclock = modes[i].pixelclock;
  return 0;
 }

 return -EINVAL;
}

static int gchd_querycap(struct file*f,void*p,struct v4l2_capability*c)
{strscpy(c->driver,"elgato-gchd",sizeof(c->driver));strscpy(c->card,"Elgato Game Capture HD",sizeof(c->card));
 strscpy(c->bus_info,"usb",sizeof(c->bus_info));c->device_caps=V4L2_CAP_VIDEO_CAPTURE|V4L2_CAP_STREAMING|V4L2_CAP_READWRITE;c->capabilities=c->device_caps|V4L2_CAP_DEVICE_CAPS;return 0;}

static int gchd_enum(struct file*f,void*p,struct v4l2_fmtdesc*x)
{
 if (x->index) return -EINVAL;
 x->pixelformat=V4L2_PIX_FMT_H264;
 x->flags = V4L2_FMT_FLAG_COMPRESSED;
 strscpy(x->description, "H.264", sizeof(x->description));
 return 0;
}

static int gchd_enum_framesizes(struct file *f, void *p,
                                struct v4l2_frmsizeenum *s)
{
 struct gchd *d = video_drvdata(f);
 unsigned int i, n = 0;

 if (s->pixel_format != V4L2_PIX_FMT_H264)
  return -EINVAL;

 for (i = 0; i < ARRAY_SIZE(gchd_modes); ++i) {
  if (!gchd_mode_allowed(d, &gchd_modes[i]))
   continue;
  if (n++ == s->index) {
   s->type = V4L2_FRMSIZE_TYPE_DISCRETE;
   s->discrete.width = gchd_modes[i].width;
   s->discrete.height = gchd_modes[i].height;
   return 0;
  }
 }
 return -EINVAL;
}

static int gchd_enum_frameintervals(struct file *f, void *p,
                                    struct v4l2_frmivalenum *v)
{
 struct gchd *d = video_drvdata(f);
 const struct gchd_mode *m;
 unsigned int i;

 if (v->pixel_format != V4L2_PIX_FMT_H264)
  return -EINVAL;

 m = NULL;
 for (i = 0; i < ARRAY_SIZE(gchd_modes); ++i) {
  if (gchd_modes[i].width == v->width &&
      gchd_modes[i].height == v->height &&
      gchd_mode_allowed(d, &gchd_modes[i])) {
   m = &gchd_modes[i];
   break;
  }
 }
 if (!m || v->index != 0)
  return -EINVAL;

 v->type = V4L2_FRMIVAL_TYPE_DISCRETE;
 v->discrete.numerator = m->fps_den;
 v->discrete.denominator = m->fps_num;
 return 0;
}

static int gchd_gfmt(struct file*f,void*p,struct v4l2_format*x)
{
 struct gchd*d=video_drvdata(f);
 x->fmt.pix.width=d->width;x->fmt.pix.height=d->height;
 x->fmt.pix.pixelformat=V4L2_PIX_FMT_H264;
 x->fmt.pix.field=d->input_interlaced ? V4L2_FIELD_INTERLACED : V4L2_FIELD_NONE;
 x->fmt.pix.sizeimage=d->sizeimage;
 x->fmt.pix.colorspace=V4L2_COLORSPACE_REC709;
 return 0;
}

static int gchd_sfmt(struct file*f,void*p,struct v4l2_format*x)
{
 struct gchd*d=video_drvdata(f);
 const struct gchd_mode *m;
 bool interlaced = x->fmt.pix.field == V4L2_FIELD_INTERLACED;

 if (vb2_is_busy(&d->vbq) || x->fmt.pix.pixelformat!=V4L2_PIX_FMT_H264)
  return-EINVAL;

 m = gchd_find_mode(x->fmt.pix.width, x->fmt.pix.height, interlaced);
 if (!m || !gchd_mode_allowed(d, m))
  return -EINVAL;

 d->width=m->width;
 d->height=m->height;
 d->input_width=m->width;
 d->input_height=m->height;
 d->input_fps_num=m->fps_num;
 d->input_fps_den=m->fps_den;
 d->input_interlaced=m->interlaced;
 d->mode_forced=true;
 d->input_configured=false;
 d->sizeimage=GCHD_MAX_FRAME;

 return gchd_gfmt(f,p,x);
}

static int gchd_tryfmt(struct file*f,void*p,struct v4l2_format*x)
{
 struct gchd*d=video_drvdata(f);
 const struct gchd_mode *m;
 bool interlaced = x->fmt.pix.field == V4L2_FIELD_INTERLACED;

 if (x->fmt.pix.pixelformat != V4L2_PIX_FMT_H264)
  return -EINVAL;

 m = gchd_find_mode(x->fmt.pix.width, x->fmt.pix.height, interlaced);
 if (!m || !gchd_mode_allowed(d, m))
  return -EINVAL;

 x->fmt.pix.width=m->width;
 x->fmt.pix.height=m->height;
 x->fmt.pix.field=m->interlaced ? V4L2_FIELD_INTERLACED : V4L2_FIELD_NONE;
 x->fmt.pix.sizeimage=GCHD_MAX_FRAME;
 x->fmt.pix.colorspace=V4L2_COLORSPACE_REC709;
 return 0;
}

static int gchd_subscribe_event(struct v4l2_fh *fh,
                                const struct v4l2_event_subscription *sub)
{
 if (sub->type != V4L2_EVENT_SOURCE_CHANGE &&
     sub->type != V4L2_EVENT_CTRL)
  return -EINVAL;
 if (sub->type == V4L2_EVENT_CTRL)
  return v4l2_ctrl_subscribe_event(fh, sub);
 return v4l2_event_subscribe(fh, sub, 8, NULL);
}

static const struct v4l2_ioctl_ops gchd_ioctl={
 .vidioc_querycap=gchd_querycap,
 .vidioc_enum_fmt_vid_cap=gchd_enum,
 .vidioc_enum_framesizes=gchd_enum_framesizes,
 .vidioc_enum_frameintervals=gchd_enum_frameintervals,
 .vidioc_enum_input=gchd_enuminput,
 .vidioc_g_input=gchd_ginput,.vidioc_s_input=gchd_sinput,
 .vidioc_g_fmt_vid_cap=gchd_gfmt,.vidioc_s_fmt_vid_cap=gchd_sfmt,
.vidioc_try_fmt_vid_cap=gchd_tryfmt,
 .vidioc_query_dv_timings=gchd_query_dv_timings,
 .vidioc_enum_dv_timings=gchd_enum_dv_timings,
 .vidioc_reqbufs=vb2_ioctl_reqbufs,.vidioc_querybuf=vb2_ioctl_querybuf,
 .vidioc_qbuf=vb2_ioctl_qbuf,.vidioc_dqbuf=vb2_ioctl_dqbuf,
 .vidioc_streamon=vb2_ioctl_streamon,.vidioc_streamoff=vb2_ioctl_streamoff,
 .vidioc_subscribe_event=gchd_subscribe_event,
 .vidioc_unsubscribe_event=v4l2_event_unsubscribe
};

static int gchd_ctrl(struct v4l2_ctrl *c)
{
 struct gchd *d = container_of(c->handler, struct gchd, ctrls);
 u8 level;

 switch (c->id) {
 case V4L2_CID_GCHD_INPUT_SOURCE:
  return gchd_set_input(d, c->val);
 }

 if (d->streaming)
  return -EBUSY;

 switch (c->id) {
 case V4L2_CID_MPEG_VIDEO_BITRATE:
  d->bitrate = c->val / 1000;
  d->bitrate_forced = true;
  if (d->bitrate < 1)
   d->bitrate = 1;
  break;
 case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
  if (c->val > V4L2_MPEG_VIDEO_H264_PROFILE_HIGH)
   return -EINVAL;
  d->h264_profile = c->val;
  break;
 case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
  switch (c->val) {
  case V4L2_MPEG_VIDEO_H264_LEVEL_3_0: level = 30; break;
  case V4L2_MPEG_VIDEO_H264_LEVEL_3_1: level = 31; break;
  case V4L2_MPEG_VIDEO_H264_LEVEL_3_2: level = 32; break;
  case V4L2_MPEG_VIDEO_H264_LEVEL_4_0: level = 40; break;
  case V4L2_MPEG_VIDEO_H264_LEVEL_4_1: level = 41; break;
  case V4L2_MPEG_VIDEO_H264_LEVEL_4_2: level = 42; break;
  case V4L2_MPEG_VIDEO_H264_LEVEL_5_0: level = 50; break;
  case V4L2_MPEG_VIDEO_H264_LEVEL_5_1: level = 51; break;
  default: return -EINVAL;
  }
  d->h264_level = level;
  d->h264_level_forced = true;
  break;
 default:
  return -EINVAL;
 }
 return 0;
}
static const struct v4l2_ctrl_ops gchd_ctrl_ops = { .s_ctrl = gchd_ctrl };

static int gchd_fop_open(struct file *file)
{
 struct gchd *d = video_drvdata(file);
 int r = 0;

 /* Protect the embedded video_device/driver state against unplug races. */
 kref_get(&d->refcount);
 if (vb2_fops.open) {
  r = vb2_fops.open(file);
  if (r) {
   kref_put(&d->refcount, gchd_release_ref);
   return r;
  }
 }

 mutex_lock(&d->users_lock);
 mutex_lock(&d->lifecycle_lock);
 if (d->disconnected) {
  r = -ENODEV;
  goto out;
 }

 if (d->open_count == 0) {
  /*
   * A previous close may have reported a shutdown error. Retry that exact
   * state-aware shutdown before starting another firmware initialization;
   * never layer a fresh init on top of a known failed teardown.
   */
  if (d->hw_initialized) {
   r = gchd_hw_shutdown(d);
   if (r) {
    dev_err(&d->intf->dev,
            "retrying previous hardware shutdown before open failed: %d\n", r);
    goto out;
   }
   d->hw_initialized = false;
  }

  d->input_forced = true;
  d->input_configured = false;
  d->input_prepared = false;
  d->encoder_started = false;
  d->signal_present = false;
  d->input_width = 0;
  d->input_height = 0;
  d->input_fps_num = 0;
  d->input_fps_den = 0;
  r = gchd_hw_init(d);
  if (r) {
   int sr;

   dev_err(&d->intf->dev, "hardware initialization on open failed: %d\n", r);
   sr = gchd_hw_shutdown(d);
   d->hw_initialized = (sr != 0);
   if (sr)
    dev_warn(&d->intf->dev,
             "cleanup after failed initialization also failed: %d\n", sr);
   goto out;
  }
  d->hw_initialized = true;
  WRITE_ONCE(d->detect_requested, true);
  schedule_delayed_work(&d->detect_work, 0);
  dev_info(&d->intf->dev, "V4L2 opened: hardware initialized, detecting input %u\n",
           d->input);
 }
 d->open_count++;

out:
 mutex_unlock(&d->lifecycle_lock);
 mutex_unlock(&d->users_lock);
 if (r && vb2_fops.release)
  vb2_fops.release(file);
 if (r)
  kref_put(&d->refcount, gchd_release_ref);
 return r;
}

static int gchd_fop_release(struct file *file)
{
 struct gchd *d = video_drvdata(file);
 int r = 0, sr;

 /* VB2 must release buffers and invoke stop_streaming before HW shutdown. */
 if (vb2_fops.release)
  r = vb2_fops.release(file);

 mutex_lock(&d->users_lock);
 if (d->open_count)
  d->open_count--;

 if (!d->open_count && !d->disconnected) {
  WRITE_ONCE(d->detect_requested, false);
  WRITE_ONCE(d->streaming, false);

  /* Never wait for work while holding lifecycle_lock: the worker takes it. */
  cancel_delayed_work_sync(&d->detect_work);
  mutex_lock(&d->lifecycle_lock);

  if (d->rx_thread) {
   kthread_stop(d->rx_thread);
   d->rx_thread = NULL;
  }

  if (d->hw_initialized) {
   /*
    * Do not call gchd_input_stop() here: userspace uninitDevice() owns
    * encoder/transcoder teardown and must observe the original HW state.
    */
   sr = gchd_hw_shutdown(d);
   if (sr) {
    dev_warn(&d->intf->dev, "hardware shutdown on last close failed: %d\n", sr);
    if (!r)
     r = sr;
    /* Keep the flag set so the next open retries shutdown before HW init. */
   } else {
    dev_info(&d->intf->dev, "V4L2 closed: hardware safely shut down\n");
    d->hw_initialized = false;
   }
  }

  d->input_configured = false;
  d->input_prepared = false;
  d->encoder_started = false;
  d->signal_present = false;
  gchd_ring_free(&d->ring);
  d->ts_partial_len = 0;
  mutex_unlock(&d->lifecycle_lock);
 }
 mutex_unlock(&d->users_lock);
 kref_put(&d->refcount, gchd_release_ref);
 return r;
}

int gchd_v4l2_register(struct gchd*d)
{
 int r=v4l2_device_register(&d->intf->dev,&d->v4l2_dev);if(r)return r;
 v4l2_ctrl_handler_init(&d->ctrls,4);
 v4l2_ctrl_new_std(&d->ctrls,&gchd_ctrl_ops,V4L2_CID_MPEG_VIDEO_BITRATE,32000,40000000,1000,16000000);
 v4l2_ctrl_new_std_menu(&d->ctrls,&gchd_ctrl_ops,V4L2_CID_MPEG_VIDEO_H264_PROFILE,V4L2_MPEG_VIDEO_H264_PROFILE_HIGH,
  ~(BIT(V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE)|BIT(V4L2_MPEG_VIDEO_H264_PROFILE_MAIN)|BIT(V4L2_MPEG_VIDEO_H264_PROFILE_HIGH)),V4L2_MPEG_VIDEO_H264_PROFILE_MAIN);
 v4l2_ctrl_new_std_menu(&d->ctrls,&gchd_ctrl_ops,V4L2_CID_MPEG_VIDEO_H264_LEVEL,V4L2_MPEG_VIDEO_H264_LEVEL_5_1,0,V4L2_MPEG_VIDEO_H264_LEVEL_4_1);
 {
  struct v4l2_ctrl_config input_cfg = {
   .ops = &gchd_ctrl_ops,
   .id = V4L2_CID_GCHD_INPUT_SOURCE,
   .name = "Input Source",
   .type = V4L2_CTRL_TYPE_MENU,
   .min = 0,
   .max = ARRAY_SIZE(gchd_input_menu) - 2,
   /* V4L2 menu controls use menu_skip_mask, not step. */
   .step = 0,
   .menu_skip_mask = 0,
   .def = d->input,
   .qmenu = gchd_input_menu,
  };
  v4l2_ctrl_new_custom(&d->ctrls, &input_cfg, NULL);
 }
 if(d->ctrls.error){r=d->ctrls.error;goto err;}
 memset(&d->vbq,0,sizeof(d->vbq));d->vbq.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;d->vbq.io_modes=VB2_MMAP|VB2_READ;d->vbq.timestamp_flags=V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;d->vbq.dev=&d->intf->dev;
 d->vbq.drv_priv=d;d->vbq.ops=&gchd_vb2_ops;d->vbq.mem_ops=&vb2_vmalloc_memops;d->vbq.lock=&d->lock;
 r=vb2_queue_init(&d->vbq);if(r)goto err;
 strscpy(d->vdev.name,d->family == GCHD_FAMILY_HDNEW ? "Elgato Game Capture HD (HDNew)" : "Elgato Game Capture HD",sizeof(d->vdev.name));d->vdev.v4l2_dev=&d->v4l2_dev;d->vdev.release=video_device_release_empty;
 gchd_fops = vb2_fops;
 gchd_fops.open = gchd_fop_open;
 gchd_fops.release = gchd_fop_release;
 d->vdev.fops=&gchd_fops;d->vdev.ioctl_ops=&gchd_ioctl;d->vdev.queue=&d->vbq;d->vdev.lock=&d->lock;
 d->vdev.ctrl_handler=&d->ctrls;d->vdev.device_caps=V4L2_CAP_VIDEO_CAPTURE|V4L2_CAP_STREAMING|V4L2_CAP_READWRITE;video_set_drvdata(&d->vdev,d);
 r=video_register_device(&d->vdev,VFL_TYPE_VIDEO,-1);if(r)goto err;return 0;
err:v4l2_ctrl_handler_free(&d->ctrls);v4l2_device_unregister(&d->v4l2_dev);return r;
}
void gchd_v4l2_unregister(struct gchd*d){video_unregister_device(&d->vdev);v4l2_ctrl_handler_free(&d->ctrls);v4l2_device_unregister(&d->v4l2_dev);}

static const struct usb_device_id gchd_ids[] = {
 { USB_DEVICE(GCHD_VID, GCHD_PID_0), .driver_info = GCHD_FAMILY_OLD },
 { USB_DEVICE(GCHD_VID, GCHD_PID_1), .driver_info = GCHD_FAMILY_OLD },
 { USB_DEVICE(GCHD_VID, GCHD_PID_2), .driver_info = GCHD_FAMILY_OLD },
 { USB_DEVICE(GCHD_VID, GCHD_PID_3), .driver_info = GCHD_FAMILY_HDNEW },
 { }
};
MODULE_DEVICE_TABLE(usb, gchd_ids);
static int gchd_probe(struct usb_interface *i,
                      const struct usb_device_id *id)
{
 struct gchd *d;
 int r;

 d = kzalloc(sizeof(*d), GFP_KERNEL);
 if (!d)
  return -ENOMEM;
 kref_init(&d->refcount);
 d->udev = usb_get_dev(interface_to_usbdev(i));
 d->intf = i;
 d->family = (enum gchd_family)id->driver_info;

 r = usb_reset_configuration(d->udev);
 if (r)
  goto err;
 mutex_init(&d->lock);
 mutex_init(&d->lifecycle_lock);
 mutex_init(&d->users_lock);
 INIT_DELAYED_WORK(&d->detect_work, gchd_detect_workfn);
 spin_lock_init(&d->qlock);
 INIT_LIST_HEAD(&d->queued);
 spin_lock_init(&d->ring.lock);
 d->width = 1280;
 d->height = 720;
 d->sizeimage = GCHD_MAX_FRAME;
 mutex_lock(&gchd_last_input_lock);
 d->input = gchd_last_input <= 2 ? gchd_last_input : 0;
 mutex_unlock(&gchd_last_input_lock);
 d->input_forced = false;
 d->bitrate_forced = false;
 d->h264_level_forced = false;
 d->input_width = 0;
 d->input_height = 0;
 d->input_fps_num = 0;
 d->input_fps_den = 0;
 d->bitrate = 40000;
 d->h264_profile = V4L2_MPEG_VIDEO_H264_PROFILE_HIGH;
 d->h264_level = 41;
 d->usb_buf = kmalloc(GCHD_USB_BUFSIZE, GFP_KERNEL);
 if (!d->usb_buf) {
  r = -ENOMEM;
  goto err;
 }

 /*
  * Keep the hardware uninitialized until a userspace client opens the V4L2
  * node. This gives each open/last-close pair a clean HW init/shutdown cycle.
  */
 usb_set_intfdata(i, d);
 r = gchd_v4l2_register(d);
 if (r) {
  usb_set_intfdata(i, NULL);
  goto err;
 }
 return 0;

err:
 kref_put(&d->refcount, gchd_release_ref);
 return r;
}

static void gchd_disconnect(struct usb_interface *i)
{
 struct gchd *d = usb_get_intfdata(i);

 if (!d)
  return;
 usb_set_intfdata(i, NULL);
 WRITE_ONCE(d->disconnected, true);
 cancel_delayed_work_sync(&d->detect_work);
 mutex_lock(&d->lifecycle_lock);
 WRITE_ONCE(d->streaming, false);
 if (d->rx_thread) {
  kthread_stop(d->rx_thread);
  d->rx_thread = NULL;
 }
 mutex_unlock(&d->lifecycle_lock);

 /* The interface may already be gone: never issue USB commands here. */
 gchd_v4l2_unregister(d);
 gchd_ring_free(&d->ring);
 kref_put(&d->refcount, gchd_release_ref);
}

static void gchd_shutdown(struct device *dev)
{
 struct usb_interface *i = to_usb_interface(dev);
 struct gchd *d = usb_get_intfdata(i);
 int r;

 if (!d || d->disconnected || !d->hw_initialized)
  return;

 cancel_delayed_work_sync(&d->detect_work);
 mutex_lock(&d->lifecycle_lock);
 WRITE_ONCE(d->streaming, false);
 if (d->rx_thread) {
  kthread_stop(d->rx_thread);
  d->rx_thread = NULL;
 }

 /*
  * Use the same ordered teardown as last-close rather than issuing RESET
  * from an arbitrary active state. System shutdown must continue even if
  * USB communication has already failed.
  */
 r = gchd_hw_shutdown(d);
 if (r) {
  dev_warn(dev, "best-effort hardware shutdown during system shutdown failed: %d\n",
           r);
 } else {
  d->hw_initialized = false;
 }
 mutex_unlock(&d->lifecycle_lock);
}

static struct usb_driver gchd_usb = {
 .name = "elgato_gchd",
 .id_table = gchd_ids,
 .probe = gchd_probe,
 .disconnect = gchd_disconnect,
 .driver = {
  .shutdown = gchd_shutdown,
 },
};
module_usb_driver(gchd_usb);
