#!/usr/bin/env python3
"""BearSSL for the pad (TLS for libwup's HTTPS client, src/https.c): built once into build/bearssl/libbearssl.a, Thumb
for size; the linker keeps only what an app uses. No operating system: time and randomness come from libwup.
Also brssl, BearSSL's command line tool, for this computer (tools/mkroots.py uses it).

    python3 tools/bearssl.py        build (or check)"""
import glob, hashlib, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import paths, deps, sdk

DIR = deps.DEPS['bearssl'][0]
OUT = os.path.join(paths.BUILD, 'bearssl')
LIB = os.path.join(OUT, 'libbearssl.a')
DEFS = ['-DBR_USE_URANDOM=0', '-DBR_USE_GETENTROPY=0', '-DBR_RDRAND=0', '-DBR_USE_UNIX_TIME=0', '-DBR_USE_WIN32_TIME=0',
        '-DBR_USE_WIN32_RAND=0', '-DBR_ARMEL_CORTEXM_GCC=0', '-DBR_64=0']


def flags():
    """for code that includes <bearssl.h>"""
    return ['-I', os.path.join(DIR, 'inc')]


def ensure():
    deps.fetch('bearssl'); sdk.ensure()
    srcs = sorted(os.path.join(d, f) for d, _, fs in os.walk(os.path.join(DIR, 'src')) for f in fs if f.endswith('.c'))
    h = hashlib.sha256((deps.DEPS['bearssl'][2] + sdk.STAMP + ' '.join(DEFS) + 'r1').encode()).hexdigest()
    stamp = os.path.join(OUT, 'stamp')
    if os.path.exists(LIB) and os.path.exists(stamp) and open(stamp).read() == h: return
    print('building BearSSL for the pad (once)', flush=True)
    odir = os.path.join(OUT, 'obj'); os.makedirs(odir, exist_ok=True)
    res = subprocess.run([paths.tool('clang'), '-print-resource-dir'], capture_output=True, text=True).stdout.strip()

    def cc(s):
        o = os.path.join(odir, os.path.relpath(s, DIR).replace(os.sep, '_') + '.o')
        r = subprocess.run([paths.tool('clang')] + sdk.flags('thumb') + ['-Os', '-isystem', res + '/include', '-ffunction-sections',
                            '-fdata-sections', '-fno-common', '-w', '-ffile-prefix-map=%s=.' % paths.ROOT, '-I', os.path.join(DIR, 'inc'),
                            '-I', os.path.join(DIR, 'src')] + DEFS + ['-c', s, '-o', o], capture_output=True, text=True)
        if r.returncode: sys.exit('BearSSL: %s failed:\n%s' % (s, r.stderr[-3000:]))
        return o
    with ThreadPoolExecutor(os.cpu_count() or 4) as ex: objs = list(ex.map(cc, srcs))
    if os.path.exists(LIB): os.remove(LIB)
    subprocess.run([paths.tool('llvm-ar'), 'rcs', LIB] + objs, check=True)
    open(stamp, 'w').write(h)


def brssl():
    """BearSSL's command line tool, built for this computer"""
    deps.fetch('bearssl')
    out = os.path.join(paths.out('host'), 'brssl')
    if not os.path.exists(out):
        srcs = glob.glob(os.path.join(DIR, 'tools', '*.c')) + glob.glob(os.path.join(DIR, 'src', '**', '*.c'), recursive=True)
        subprocess.run([paths.host_cc(), '-O1', '-w', '-I', os.path.join(DIR, 'inc'), '-I', os.path.join(DIR, 'src'),
                        '-I', os.path.join(DIR, 'tools'), '-o', out] + srcs, check=True)
    return out


if __name__ == '__main__':
    ensure(); print(LIB, os.path.getsize(LIB) >> 10, 'KB')
