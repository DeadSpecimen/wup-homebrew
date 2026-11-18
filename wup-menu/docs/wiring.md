# Wiring the GamePad for UART boot

Source: the wupbootdump README (https://github.com/Arisotura/wupbootdump). **Open it and look at its
photo before soldering.** I could only read the text; the photo shows exactly which pads these are.
This is the same wiring you used before, for wupbootdump and wuptool. Your `bootrom.bin` matches
wupbootdump's published SHA-256 (`1ba084d3…7fc1`).

## The serial header

It is under the **battery cover**: a row of 5 pads. You do not need to open the case to reach them.

| Pad | Signal | Connect to the USB-UART | Wire colour in the wupbootdump photo |
|---|---|---|---|
| 1 | RXD (into the pad) | adapter **TX** | black |
| 2 | UART BOOT | **GND** to UART-boot; leave floating for a normal boot | green |
| 3 | TXD (out of the pad) | adapter **RX** | blue |
| 4 | Vcc **2.8 V** | adapter VCCIO / reference, if it has one; **never connect the adapter's 3.3 V or 5 V here** | yellow |
| 5 | GND | adapter **GND** | red |

**Ground:** use one of these:
- TP128, inside the case;
- the ground plane next to the header, after scraping off a little solder mask;
- the black wire of a battery pigtail.

## Notes

- **Logic level.** The pad's I/O is **2.8 V**. If your adapter can use an external I/O voltage
  (VCCIO pin), feed it from pad 4, as wupbootdump suggests. If it's fixed at 3.3 V, many people use
  it as-is. A 1 kΩ resistor in series on adapter TX → pad RXD is cheap insurance.
- **If the pad won't start with the adapter attached,** the adapter is back-powering it through RXD.
  Disconnect pad RXD while powering on, as wupbootdump suggests.
- **Keep the joints low and flat** so the battery and cover still fit. A small switch or jumper from
  pad 2 to GND makes switching between UART boot and normal boot easy.
- **UART-boot mode looks dead:** no screen, no sound. That is expected. The boot ROM is waiting for a
  payload at **115200 8N1**.
- **Normal boot:** pad 2 left floating.
- **The UART boot path is in the chip's mask ROM.** It works no matter what is in flash, so a bad
  flash can always be recovered over these wires, as long as you have a good backup.

## What actually worked on this pad (2026-11-14)

- **RX/TX had to be swapped** compared with the table above for this adapter. If you get no reply at
  all, swap the two data wires first; it's harmless.
- After any failed attempt, **fully power the pad off, then hold the UART button and power on
  again**, then run the tool. The boot ROM loader only accepts one clean upload per power-on.
- A push button on the back from pad 2 to GND works well: hold it while powering on.

## Quick test before anything else

1. With pad 2 **floating**, power on: the GamePad must boot normally.
2. With pad 2 **grounded**, power on (screen stays off), then run
   `python3 tools/flash/wuptool/wuptool.py info --no-poweroff`. You should see `Flash: 20 BA 19`
   (Micron N25Q256, 32 MiB, confirmed on this pad 2026-11-14) and the boot partition.
