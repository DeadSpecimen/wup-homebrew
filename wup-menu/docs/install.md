# Installing on a GamePad

## From a Wii U (no soldering)

Open the **Homebrew Menu Installer** on a Wii U running Aroma and choose Install ([wup-menu-installer](../../wup-menu-installer)).
The console sends the GamePad its own firmware with the hook and the menu added; the GamePad writes it to its other
firmware bank and switches over only when it is complete. Remove (same app) puts the original firmware back.
Afterwards: hold A while turning the GamePad on.

Proven on a real Wii U (Aroma) and GamePad, 2026-11-18: both banks stock (DRXUtil's Original firmware, twice), then
Install: the pad switched to bank 2 and holding A opened the Homebrew Menu. Before that, the firmware it sends was checked
byte for byte against the UART install and booted in the emulator (wup-menu-installer `test/test_fwpatch.py`).

## Over UART (soldering)

The tools are in `tools/flash/`. Everything for backing up and flashing a real GamePad over its UART-boot pads: wuptool (the payload
the boot ROM's UART loader runs, and its host tool) plus a safety wrapper. Needed once per pad: after the
custom firmware is installed, updates go over Wi-Fi (`tools/menu_install.py`).

| File | What it is |
|---|---|
| [wiring.md](wiring.md) | where to solder, voltages, how to enter UART boot |
| `gamepad_flash.py` | **use this**: backup / install-cfw / restore / compare, with the safety checks |
| `wuptool/wuptool.py` | the original tool (interactive menu and all the other commands) |
| `wuptool/wuptool.bin` | the UART-boot payload it uploads (source in `wuptool/source/`; `make` needs arm-none-eabi-gcc) |
| `wuptool/patches/` | earlier Wi-Fi patches (standard WPA2 OUI, PTK rotation, CRC fix). Not needed for the custom firmware |
| `../wup-firmware/backups/` | where backups go (they contain your pairing key: never in a repository) |

Needs: `pip3 install pyserial`, and `brew install lld llvm` for building the CFW.

## How it works

- With the UART-boot pad grounded, the boot ROM's UART loader (ROM 0x3c4) waits on UART1 at 115200
  baud. It reads a 4-byte length, then that many bytes into RAM from address 0, then resets into
  them.
- wuptool sends 64 KiB of zeros (ARM no-ops) followed by its payload at 0x10000, so the CPU slides
  into the payload. The payload then talks a one-letter command protocol: R/W flash, EEPROM, LEDs,
  and so on.
- The whole sequence has been run against the emulator (wup-emu: `./wup-emu --uart-boot --uart-pty`, see below)
  with this exact payload and script: upload, `info`, a full 32 MiB read with a distinct upper half,
  and the install/restore flow.

## The procedure

**Plan for time:** a full 32 MiB read at 115200 baud takes about **50 minutes**; `--verify` doubles
it. Use a charged battery or the AC adapter. A power cut during a *write* is the main real risk;
during a read it's harmless.

1. **Wire it up** ([WIRING.md](WIRING.md)) and do the quick test: normal boot with pad 2 floating;
   `wuptool.py info` with pad 2 grounded.

2. **Full backup.** UART-boot mode, then run:
   ```
   python3 tools/flash/gamepad_flash.py backup        (resume after an interruption: backup --resume <folder>)
   ```
   - It must end with `BACKUP OK`. It rejects a mirrored upper half, the wrong size, or a missing
     Product1.
   - It compares the lower 16 MiB with the old `flash.bin`. Most blocks should match; the log and
     settings areas can differ.
   - **Copy the backup somewhere safe** (it is your whole pad). Optionally also save the UIC
     EEPROM: `python3 tools/flash/wuptool/wuptool.py eeprom ../wup-firmware/backups/eeprom.bin`.

3. **Install the boot menu.** UART-boot mode again (power-cycle), then run:
   ```
   python3 tools/flash/gamepad_flash.py install-cfw
   ```
   - It builds the hook, the loader and the menu (`tools/mkflash.py`), which refuses anything but
     firmware rev 7677 and checks the bytes it replaces in Product1.
   - It writes about 6 **64 KiB blocks**, each read from the pad first and changed only in its own bytes:
     the menu (0x30000 onwards), the loader (0x10000), then the two hook blocks in Product1 (0x1b0000,
     0x160000), the bl last. Everything but those two patches is in the free gap 0x10000-0xFFFFF.
     Apps go on afterwards through file sharing or wuplink.
   - Each block is read back and verified after writing.

4. **Test.**
   - Unground pad 2 and power on normally. The stock firmware must boot exactly as before.
   - Then power off, **hold A** and power on: you should get the boot menu.
   - If something looks wrong, go straight to step 5.

5. **Undo (any time).** UART-boot mode, then run:
   ```
   python3 tools/flash/gamepad_flash.py restore ../wup-firmware/backups/gamepad_YYYYMMDD-HHMMSS
   ```
   - This writes the original contents of the CFW blocks back and verifies them.
   - `compare` reports, block by block, whether the pad currently holds stock or CFW.
   - `--block ADDR` restores any other 64 KiB block from the backup.

## Rehearsal results (emulator, 2026-11-14)

All against a simulated chip whose upper 16 MiB differs from the lower half:

| Step | Result |
|---|---|
| Upload | the wuptool payload booted through the emulated boot ROM UART loader; `info` showed the emulated flash ID (the real pad reports 20 BA 19, Micron N25Q256) |
| `backup` | 32 MiB, bit-identical to the chip, upper half included (so 4-byte addressing works) |
| `install-cfw` | 4 blocks written and verified. Normal boot was byte-identical to stock; holding A showed the boot menu (tested with the first CFW design; the current image from `tools/mkflash.py` has been boot-tested in the emulator, its UART write not yet) |
| `restore` | chip bit-identical to the original again |

**Fixed in wuptool.py:** its `read` command dropped the bytes that `wait_for("OK")` had already read
ahead into `rx_buf`, so a read could wait forever for data it already had. It now starts from
`rx_buf`, as `cmd_read_data` always did. `gamepad_flash.py` uses `cmd_read_data`.

## Rules

- **Never use wuptool's `write` with a file smaller than whole 64 KiB blocks.** Its W command erases
  every 64 KiB block the range touches, then writes only your bytes; the rest of each block is lost.
  `gamepad_flash.py` always writes whole blocks from a full image.
- **Never write the old `flash.bin` back.** Its upper 16 MiB is a mirror of the lower half, so
  writing it would wipe Lang2, TVcode, Screen, Service and the inspection logs. Only fresh backups
  from step 2 are trusted.
- **Keep the UART wires** (or at least the pads accessible) until you are happy. They are the
  recovery path.

## Rehearsing against the emulator

```
../wup-emu/wup-emu --headless --uart-boot --uart-pty --flash some_image.bin --cycles 400000000000
#   prints: [uart] UART1 is on /dev/ttysNNN
python3 tools/flash/gamepad_flash.py --port /dev/ttysNNN backup
```

- `--uart-boot` grounds the virtual UART-boot pad (boot strap bit 18 clear).
- `--uart-pty` exposes UART1 as a pseudo-terminal.
- Add `--flash-writeback` to save the emulated flash's writes back into the image file, so a later
  emulator run (or `compare`) sees them.
