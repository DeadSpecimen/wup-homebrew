#!/usr/bin/env python3
"""The C library and compiler runtime everything on the pad links: picolibc and LLVM's compiler-rt builtins, built
once for this CPU (ARM926EJ-S: armv5te, no FPU) in ARM and Thumb.

    python3 tools/sdk.py            build (or check) build/sdk/{arm,thumb}/{include,lib}

Needs LLVM (clang, ld.lld, llvm-ar), and meson + ninja for picolibc (`python -m pip install meson ninja`)."""
import os, re, shutil, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import paths, deps

STAMP = 'picolibc 1.8.10, compiler-rt 19.1.7, r3'
CLANG, AR = paths.tool('clang'), paths.tool('llvm-ar')
MODES = ('arm', 'thumb')
# builtins that make no sense here: other targets, 128-bit and 80-bit floats, unwinding, executable stacks, atomics
SKIP = re.compile(r'^(apple_versioning|atomic.*|clear_cache|cpu_model|emutls|enable_execute_stack|eprintf|gcc_personality_v0|'
                  r'os_version_check|trampoline_setup|.*(tf|xf|ti|tc|xc|hf|bf)(\d|si|di|ti|sf|df|tf|xf|hf|bf)*)$')


def sysroot(mode):
    return os.path.join(paths.SDK, mode)


def flags(mode):
    """compiler flags for code on the pad"""
    return ['--target=armv5te-none-eabi', '-m' + mode, '-nostdlib', '-nostdlibinc', '-isystem', os.path.join(sysroot(mode), 'include')]


def libs(mode):
    """what every image links, in order"""
    d = os.path.join(sysroot(mode), 'lib')
    return [os.path.join(d, 'libc.a'), os.path.join(d, 'libclang_rt.builtins.a')]


def _picolibc(mode):
    try: import mesonbuild  # noqa: F401
    except ImportError: sys.exit('picolibc needs meson: python -m pip install meson ninja')
    if not shutil.which('ninja'): sys.exit('picolibc needs ninja: python -m pip install ninja (or brew install ninja)')
    bdir, cross = os.path.join(paths.SDK, 'build-' + mode), os.path.join(paths.SDK, 'cross-%s.txt' % mode)
    q = lambda x: "'%s'" % x.replace('\\', '/')
    open(cross, 'w').write("[binaries]\nc = [%s, '--target=armv5te-none-eabi', '-m%s', '-nostdlib', '--ld-path=%s']\nc_ld = 'lld'\nar = %s\nnm = %s\nstrip = %s\n"
                           "[host_machine]\nsystem = 'none'\ncpu_family = 'arm'\ncpu = 'arm926ej-s'\nendian = 'little'\n[properties]\nskip_sanity_check = true\n"
                           % (q(CLANG), mode, paths.LD.replace('\\', '/'), q(AR), q(paths.NM), q(paths.tool('llvm-strip'))))
    shutil.rmtree(bdir, ignore_errors=True)
    opts = ['-Dmultilib=false', '-Dpicocrt=false', '-Dsemihost=false', '-Dtests=false', '-Dspecsdir=none', '-Dprefix=' + sysroot(mode),
            '-Dincludedir=include', '-Dlibdir=lib', '-Doptimization=s', '-Dthread-local-storage=false', '-Dnewlib-global-errno=true',
            '-Dformat-default=integer', '-Dio-long-long=true', '-Dc_args=-ffunction-sections -fdata-sections -fno-builtin']
    for cmd in ([sys.executable, '-m', 'mesonbuild.mesonmain', 'setup', bdir, deps.DEPS['picolibc'][0], '--cross-file', cross] + opts,
                ['ninja', '-C', bdir, 'install']):
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode: sys.exit('picolibc (%s) failed:\n%s' % (mode, (r.stdout + r.stderr)[-3000:]))
    shutil.rmtree(bdir, ignore_errors=True)


def _builtins(mode):
    src = os.path.join(deps.DEPS['compiler-rt'][0], 'compiler-rt', 'lib', 'builtins')
    odir = os.path.join(paths.SDK, 'rt-' + mode); os.makedirs(odir, exist_ok=True)
    files = [os.path.join(src, f) for f in sorted(os.listdir(src)) if f.endswith('.c')
             and not SKIP.match(f[:-2])]
    # the EABI entry points the compiler calls (__aeabi_*); the VFP and ARMv6+ ones are left out
    arm = os.path.join(src, 'arm')
    files += [os.path.join(arm, f) for f in sorted(os.listdir(arm)) if f.startswith('aeabi_') and not f.startswith('aeabi_mem')]
    objs = []
    for f in files:
        o = os.path.join(odir, os.path.basename(f) + '.o')
        r = subprocess.run([CLANG] + flags(mode) + ['-marm', '-O2',   # (always ARM: Thumb-1 has no room for its assembly; callers interwork)
                            '-ffunction-sections', '-fdata-sections', '-fno-builtin', '-w', '-I', src, '-c', f, '-o', o],
                           capture_output=True, text=True)
        if r.returncode: sys.exit('compiler-rt %s (%s) failed:\n%s' % (os.path.basename(f), mode, r.stderr[-2000:]))
        objs.append(o)
    lib = os.path.join(sysroot(mode), 'lib', 'libclang_rt.builtins.a')
    if os.path.exists(lib): os.remove(lib)
    subprocess.run([AR, 'rcs', lib] + objs, check=True)
    shutil.rmtree(odir, ignore_errors=True)


def ensure():
    """build the SDK if it is not there (or is another version); returns nothing: use flags() and libs()"""
    stamp = os.path.join(paths.SDK, 'stamp')
    if os.path.exists(stamp) and open(stamp).read() == STAMP: return
    deps.fetch('picolibc', 'compiler-rt')
    os.makedirs(paths.SDK, exist_ok=True)
    for m in MODES:
        print('building the C library for the pad (%s; a minute, once)' % m, flush=True)
        shutil.rmtree(sysroot(m), ignore_errors=True)
        _picolibc(m); _builtins(m)
    open(stamp, 'w').write(STAMP)


if __name__ == '__main__':
    ensure()
    for m in MODES: print(sysroot(m), ' '.join(os.path.basename(x) for x in libs(m)))
