# Homebrew Menu Installer

A Wii U app that puts the Homebrew Menu on the GamePad, or takes it off again, through the console's own GamePad
firmware update. No soldering, no backup to restore: the GamePad writes the new firmware to its *other* firmware
bank and switches to it only once it is complete, so an update that fails leaves the GamePad as it was.

## Use

Needs a Wii U running [Aroma](https://aroma.foryour.cafe/) (or Tiramisu) and a GamePad with the console's last firmware
(it reports `190c0117`, revision 7677; any console on system 5.5.x has it).

1. Put `homebrew_menu_installer.wuhb` in `sd:/wiiu/apps/` and open it from the Wii U Menu.
2. **Install**, then **A**. Keep the GamePad charging and close to the console; it takes a couple of minutes.
3. Turn the GamePad off, then on while holding **A**: the Homebrew Menu. Without A it starts as always.

**Remove** puts the console's copy of the original firmware back the same way. Apps and settings on the GamePad stay
(in flash the firmware never reads) and come back if you install again.

## What it changes

The console keeps a copy of Nintendo's GamePad firmware (`drc_fw.bin`, in the GamePad firmware title on the
system memory). The installer reads it, checks that it is exactly the firmware the boot hook was made for (its version
and the CRC-32 of the code), and adds three things (`source/fwpatch.c`):

- one 4-byte `bl` in the firmware's boot-app selection, redirected to the hook;
- the hook (wup-menu's `boot/hook.S`, 156 bytes) over 160 bytes of unused debug strings;
- the bundle after the firmware's last section: the loader, the Homebrew Menu, and App Store and Software Update
  (the menu copies them onto the GamePad at its first start).

Everything else is Nintendo's firmware as it was, byte for byte, version included. On a normal boot the hook loads the
loader from the bundle in the bank that runs; the loader starts the menu when A is held (or autoboot is on) and
otherwise returns at once to the stock boot. The menu keeps its settings in the free flash below the first
firmware bank and its files (apps, their data) in the GamePad's spare copy of its language data, which the firmware
never reads, as on a pad installed over UART (wup-menu: `docs/architecture.md`).

## Build

```
colima start                         # (macOS: Docker)
python3 build.py                     # -> homebrew_menu_installer.wuhb
python3 test/test_fwpatch.py ../wup-firmware/flash.bin build/emu.bin   # the patcher, on the host
../wup-emu/wup-emu --flash build/emu.bin --hold a 0 4000 ...             # a Wii U-installed pad
test/preview/run.sh                  # the screens as PNGs (build/preview)
python3 art/make.py                  # the icon and splash screens (icon.png, tv-splash.png, drc-splash.png)
```

It builds next to [wup-menu](../wup-menu) and [libwup](../libwup)
(`../wup-menu`, `../libwup`; or `$WUPMENU`): the payload (the hook, the loader and the Homebrew Menu) comes from
wup-menu's `tools/mkinstaller.py`, so the installer always carries the menu of that checkout. The app is built with
devkitPro's wut, libmocha and SDL2 in the same Docker image DRXUtil uses.

## Credits and licence

Built on [DRXUtil](https://github.com/GaryOderNichts/DRXUtil) by GaryOderNichts: the firmware update sequence
(`source/drc.cpp`), the drawing code (`source/Gfx.cpp`, `source/SDL_FontCache.c`), `source/ProcUI.cpp`, the fonts
in `data/` and the firmware format. Like DRXUtil, this folder is under the **GNU GPL, version 2 or later**
([LICENSE](LICENSE)). The fonts keep their own licences: Font Awesome Free (SIL OFL 1.1), Terminus (SIL OFL 1.1).
