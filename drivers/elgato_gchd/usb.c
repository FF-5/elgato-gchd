#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "elgato_gchd.h"


MODULE_FIRMWARE("gchd/MB86H57_H58_IDLE");
MODULE_FIRMWARE("gchd/MB86H57_H58_ENC_H");
MODULE_FIRMWARE("gchd/MB86M01_ASSP_NSEC_IDLE");
MODULE_FIRMWARE("gchd/MB86M01_ASSP_NSEC_ENC_H");

#define REQ_READ  0xc0
#define REQ_WRITE 0x40

#define REG_REQ 0xbc
#define MAIL_REG 0xbd
#define SCMD_REG 0xb8
#define SCMD_IDLE 1
#define SCMD_RESET 2
#define SCMD_INIT 4
#define SCMD_STATE_CHANGE 5

#define BANKSEL_INDEX 0x0000
#define ENABLE_INDEX 0x0018
#define ENABLE_STATE_INDEX 0x0014
#define MAIL_READY_INDEX 0x001c
#define STATE_INDEX 0x2008
#define STATE_COMPLETE_INDEX 0x0074
#define HDNEW_INTERRUPT_STATUS_INDEX 0x0016
#define HDNEW_SCMD_READBACK_INDEX 0x0014
#define HDNEW_MAIL_READ_INDEX 0x23be
#define HDNEW_MAIL_WRITE_INDEX 0x00c0
#define HDNEW_MAIL_CONFIG_INDEX 0x0000

#define FW_IDLE_OLD "gchd/MB86H57_H58_IDLE"
#define FW_ENC_OLD  "gchd/MB86H57_H58_ENC_H"
#define FW_IDLE_NEW "gchd/MB86M01_ASSP_NSEC_IDLE"
#define FW_ENC_NEW  "gchd/MB86M01_ASSP_NSEC_ENC_H"

#define EB_FIRMWARE_PROCESSOR BIT(1)
#define EB_ANALOG_INPUT BIT(2)
#define EB_ENCODER_ENABLE BIT(3)
#define EB_ENCODER_TRIGGER BIT(4)

static int gchd_ctrl_read(struct gchd *d, u8 req, u16 value, u16 index,
                          void *buf, u16 len)
{
 void *tmp;
 int r;

 if (!len)
  return 0;

 tmp = kmalloc(len, GFP_KERNEL);
 if (!tmp)
  return -ENOMEM;

 r = usb_control_msg(d->udev, usb_rcvctrlpipe(d->udev, 0),
                     req, REQ_READ, value, index, tmp, len, 2000);
 if (r == len)
  memcpy(buf, tmp, len);
 else if (r >= 0)
  r = -EIO;

 kfree(tmp);
 return r < 0 ? r : 0;
}

static int gchd_ctrl_write(struct gchd *d, u8 req, u16 value, u16 index,
                           const void *buf, u16 len)
{
 void *tmp;
 int r;

 if (!len)
  return 0;

 tmp = kmemdup(buf, len, GFP_KERNEL);
 if (!tmp)
  return -ENOMEM;

 r = usb_control_msg(d->udev, usb_sndctrlpipe(d->udev, 0),
                     req, REQ_WRITE, value, index, tmp, len, 2000);
 kfree(tmp);

 return r < 0 ? r : (r == len ? 0 : -EIO);
}

static int gchd_reg_read16(struct gchd *d, u16 index, u16 *v)
{
 __be16 x;
 int r = gchd_ctrl_read(d, REG_REQ, 0x0900, index, &x, sizeof(x));
 if (!r)
  *v = be16_to_cpu(x);
 return r;
}

static int gchd_reg_write16(struct gchd *d, u16 index, u16 v)
{
 __be16 x = cpu_to_be16(v);
 return gchd_ctrl_write(d, REG_REQ, 0x0900, index, &x, sizeof(x));
}

static int gchd_req_read16(struct gchd *d, u16 value, u16 index, u16 *v)
{
 __be16 x;
 int r = gchd_ctrl_read(d, REG_REQ, value, index, &x, sizeof(x));
 if (!r)
  *v = be16_to_cpu(x);
 return r;
}

static int gchd_load_firmware(struct gchd *d, const char *name);

int gchd_load_encoder_firmware(struct gchd *d)
{
 const char *name = d->family == GCHD_FAMILY_HDNEW ? FW_ENC_NEW : FW_ENC_OLD;
 return gchd_load_firmware(d, name);
}

