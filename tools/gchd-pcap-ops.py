#!/usr/bin/env python3
"""Decode a USBPcap pcapng of the Elgato Game Capture HD into protocol ops.

Usage: gchd-pcap-ops.py capture.pcapng [fields.h] > capture.ops.txt
Mailbox handshakes (00C0 write, B9 commit, interrupt, 0016/001C/23BE/0014)
are folded into one MAIL_W / MAIL_R line; --raw keeps every transfer.
"""
import struct, sys, re

DEV = {0x33: 'BRIDGE', 0x44: '88', 0x4c: '98', 0x4e: '9C'}
RD = {(0x89, 0x89): '88', (0x99, 0x89): '98', (0x9d, 0xcd): '9C'}
SCMD = {1: 'IDLE', 2: 'RESET', 4: 'INIT', 5: 'STATE_CHANGE'}
HANDSHAKE = {(0x0800, 0x0016), (0x0900, 0x001c), (0x0800, 0x23be), (0x0900, 0x0014)}


def load_fields(path):
    f = {}
    try:
        for m in re.finditer(r'#define GCHD_TC_(\w+)\s+0x([0-9a-f]+),\s*(\d+),\s*(\d+)', open(path).read()):
            f[(int(m.group(2), 16), int(m.group(3)), int(m.group(4)))] = m.group(1).lower()
    except OSError:
        pass
    return f


def packets(path):
    f = open(path, 'rb').read()
    off, tsres = 0, 1e-6
    while off + 12 <= len(f):
        bt, bl = struct.unpack_from('<II', f, off)
        if bt == 6:
            _, th, tl, cl, _ = struct.unpack_from('<IIIII', f, off + 8)
            yield ((th << 32) | tl) * tsres, f[off + 28:off + 28 + cl]
        off += bl


def sparam_fields(addr, value, mask, fields):
    out = []
    for half, base in ((0, addr), (16, addr + 0)):
        pass
    # split mask into contiguous runs, map back to 16-bit field addresses
    bits = [(i, (mask >> i) & 1) for i in range(32)]
    i = 0
    while i < 32:
        if (mask >> i) & 1:
            j = i
            while j < 32 and (mask >> j) & 1:
                j += 1
            width = j - i
            if i >= 16:
                a, lsb = addr, i - 16
            else:
                a, lsb = addr + 2, i
            v = (value >> i) & ((1 << width) - 1)
            name = fields.get((a, lsb, width), '%04x/%d/%d' % (a, lsb, width))
            out.append('%s=%d' % (name, v))
            i = j
        else:
            i += 1
    return ' '.join(out)


