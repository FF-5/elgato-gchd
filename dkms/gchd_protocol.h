/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Elgato Game Capture HD USB protocol constants.
 *
 * Names follow docs/commands.md and docs/hardware.md. A constant gets a name
 * only when its role is documented; registers whose meaning is unknown stay as
 * hex literals at the call site, where the device is already named by the
 * access macro (see gchd_mailbox.h). Never add aliases that only restate a
 * number (for example REG_5B 0x5b).
 */
#ifndef GCHD_PROTOCOL_H
#define GCHD_PROTOCOL_H

/* ---------------------------------------------------------------------- */
/* USB identity and endpoints                                             */
/* ---------------------------------------------------------------------- */

#define GCHD_VID			0x0fd9
#define GCHD_PID_HD_0044		0x0044	/* original Game Capture HD */
#define GCHD_PID_HD_004E		0x004e	/* original Game Capture HD */
#define GCHD_PID_HD_0051		0x0051	/* original Game Capture HD */
#define GCHD_PID_HDNEW			0x005d	/* HD-New (MB86M01) */

#define GCHD_EP_FW_OUT			0x02	/* bulk OUT: firmware download */
#define GCHD_EP_TS_IN			0x81	/* bulk IN: MPEG transport stream */
#define GCHD_EP_INT_IN			0x83	/* interrupt IN: mailbox/SCMD notify */
#define GCHD_EP_AUX_IN			0x84	/* bulk IN: queued by Windows, unused by C++ */

#define GCHD_FW_CHUNK_HD		16384	/* firmware chunk size, original HD */
#define GCHD_FW_CHUNK_HDNEW		32768	/* firmware chunk size, HD-New */

/* ---------------------------------------------------------------------- */
/* Vendor control requests (docs/hardware.md §2)                          */
/* ---------------------------------------------------------------------- */

#define GCHD_RT_VENDOR_IN		0xc0	/* bmRequestType device-to-host */
#define GCHD_RT_VENDOR_OUT		0x40	/* bmRequestType host-to-device */

#define GCHD_REQ_REG			0xbc	/* register / transcoder access */
#define GCHD_REQ_SCMD			0xb8	/* system command */
#define GCHD_REQ_HDNEW_MAIL_CMD		0xb9	/* HD-New mailbox commit/read request */
#define GCHD_REQ_HD_MAIL		0xbd	/* original-HD mailbox, wValue = port << 8 */

/* wValue of GCHD_REQ_REG: register bank */
#define GCHD_BANK_TC			0x0000	/* transcoder 16-bit words (slsi), post-firmware probes */
#define GCHD_BANK_TC_FIELD		0x0001	/* transcoder masked 32-bit write (sparam) */
#define GCHD_BANK_SYS			0x0800	/* version, SCMD state, HD-New mailbox buffers */
#define GCHD_BANK_CTRL			0x0900	/* enable, shadow, mailbox ready, SCMD completion */

/* Bank GCHD_BANK_SYS (0x0800) */
#define GCHD_REG_HD_VERSION0		0x0010	/* original HD: "V110" ... */
#define GCHD_REG_HD_VERSION1		0x0014
#define GCHD_REG_HD_VERSION2		0x0018
#define GCHD_REG_HDNEW_SCMD_RECEIPT	0x0014	/* high byte = last SCMD received (HD-New) */
#define GCHD_REG_HDNEW_INT_STATUS	0x0016	/* read after every interrupt; "09 00" */
#define GCHD_REG_HDNEW_VERSION0		0x0094	/* HD-New: "V130" */
#define GCHD_REG_HDNEW_VERSION1		0x0098
#define GCHD_REG_HDNEW_VERSION2		0x009c
#define GCHD_REG_HDNEW_MAIL_WRITE	0x00c0	/* mailbox payload */
#define GCHD_REG_SCMD_STATE		0x2008	/* low 5 bits = SCMD state */
#define GCHD_REG_HDNEW_MAIL_READ	0x23be	/* mailbox read data (2-byte header) */

/* GCHD_REQ_HDNEW_MAIL_CMD: wIndex and 4-byte payload
 * { GCHD_HDNEW_MAIL_OP, WRITE|READ, port << 1, length } */
