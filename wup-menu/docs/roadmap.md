# Roadmap

The aim: mainstream, maintained libraries wherever one exists, only the hardware glue written here; clean
enough to publish; installable from a homebrewed Wii U.

## Done (each proven in the emulator and on a real pad)

| Was hand-written | Now |
|---|---|
| two C libraries, a soft-float file | **picolibc** + LLVM **compiler-rt** (`tools/sdk.py`) |
| file I/O | picolibc stdio over POSIX calls on **littlefs** (libwup `src/sys.c`) |
| drawing, fonts and widgets of the menu | **LVGL** 9 (libwup `tools/lvgl.py`, `src/lvgl.c`) |
| a menu runtime of its own | the menu is an ordinary app; the loader runs deflated app images (**tinf**) |
| one extra task, by hand | threads on the firmware's µITRON kernel (libwup `src/kernel.c`, `docs/kernel.md`) under **`<pthread.h>`** |
| sockets on lwIP's callback API (590 lines) | **lwIP**'s own sockets API, the stack in its own threads (`net = yes`) |

Also: one name (libwup; the Homebrew Menu and its Netloader keep their names) and one layout; the menu slot widened to 380 KB.

Lessons worth keeping:

- picolibc must be built with `-fno-builtin` (else clang turns `__aeabi_memset` into a call to itself).
- libwup must not cache the Wi-Fi driver's memory at the top of the 2-3 MB section (error 165-3423), and nothing
  may be sent before the network is joined.
- The firmware leaves 4 of the kernel's 52 semaphores: libwup's semaphores are counted in software, and waiting
  threads park on one kernel event flag (a bit each). Finished tasks are deleted (`del_tsk`).
- lwIP needs `SO_REUSE` for FTP's passive ports. Emulator runs: always cap the output; a crash loop logs for ever.

## Next

1. **Install from a Wii U**: done and proven on a real console and GamePad (wup-menu-installer, on DRXUtil;
   2026-11-18). Next: apps from the console's SD card.
2. The Homebrew Menu on sockets with a maintained FTP server, if one holds up (ours is small and tested).
3. Publish: done for 1.0.0 (GPL-3.0-or-later; the installer GPL-2.0-or-later; one repository, its history checked
   for dumps and keys). The App Store's catalogue is its own repository, wup-homebrew-apps, on GitHub Pages. Next: CI.
4. App Store and Software Update (2026-11-18): over HTTPS (BearSSL) from that catalogue, in libget's format.

Not measured yet: socket throughput over Wi-Fi to another machine; LVGL touch input on the pad; the loader takes
about 3 s to inflate the menu.