def main():
    path = sys.argv[1]
    raw = '--raw' in sys.argv
    fields = load_fields(next((a for a in sys.argv[2:] if a.endswith('.h')), ''))
    pend = {}          # irp -> (t, setup, outdata)
    t0 = None
    mail_payload = None
    lines = []
    bulk = {}          # ep -> [count, bytes, first_t]
    def emit(t, s):
        lines.append('%9.4f  %s' % (t - t0, s))
    def flush_bulk(t):
        for ep, (n, b, ft) in sorted(bulk.items()):
            emit(ft, 'BULK_IN ep=%02x completions=%d bytes=%d (until %.4f)' % (ep, n, b, t - t0))
        bulk.clear()
    for t, d in packets(path):
        if t0 is None:
            t0 = t
        if len(d) < 27:
            continue
        hl, irp, st, fn, info, bus, dev, ep, tr, dl = struct.unpack_from('<HQIHBHHBBI', d, 0)
        payload = d[hl:]
        if tr == 2:  # control
            stage = d[27]
            if stage == 0 and not (info & 1):
                setup = payload[:8]
                pend[irp] = (t, setup, payload[8:])
                continue
            if stage == 3 and (info & 1) and irp in pend:
                ts, setup, outd = pend.pop(irp)
                bmrt, breq, wv, wi, wl = struct.unpack('<BBHHH', setup)
                ind = payload
                if bmrt == 0xc0 and breq == 0xbc:
                    if (wv, wi) == (0x0800, 0x23be) and not raw:
                        if mail_payload and mail_payload[0] == 'R':
                            _, port, n, req = mail_payload
                            data = ind[2:2 + n]
                            dv = DEV.get(port, '%02x' % port)
                            if port == 0x33 and req and len(req) >= 3 and (req[0], req[1]) in RD:
                                emit(ts, 'READ   %s:%02X -> %s' % (RD[(req[0], req[1])], req[2], data.hex(' ').upper()))
                            else:
                                emit(ts, 'MAIL_R port=%s len=%d -> %s' % (dv, n, data.hex(' ').upper()))
                            mail_payload = ('W', None, None, None)
                        continue
                    if (wv, wi) in HANDSHAKE and not raw:
                        continue
                    emit(ts, 'REG_R  %04x:%04x -> %s' % (wv, wi, ind.hex(' ').upper()))
                elif bmrt == 0x40 and breq == 0xbc:
                    if wv == 0x0001 and len(outd) == 8:
                        v, m = struct.unpack('>II', outd)
                        emit(ts, 'SOC    %04x %s' % (wi, sparam_fields(wi, v, m, fields)))
                    elif wv == 0x0000 and wi >= 0x1000:
                        emit(ts, 'SLSI   %04x = %s' % (wi, outd.hex().upper()))
                    elif (wv, wi) == (0x0800, 0x00c0) and not raw:
                        mail_payload = ('P', outd)
                    elif (wv, wi) == (0x0900, 0x0014) and not raw:
                        continue
                    else:
                        emit(ts, 'REG_W  %04x:%04x = %s' % (wv, wi, outd.hex(' ').upper()))
                elif bmrt == 0x40 and breq == 0xb9 and len(outd) >= 4 and not raw:
                    op, rw, pp, ln = outd[:4]
                    port = pp >> 1
                    if rw == 0:
                        body = mail_payload[1][:ln] if mail_payload and mail_payload[0] == 'P' else b''
                        dv = DEV.get(port, '%02x' % port)
                        if port == 0x33 and len(body) == 3 and body[0] == 0x94:
                            emit(ts, 'WRITE  94:%02X = %02X' % (body[1], body[2]))
                            mail_payload = None
                        elif port == 0x33 and len(body) == 3 and (body[0], body[1]) in RD:
                            mail_payload = ('Q', body)      # read request, result follows
                        elif port != 0x33 and len(body) == 2:
                            emit(ts, 'WRITE  %s:%02X = %02X' % (dv, body[0], body[1]))
                            mail_payload = None
                        else:
                            emit(ts, 'MAIL_W port=%s %s' % (dv, body.hex(' ').upper()))
                            mail_payload = ('Qx', body)
                    else:
                        req = mail_payload[1] if mail_payload and mail_payload[0] in ('Q', 'Qx') else None
                        mail_payload = ('R', port, ln, req)
                elif bmrt == 0x40 and breq == 0xb8:
                    c = outd[-4:]
                    emit(ts, 'SCMD   %s mode=%02X data=%02X%02X' % (SCMD.get(c[0], '%02x' % c[0]), c[1], c[2], c[3]))
                elif bmrt in (0x40, 0xc0):
                    emit(ts, 'CTRL   %02x %02x %04x %04x out=%s in=%s' % (bmrt, breq, wv, wi, outd.hex(), ind.hex()))
            continue
        if tr == 1:  # interrupt
            if (info & 1) and payload and raw:
                emit(t, 'INT    %02x %s' % (ep, payload.hex()))
            continue
        if tr == 3:  # bulk
            if ep == 0x02:
                if not (info & 1) and payload:
                    emit(t, 'FW_OUT %d bytes' % len(payload))
            elif (info & 1) and dl:
                b = bulk.setdefault(ep, [0, 0, t])
                b[0] += 1
                b[1] += dl
            elif not (info & 1) and not raw:
                pass
    if bulk:
        flush_bulk(t)
    # collapse firmware chunks
    out, fw = [], None
    for l in lines:
        if 'FW_OUT' in l:
            n = int(l.split()[2])
            fw = [l, n, 1] if fw is None else [fw[0], fw[1] + n, fw[2] + 1]
            continue
        if fw:
            out.append(fw[0].split('FW_OUT')[0] + 'FIRMWARE %d chunks, %d bytes' % (fw[2], fw[1]))
            fw = None
        out.append(l)
    print('\n'.join(out))


main()