#define GCHD_REG_HDNEW_MAIL_CMD		0x0000
#define GCHD_HDNEW_MAIL_OP		0x09
#define GCHD_HDNEW_MAIL_WRITE		0x00
#define GCHD_HDNEW_MAIL_READ		0x01

/* Bank GCHD_BANK_CTRL (0x0900) */
#define GCHD_REG_BANKSEL		0x0000	/* written 0x0000; effect unknown */
#define GCHD_REG_ENABLE_SHADOW		0x0014	/* session-specific; restore, never hard-code */
#define GCHD_REG_ENABLE			0x0018	/* enable bits GCHD_EB_* */
#define GCHD_REG_MAIL_READY		0x001c	/* bit 0 ready, bits 8-11 cable */
#define GCHD_REG_POST_IDLE_FW		0x0070	/* written 0x0004 after idle firmware; unknown */
#define GCHD_REG_SCMD_COMPLETE		0x0074	/* bit 2 sticky completion */
#define GCHD_REG_SCMD_COMPLETE_AUX	0x01b0	/* read each round, cleared after ack; unknown */

/* Bank GCHD_BANK_TC (0x0000): read after firmware loads, values unused */
#define GCHD_REG_TC_PROBE_FIRST		0x0010
#define GCHD_REG_TC_PROBE_LAST		0x001e

#define GCHD_MAIL_READY_BIT		0x0001
/* GCHD_REG_MAIL_READY bits 8-9: cable type (00 HDMI, 10 component,
 * 11 composite); bits 10-11: cable presence. C++ folds them into a
 * detection mask: cable type in bits 0-1, "anything present" in bit 3. */
#define GCHD_MAIL_READY_DETECT(status) \
	((((status) >> 8) & 3) | ((((status) >> 10) & 3) != 0 ? BIT(3) : 0))
#define GCHD_SCMD_COMPLETE_BIT		0x0004	/* also the acknowledge value */
#define GCHD_POST_IDLE_FW_VALUE		0x0004

/* Enable bits, in GCHD_REG_ENABLE and its shadow (docs/hardware.md §3) */
#define GCHD_EB_FIRMWARE_PROCESSOR	BIT(1)
#define GCHD_EB_ANALOG_INPUT		BIT(2)
#define GCHD_EB_ENCODER_ENABLE		BIT(3)
#define GCHD_EB_ENCODER_TRIGGER		BIT(4)
#define GCHD_EB_COMPOSITE_MUX		BIT(8)
#define GCHD_EB_ANALOG_MUX		BIT(9)

/* ---------------------------------------------------------------------- */
/* SCMD (docs/hardware.md §4)                                             */
/* ---------------------------------------------------------------------- */

#define GCHD_SCMD_IDLE			0x01
#define GCHD_SCMD_RESET			0x02
#define GCHD_SCMD_INIT			0x04
#define GCHD_SCMD_STATE_CHANGE		0x05

#define GCHD_SCMD_MODE_DEFAULT		0x00	/* INIT: also loads encode firmware */
#define GCHD_SCMD_INIT_NO_RELOAD	0xa0	/* INIT without firmware load, no interrupt */
#define GCHD_SCMD_RESET_SHUTDOWN	0x01	/* RESET mode used as the last shutdown step */

/* GCHD_REG_SCMD_STATE values (low 5 bits); STOP/START/NULL are also the
 * GCHD_SCMD_STATE_CHANGE data values. */
#define GCHD_STATE_MASK			0x1f
#define GCHD_STATE_BOOT			0x00
#define GCHD_STATE_STOP			0x01
#define GCHD_STATE_START		0x02
#define GCHD_STATE_NULL			0x04
#define GCHD_STATE_FW_LOADED		0x10	/* idle firmware loaded, IDLE not sent */
#define GCHD_STATE_IDLE			0x11
#define GCHD_STATE_SHUTDOWN		0x12	/* after RESET mode 1 */

/* ---------------------------------------------------------------------- */
/* Mailbox ports and devices (docs/README.md "Mailbox ports")             */
/* ---------------------------------------------------------------------- */

