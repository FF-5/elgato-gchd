#include <linux/firmware.h>
#include <linux/module.h>
#include "elgato_gchd.h"

#define REQ_READ  0xc0
#define REQ_WRITE 0x40
#define GCHD_REQ_REG 0xbc
#define GCHD_MAIL_REG 0xbd
#define GCHD_SCMD_REG 0xb8
#define GCHD_MAIL_REQ_READY 0xbc
#define GCHD_SCMD_IDLE 1
#define GCHD_SCMD_RESET 2
#define GCHD_SCMD_INIT 4
#define GCHD_SCMD_STATE_CHANGE 5
#define GCHD_STATE_READBACK 0xbc
#define GCHD_STATE_COMPLETE 0xbc
#define GCHD_ENABLE_STATE 0xbc
#define GCHD_ENABLE_REG 0xbc
#define GCHD_BANKSEL 0xbc
#define GCHD_FW_IDLE "gchd/mb86m01_assp_nsec_idle.bin"
#define GCHD_FW_ENC  "gchd/mb86m01_assp_nsec_enc_h.bin"

static int gchd_ctrl_read(struct gchd *d,u8 req,u16 value,u16 index,void *buf,u16 len)
{
 int r=usb_control_msg(d->udev,usb_rcvctrlpipe(d->udev,0),req,value,index,buf,len,2000);
 return r<0?r:(r==len?0:-EIO);
}
static int gchd_ctrl_write(struct gchd *d,u8 req,u16 value,u16 index,const void *buf,u16 len)
{
 int r=usb_control_msg(d->udev,usb_sndctrlpipe(d->udev,0),req,value,index,(void *)buf,len,2000);
 return r<0?r:(r==len?0:-EIO);
}
static int gchd_reg_read16(struct gchd*d,u16 index,u16 *v)
{
 __be16 x; int r=gchd_ctrl_read(d,GCHD_REQ_REG,0x0800,index,&x,2);if(!r)*v=be16_to_cpu(x);return r;
}
static int gchd_reg_write16(struct gchd*d,u16 index,u16 v)
{
 __be16 x=cpu_to_be16(v);return gchd_ctrl_write(d,GCHD_REQ_REG,0x0900,index,&x,2);
}
static int gchd_load_firmware(struct gchd*d,const char*name)
{
 const struct firmware*fw; size_t off=0; unsigned int chunk=32768; int r,actual;
 r=request_firmware(&fw,name,&d->intf->dev);if(r){dev_err(&d->intf->dev,"firmware %s unavailable: %d
",name,r);return r;}
 while(off<fw->size){
  size_t n=min_t(size_t,chunk,fw->size-off);
  r=usb_bulk_msg(d->udev,usb_sndbulkpipe(d->udev,0x02),(void *)(fw->data+off),n,&actual,5000);
  if(r||actual!=(int)n){r=r?:-EIO;dev_err(&d->intf->dev,"firmware transfer failed at %zu: %d
",off,r);break;}
  off+=n;
 }
 release_firmware(fw);return r;
}
static int gchd_scmd(struct gchd*d,u8 command,u8 mode,u16 data)
{
 u8 b[4]={command,mode,data>>8,data}; int r;
 r=gchd_ctrl_write(d,GCHD_SCMD_REG,0,0,b,4);
 if(r)dev_err(&d->intf->dev,"SCMD %u failed: %d
",command,r);
 return r;
}
int gchd_hw_init(struct gchd*d)
{
 u16 state; int r;
 r=gchd_reg_write16(d,0x0000,0);if(r)return r;
 r=gchd_reg_read16(d,0x0014,&d->hw_enable_state);if(r)return r;
 r=gchd_reg_read16(d,0x2008,&state);if(r)return r;state&=0x1f;
 if(state==0){
  r=gchd_load_firmware(d,GCHD_FW_IDLE);if(r)return r;
  r=gchd_reg_write16(d,0x0070,4);if(r)return r;
 } else {
  r=gchd_scmd(d,GCHD_SCMD_RESET,0,0);if(r)return r;
 }
 r=gchd_scmd(d,GCHD_SCMD_IDLE,0,0);if(r)return r;
 dev_info(&d->intf->dev,"hardware entered idle state
");
 return 0;
}
int gchd_hw_shutdown(struct gchd*d)
{
 gchd_scmd(d,GCHD_SCMD_RESET,1,0);
 return 0;
}