static int gchd_load_firmware(struct gchd *d, const char *name)
{
 const struct firmware *fw;
 void *buf;
 size_t off = 0;
 size_t chunk = d->family == GCHD_FAMILY_HDNEW ? 32768 : 16384;
 int r, actual;

 r = request_firmware(&fw, name, &d->intf->dev);
 if (r) {
  dev_err(&d->intf->dev, "firmware %s unavailable: %d\n", name, r);
  return r;
 }

 /*
  * request_firmware() does not guarantee that fw->data is physically
  * contiguous or DMA-suitable. usb_bulk_msg() submits a URB directly
  * from the supplied buffer, so use kmalloc memory for the transfer.
  */
 buf = kmalloc(chunk, GFP_KERNEL);
 if (!buf) {
  release_firmware(fw);
  return -ENOMEM;
 }

 while (off < fw->size) {
  size_t n = min_t(size_t, chunk, fw->size - off);

  memcpy(buf, fw->data + off, n);
  /* The original userspace driver waits indefinitely for firmware writes. */
  {
   struct usb_host_endpoint *ep;
   int attempt;

   ep = usb_pipe_endpoint(d->udev, usb_sndbulkpipe(d->udev, GCHD_EP_OUT));
   if (!ep) {
    dev_err(&d->intf->dev, "firmware endpoint 0x%02x is missing\n",
            GCHD_EP_OUT);
    r = -ENODEV;
    break;
   }

   for (attempt = 0; attempt < 5; ++attempt) {
    actual = 0;
    r = usb_bulk_msg(d->udev, usb_sndbulkpipe(d->udev, GCHD_EP_OUT),
                     buf, n, &actual, 0);
    if (r != -EAGAIN)
     break;
    usleep_range(1000, 5000);
   }

   if (r == -EPIPE) {
    int clear_r = usb_clear_halt(d->udev,
                                 usb_sndbulkpipe(d->udev, GCHD_EP_OUT));
    if (!clear_r) {
     actual = 0;
     r = usb_bulk_msg(d->udev, usb_sndbulkpipe(d->udev, GCHD_EP_OUT),
                      buf, n, &actual, 0);
    }
   }

   if (r || actual != (int)n) {
    dev_err(&d->intf->dev,
            "firmware transfer failed at %zu: %d (actual=%d requested=%zu)\n",
            off, r ? r : -EIO, actual, n);
    r = r ? r : -EIO;
    break;
   }
  }
  off += n;
 }

 kfree(buf);
 release_firmware(fw);
 return r;
}

int gchd_slsi(struct gchd *d, u16 address, u16 data)
{
 __be16 x = cpu_to_be16(data);
 return gchd_ctrl_write(d, REG_REQ, 0x0000, address, &x, sizeof(x));
}

int gchd_sparam(struct gchd *d, u16 address, u8 lsb, u8 bits, u16 data)
{
 u32 value = (u32)data, mask;
 u32 shift = lsb;
 __be32 buf[2];
 if (!bits || bits > 16 || lsb + bits > 32)
  return -EINVAL;
 if (!(address & 2))
  shift += 16;
 mask = (1U << bits) - 1;
 value <<= shift;
 mask <<= shift;
 buf[0] = cpu_to_be32(value);
 buf[1] = cpu_to_be32(mask);
 return gchd_ctrl_write(d, REG_REQ, 0x0001, address & ~3U, buf, sizeof(buf));
}

static int gchd_interrupt_pend(struct gchd *d)
{
 u8 status[3];
 int actual, r;

 r = usb_interrupt_msg(d->udev, usb_rcvintpipe(d->udev, GCHD_EP_INT),
                       status, sizeof(status), &actual, 5000);
 if (r)
  dev_err(&d->intf->dev, "USB interrupt wait failed: %d\n", r);
 return r;
}

static int gchd_mail_ready(struct gchd *d)
{
 u16 status;
 int r, tries;

 for (tries = 0; tries < 500; ++tries) {
  r = gchd_req_read16(d, 0x0900, MAIL_READY_INDEX, &status);
  if (r)
   return r;
  if (status & BIT(0))
   return 0;
  usleep_range(1000, 2000);
 }
 return -ETIMEDOUT;
}

static int gchd_send_enable_state(struct gchd *d)
{
 int r;
 u16 status;
 for (int tries = 0; tries < 500; ++tries) {
  r = gchd_reg_write16(d, ENABLE_STATE_INDEX, d->hw_enable_state);
  if (r)
   return r;
  r = gchd_reg_read16(d, MAIL_READY_INDEX, &status);
  if (r)
   return r;
  if (status & BIT(0))
   return 0;
  usleep_range(1000, 2000);
 }
 return -ETIMEDOUT;
}

