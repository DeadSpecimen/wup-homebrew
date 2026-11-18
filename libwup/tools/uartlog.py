#!/usr/bin/env python3
"""The pad's log over the serial cable (UART1, 115200 8N1): the loader, the menu and apps (printf, wup_log), from
power-on. Boot the pad normally (not UART-boot mode); the stock firmware itself prints nothing here.

    python3 tools/uartlog.py [--port /dev/cu.usbserial-XXXX] [--seconds N] [--out FILE]

With Wi-Fi up the same lines are also broadcast (tools/wuplog.py); the cable is for what happens before that."""
import sys, time


def main():
    import serial
    a = sys.argv[1:]
    opt = lambda k, d=None: a[a.index(k) + 1] if k in a else d
    s = serial.Serial(opt('--port', '/dev/cu.usbserial-0001'), 115200, timeout=0.2)
    end = time.time() + float(opt('--seconds', '1e9'))
    out = open(opt('--out'), 'a') if opt('--out') else None
    print('listening on %s; turn the pad on (hold A)' % s.port, flush=True)
    t0, line = time.time(), b''
    while time.time() < end:
        line += s.read(256)
        while b'\n' in line:
            l, line = line.split(b'\n', 1)
            txt = '%7.2f  %s' % (time.time() - t0, l.decode('latin1').rstrip())
            print(txt, flush=True)
            if out: out.write(txt + '\n'); out.flush()


if __name__ == '__main__':
    main()
