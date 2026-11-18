#!/usr/bin/env python3
"""LVGL for the pad: built once into build/lvgl/liblvgl.a from config/lv_conf.h (rebuilt when that, the fonts
or LVGL's version change). Thumb for size; the software renderer in ARM, where the time goes.

    python3 tools/lvgl.py        build (or check)"""
import hashlib, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import paths, deps, sdk

DIR = deps.DEPS['lvgl'][0]
OUT = os.path.join(paths.BUILD, 'lvgl')
LIB = os.path.join(OUT, 'liblvgl.a')
FONTS = paths.FONTS


def flags():
    """for code that includes <lvgl.h>"""
    return ['-DLV_CONF_INCLUDE_SIMPLE', '-I', paths.CONFIG, '-I', paths.INCLUDE, '-I', DIR]


def ensure():
    deps.fetch('lvgl'); sdk.ensure()
    srcs = sorted(os.path.join(d, f) for d, _, fs in os.walk(os.path.join(DIR, 'src')) for f in fs if f.endswith('.c'))
    srcs += sorted(os.path.join(FONTS, f) for f in os.listdir(FONTS) if f.endswith('.c'))
    h = hashlib.sha256((deps.DEPS['lvgl'][2] + sdk.STAMP + 'r1').encode())
    for f in [os.path.join(paths.CONFIG, 'lv_conf.h')] + srcs[-8:]: h.update(open(f, 'rb').read())
    stamp = os.path.join(OUT, 'stamp')
    if os.path.exists(LIB) and os.path.exists(stamp) and open(stamp).read() == h.hexdigest(): return
    print('building LVGL for the pad (half a minute, once)', flush=True)
    odir = os.path.join(OUT, 'obj'); os.makedirs(odir, exist_ok=True)
    res = subprocess.run([paths.tool('clang'), '-print-resource-dir'], capture_output=True, text=True).stdout.strip()

    def cc(s):
        fast = os.sep + os.path.join('draw', 'sw') + os.sep in s
        o = os.path.join(odir, os.path.relpath(s, paths.ROOT).replace(os.sep, '_') + '.o')
        r = subprocess.run([paths.tool('clang')] + sdk.flags('thumb') + (['-marm', '-O2'] if fast else ['-Os']) +
                           ['-isystem', res + '/include', '-ffunction-sections', '-fdata-sections', '-fno-common', '-w',
                            '-ffile-prefix-map=%s=.' % paths.ROOT] + flags() + ['-c', s, '-o', o], capture_output=True, text=True)
        if r.returncode: sys.exit('LVGL: %s failed:\n%s' % (s, r.stderr[-3000:]))
        return o
    with ThreadPoolExecutor(os.cpu_count() or 4) as ex: objs = list(ex.map(cc, srcs))
    if os.path.exists(LIB): os.remove(LIB)
    subprocess.run([paths.tool('llvm-ar'), 'rcs', LIB] + objs, check=True)
    open(stamp, 'w').write(h.hexdigest())


if __name__ == '__main__':
    ensure(); print(LIB, os.path.getsize(LIB) >> 10, 'KB')
