#!/usr/bin/env python3
"""Test "console" for wup-emu: talks to the emulated GamePad over the emulator's UDP bridge
(src/net.c, NET_HOST 127.0.0.1). Everything here is derived from the pad firmware itself:

  ports (probed against the firmware's parsers):  console 50020 -> pad 50120  VSTRM (video)
                                                  console 50021 -> pad 50121  ASTRM (audio)
                                                  console 50023 <-> pad 50123 CMD
                                                  pad 50122 -> console 50022  HID input (128 B)
                                                  console 50010 <-> pad 50110 MSG

  VSTRM header (parser LVC 0x39790 / ext-header 0x39b20), 16 bytes, big endian fields:
    b0[7:4] = 0xF (sync)   b0[3:2] = packet type 0   b0[1:0]:b1 = 10-bit sequence
    b2 bit7..4 = flags (bit6 = frame start, bit4 = frame end: inferred, see below)
    b2 bit3 = timestamp/ext present   b2[2:0]:b3 = 11-bit payload length
    b4..b7 = timestamp   b8..b15 = option bytes: 0x80 (IDR), 0x82 n (fps 0=59.94 1=50 2=29.97 3=25),
                                                  0x83/0x84 (flag on/off), 0x85 n, 0x00 end
  CMD (MC_CMD parser LVC 0x1e01a): 8-byte header, LE16 fields: packet type (0 console request,
    1 pad ack, 2 pad request, 3 console response), command id (0..2), payload length, transaction id.
    Command id 1 goes to sc_wii (LVC 0x39d82): payload u16 magic 0xA55A at +0, byte +8 = mode
    (1 -> mc_sync event 2 -> app message 0x7f, which re-enables video in the ACTIVE app).
  Decoder queue acceptance (LVC 0x191b6): an idle decoder only takes a packet carrying option 0x80 and
  flag bit6, so bit6 = frame start and 0x80 = IDR are the working hypothesis.

Usage: python3 tools/console.py video.h264 [--fps 60]
"""
import socket, struct, sys, time, threading, argparse

HOST = '127.0.0.1'

def sock(port):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind((HOST, port))
    return s

def nal_units(data):
    """split an Annex-B stream into NAL units (without start codes)"""
    i, n, starts = 0, len(data), []
    while i + 3 <= n:
        if data[i] == 0 and data[i + 1] == 0 and (data[i + 2] == 1 or (data[i + 2] == 0 and i + 3 < n and data[i + 3] == 1)):
            sc = 3 if data[i + 2] == 1 else 4
            starts.append((i, i + sc)); i += sc
        else:
            i += 1
    out = []
    for k, (s, p) in enumerate(starts):
        e = starts[k + 1][0] if k + 1 < len(starts) else n
        out.append(data[p:e])
    return out

def frames(nals):
    """group slice NALs into access units (frames); returns list of (is_idr, [slice NALs])"""
    fr, cur, idr = [], [], False
    for nal in nals:
        t = nal[0] & 0x1f
        if t in (1, 5):
            first_mb_zero = nal[1] & 0x80          # ue(v) first_mb_in_slice == 0 starts with bit '1'
            if first_mb_zero and cur:
                fr.append((idr, cur)); cur = []
            if not cur: idr = (t == 5)
            cur.append(nal)
    if cur: fr.append((idr, cur))
    return fr

