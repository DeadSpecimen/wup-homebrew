#!/usr/bin/env python3
"""Live progress of a running gamepad_flash.py backup:  python3 tools/flash/progress.py [BACKUP_FOLDER]
(default: the newest folder in ../wup-firmware/backups, or $WUP_FIRMWARE/backups). Ctrl-C to quit; it does not touch the pad."""
import glob, json, os, sys, time
HERE = os.path.dirname(os.path.abspath(__file__))
d = sys.argv[1] if len(sys.argv) > 1 else max(glob.glob(os.path.join(os.environ.get('WUP_FIRMWARE') or os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(HERE))), 'wup-firmware'), 'backups', 'gamepad_*')), key=os.path.getmtime)
prog, part, final = (os.path.join(d, n) for n in ('progress.json', 'flash.bin.partial', 'flash.bin'))
AREAS = [(0x0, 'SPL/boot'), (0x100000, 'Product1'), (0x500000, 'Product2'), (0x900000, 'Lang1'), (0x1100000, 'Lang2'),
         (0x1900000, 'TVcode'), (0x1B00000, 'Screen'), (0x1C00000, 'Service'), (0x1FDE000, 'Insp data/logs')]
area = lambda a: [n for s, n in AREAS if s <= a][-1]
import datetime
try: t0 = datetime.datetime.strptime(os.path.basename(d.rstrip('/'))[8:], '%Y%m%d-%H%M%S').timestamp()
except ValueError: t0 = time.time()
while True:
    if os.path.exists(final) and not os.path.exists(prog):
        print('\n\033[32mFlash backup finished: %s\033[0m' % final); break
    try: done = sorted(json.load(open(prog)))
    except Exception: done = []
    n = len(done); el = time.time() - t0
    eta = el / n * (32 - n) if n else 0
    cur = (max(done) + 1) if done else 0
    bar = ''.join('█' if i in done else ('▒' if i == cur else '·') for i in range(32))
    sys.stdout.write('\r\033[K[%s] %2d/32 MiB  %3d%%  elapsed %d:%02d  left ~%d:%02d  now: 0x%07x %s' % (
        bar, n, n * 100 // 32, el // 3600, el % 3600 // 60, eta // 3600, eta % 3600 // 60, cur << 20, area(cur << 20)))
    sys.stdout.flush(); time.sleep(2)