#define GCHD_PORT_BRIDGE		0x33	/* on-board controller, relays reads (wire 0x66) */
#define GCHD_PORT_DECODER		0x44	/* analog video decoder, device 0x88 */
#define GCHD_PORT_BACKEND		0x4c	/* video back end / scaler, device 0x98 */
#define GCHD_PORT_FRONTEND		0x4e	/* HDMI receiver / input front end, device 0x9c */

/* Bridge read requests: <prefix0> <prefix1> <register>, then read 1 byte */
#define GCHD_RD_DEC_0			0x89	/* read device 0x88 */
#define GCHD_RD_DEC_1			0x89
#define GCHD_RD_BE_0			0x99	/* read device 0x98 */
#define GCHD_RD_BE_1			0x89
#define GCHD_RD_FE_0			0x9d	/* read device 0x9c */
#define GCHD_RD_FE_1			0xcd

/* Bridge write to auxiliary device 0x94: 94 <register> <value> */
#define GCHD_DEV_AUX			0x94

/* Bridge commands (docs/commands.md §2) */
#define GCHD_BRIDGE_PROBE_0		0xab	/* AB A9 0F A4 xx: processor/encoder probe */
#define GCHD_BRIDGE_PROBE_1		0xa9
#define GCHD_BRIDGE_PROBE_2		0x0f
#define GCHD_BRIDGE_PROBE_3		0xa4
#define GCHD_BRIDGE_PROBE_PROCESSOR	0x55	/* reply 33 44 55 / 27 F9 7B */
#define GCHD_BRIDGE_PROBE_ENCODER	0x5b	/* reply (r & F8F0F0) == 78E040 */
#define GCHD_BRIDGE_TRIGGER_0		0x43	/* 43 23 84: poll while ENCODER_TRIGGER held */
#define GCHD_BRIDGE_TRIGGER_1		0x23
#define GCHD_BRIDGE_TRIGGER_2		0x84

#define GCHD_PROC_OFF			0x334455	/* processor probe: off / not ready */
#define GCHD_PROC_RUNNING		0x27f97b	/* processor probe: running */
#define GCHD_ENC_READY_MASK		0xf8f0f0	/* encoder probe mask ... */
#define GCHD_ENC_READY			0x78e040	/* ... and expected value (78E045, 7BEC47 seen) */
#define GCHD_TRIGGER_DONE		0xf7	/* reply to 43 23 84 */

/* ---------------------------------------------------------------------- */
/* Device registers with a documented role (docs/commands.md §3)          */
/* ---------------------------------------------------------------------- */

/* Front end, device 0x9c (GCHD_PORT_FRONTEND) */
#define GCHD_FE_REG_BANK		0x00	/* bank select: 0xcc/0xce/0xcd/0x4c */
#define GCHD_FE_REG_INPUT_SEL		0x51	/* input selector */
#define GCHD_FE_SEL_HDMI		0x45
#define GCHD_FE_SEL_COMPONENT		0xed
#define GCHD_FE_SEL_ANALOG		0x44	/* composite/S-video, switched from HDMI */
#define GCHD_FE_SEL_ANALOG_ALT		0xec	/* composite/S-video, switched from analog */

/* Analog decoder, device 0x88 (GCHD_PORT_DECODER) */
#define GCHD_DEC_REG_LOCK		0xca	/* lock/format status */
#define GCHD_DEC_LOCK_480I		0x7e
#define GCHD_DEC_LOCK_240P		0x66
#define GCHD_DEC_REG_FAMILY		0xfa	/* low nibble: 6 = NTSC/60 Hz, 7 = PAL/50 Hz */

/* Back end, device 0x98 (GCHD_PORT_BACKEND) */
#define GCHD_BE_REG_RETRIGGER		0x0d	/* written 0xc8 around the 0x0e pulse */
#define GCHD_BE_RETRIGGER_VALUE		0xc8
#define GCHD_BE_REG_PULSE		0x0e	/* (status & 0x10) | 0x65, then | 0x64 */

#endif /* GCHD_PROTOCOL_H */
