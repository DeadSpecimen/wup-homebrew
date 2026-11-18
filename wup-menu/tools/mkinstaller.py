#!/usr/bin/env python3
"""The Wii U installer's payload: what installer/source/fwpatch.c adds to the console's copy of the GamePad firmware.

    python3 tools/mkinstaller.py [OUT]            -> build/installer/payload.bin (the installer's build embeds it)

The payload (little-endian): {"WUPI", 1, firmware version, LVC_ size, LVC_ CRC-32, hook site, hook address, hook room,
BUNDLE_OFF, hook length, bundle length, the hook's bl}, the hook (boot/hook.S), the bundle (tools/mkflash.py: the
loader, the menu's header sector and image). The firmware must be 7677 (NR4_12_20140805_release), the only one the hook
was made for: its version and LVC_'s CRC-32 are checked on the console before anything is written."""
import os, struct, sys
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import paths, mkflash

FW_VERSION = 0x190c0117          # drc_fw.bin's image version (the VER_ section holds the same)
LVC_CRC = 0x82d87264             # CRC-32 of LVC_ in 7677 (DRXUtil checks the same before its patches)


def build():
    stub = mkflash.build_stub()
    bundle = mkflash.build_bundle()
    bl = struct.unpack('<I', mkflash.thumb_bl(mkflash.HOOK_SITE, mkflash.STUB_ADDR))[0]
    head = struct.pack('<4s11I', b'WUPI', 1, FW_VERSION, mkflash.LVC_SIZE, LVC_CRC, mkflash.HOOK_SITE, mkflash.STUB_ADDR,
                       mkflash.STUB_MAX, mkflash.BUNDLE_OFF, len(stub), len(bundle), bl)
    return head + stub + bundle


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(paths.local_out('installer'), 'payload.bin')
    p = build()
    open(out, 'wb').write(p)
    print('%s: %d KB' % (out, len(p) >> 10))


if __name__ == '__main__':
    main()
