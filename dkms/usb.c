#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "elgato_gchd.h"
#include "gchd_mailbox.h"


MODULE_FIRMWARE("gchd/MB86H57_H58_IDLE");
MODULE_FIRMWARE("gchd/MB86H57_H58_ENC_H");
MODULE_FIRMWARE("gchd/MB86M01_ASSP_NSEC_IDLE");
MODULE_FIRMWARE("gchd/MB86M01_ASSP_NSEC_ENC_H");


#define FW_IDLE_OLD "gchd/MB86H57_H58_IDLE"
#define FW_ENC_OLD  "gchd/MB86H57_H58_ENC_H"
#define FW_IDLE_NEW "gchd/MB86M01_ASSP_NSEC_IDLE"
#define FW_ENC_NEW  "gchd/MB86M01_ASSP_NSEC_ENC_H"


/* Log every mailbox transfer. */
static bool trace_mailbox;
module_param(trace_mailbox, bool, 0644);
MODULE_PARM_DESC(trace_mailbox, "Log raw mailbox commands and replies for input-detection diagnostics");

static int gchd_ctrl_read(struct gchd *d, u8 req, u16 value, u16 index,
                          void *buf, u16 len)
{
 unsigned long page;
 void *tmp;
 int r;

 if (!len)
  return 0;

 /*
  * libusb's synchronous Linux backend uses USBDEVFS_CONTROL, whose
  * kernel implementation allocates a full page for the control data
  * buffer before calling usb_control_msg(). Match that transfer-buffer
  * layout rather than using a len-sized kmalloc buffer.
  */
 page = __get_free_page(GFP_KERNEL);
 if (!page)
  return -ENOMEM;
 tmp = (void *)page;

 r = usb_control_msg(d->udev, usb_rcvctrlpipe(d->udev, 0),
                     req, GCHD_RT_VENDOR_IN, value, index, tmp, len, 0);
 if (r == len)
  memcpy(buf, tmp, len);
 else if (r >= 0)
  r = -EIO;

 free_page(page);
 return r < 0 ? r : 0;
}

static int gchd_ctrl_write(struct gchd *d, u8 req, u16 value, u16 index,
                           const void *buf, u16 len)
{
 unsigned long page;
 void *tmp;
 int r;

 if (!len)
  return 0;

 /*
  * Match USBDEVFS_CONTROL/libusb's page-sized synchronous control
  * transfer buffer. Only the first len bytes are part of the USB
  * data stage, so the wire-visible request is unchanged.
  */
 page = __get_free_page(GFP_KERNEL);
 if (!page)
  return -ENOMEM;
 tmp = (void *)page;
 memcpy(tmp, buf, len);

 r = usb_control_msg(d->udev, usb_sndctrlpipe(d->udev, 0),
                     req, GCHD_RT_VENDOR_OUT, value, index, tmp, len, 0);

 free_page(page);
 return r < 0 ? r : (r == len ? 0 : -EIO);
}

static int gchd_reg_read16(struct gchd *d, u16 index, u16 *v)
{
 __be16 x;
 int r = gchd_ctrl_read(d, GCHD_REQ_REG, GCHD_BANK_CTRL, index, &x, sizeof(x));
 if (!r)
  *v = be16_to_cpu(x);
 return r;
}

static int gchd_reg_write16(struct gchd *d, u16 index, u16 v)
{
 __be16 x = cpu_to_be16(v);
 return gchd_ctrl_write(d, GCHD_REQ_REG, GCHD_BANK_CTRL, index, &x, sizeof(x));
}

int gchd_raw_read16(struct gchd *d, u16 value, u16 index, u16 *v)
{
 __be16 x;
 int r = gchd_ctrl_read(d, GCHD_REQ_REG, value, index, &x, sizeof(x));
 if (!r)
  *v = be16_to_cpu(x);
 return r;
}


