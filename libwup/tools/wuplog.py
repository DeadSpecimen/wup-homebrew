#!/usr/bin/env python3
"""Show the debug log of apps running on the GamePad, over Wi-Fi: libwup sends every wup_log() line as a UDP
broadcast to port 28803 once an app's networking is up (no UART needed).

    python3 tools/wuplog.py [--from WUP_IP]         Ctrl-C to stop"""
import socket, sys, time
a = sys.argv[1:]
only = a[a.index('--from') + 1] if '--from' in a else None
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
try: s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
except (AttributeError, OSError): pass
s.bind(('', 28803))
print('listening on UDP 28803 for GamePad logs...', flush=True)
while True:
    d, (ip, _) = s.recvfrom(2048)
    if only and ip != only: continue
    sys.stdout.write('%s %s  %s' % (time.strftime('%H:%M:%S'), ip, d.decode('utf-8', 'replace')))
    if not d.endswith(b'\n'): sys.stdout.write('\n')
    sys.stdout.flush()
