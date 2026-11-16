/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Mailbox access helpers: the only way protocol sequences talk to the devices
 * behind the bridge. Devices and their ports are described in
 * docs/README.md ("Mailbox ports and device addresses").
 *
 *   GCHD_FE_W(d, reg, val)    write front end   0x9c  (port 0x4e)
 *   GCHD_DEC_W(d, reg, val)   write decoder     0x88  (port 0x44)
 *   GCHD_BE_W(d, reg, val)    write back end    0x98  (port 0x4c)
 *   GCHD_AUX_W(d, reg, val)   write aux device  0x94  (bridge: 94 reg val)
 *   GCHD_FE_R(d, reg, var)    read  front end   (bridge: 9D CD reg, 1 byte)
 *   GCHD_DEC_R(d, reg, var)   read  decoder     (bridge: 89 89 reg, 1 byte)
 *   GCHD_BE_R(d, reg, var)    read  back end    (bridge: 99 89 reg, 1 byte)
 *   GCHD_BRIDGE_W(d, b0, ...) raw bridge command bytes
 *   GCHD_BRIDGE_R(d, buf, n)  read n bytes of a bridge reply
 *
 * Every macro in this file RETURNS FROM THE CALLING FUNCTION with the error
 * code when the USB operation fails (RULES.md D10). The calling function
 * must return int. Reads store the byte in the lvalue `var`.
 *
 * Long fixed sequences are written as data tables of struct gchd_mail_cmd
 * using the GCHD_SEQ_* initializers and sent with gchd_mail_seq().
 */
#ifndef GCHD_MAILBOX_H
#define GCHD_MAILBOX_H

#include "gchd_protocol.h"

#define GCHD_TRY(expr) \
	do { int __gchd_r = (expr); if (__gchd_r) return __gchd_r; } while (0)

#define GCHD_MAIL_W(d, port, ...) \
	do { u8 __gchd_b[] = { __VA_ARGS__ }; \
	     GCHD_TRY(gchd_mail_write((d), (port), __gchd_b, sizeof(__gchd_b))); } while (0)

#define GCHD_FE_W(d, reg, val)	GCHD_MAIL_W(d, GCHD_PORT_FRONTEND, reg, val)
#define GCHD_DEC_W(d, reg, val)	GCHD_MAIL_W(d, GCHD_PORT_DECODER, reg, val)
#define GCHD_BE_W(d, reg, val)	GCHD_MAIL_W(d, GCHD_PORT_BACKEND, reg, val)
#define GCHD_AUX_W(d, reg, val)	GCHD_MAIL_W(d, GCHD_PORT_BRIDGE, GCHD_DEV_AUX, reg, val)
#define GCHD_BRIDGE_W(d, ...)	GCHD_MAIL_W(d, GCHD_PORT_BRIDGE, __VA_ARGS__)

#define GCHD_BRIDGE_R(d, buf, n) \
	GCHD_TRY(gchd_mail_read((d), GCHD_PORT_BRIDGE, (buf), (n)))

#define GCHD_BRIDGE_RD1(d, p0, p1, reg, var) \
	do { GCHD_BRIDGE_W(d, p0, p1, reg); GCHD_BRIDGE_R(d, &(var), 1); } while (0)

#define GCHD_FE_R(d, reg, var)	GCHD_BRIDGE_RD1(d, GCHD_RD_FE_0, GCHD_RD_FE_1, reg, var)
#define GCHD_DEC_R(d, reg, var)	GCHD_BRIDGE_RD1(d, GCHD_RD_DEC_0, GCHD_RD_DEC_1, reg, var)
#define GCHD_BE_R(d, reg, var)	GCHD_BRIDGE_RD1(d, GCHD_RD_BE_0, GCHD_RD_BE_1, reg, var)

/* Table entries for gchd_mail_seq() */
struct gchd_mail_cmd {
	u8 port;
	u8 len;
	u8 data[8];
};

#define GCHD_SEQ_FE(reg, val)	{ GCHD_PORT_FRONTEND, 2, { (reg), (val) } }
#define GCHD_SEQ_DEC(reg, val)	{ GCHD_PORT_DECODER, 2, { (reg), (val) } }
#define GCHD_SEQ_BE(reg, val)	{ GCHD_PORT_BACKEND, 2, { (reg), (val) } }
#define GCHD_SEQ_AUX(reg, val)	{ GCHD_PORT_BRIDGE, 3, { GCHD_DEV_AUX, (reg), (val) } }
#define GCHD_SEQ_BRIDGE3(b0, b1, b2) { GCHD_PORT_BRIDGE, 3, { (b0), (b1), (b2) } }

#endif /* GCHD_MAILBOX_H */
