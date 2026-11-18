#!/usr/bin/env python3
"""wuplink: send an app to the GamePad's the Homebrew Menu over Wi-Fi (press Y on the pad first).

    python3 tools/wuplink.py APP.wup [-a IP]
    the pad keeps it in /apps/<name>/<name>.wup and starts it; when it ends, the menu opens at home.
Only .wup files (tools/wupbuild.py makes them); the pad refuses anything else.

Protocol v1 (menu/wuplink.c is the receiver):
  discovery  UDP 28801: "wupboot" broadcast -> the pad answers "bootwup"
  transfer   TCP 28800: "WUPL" u32 version u32 size u32 crc32 u32 namelen name -> u32 status
             (0 go, 1 too big, 2 bad header, 3 busy, 5 no room), then the image -> u32 status (0 ok, 4 CRC, 5 no room)."""
import os, socket, struct, sys, time, zlib
TCP_PORT, UDP_PORT, MAX = 28800, 28801, 0x100000     # over 192 KB the pad writes it to flash as it arrives


def discover(timeout=3.0):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    s.settimeout(0.5); t0 = time.time()
    while time.time() - t0 < timeout:
        s.sendto(b'wupboot', ('255.255.255.255', UDP_PORT))
        try:
            d, addr = s.recvfrom(64)
            if d.startswith(b'bootwup'): return addr[0]
        except socket.timeout: pass
    return None


def recv_status(c):
    d = b''
    while len(d) < 4:
        x = c.recv(4 - len(d))
        if not x: raise ConnectionError('the pad closed the connection')
        d += x
    return struct.unpack('<I', d)[0]


def main():
    a = sys.argv[1:]
    if not a: sys.exit(__doc__)
    path = a[0]
    ip = a[a.index('-a') + 1] if '-a' in a else None
    img, name = open(path, 'rb').read(), os.path.basename(path)
    if img[:4] != b'WUP0': sys.exit('%s is not a .wup file (build apps with tools/wupbuild.py)' % name)
    if len(img) > MAX: sys.exit('%s is %d bytes; wuplink takes up to %d' % (name, len(img), MAX))
    if not ip:
        print('looking for the GamePad (press Y on it)...', flush=True)
        ip = discover()
        if not ip: sys.exit('no GamePad answered; is wuplink open on it (Y)? or pass -a IP')
    print('sending %s (%d bytes) to %s' % (name, len(img), ip), flush=True)
    c = socket.create_connection((ip, TCP_PORT), timeout=10)
    NO_ROOM = 'not enough space on the pad (an app needs its size free, even to replace itself): delete something first, over file sharing'
    nb = name.encode()[:63]
    version = 1
    c.sendall(b'WUPL' + struct.pack('<IIII', version, len(img), zlib.crc32(img) & 0xffffffff, len(nb)) + nb)
    r = recv_status(c)
    if r: sys.exit('the pad refused: %s' % {1: 'too big (a menu older than large-app support takes up to 192 KB)', 2: 'bad header', 3: 'busy', 5: NO_ROOM}.get(r, r))
    t0 = time.time()
    for off in range(0, len(img), 4096):
        c.sendall(img[off:off + 4096])
        print('\r  %3d%%  %d / %d bytes' % ((off + 4096 >= len(img)) * 100 or (off * 100 // len(img)), min(off + 4096, len(img)), len(img)), end='', flush=True)
    r = recv_status(c)
    dt = time.time() - t0
    print('\n%s in %.2f s (%.0f KB/s)' % ('received OK, the pad is starting it' if r == 0 else 'FAILED: %s' % {4: 'CRC mismatch', 5: NO_ROOM}.get(r, r),
                                          dt, len(img) / 1024 / max(dt, 1e-3)))
    c.close()


if __name__ == '__main__':
    main()
