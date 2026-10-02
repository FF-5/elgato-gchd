#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/timekeeping.h>
#include "elgato_gchd.h"

MODULE_DESCRIPTION("Elgato Game Capture HD 0fd9:005d V4L2 driver");
MODULE_AUTHOR("FF-5 / elgato-gchd contributors");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1.0");

static unsigned long ring_bytes = GCHD_RING_BYTES;
module_param(ring_bytes, ulong, 0644);
MODULE_PARM_DESC(ring_bytes, "Maximum encoded video bytes retained in RAM");

int gchd_ring_push(struct gchd_ring *r, const u8 *data, size_t len)
{
 unsigned long flags; u8 *copy; struct gchd_frame *f; unsigned int tail;
 if (!len || len > GCHD_MAX_FRAME) return -EMSGSIZE;
 copy = kmemdup(data, len, GFP_KERNEL); if (!copy) return -ENOMEM;
 spin_lock_irqsave(&r->lock, flags);
 while (r->count && (r->count >= GCHD_RING_FRAMES || r->bytes + len > ring_bytes)) {
  tail=(r->head+GCHD_RING_FRAMES-r->count)%GCHD_RING_FRAMES;
  kfree(r->frames[tail].data); r->bytes-=r->frames[tail].len;
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
 for(i=0;i<GCHD_RING_FRAMES;i++){ kfree(r->frames[i].data); r->frames[i].data=NULL; r->frames[i].len=0; }
 r->count=0; r->bytes=0; spin_unlock_irqrestore(&r->lock,flags);
}

/* The device emits MPEG-TS. The existing transcoder assigns video PID 0x1011.
 * Each PES payload is retained as one encoded frame. */
static void gchd_ts(struct gchd *d,const u8 *p,u8 *pes,size_t *n)
{
 u16 pid; unsigned int off=4,afc; bool start;
 if(p[0]!=0x47)return;
 pid=((p[1]&0x1f)<<8)|p[2]; if(pid!=0x1011)return;
 start=!!(p[1]&0x40); afc=(p[3]>>4)&3; if(!afc||afc==2)return;
 if(afc==3){off+=1+p[4];if(off>=188)return;}
 if(start){
  if(*n)gchd_ring_push(&d->ring,pes,*n); *n=0;
  if(188-off>=9 && p[off]==0 && p[off+1]==0 && p[off+2]==1){
   unsigned int h=9+p[off+8]; if(h>=188-off)return; off+=h;
  }
 }
 if(off<188 && *n<GCHD_MAX_FRAME){size_t m=min_t(size_t,188-off,GCHD_MAX_FRAME-*n);
  memcpy(pes+*n,p+off,m);*n+=m;}
}

static void gchd_deliver(struct gchd *d)
{
 struct gchd_frame *f; struct gchd_buffer *b; unsigned long flags; void *dst;
 if(!d->streaming)return;
 f=gchd_ring_pop(&d->ring); if(!f)return;
 spin_lock_irqsave(&d->qlock,flags);
 if(list_empty(&d->queued)){spin_unlock_irqrestore(&d->qlock,flags);kfree(f->data);kfree(f);return;}
 b=list_first_entry(&d->queued,struct gchd_buffer,list);list_del(&b->list);
 spin_unlock_irqrestore(&d->qlock,flags);
 dst=vb2_plane_vaddr(&b->vb.vb2_buf,0);
 if(!dst || f->len>d->sizeimage) vb2_buffer_done(&b->vb.vb2_buf,VB2_BUF_STATE_ERROR);
 else {memcpy(dst,f->data,f->len);vb2_set_plane_payload(&b->vb.vb2_buf,0,f->len);
  b->vb.sequence=f->sequence;b->vb.vb2_buf.timestamp=ktime_get_ns();
  vb2_buffer_done(&b->vb.vb2_buf,VB2_BUF_STATE_DONE);}
 kfree(f->data);kfree(f);
}

static int gchd_rx(void *arg)
{
 struct gchd *d=arg; u8 *pes; size_t n=0; int ret,actual,pos;
 pes=kmalloc(GCHD_MAX_FRAME,GFP_KERNEL);if(!pes)return -ENOMEM;
 while(!kthread_should_stop()&&!d->disconnected){
  ret=usb_bulk_msg(d->udev,usb_rcvbulkpipe(d->udev,GCHD_EP_IN),d->usb_buf,
                   GCHD_USB_BUFSIZE,&actual,1000);
  if(ret==-ETIMEDOUT||ret==-EAGAIN)continue;if(ret)break;
  for(pos=0;pos+188<=actual;pos+=188)gchd_ts(d,d->usb_buf+pos,pes,&n);
  gchd_deliver(d);
 }
 if(n)gchd_ring_push(&d->ring,pes,n);kfree(pes);return 0;
}

static int gchd_queue_setup(struct vb2_queue *q,unsigned int *nb,unsigned int *np,
                            unsigned int sizes[],struct device *alloc[])
{struct gchd*d=vb2_get_drv_priv(q);if(*np)return sizes[0]>=d->sizeimage?0:-EINVAL;
 *np=1;sizes[0]=d->sizeimage;return 0;}
static void gchd_buf_queue(struct vb2_buffer *vb)
{struct gchd*d=vb2_get_drv_priv(vb->vb2_queue);struct gchd_buffer*b=container_of(to_vb2_v4l2_buffer(vb),struct gchd_buffer,vb);
 unsigned long f;spin_lock_irqsave(&d->qlock,f);list_add_tail(&b->list,&d->queued);spin_unlock_irqrestore(&d->qlock,f);gchd_deliver(d);}
static int gchd_start(struct vb2_queue*q,unsigned int c){vb2_get_drv_priv(q)->streaming=true;return 0;}
static void gchd_stop(struct vb2_queue*q)
{struct gchd*d=vb2_get_drv_priv(q);struct gchd_buffer*b,*tmp;unsigned long f;d->streaming=false;
 spin_lock_irqsave(&d->qlock,f);list_for_each_entry_safe(b,tmp,&d->queued,list){list_del(&b->list);vb2_buffer_done(&b->vb.vb2_buf,VB2_BUF_STATE_ERROR);}spin_unlock_irqrestore(&d->qlock,f);}
static const struct vb2_ops gchd_vb2_ops={.queue_setup=gchd_queue_setup,.buf_queue=gchd_buf_queue,.start_streaming=gchd_start,.stop_streaming=gchd_stop,.wait_prepare=vb2_ops_wait_prepare,.wait_finish=vb2_ops_wait_finish};

static int gchd_querycap(struct file*f,void*p,struct v4l2_capability*c)
{strscpy(c->driver,"elgato-gchd",sizeof(c->driver));strscpy(c->card,"Elgato Game Capture HD",sizeof(c->card));
 strscpy(c->bus_info,"usb",sizeof(c->bus_info));c->device_caps=V4L2_CAP_VIDEO_CAPTURE|V4L2_CAP_STREAMING|V4L2_CAP_READWRITE;c->capabilities=c->device_caps|V4L2_CAP_DEVICE_CAPS;return 0;}
static int gchd_enum(struct file*f,void*p,struct v4l2_fmtdesc*x){if(x->index)return-EINVAL;x->pixelformat=V4L2_PIX_FMT_H264;return 0;}
static int gchd_gfmt(struct file*f,void*p,struct v4l2_format*x)
{struct gchd*d=video_drvdata(f);x->fmt.pix.width=d->width;x->fmt.pix.height=d->height;x->fmt.pix.pixelformat=V4L2_PIX_FMT_H264;
 x->fmt.pix.field=V4L2_FIELD_NONE;x->fmt.pix.sizeimage=d->sizeimage;x->fmt.pix.colorspace=V4L2_COLORSPACE_REC709;return 0;}
static int gchd_sfmt(struct file*f,void*p,struct v4l2_format*x)
{struct gchd*d=video_drvdata(f);if(vb2_is_busy(&d->vbq)||x->fmt.pix.pixelformat!=V4L2_PIX_FMT_H264)return-EINVAL;
 d->width=clamp_t(u32,x->fmt.pix.width,320,1920);d->height=clamp_t(u32,x->fmt.pix.height,240,1080);d->sizeimage=GCHD_MAX_FRAME;return gchd_gfmt(f,p,x);}
static const struct v4l2_ioctl_ops gchd_ioctl={
 .vidioc_querycap=gchd_querycap,.vidioc_enum_fmt_vid_cap=gchd_enum,
 .vidioc_g_fmt_vid_cap=gchd_gfmt,.vidioc_s_fmt_vid_cap=gchd_sfmt,.vidioc_try_fmt_vid_cap=gchd_sfmt,
 .vidioc_reqbufs=vb2_ioctl_reqbufs,.vidioc_querybuf=vb2_ioctl_querybuf,.vidioc_qbuf=vb2_ioctl_qbuf,
 .vidioc_dqbuf=vb2_ioctl_dqbuf,.vidioc_streamon=vb2_ioctl_streamon,.vidioc_streamoff=vb2_ioctl_streamoff,
 .vidioc_subscribe_event=v4l2_ctrl_subscribe_event};

static int gchd_ctrl(struct v4l2_ctrl*c){return 0;}
static const struct v4l2_ctrl_ops gchd_ctrl_ops={.s_ctrl=gchd_ctrl};

int gchd_v4l2_register(struct gchd*d)
{
 int r=v4l2_device_register(&d->intf->dev,&d->v4l2_dev);if(r)return r;
 v4l2_ctrl_handler_init(&d->ctrls,3);
 v4l2_ctrl_new_std(&d->ctrls,&gchd_ctrl_ops,V4L2_CID_MPEG_VIDEO_BITRATE,32000,40000000,1000,8000000);
 v4l2_ctrl_new_std_menu(&d->ctrls,&gchd_ctrl_ops,V4L2_CID_MPEG_VIDEO_H264_PROFILE,V4L2_MPEG_VIDEO_H264_PROFILE_HIGH,
  ~(BIT(V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE)|BIT(V4L2_MPEG_VIDEO_H264_PROFILE_MAIN)|BIT(V4L2_MPEG_VIDEO_H264_PROFILE_HIGH)),V4L2_MPEG_VIDEO_H264_PROFILE_HIGH);
 v4l2_ctrl_new_std_menu(&d->ctrls,&gchd_ctrl_ops,V4L2_CID_MPEG_VIDEO_H264_LEVEL,V4L2_MPEG_VIDEO_H264_LEVEL_5_1,0,V4L2_MPEG_VIDEO_H264_LEVEL_4_0);
 if(d->ctrls.error){r=d->ctrls.error;goto err;}
 memset(&d->vbq,0,sizeof(d->vbq));d->vbq.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;d->vbq.io_modes=VB2_MMAP|VB2_READ;
 d->vbq.drv_priv=d;d->vbq.ops=&gchd_vb2_ops;d->vbq.mem_ops=&vb2_vmalloc_memops;d->vbq.lock=&d->lock;
 r=vb2_queue_init(&d->vbq);if(r)goto err;
 strscpy(d->vdev.name,"Elgato Game Capture HD",sizeof(d->vdev.name));d->vdev.v4l2_dev=&d->v4l2_dev;
 d->vdev.fops=&vb2_fops;d->vdev.ioctl_ops=&gchd_ioctl;d->vdev.queue=&d->vbq;d->vdev.lock=&d->lock;
 d->vdev.ctrl_handler=&d->ctrls;d->vdev.device_caps=V4L2_CAP_VIDEO_CAPTURE|V4L2_CAP_STREAMING|V4L2_CAP_READWRITE;video_set_drvdata(&d->vdev,d);
 r=video_register_device(&d->vdev,VFL_TYPE_VIDEO,-1);if(r)goto err;return 0;
err:v4l2_ctrl_handler_free(&d->ctrls);v4l2_device_unregister(&d->v4l2_dev);return r;
}
void gchd_v4l2_unregister(struct gchd*d){video_unregister_device(&d->vdev);v4l2_ctrl_handler_free(&d->ctrls);v4l2_device_unregister(&d->v4l2_dev);}

static const struct usb_device_id gchd_ids[]={{USB_DEVICE(GCHD_VID,GCHD_PID)},{}};MODULE_DEVICE_TABLE(usb,gchd_ids);
static int gchd_probe(struct usb_interface*i,const struct usb_device_id*id)
{
 struct gchd*d;int r;d=kzalloc(sizeof(*d),GFP_KERNEL);if(!d)return-ENOMEM;
 d->udev=usb_get_dev(interface_to_usbdev(i));d->intf=i;mutex_init(&d->lock);spin_lock_init(&d->qlock);INIT_LIST_HEAD(&d->queued);
 spin_lock_init(&d->ring.lock);d->width=1920;d->height=1080;d->sizeimage=GCHD_MAX_FRAME;d->usb_buf=kmalloc(GCHD_USB_BUFSIZE,GFP_KERNEL);
 if(!d->usb_buf){r=-ENOMEM;goto err;}r=gchd_v4l2_register(d);if(r)goto errbuf;usb_set_intfdata(i,d);
 d->rx_thread=kthread_run(gchd_rx,d,"gchd-rx");if(IS_ERR(d->rx_thread)){r=PTR_ERR(d->rx_thread);d->rx_thread=NULL;goto errv4l2;}return 0;
errv4l2:gchd_v4l2_unregister(d);errbuf:kfree(d->usb_buf);err:usb_put_dev(d->udev);kfree(d);return r;
}
static void gchd_disconnect(struct usb_interface*i)
{struct gchd*d=usb_get_intfdata(i);if(!d)return;usb_set_intfdata(i,NULL);d->disconnected=true;if(d->rx_thread)kthread_stop(d->rx_thread);
 if(d->hw_initialized)gchd_hw_shutdown(d);gchd_v4l2_unregister(d);gchd_ring_free(&d->ring);kfree(d->usb_buf);usb_put_dev(d->udev);kfree(d);}
static struct usb_driver gchd_usb={.name="elgato_gchd",.id_table=gchd_ids,.probe=gchd_probe,.disconnect=gchd_disconnect};
module_usb_driver(gchd_usb);
