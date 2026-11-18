#!/usr/bin/env python3
"""Build a small on-pad C routine, upload it through the UART monitor and call it by symbol name
(the entry address always comes from the ELF, never typed by hand).

    python3 tools/wupcall.py FILE.c SYMBOL [--base 0x1fa000] [--out 0x1f9f00] [--words N] [--arm]
The routine is called as SYMBOL(out) and its first N result words are printed."""
import os, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import paths


def build(src, base, arm=False):
    import paths; out = paths.out('wupcall')
    name = os.path.splitext(os.path.basename(src))[0]
    o, elf, binf, ld = (os.path.join(out, name + x) for x in ('.o', '.elf', '.bin', '.ld'))
    subprocess.run(['clang', '-target', 'armv5te-none-eabi', '-marm' if arm else '-mthumb', '-O2', '-ffreestanding', '-nostdlib',
                    '-c', src, '-o', o], check=True)
    open(ld, 'w').write('SECTIONS { . = 0x%x; .text : { *(.text.entry) *(.text*) *(.rodata*) *(.data*) *(.bss*) } }\n' % base)
    subprocess.run([paths.LD, '-e', '0', '-T', ld, '-o', elf, o], check=True)
    subprocess.run([paths.OBJCOPY, '-O', 'binary', elf, binf], check=True)
    syms = {}
    for l in subprocess.run([paths.NM, elf], capture_output=True, text=True).stdout.splitlines():
        p = l.split()
        if len(p) == 3: syms[p[2]] = int(p[0], 16)
        elif len(p) == 2 and p[0] == 'U': sys.exit('undefined symbol %s' % p[1])
    return open(binf, 'rb').read(), syms


def main():
    a = sys.argv[1:]
    src, sym = a[0], a[1]
    opt = lambda k, d: int(a[a.index(k) + 1], 0) if k in a else d
    base, outp, n = opt('--base', 0x1fa000), opt('--out', 0x1f9f00), opt('--words', 16)
    code, syms = build(src, base, '--arm' in a)
    if sym not in syms: sys.exit('no symbol %s' % sym)
    entry = syms[sym] | (0 if '--arm' in a else 1)
    from wupmon import WupMon
    m = WupMon(os.environ.get('WUP_PORT', '/dev/cu.usbserial-0001'))
    m.write(base, code)
    print('%s at 0x%x (%d bytes); calling 0x%x(0x%x)' % (sym, syms[sym], len(code), entry, outp))
    r, _ = m.call(entry, outp, timeout=120)
    print('return 0x%x' % r)
    print('out:', ' '.join('%x' % x for x in m.r32(outp, n)))


if __name__ == '__main__':
    main()