int gchd_mail_write(struct gchd *d, u8 port, const u8 *data, u8 len)
{
 u8 padded[256];
 u8 cfg[4] = { 0x09, 0x00, (u8)(port << 1), len };
 int r;

 if (len > 254)
  return -EINVAL;

 if (d->family == GCHD_FAMILY_OLD) {
  r = gchd_ctrl_write(d, MAIL_REG, (u16)port << 8, 0, data, len);
  if (r)
   return r;
  return gchd_send_enable_state(d);
 }

 memcpy(padded, data, len);
 if (len & 1)
  padded[len++] = 0;

 r = gchd_ctrl_write(d, REG_REQ, 0x0800, HDNEW_MAIL_WRITE_INDEX,
                     padded, len);
 if (r)
  return r;

 r = gchd_ctrl_write(d, 0xb9, 0x0000, HDNEW_MAIL_CONFIG_INDEX,
                     cfg, sizeof(cfg));
 if (r)
  return r;

 r = gchd_interrupt_pend(d);
 if (r)
  return r;

 r = gchd_req_read16(d, 0x0800, HDNEW_INTERRUPT_STATUS_INDEX, &((u16){0}));
 if (r)
  return r;

 r = gchd_mail_ready(d);
 if (r)
  return r;

 {
  u8 dummy[2];
  r = gchd_ctrl_read(d, REG_REQ, 0x0800, HDNEW_MAIL_READ_INDEX,
                     dummy, sizeof(dummy));
 }
 if (r)
  return r;

 r = gchd_reg_write16(d, ENABLE_STATE_INDEX, d->hw_enable_state);
 if (r)
  return r;
 return gchd_mail_ready(d);
}

int gchd_mail_read(struct gchd *d, u8 port, u8 *data, u8 len)
{
 u8 cfg[4] = { 0x09, 0x01, (u8)(port << 1), len };
 u8 tmp[2 + 256 + 1];
 int r, total;

 if (len > 255)
  return -EINVAL;

 if (d->family == GCHD_FAMILY_OLD)
  return gchd_ctrl_read(d, MAIL_REG, (u16)port << 8, 0, data, len);

 r = gchd_ctrl_write(d, REG_REQ, 0x0000, HDNEW_MAIL_CONFIG_INDEX,
                     cfg, sizeof(cfg));
 if (r)
  return r;
 r = gchd_interrupt_pend(d);
 if (r)
  return r;
 r = gchd_req_read16(d, 0x0800, HDNEW_INTERRUPT_STATUS_INDEX, &((u16){0}));
 if (r)
  return r;
 r = gchd_mail_ready(d);
 if (r)
  return r;

 total = 2 + len + (len & 1);
 r = gchd_ctrl_read(d, REG_REQ, 0x0800, HDNEW_MAIL_READ_INDEX,
                    tmp, total);
 if (r)
  return r;
 memcpy(data, tmp + 2, len);
 return 0;
}

static int gchd_mail_write33(struct gchd *d, const u8 *data, u8 len)
{
 return gchd_mail_write(d, 0x33, data, len);
}

int gchd_scmd(struct gchd *d, u8 command, u8 mode, u16 data)
{
 u8 b_old[6] = { 0, 0, command, mode, data >> 8, data };
 u8 b_new[4] = { command, mode, data >> 8, data };
 int r;

 if (d->family == GCHD_FAMILY_HDNEW)
  r = gchd_ctrl_write(d, SCMD_REG, 0, 0, b_new, sizeof(b_new));
 else
  r = gchd_ctrl_write(d, SCMD_REG, 0, 0, b_old, sizeof(b_old));
 if (r)
  return r;

 if (d->family == GCHD_FAMILY_OLD)
  return 0;

 if (command == SCMD_IDLE || command == SCMD_INIT ||
     command == SCMD_STATE_CHANGE) {
  u16 rb;
  int tries;

  r = gchd_interrupt_pend(d);
  if (r)
   return r;
  for (tries = 0; tries < 500; ++tries) {
   r = gchd_req_read16(d, 0x0800, HDNEW_SCMD_READBACK_INDEX, &rb);
   if (r)
    return r;
   if ((rb >> 8) == command)
    return 0;
   usleep_range(1000, 2000);
  }
  return -ETIMEDOUT;
 }
 return 0;
}

