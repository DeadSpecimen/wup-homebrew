#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t buttons;        /* A=0x8000 B=0x4000 X=0x2000 Y=0x1000 LEFT=0x0800 RIGHT=0x0400 UP=0x0200 DOWN=0x0100
                                ZL=0x0080 ZR=0x0040 L=0x0020 R=0x0010 PLUS=0x0008 MINUS=0x0004 HOME=0x0002 SYNC=0x0001 */
    uint8_t extra_buttons;   /* L3=0x80 R3=0x40 TV=0x20 (report byte 0x50) */
    uint8_t power_status;
    uint8_t battery;
    uint8_t volume;
    uint16_t lx, ly, rx, ry; /* 12-bit, 0x800 = centre */
    bool touch_down;
    uint16_t tx, ty;         /* 12-bit raw touch coordinates */
} UicInput;

extern UicInput uic_input;
void uic_cs(bool active);
uint8_t uic_xfer(uint8_t out, bool rx);
void uic_init(void);
/* UIC-level buttons: the UIC reboots the SoC into a different app mode */
#define UIC_MODE_NORMAL 0
#define UIC_MODE_TVREMOTE 9
#define UIC_MODE_PAIRING 5
extern int uic_reboot_request;   /* -1 = none, else mode to boot */
void uic_press_special(int mode);
extern bool uic_powered_off;