static int gchd_req_read16(struct gchd *d, u16 value, u16 index, u16 *v)
{
 __be16 x;
 int r = gchd_ctrl_read(d, GCHD_REQ_REG, value, index, &x, sizeof(x));
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
  struct usb_host_endpoint *ep;
  int attempt;

  memcpy(buf, fw->data + off, n);

  ep = usb_pipe_endpoint(d->udev, usb_sndbulkpipe(d->udev, GCHD_EP_FW_OUT));
  if (!ep) {
   dev_err(&d->intf->dev, "firmware endpoint 0x%02x is missing\n",
           GCHD_EP_FW_OUT);
   r = -ENODEV;
   break;
  }

  dev_info(&d->intf->dev,
           "firmware chunk offset=%zu size=%zu OUT ep 0x%02x maxpacket=%u speed=%u alt=%u\n",
           off, n, GCHD_EP_FW_OUT, usb_endpoint_maxp(&ep->desc), d->udev->speed,
           d->intf->cur_altsetting->desc.bAlternateSetting);

  for (attempt = 0; attempt < 5; ++attempt) {
   actual = 0;
   r = usb_bulk_msg(d->udev, usb_sndbulkpipe(d->udev, GCHD_EP_FW_OUT),
                    buf, n, &actual, 0);
   if (r != -EAGAIN)
    break;
   usleep_range(1000, 5000);
  }

  if (r == -EPIPE) {
   int clear_r = usb_clear_halt(d->udev,
                                usb_sndbulkpipe(d->udev, GCHD_EP_FW_OUT));
   if (!clear_r) {
    actual = 0;
    r = usb_bulk_msg(d->udev, usb_sndbulkpipe(d->udev, GCHD_EP_FW_OUT),
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

  off += n;
 }
 kfree(buf);
 release_firmware(fw);
 return r;
}

int gchd_slsi(struct gchd *d, u16 address, u16 data)
{
 __be16 x = cpu_to_be16(data);
 return gchd_ctrl_write(d, GCHD_REQ_REG, GCHD_BANK_TC, address, &x, sizeof(x));
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
 return gchd_ctrl_write(d, GCHD_REQ_REG, GCHD_BANK_TC_FIELD, address & ~3U, buf, sizeof(buf));
}

static int gchd_interrupt_pend(struct gchd *d)
{
 u8 *status;
 int actual, r;

 status = kmalloc(3, GFP_KERNEL);
 if (!status)
  return -ENOMEM;

 r = usb_interrupt_msg(d->udev, usb_rcvintpipe(d->udev, GCHD_EP_INT_IN),
                        status, 3, &actual, 0);

 kfree(status);

 if (r)
  return r;
 if (actual != 3)
  return -EIO;
 return 0;
}

static int gchd_mail_ready(struct gchd *d)
{
 u16 status;
 int r, tries;

 for (tries = 0; tries < 500; ++tries) {
  r = gchd_req_read16(d, GCHD_BANK_CTRL, GCHD_REG_MAIL_READY, &status);
  if (r)
   return r;
  d->special_detect_mask &= GCHD_MAIL_READY_DETECT(status);
  if (status & GCHD_MAIL_READY_BIT)
   return 0;
 }
 return -ETIMEDOUT;
}

int gchd_send_enable_state(struct gchd *d)
{
 int r;
 u16 status;

 for (int tries = 0; tries < 500; ++tries) {
  r = gchd_reg_write16(d, GCHD_REG_ENABLE_SHADOW, d->hw_enable_state);
  if (r)
   return r;

  r = gchd_reg_read16(d, GCHD_REG_MAIL_READY, &status);
  if (r)
   return r;

  /*
   * Every successful status read narrows the cable/source detection mask
   * before waiting for the mailbox-ready bit.
   */
  d->special_detect_mask &= GCHD_MAIL_READY_DETECT(status);

  if (status & GCHD_MAIL_READY_BIT)
   return 0;
 }
 return -ETIMEDOUT;
}

int gchd_mail_write(struct gchd *d, u8 port, const u8 *data, u8 len)
{
 u8 padded[256];
 u8 cfg[4] = { GCHD_HDNEW_MAIL_OP, GCHD_HDNEW_MAIL_WRITE, (u8)(port << 1), len };
 int r;

 if (len > 254)
  return -EINVAL;
 if (trace_mailbox)
  dev_info(&d->intf->dev, "gchd-diag mailbox TX port=0x%02x len=%u data=%*ph\n",
           port, len, (int)len, data);

 if (d->family == GCHD_FAMILY_OLD) {
  r = gchd_ctrl_write(d, GCHD_REQ_HD_MAIL, (u16)port << 8, 0, data, len);
  if (r)
   return r;
  return gchd_send_enable_state(d);
 }

 memcpy(padded, data, len);
 if (len & 1)
  padded[len++] = 0;

 r = gchd_ctrl_write(d, GCHD_REQ_REG, GCHD_BANK_SYS, GCHD_REG_HDNEW_MAIL_WRITE,
                     padded, len);
 if (r)
  return r;

 r = gchd_ctrl_write(d, GCHD_REQ_HDNEW_MAIL_CMD, 0x0000, GCHD_REG_HDNEW_MAIL_CMD,
                     cfg, sizeof(cfg));
 if (r)
  return r;

 r = gchd_interrupt_pend(d);
 if (r)
  return r;

 r = gchd_req_read16(d, GCHD_BANK_SYS, GCHD_REG_HDNEW_INT_STATUS, &((u16){0}));
 if (r)
  return r;

 r = gchd_mail_ready(d);
 if (r)
  return r;

 {
  u8 dummy[2];
  r = gchd_ctrl_read(d, GCHD_REQ_REG, GCHD_BANK_SYS, GCHD_REG_HDNEW_MAIL_READ,
                     dummy, sizeof(dummy));
 }
 if (r)
  return r;

 r = gchd_reg_write16(d, GCHD_REG_ENABLE_SHADOW, d->saved_enable_state);
 if (r)
  return r;
 return gchd_mail_ready(d);
}

int gchd_mail_read(struct gchd *d, u8 port, u8 *data, u8 len)
{
 u8 cfg[4] = { GCHD_HDNEW_MAIL_OP, GCHD_HDNEW_MAIL_READ, (u8)(port << 1), len };
 u8 tmp[2 + 256 + 1];
 int r, total;

 if (len > 255)
  return -EINVAL;

 if (d->family == GCHD_FAMILY_OLD) {
  r = gchd_ctrl_read(d, GCHD_REQ_HD_MAIL, (u16)port << 8, 0, data, len);
  if (!r && trace_mailbox)
   dev_info(&d->intf->dev, "gchd-diag mailbox RX port=0x%02x len=%u data=%*ph\n",
            port, len, (int)len, data);
  return r;
 }

 r = gchd_ctrl_write(d, GCHD_REQ_HDNEW_MAIL_CMD, 0x0000, GCHD_REG_HDNEW_MAIL_CMD,
                     cfg, sizeof(cfg));
 if (r)
  return r;
 r = gchd_interrupt_pend(d);
 if (r)
  return r;
 r = gchd_req_read16(d, GCHD_BANK_SYS, GCHD_REG_HDNEW_INT_STATUS, &((u16){0}));
 if (r)
  return r;
 r = gchd_mail_ready(d);
 if (r)
  return r;

 total = 2 + len + (len & 1);
 r = gchd_ctrl_read(d, GCHD_REQ_REG, GCHD_BANK_SYS, GCHD_REG_HDNEW_MAIL_READ,
                    tmp, total);
 if (r)
  return r;
 memcpy(data, tmp + 2, len);
 if (trace_mailbox)
  dev_info(&d->intf->dev, "gchd-diag mailbox RX port=0x%02x len=%u data=%*ph\n",
           port, len, (int)len, data);
 return 0;
}

static int gchd_mail_write33(struct gchd *d, const u8 *data, u8 len)
{
 return gchd_mail_write(d, GCHD_PORT_BRIDGE, data, len);
}

/*
 * Named SCMD operations keep their complete wire payloads together. The
 * original-HD transport has a two-byte prefix; HD-New sends the four-byte
 * command payload directly.
 */
struct gchd_scmd_definition {
 u8 command;
 u8 mode;
 u16 data;
 u8 old_payload[6];
 u8 hdnew_payload[4];
};

static const struct gchd_scmd_definition gchd_scmd_idle = {
 GCHD_SCMD_IDLE, 0, 0,
 { 0x00, 0x00, GCHD_SCMD_IDLE, 0x00, 0x00, 0x00 },
 { GCHD_SCMD_IDLE, 0x00, 0x00, 0x00 }
};

static const struct gchd_scmd_definition gchd_scmd_start = {
 GCHD_SCMD_STATE_CHANGE, 0, GCHD_STATE_START,
 { 0x00, 0x00, GCHD_SCMD_STATE_CHANGE, 0x00, 0x00, GCHD_STATE_START },
 { GCHD_SCMD_STATE_CHANGE, 0x00, 0x00, GCHD_STATE_START }
};

static const struct gchd_scmd_definition gchd_scmd_init_hdnew = {
 GCHD_SCMD_INIT, GCHD_SCMD_INIT_NO_RELOAD, 0,
 { 0x00, 0x00, GCHD_SCMD_INIT, GCHD_SCMD_INIT_NO_RELOAD, 0x00, 0x00 },
 { GCHD_SCMD_INIT, GCHD_SCMD_INIT_NO_RELOAD, 0x00, 0x00 }
};

int gchd_scmd(struct gchd *d, u8 command, u8 mode, u16 data)
{
 u8 b_old[6] = { 0, 0, command, mode, data >> 8, data };
 u8 b_new[4] = { command, mode, data >> 8, data };
 const u8 *payload_old = b_old, *payload_new = b_new;
 const struct gchd_scmd_definition *definition = NULL;
 int r;

 if (command == GCHD_SCMD_IDLE && mode == 0 && data == 0)
  definition = &gchd_scmd_idle;
 else if (command == GCHD_SCMD_STATE_CHANGE && mode == 0 &&
          data == GCHD_STATE_START)
  definition = &gchd_scmd_start;
 else if (command == GCHD_SCMD_INIT && mode == GCHD_SCMD_INIT_NO_RELOAD &&
          data == 0)
  definition = &gchd_scmd_init_hdnew;

 if (definition) {
  payload_old = definition->old_payload;
  payload_new = definition->hdnew_payload;
 }

 if (d->family == GCHD_FAMILY_HDNEW)
  r = gchd_ctrl_write(d, GCHD_REQ_SCMD, 0, 0, payload_new, 4);
 else
  r = gchd_ctrl_write(d, GCHD_REQ_SCMD, 0, 0, payload_old, 6);
 if (r)
  return r;

 if (d->family == GCHD_FAMILY_HDNEW &&
     (command == GCHD_SCMD_IDLE || command == GCHD_SCMD_INIT ||
      command == GCHD_SCMD_STATE_CHANGE))
  dev_info(&d->intf->dev,
           "SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=%02x %02x %02x %02x\n",
           payload_new[0], payload_new[1], payload_new[2], payload_new[3]);

 if (d->family == GCHD_FAMILY_OLD)
  return 0;

 /*
  * HD-New SCMD receipt: mode 0xa0 suppresses the interrupt/readback
  * handshake (notably for GCHD_SCMD_INIT).
  */
 if ((command == GCHD_SCMD_IDLE || command == GCHD_SCMD_INIT ||
      command == GCHD_SCMD_STATE_CHANGE) &&
     (mode & GCHD_SCMD_INIT_NO_RELOAD) != GCHD_SCMD_INIT_NO_RELOAD) {
  u16 rb = 0;
  int tries;

  r = gchd_interrupt_pend(d);
  if (r)
   return r;
  dev_info(&d->intf->dev,
           "SCMD interrupt completed: command=0x%02x\n", command);
  for (tries = 0; tries < 500; ++tries) {
   r = gchd_req_read16(d, GCHD_BANK_SYS, GCHD_REG_HDNEW_SCMD_RECEIPT, &rb);
   if (r)
    return r;
   if ((rb >> 8) == command)
    return 0;
  }
  dev_err(&d->intf->dev,
          "SCMD timeout: command=0x%02x mode=0x%02x data=0x%04x readback=0x%04x polls=%d\n",
          command, mode, data, rb, tries);
  return -ETIMEDOUT;
 }
 return 0;
}

static int gchd_complete_state_change(struct gchd *d, u16 current_state, u16 next)
{
 u16 state, completion, clear_index = 0;
 u16 last_state = 0xffff, last_completion = 0xffff, last_clear_index = 0xffff;
 int r, tries;
 bool first = true;
 bool trace_start = current_state != next && next == GCHD_STATE_START;

 for (;;) {
  for (tries = 0; tries < 2000; ++tries) {
   u16 raw_state;

   r = gchd_req_read16(d, GCHD_BANK_SYS, GCHD_REG_SCMD_STATE, &raw_state);
   if (r)
    return r;
   state = raw_state & GCHD_STATE_MASK;
   if (state != current_state && state != next) {
    if (trace_start)
     dev_err(&d->intf->dev,
             "START poll=%d unexpected state raw=0x%04x masked=0x%04x current=0x%02x expected=0x%02x\n",
             tries, raw_state, state, current_state, next);
    if (first)
     return state == next ? 0 : -EIO;
    return -EIO;
   }
   first = false;

   r = gchd_req_read16(d, GCHD_BANK_CTRL, GCHD_REG_SCMD_COMPLETE, &completion);
   if (r)
    return r;
   r = gchd_req_read16(d, GCHD_BANK_CTRL, GCHD_REG_SCMD_COMPLETE_AUX, &clear_index);
   if (r)
    return r;

   /*
    * Keep START diagnostics useful without flooding dmesg for the full
    * 2000-poll timeout: log initial polls, periodic samples, and changes
    * in the three values used by the completion handshake.
    */
   if (trace_start &&
       (tries < 16 || (tries % 100) == 0 ||
        state != last_state || completion != last_completion ||
        clear_index != last_clear_index)) {
    dev_info(&d->intf->dev,
             "START poll=%d state=0x%04x completion(0900:0074)=0x%04x clear_index(0900:01b0)=0x%04x ack=%u\n",
             tries, state, completion, clear_index,
             !!(completion & GCHD_SCMD_COMPLETE_BIT));
   }
   last_state = state;
   last_completion = completion;
   last_clear_index = clear_index;

   if (completion & GCHD_SCMD_COMPLETE_BIT)
    break;
  }
  if (tries == 2000) {
   dev_err(&d->intf->dev,
           "state-change timeout: current=0x%02x expected=0x%02x state=0x%04x completion=0x%04x\n",
           current_state, next, state, completion);
   return -ETIMEDOUT;
  }

  r = gchd_req_read16(d, GCHD_BANK_SYS, GCHD_REG_SCMD_STATE, &state);
  if (r) return r;
  r = gchd_req_read16(d, GCHD_BANK_SYS, GCHD_REG_SCMD_STATE, &state);
  if (r) return r;
  state &= GCHD_STATE_MASK;

  r = gchd_reg_write16(d, GCHD_REG_SCMD_COMPLETE, GCHD_SCMD_COMPLETE_BIT);
  if (r) return r;
  r = gchd_reg_write16(d, GCHD_REG_SCMD_COMPLETE_AUX, 0x0000);
  if (r) return r;

  if (state == next)
   return 0;
  if (state != current_state)
   return -EIO;
 }
}

int gchd_state_cmd(struct gchd *d, u8 command, u8 mode, u16 data,
                          u16 expected)
{
 u16 current_state;
 int r;

 /*
  * Match stateConfirmedScmd(): read the current state before issuing
  * the command, then complete the transition from that state.
  */
 r = gchd_req_read16(d, GCHD_BANK_SYS, GCHD_REG_SCMD_STATE, &current_state);
 if (r)
  return r;
 current_state &= GCHD_STATE_MASK;

 /*
  * GCHD_SCMD_IDLE is used when entering/re-entering input detection. HDNew reports
  * 0x11 when already IDLE, but its completion flag does not signal a second
  * transition to the same state. Treat this state request as idempotent so an
  * input switch from IDLE can proceed to signal detection instead of timing
  * out while waiting for a transition that cannot occur.
  */
 if (command == GCHD_SCMD_IDLE && current_state == expected) {
  dev_dbg(&d->intf->dev,
          "state already IDLE (0x%02x); skipping redundant SCMD_IDLE\n",
          current_state);
  return 0;
 }

 if (command == GCHD_SCMD_STATE_CHANGE && expected == GCHD_STATE_START) {
  u16 enable_state = 0xffff, enable = 0xffff, completion = 0xffff;
  int er1, er2, er3;

  er1 = gchd_req_read16(d, GCHD_BANK_CTRL, GCHD_REG_ENABLE_SHADOW, &enable_state);
  er2 = gchd_req_read16(d, GCHD_BANK_CTRL, GCHD_REG_ENABLE, &enable);
  er3 = gchd_req_read16(d, GCHD_BANK_CTRL, GCHD_REG_SCMD_COMPLETE, &completion);
  dev_info(&d->intf->dev,
           "START before SCMD: state=0x%04x enable_state=0x%04x (r=%d) enable=0x%04x (r=%d) completion=0x%04x (r=%d) saved=0x%04x hw_state=0x%04x hw_enable=0x%04x\n",
           current_state, enable_state, er1, enable, er2, completion, er3,
           d->saved_enable_state, d->hw_enable_state, d->hw_enable_register);
 }

 r = gchd_scmd(d, command, mode, data);
 if (r)
  return r;

 r = gchd_complete_state_change(d, current_state, expected);
 if (r && command == GCHD_SCMD_STATE_CHANGE && expected == GCHD_STATE_START) {
  u16 state = 0xffff, enable_state = 0xffff, enable = 0xffff;
  u16 completion = 0xffff;
  int sr, er1, er2, er3;

  sr = gchd_req_read16(d, GCHD_BANK_SYS, GCHD_REG_SCMD_STATE, &state);
  er1 = gchd_req_read16(d, GCHD_BANK_CTRL, GCHD_REG_ENABLE_SHADOW, &enable_state);
  er2 = gchd_req_read16(d, GCHD_BANK_CTRL, GCHD_REG_ENABLE, &enable);
  er3 = gchd_req_read16(d, GCHD_BANK_CTRL, GCHD_REG_SCMD_COMPLETE, &completion);
  dev_err(&d->intf->dev,
          "START failed: ret=%d state=0x%04x (r=%d) enable_state=0x%04x (r=%d) enable=0x%04x (r=%d) completion=0x%04x (r=%d) saved=0x%04x hw_state=0x%04x hw_enable=0x%04x\n",
          r, state, sr, enable_state, er1, enable, er2, completion, er3,
          d->saved_enable_state, d->hw_enable_state, d->hw_enable_register);
 }
 return r;
}


/*
 * Stop the stream (START -> NULL -> STOP) while draining. The caller
 * must stop/join the RX thread first: this function owns bulk-IN endpoint
 * 0x81 while draining the device's queued transport stream.
 */
static int gchd_drain_stream(struct gchd *d, unsigned int count)
{
 unsigned int i;
 int actual, r;

 /* The RX thread's receive queue is consuming the stream: only pace. */
 if (READ_ONCE(d->rx_thread)) {
  msleep(10);
  return 0;
 }

 for (i = 0; i < count; ++i) {
  actual = 0;
  r = usb_bulk_msg(d->udev, usb_rcvbulkpipe(d->udev, GCHD_EP_TS_IN),
                   d->usb_buf, GCHD_USB_BUFSIZE, &actual, 50);
  /* Nothing pending: the queued data has been drained. */
  if (r == -ETIMEDOUT || r == -EAGAIN)
   break;
  if (r)
   return r;
 }
 return 0;
}

/* completeStateChange(..., forceStreamEmpty=true), including its 50-read
 * drain before each completion-register poll. */
static int gchd_complete_state_change_draining(struct gchd *d,
                                                u16 current_state, u16 next_state)
{
 u16 state = 0, completion = 0, dummy;
 int r, tries;
 bool first = true;

 for (;;) {
  for (tries = 0; tries < 2000; ++tries) {
   r = gchd_req_read16(d, GCHD_BANK_SYS, GCHD_REG_SCMD_STATE, &state);
   if (r) return r;
   state &= GCHD_STATE_MASK;
   if (first && state != current_state && state != next_state)
    return -EIO;
   first = false;

   r = gchd_drain_stream(d, 50);
   if (r) return r;
   r = gchd_req_read16(d, GCHD_BANK_CTRL, GCHD_REG_SCMD_COMPLETE, &completion);
   if (r) return r;
   r = gchd_req_read16(d, GCHD_BANK_CTRL, GCHD_REG_SCMD_COMPLETE_AUX, &dummy);
   if (r) return r;
   if (completion & GCHD_SCMD_COMPLETE_BIT)
    break;
  }
  if (tries == 2000)
   return -ETIMEDOUT;

  r = gchd_req_read16(d, GCHD_BANK_SYS, GCHD_REG_SCMD_STATE, &state);
  if (r) return r;
  r = gchd_req_read16(d, GCHD_BANK_SYS, GCHD_REG_SCMD_STATE, &state);
  if (r) return r;
  state &= GCHD_STATE_MASK;
  r = gchd_reg_write16(d, GCHD_REG_SCMD_COMPLETE, GCHD_SCMD_COMPLETE_BIT);
  if (r) return r;
  r = gchd_reg_write16(d, GCHD_REG_SCMD_COMPLETE_AUX, 0x0000);
  if (r) return r;
  if (state == next_state) return 0;
  if (state != current_state) return -EIO;
 }
}

/* START -> NULL, drain 200, complete; drain 20, NULL -> STOP, complete;
 * drain 5. */
int gchd_stream_stop(struct gchd *d)
{
 int r;
 u16 state;

 r = gchd_req_read16(d, GCHD_BANK_SYS, GCHD_REG_SCMD_STATE, &state);
 if (r) return r;
 state &= GCHD_STATE_MASK;
 if (state == GCHD_STATE_STOP || state == GCHD_STATE_IDLE) {
  /* STOP/IDLE leaves the selected input's setup intact. */
  return 0;
 }
 if (state != GCHD_STATE_START && state != GCHD_STATE_NULL)
  return -EINVAL;

 r = gchd_scmd(d, GCHD_SCMD_STATE_CHANGE, 0, GCHD_STATE_NULL);
 if (r) return r;
 r = gchd_drain_stream(d, 200);
 if (r) return r;
 r = gchd_complete_state_change_draining(d, GCHD_STATE_START, GCHD_STATE_NULL);
 if (r) return r;
 r = gchd_drain_stream(d, 20);
 if (r) return r;

 r = gchd_scmd(d, GCHD_SCMD_STATE_CHANGE, 0, GCHD_STATE_STOP);
 if (r) return r;
 r = gchd_complete_state_change_draining(d, GCHD_STATE_NULL, GCHD_STATE_STOP);
 if (r) return r;
 /* Successful STOP keeps input configuration for the next STREAMON. */
 if (READ_ONCE(d->rx_thread))
  return 0;
 return gchd_drain_stream(d, 5);
}

/*
 * Bring the encoder to the stopped state (0x01) without leaving it
 * initialized: a running stream is stopped (NULL -> STOP); the stopped state
 * is kept. IDLE (0x11) is not used, because INIT mode 0xa0 does not leave it.
 */
int gchd_return_to_stop(struct gchd *d)
{
 u16 state;
 int r;

 r = gchd_req_read16(d, GCHD_BANK_SYS, GCHD_REG_SCMD_STATE, &state);
 if (r)
  return r;
 state &= GCHD_STATE_MASK;
 if (state == GCHD_STATE_START || state == GCHD_STATE_NULL)
  return gchd_stream_stop(d);
 return 0;
}

int gchd_do_enable(struct gchd *d, u16 mask, u16 values)
{
 int r;

 if (values & ~mask)
  return -EINVAL;

 d->hw_enable_state |= mask;
 d->hw_enable_register &= ~mask;
 d->hw_enable_register |= values;

 r = gchd_reg_write16(d, GCHD_REG_ENABLE_SHADOW, d->hw_enable_state);
 if (r)
  return r;
 r = gchd_reg_write16(d, GCHD_REG_ENABLE, d->hw_enable_register);
 if (r)
  return r;

 r = gchd_reg_read16(d, GCHD_REG_ENABLE_SHADOW, &d->hw_enable_state);
 if (r)
  return r;
 r = gchd_reg_read16(d, GCHD_REG_ENABLE, &d->hw_enable_register);
 if (r)
  return r;

 /*
  * The saved enable-state snapshot is updated after reading back the
  * hardware. Mailbox writes restore this
  * snapshot, so leaving it stale would undo the processor-enable bits on
  * the next mailbox transaction.
  */
 d->saved_enable_state = d->hw_enable_state;
 return 0;
}

int gchd_enable_analog(struct gchd *d)
{
 u16 value = d->input ? GCHD_EB_ANALOG_INPUT : 0;
 return gchd_do_enable(d, GCHD_EB_ANALOG_INPUT, value);
}

static int gchd_processor_state(struct gchd *d, u32 *magic)
{
 u8 cmd[] = { GCHD_BRIDGE_PROBE_0, GCHD_BRIDGE_PROBE_1, GCHD_BRIDGE_PROBE_2, GCHD_BRIDGE_PROBE_3, GCHD_BRIDGE_PROBE_PROCESSOR };
 u8 reply[3];
 int r;

 r = gchd_mail_write33(d, cmd, sizeof(cmd));
 if (r)
  return r;
 r = gchd_mail_read(d, GCHD_PORT_BRIDGE, reply, sizeof(reply));
 if (r)
  return r;
 *magic = ((u32)reply[0] << 16) | ((u32)reply[1] << 8) | reply[2];
 return 0;
}

int gchd_hw_init(struct gchd *d)
{
 u16 state;
 u32 magic;
 int r, tries;
 u32 version0, version1;

 /* Read the hardware revision before BANKSEL. */
 r = gchd_ctrl_read(d, GCHD_REQ_REG, GCHD_BANK_SYS, GCHD_REG_HDNEW_VERSION0, &version0, sizeof(version0));
 if (r)
  return r;
 r = gchd_ctrl_read(d, GCHD_REQ_REG, GCHD_BANK_SYS, GCHD_REG_HDNEW_VERSION1, &version1, sizeof(version1));
 if (r)
  return r;
 if (!version0 && !version1) {
  u8 version[4];
  r = gchd_ctrl_read(d, GCHD_REQ_REG, GCHD_BANK_SYS, GCHD_REG_HD_VERSION0, version, sizeof(version));
  if (r)
   return r;
  r = gchd_ctrl_read(d, GCHD_REQ_REG, GCHD_BANK_SYS, GCHD_REG_HD_VERSION1, version, sizeof(version));
  if (r)
   return r;
  r = gchd_ctrl_read(d, GCHD_REQ_REG, GCHD_BANK_SYS, GCHD_REG_HD_VERSION2, version, sizeof(version));
  if (r)
   return r;
 } else {
  u8 version[4];
  /* readVersion() first probes registers 0 and 1, then reads the same
   * two registers again as part of the version buffer, followed by 2. */
  r = gchd_ctrl_read(d, GCHD_REQ_REG, GCHD_BANK_SYS, GCHD_REG_HDNEW_VERSION0, version, sizeof(version));
  if (r)
   return r;
  r = gchd_ctrl_read(d, GCHD_REQ_REG, GCHD_BANK_SYS, GCHD_REG_HDNEW_VERSION1, version, sizeof(version));
  if (r)
   return r;
  r = gchd_ctrl_read(d, GCHD_REQ_REG, GCHD_BANK_SYS, GCHD_REG_HDNEW_VERSION2, version, sizeof(version));
  if (r)
   return r;
 }

 r = gchd_reg_write16(d, GCHD_REG_BANKSEL, 0);
 if (r)
  return r;

 r = gchd_reg_read16(d, GCHD_REG_ENABLE_SHADOW, &d->saved_enable_state);
 if (r)
  return r;
 d->hw_enable_state = d->saved_enable_state;

 r = gchd_req_read16(d, GCHD_BANK_SYS, GCHD_REG_SCMD_STATE, &state);
 if (r)
  return r;
 state &= GCHD_STATE_MASK;

 if (state == 0) {
  if (d->family == GCHD_FAMILY_HDNEW) {
   u16 completion = 0;
   int tries;

   /*
    * HD-New triggers this initial transition when the state is read.
    * Consume its interrupt, poll the sticky completion flag, and
    * acknowledge it before downloading idle firmware.
    */
   r = gchd_interrupt_pend(d);
   if (r)
    return r;

   for (tries = 0; tries < 1000; ++tries) {
    r = gchd_req_read16(d, GCHD_BANK_CTRL, GCHD_REG_SCMD_COMPLETE, &completion);
    if (r)
     return r;
    if (completion & GCHD_SCMD_COMPLETE_BIT)
     break;
   }
   if (tries == 1000)
    return -ETIMEDOUT;

   r = gchd_reg_write16(d, GCHD_REG_SCMD_COMPLETE, GCHD_SCMD_COMPLETE_BIT);
   if (r)
    return r;
  } else {
   /*
    * On the original HD model, reading state 0 triggers the transition;
    * complete it before loading the idle firmware.
    */
   r = gchd_complete_state_change(d, 0, 0);
   if (r)
    return r;
  }

  r = gchd_load_firmware(d, d->family == GCHD_FAMILY_HDNEW ? FW_IDLE_NEW : FW_IDLE_OLD);
  if (r)
   return r;

  /*
   * The post-download acknowledgement is a different register from the
   * state-transition completion flag above: 0900:0070.
   */
  r = gchd_reg_write16(d, GCHD_REG_POST_IDLE_FW, GCHD_SCMD_COMPLETE_BIT);
  if (r)
   return r;

  /*
   * configureDevice() refreshes the saved enable-state snapshot after
   * loading idle firmware.  mailWrite() restores this exact snapshot;
   * the pre-firmware value is only useful for the already-running path.
   */
  r = gchd_reg_read16(d, GCHD_REG_ENABLE_SHADOW, &d->saved_enable_state);
  if (r)
   return r;

  /* ENABLE_STATE is read only once here; reuse that value. */
  d->hw_enable_state = d->saved_enable_state;
  r = gchd_reg_read16(d, GCHD_REG_ENABLE, &d->hw_enable_register);
  if (r)
   return r;

  {
   u16 dummy;
   r = gchd_ctrl_read(d, GCHD_REQ_REG, GCHD_BANK_TC, 0x0010, &dummy, sizeof(dummy)); if (r) return r;
   r = gchd_ctrl_read(d, GCHD_REQ_REG, GCHD_BANK_TC, 0x0012, &dummy, sizeof(dummy)); if (r) return r;
   r = gchd_ctrl_read(d, GCHD_REQ_REG, GCHD_BANK_TC, 0x0014, &dummy, sizeof(dummy)); if (r) return r;
   r = gchd_ctrl_read(d, GCHD_REQ_REG, GCHD_BANK_TC, 0x0016, &dummy, sizeof(dummy)); if (r) return r;
   r = gchd_ctrl_read(d, GCHD_REQ_REG, GCHD_BANK_TC, 0x0018, &dummy, sizeof(dummy)); if (r) return r;
   r = gchd_ctrl_read(d, GCHD_REQ_REG, GCHD_BANK_TC, 0x001a, &dummy, sizeof(dummy)); if (r) return r;
   r = gchd_ctrl_read(d, GCHD_REQ_REG, GCHD_BANK_TC, 0x001c, &dummy, sizeof(dummy)); if (r) return r;
   r = gchd_ctrl_read(d, GCHD_REQ_REG, GCHD_BANK_TC, 0x001e, &dummy, sizeof(dummy)); if (r) return r;
  }
 } else {
  r = gchd_state_cmd(d, GCHD_SCMD_RESET, 0, 0, GCHD_STATE_FW_LOADED);
  if (r)
   return r;
 }

 r = gchd_state_cmd(d, GCHD_SCMD_IDLE, 0, 0, GCHD_STATE_IDLE);
 if (r)
  return r;

 /*
  * Match configureDevice() exactly here:
  *   specialDetectMask_ = 0xffff;
  *   one ignored 0x55 mailbox read;
  *   then repeat the same 0x55 query until 0x27f97b.
  */
 d->special_detect_mask = 0xffff;
 r = gchd_processor_state(d, &magic);
 if (r)
  return r;

 bool first_time = true;

 for (tries = 0; tries < 2000; ++tries) {
  r = gchd_processor_state(d, &magic);
  if (r)
   return r;

  switch (magic) {
  case GCHD_PROC_OFF:
   /*
    * Exact configureDevice() processor bring-up:
    * sendEnableState(), enableAnalogInput(), then enable the firmware
    * processor. sendEnableState() also updates special_detect_mask.
    */
   r = gchd_send_enable_state(d);
   if (r)
    return r;

   r = gchd_do_enable(d, GCHD_EB_ANALOG_INPUT, GCHD_EB_ANALOG_INPUT);
   if (r)
    return r;

   r = gchd_do_enable(d, GCHD_EB_FIRMWARE_PROCESSOR,
                      GCHD_EB_FIRMWARE_PROCESSOR);
   if (r)
    return r;
   break;

  case GCHD_PROC_RUNNING:
   /*
    * The first 0x27f97b is an initialization milestone, not input
    * selection. Keep the boot/init state machine independent of the
    * remembered connector; input selection begins after hw_init returns.
    */
   if (first_time) {
    /*
     * The analog-input enable bit is always updated here, even for HDMI.
     * Keep the operation.
     */
    r = gchd_enable_analog(d);
    if (r)
     return r;

    r = gchd_transcoder_init(d);
    if (r)
     return r;

    r = gchd_scmd(d, GCHD_SCMD_INIT, GCHD_SCMD_MODE_DEFAULT, 0x0000);
    if (r)
     return r;

    r = gchd_load_encoder_firmware(d);
    if (r)
     return r;

    /*
     * configureDevice() reads these four words after encoder firmware
     * loading. They are deliberately retained even though their values
     * are not consumed by the kernel driver.
     */
    {
     u16 dummy;
     r = gchd_ctrl_read(d, GCHD_REQ_REG, GCHD_BANK_TC, 0x0010,
                        &dummy, sizeof(dummy)); if (r) return r;
     r = gchd_ctrl_read(d, GCHD_REQ_REG, GCHD_BANK_TC, 0x0012,
                        &dummy, sizeof(dummy)); if (r) return r;
     r = gchd_ctrl_read(d, GCHD_REQ_REG, GCHD_BANK_TC, 0x0014,
                        &dummy, sizeof(dummy)); if (r) return r;
     r = gchd_ctrl_read(d, GCHD_REQ_REG, GCHD_BANK_TC, 0x0016,
                        &dummy, sizeof(dummy)); if (r) return r;
    }

    first_time = false;
   } else {
    /*
     * This is the second 0x27f97b. Only now does configureDevice()
     * leave this state machine and proceed to the 0x5b phase.
     * A break here would only exit the switch, not the polling loop.
     */
    goto processor_state_ready;
   }
   break;
  }
 }
 if (tries == 2000) {
  dev_err(&d->intf->dev,
          "processor-state timeout after encoder init: last magic=0x%06x first_time=%u\n",
          magic, first_time);
  return -ETIMEDOUT;
 }

processor_state_ready:
 dev_info(&d->intf->dev, "device idle, processor state 0x%06x\n", magic);
 return 0;
}

static int gchd_hw_shutdown_steps(struct gchd *d, const char **step)
{
 int r;
 u16 state;
 u8 v;

 if (!d->udev)
  return 0;

 /*
  * Stop transport only when the firmware is
  * actually in START/NULL, and do not run the teardown sequence twice when
  * the processor is already reset or still in boot state.
  */
 *step = "read state";
 r = gchd_req_read16(d, GCHD_BANK_SYS, GCHD_REG_SCMD_STATE, &state);
 if (r)
  return r;
 state &= GCHD_STATE_MASK;

 if (state == GCHD_STATE_START || state == GCHD_STATE_NULL) {
  *step = "stream stop (NULL, STOP)";
  r = gchd_stream_stop(d);
  if (r)
   return r;
  r = gchd_req_read16(d, GCHD_BANK_SYS, GCHD_REG_SCMD_STATE, &state);
  if (r)
   return r;
  state &= GCHD_STATE_MASK;
 }

 if (state == GCHD_STATE_SHUTDOWN || state == GCHD_STATE_BOOT || state == GCHD_STATE_FW_LOADED) {
  d->hw_enable_state = 0;
  d->hw_enable_register = 0;
  d->saved_enable_state = 0;
  return 0;
 }

 *step = "decoder and enable writes";
 /* Pre-shutdown decoder sequence. */
 GCHD_DEC_W(d, 0x06, 0x86);
 GCHD_DEC_R(d, 0xf8, v);
 GCHD_DEC_W(d, 0x03, 0x2f);

 r = gchd_reg_write16(d, GCHD_REG_BANKSEL, 0x0000);
 if (r) return r;
 r = gchd_reg_read16(d, GCHD_REG_ENABLE_SHADOW, &d->hw_enable_state);
 if (r) return r;

 /* Read the state before disabling output. */
 r = gchd_req_read16(d, GCHD_BANK_SYS, GCHD_REG_SCMD_STATE, &state);
 if (r) return r;
 *step = "output disable";
 r = gchd_transcoder_output_enable(d, false);
 if (r) return r;

 *step = "INIT";
 r = gchd_scmd(d, GCHD_SCMD_INIT, GCHD_SCMD_INIT_NO_RELOAD, 0x0000);
 if (r) return r;

 /* Clear the state mask before switching the processor off. */
 r = gchd_reg_write16(d, GCHD_REG_ENABLE_SHADOW, 0);
 if (r) return r;
 d->hw_enable_state = 0;
 d->saved_enable_state = 0;

 *step = "processor off";
 r = gchd_do_enable(d, GCHD_EB_FIRMWARE_PROCESSOR, 0);
 if (r) return r;

 /*
  * The normal capture-close path may already have returned the device to
  * IDLE (0x11). Sending GCHD_SCMD_IDLE again from IDLE is not idempotent on
  * HDNew: it can wait forever for a transition that will never occur.
  * Re-read the state after processor shutdown and only request IDLE when
  * the hardware is not already there. RESET is reserved for device close,
  * never for connector switching.
  */
 r = gchd_req_read16(d, GCHD_BANK_SYS, GCHD_REG_SCMD_STATE, &state);
 if (r) return r;
 state &= GCHD_STATE_MASK;
 if (state != GCHD_STATE_IDLE) {
  *step = "IDLE";
  r = gchd_state_cmd(d, GCHD_SCMD_IDLE, 0, 0, GCHD_STATE_IDLE);
  if (r) return r;
 }

 /* RESET is the final close-time transition to a known boot state. */
 *step = "RESET";
 r = gchd_state_cmd(d, GCHD_SCMD_RESET, 1, 0, GCHD_STATE_SHUTDOWN);
 if (r == -EIO) {
  /*
   * From IDLE (no capture started in the session) the device can be in
   * STOP when RESET completes. From STOP, IDLE followed by RESET reaches
   * the shutdown state.
   */
  r = gchd_req_read16(d, GCHD_BANK_SYS, GCHD_REG_SCMD_STATE, &state);
  if (r) return r;
  state &= GCHD_STATE_MASK;
  if (state != GCHD_STATE_STOP)
   return -EIO;
  dev_info(&d->intf->dev, "shutdown: device in STOP after RESET; IDLE and RESET again\n");
  *step = "IDLE (after STOP)";
  r = gchd_state_cmd(d, GCHD_SCMD_IDLE, 0, 0, GCHD_STATE_IDLE);
  if (r) return r;
  *step = "RESET (after STOP)";
  r = gchd_state_cmd(d, GCHD_SCMD_RESET, 1, 0, GCHD_STATE_SHUTDOWN);
 }
 if (r) return r;

 d->hw_enable_state = 0;
 d->hw_enable_register = 0;
 d->saved_enable_state = 0;
 return 0;
}

/* Shutdown with the failing step and the device state logged. */
int gchd_hw_shutdown(struct gchd *d)
{
 const char *step = "start";
 u16 before = 0xffff, after = 0xffff;
 int r;

 if (!d->udev)
  return 0;
 gchd_req_read16(d, GCHD_BANK_SYS, GCHD_REG_SCMD_STATE, &before);
 r = gchd_hw_shutdown_steps(d, &step);
 gchd_req_read16(d, GCHD_BANK_SYS, GCHD_REG_SCMD_STATE, &after);
 if (r)
  dev_warn(&d->intf->dev,
           "hardware shutdown failed at step '%s': %d (state 0x%04x before, 0x%04x now)\n",
           step, r, before, after);
 else
  dev_info(&d->intf->dev,
           "hardware shutdown done (state 0x%04x before, 0x%04x now)\n",
           before, after);
 return r;
}