static int gchd_wait_state(struct gchd *d, u16 expected)
{
 u16 completion, state;
 int r, tries;

 for (tries = 0; tries < 1000; ++tries) {
  r = gchd_req_read16(d, 0x0900, STATE_COMPLETE_INDEX, &completion);
  if (r)
   return r;
  if (completion & 0x0004)
   break;
  usleep_range(1000, 2000);
 }
 if (tries == 1000)
  return -ETIMEDOUT;

 r = gchd_req_read16(d, 0x0800, STATE_INDEX, &state);
 if (r)
  return r;
 state &= 0x1f;
 r = gchd_reg_write16(d, STATE_COMPLETE_INDEX, 0x0004);
 if (r)
  return r;
 return state == expected ? 0 : -EIO;
}

int gchd_state_cmd(struct gchd *d, u8 command, u8 mode, u16 data,
                          u16 expected)
{
 int r = gchd_scmd(d, command, mode, data);
 if (r)
  return r;
 return gchd_wait_state(d, expected);
}

int gchd_do_enable(struct gchd *d, u16 mask, u16 values)
{
 int r;

 if (values & ~mask)
  return -EINVAL;

 d->hw_enable_state |= mask;
 d->hw_enable_register &= ~mask;
 d->hw_enable_register |= values;

 r = gchd_reg_write16(d, ENABLE_STATE_INDEX, d->hw_enable_state);
 if (r)
  return r;
 r = gchd_reg_write16(d, ENABLE_INDEX, d->hw_enable_register);
 if (r)
  return r;

 r = gchd_reg_read16(d, ENABLE_STATE_INDEX, &d->hw_enable_state);
 if (r)
  return r;
 return gchd_reg_read16(d, ENABLE_INDEX, &d->hw_enable_register);
}

static int gchd_enable_analog(struct gchd *d)
{
 return gchd_do_enable(d, EB_ANALOG_INPUT, 0);
}

static int gchd_processor_state(struct gchd *d, u32 *magic)
{
 u8 cmd[] = { 0xab, 0xa9, 0x0f, 0xa4, 0x55 };
 u8 reply[3];
 int r;

 r = gchd_mail_write33(d, cmd, sizeof(cmd));
 if (r)
  return r;
 r = gchd_mail_read(d, 0x33, reply, sizeof(reply));
 if (r)
  return r;
 *magic = ((u32)reply[0] << 16) | ((u32)reply[1] << 8) | reply[2];
 return 0;
}

int gchd_hw_init(struct gchd *d)
{
 u16 state;
 u32 magic;
 int r;

 r = gchd_reg_write16(d, BANKSEL_INDEX, 0);
 if (r)
  return r;

 r = gchd_reg_read16(d, ENABLE_STATE_INDEX, &d->hw_enable_state);
 if (r)
  return r;
 r = gchd_reg_read16(d, ENABLE_INDEX, &d->hw_enable_register);
 if (r)
  return r;

 r = gchd_req_read16(d, 0x0800, STATE_INDEX, &state);
 if (r)
  return r;
 state &= 0x1f;

 if (state == 0) {
  r = gchd_load_firmware(d, d->family == GCHD_FAMILY_HDNEW ? FW_IDLE_NEW : FW_IDLE_OLD);
  if (r)
   return r;
  r = gchd_reg_write16(d, 0x0070, 4);
  if (r)
   return r;
 } else {
  r = gchd_state_cmd(d, SCMD_RESET, 0, 0, 0x10);
  if (r)
   return r;
 }

 r = gchd_state_cmd(d, SCMD_IDLE, 0, 0, 0x11);
 if (r)
  return r;

 r = gchd_processor_state(d, &magic);
 if (r)
  return r;

 if (magic == 0x334455) {
  r = gchd_enable_analog(d);
  if (r)
   return r;
  r = gchd_do_enable(d, EB_FIRMWARE_PROCESSOR, EB_FIRMWARE_PROCESSOR);
  if (r)
   return r;
 }

 dev_info(&d->intf->dev, "device idle, processor state 0x%06x\n", magic);
 return 0;
}

int gchd_hw_shutdown(struct gchd *d)
{
 int r = 0;

 if (!d->udev)
  return 0;

 if (d->hw_enable_register & EB_ENCODER_ENABLE)
  gchd_do_enable(d, EB_ENCODER_ENABLE, 0);
 if (d->hw_enable_register & EB_ENCODER_TRIGGER)
  gchd_do_enable(d, EB_ENCODER_TRIGGER, 0);

 if (d->hw_enable_register & EB_FIRMWARE_PROCESSOR)
  gchd_do_enable(d, EB_FIRMWARE_PROCESSOR, 0);
 gchd_reg_write16(d, ENABLE_STATE_INDEX, 0);
 d->hw_enable_state = 0;

 r = gchd_scmd(d, SCMD_IDLE, 0, 0);
 if (!r)
  r = gchd_scmd(d, SCMD_RESET, 1, 0);
 return r;
}