class Vstrm:
    def __init__(self, s, dst):
        self.s, self.dst, self.seq = s, dst, 0
    def packet(self, payload, flags, ts, idr, fps_code=0):
        opts = bytearray([0x80] if idr else []) + bytearray([0x82, fps_code])
        opts = (opts + b'\x00' * 8)[:8]
        n = len(payload)
        hdr = bytes([0xF0 | ((self.seq >> 8) & 3), self.seq & 0xff,
                     (flags & 0xf0) | 0x08 | ((n >> 8) & 7), n & 0xff]) + struct.pack('>I', ts) + bytes(opts)
        self.s.sendto(hdr + payload, self.dst)
        self.seq = (self.seq + 1) & 0x3ff

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('video')
    ap.add_argument('--fps', type=float, default=60)
    ap.add_argument('--chunk', type=int, default=1400)
    ap.add_argument('--loop', action='store_true')
    ap.add_argument('--delay-ms', type=float, default=30, help='presentation delay added to frame timestamps')
    ap.add_argument('--vmode', type=int, default=0)
    ap.add_argument('--vf4', type=lambda x: int(x, 0), default=0)
    ap.add_argument('--vf8', type=lambda x: int(x, 0), default=0)
    ap.add_argument('--vfc', type=lambda x: int(x, 0), default=0)
    ap.add_argument('--vf10', type=lambda x: int(x, 0), default=0)
    a = ap.parse_args()
    fr = frames(nal_units(open(a.video, 'rb').read()))
    print('[console] %d frames, %d IDR' % (len(fr), sum(1 for f in fr if f[0])), flush=True)

    s_vid, s_aud, s_hid, s_cmd, s_msg = sock(50020), sock(50021), sock(50022), sock(50023), sock(50010)
    wup_alive = threading.Event()
    stats = {'hid': 0, 'cmd': 0, 'msg': 0}
    def rx(s, name):
        while True:
            d, _ = s.recvfrom(4096)
            stats[name] += 1
            if name == 'hid': wup_alive.set()
            elif stats[name] <= 20: print('[console] %s from pad: %d bytes %s' % (name, len(d), d[:32].hex()), flush=True)
    for s, nm in ((s_hid, 'hid'), (s_cmd, 'cmd'), (s_msg, 'msg')):
        threading.Thread(target=rx, args=(s, nm), daemon=True).start()
    print('[console] waiting for the pad...', flush=True)
    wup_alive.wait()
    print('[console] pad is up (input arriving)', flush=True)
    # sc_wii mode 0xFF: mc_sync event 2 arg 4 -> app msg 0x7f(4) -> video re-enabled (LVC 0x635a8): lets the pad's video manager accept the stream
    pl = bytearray(0x60); struct.pack_into('<H', pl, 0, 0xA55A); pl[5] = 3; pl[8] = 0xFF
    s_cmd.sendto(struct.pack('<HHHH', 0, 1, len(pl), 1) + bytes(pl), (HOST, 50123))
    print('[console] sent CMD id 1 (sc_wii mode 0xFF); streaming video', flush=True)
    # ASTRM "video format" packet (byte0 bit2 = 1), handled by the ASTRM task (LVC 0x3516c):
    # payload P = pkt+8: u32 P+0, u32 P+4/P+0xC (pairing state 0/1) or P+8/P+0x10 (state 2), u8 P+0x14 (0..2).
    # These become the 16-byte video config (0x33566) that configures/starts drvvout (0x190c2 -> flag 0x100).
    def send_vfmt():
        P = bytearray(0x18)
        struct.pack_into('<IIIII', P, 0, int(time.time() * 1e6) & 0xffffffff, a.vf4, a.vf8, a.vfc, a.vf10)
        P[0x14] = a.vmode
        hdr = bytes([0x04, 0, (len(P) >> 8) & 0xff, len(P) & 0xff]) + struct.pack('>I', 0)
        s_aud.sendto(hdr + bytes(P), (HOST, 50121))
    send_vfmt()
    v = Vstrm(s_vid, (HOST, 50120))
    t0, ts = time.time(), 0
    while True:
        for k, (idr, slices) in enumerate(fr):
            chunks = []
            for nal in slices:
                for o in range(0, len(nal), a.chunk):
                    chunks.append(nal[o:o + a.chunk])
            # timestamps are in the console's TSF (microseconds). The emulator's fake Wi-Fi chip reports
            # host wall-clock time as TSF (event 82), so the same clock is used here.
            ts = int(time.time() * 1e6 + a.delay_ms * 1000) & 0xffffffff
            for i, c in enumerate(chunks):
                flags = (0x40 if i == 0 else 0) | (0x10 if i == len(chunks) - 1 else 0)
                v.packet(c, flags, ts, idr)
            target = t0 + (k + 1) / a.fps
            dt = target - time.time()
            if dt > 0: time.sleep(dt)
        if not a.loop: break
        t0 = time.time()
    print('[console] done; hid reports received: %d' % stats['hid'], flush=True)

main()
