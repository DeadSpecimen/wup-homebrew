/* LCD controller (0xF0009400): one 8bpp paletted layer, 854x480. */
#include "dev.h"
#include "lcd.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static Stub lcd_stub;
static uint32_t palette[256];
static uint32_t pal_idx;

static uint32_t lcd_read(void *ctx, uint32_t off, int size) {
    switch (off) {
    case 0x100: return pal_idx;
    case 0x104: return palette[pal_idx & 0xff];
    }
    return lcd_stub.regs[off / 4];
}
static void lcd_write(void *ctx, uint32_t off, uint32_t val, int size) {
    switch (off) {
    case 0x100: pal_idx = val & 0xff; return;
    case 0x104: palette[pal_idx & 0xff] = val; pal_idx = (pal_idx + 1) & 0xff; return;
    }
    lcd_stub.regs[off / 4] = val;
}

void lcd_init(void) {
    lcd_stub.name = "lcd"; lcd_stub.base = 0xf0009400; lcd_stub.size = 0x400;
    lcd_stub.regs = calloc(0x400 / 4, 4);
    for (int i = 0; i <= 32; i++) lcd_stub.regs[0x200 / 4 + i] = (uint32_t)i * 8;   /* identity, as on the pad */
    lcd_stub.regs[0x284 / 4] = 7;
    bus_register(0xf0009400, 0x400, "lcd", &lcd_stub, lcd_read, lcd_write);
}

/* gamma: 33 points (+0x200, 9 bits, output 0..256 per input step of 8), applied to the channels enabled
   in +0x284 (bit 0..2). The firmware leaves the identity curve 0, 8, .. 256 with all three enabled. */
static uint32_t gamma_px(uint32_t c) {
    uint32_t *r = lcd_stub.regs, en = r[0x284 / 4] & 7, o = c & 0xff000000;
    for (int ch = 0; ch < 3; ch++) {
        uint32_t v = c >> (16 - 8 * ch) & 0xff;
        if (en & (1u << ch)) {
            uint32_t a = r[0x200 / 4 + (v >> 3)] & 0x1ff, b = r[0x200 / 4 + (v >> 3) + 1] & 0x1ff;
            v = (a * (8 - (v & 7)) + b * (v & 7)) / 8; if (v > 255) v = 255;
        }
        o |= v << (16 - 8 * ch);
    }
    return o;
}
/* Render to ARGB8888 buffer of LCD_W x LCD_H. */
void lcd_render(uint32_t *out) {
    uint32_t *r = lcd_stub.regs;
    uint32_t w = r[0x64 / 4], h = r[0x6c / 4], stride = r[0x70 / 4], addr = r[0x74 / 4];
    if (!w || !h || w > LCD_W || h > LCD_H) { memset(out, 0, LCD_W * LCD_H * 4); return; }
    if (!stride) stride = w;
    for (uint32_t y = 0; y < LCD_H; y++) {
        for (uint32_t x = 0; x < LCD_W; x++) {
            uint32_t c = 0xff000000;
            if (y < h && x < w) {
                if ((r[0xb0 / 4] & 3) == 3) {                 /* RGB565 (stride in pixels) */
                    uint8_t *p = bus_ptr(addr + (y * stride + x) * 2);
                    if (p) { uint32_t v = p[0] | p[1] << 8, R = v >> 11, G = v >> 5 & 63, B = v & 31;
                             c = 0xff000000 | (R << 3 | R >> 2) << 16 | (G << 2 | G >> 4) << 8 | (B << 3 | B >> 2); }
                } else {
                    uint8_t *p = bus_ptr(addr + y * stride + x);
                    if (p) c = palette[*p] | 0xff000000;
                }
            }
            out[y * LCD_W + x] = r[0x284 / 4] & 7 ? gamma_px(c) : c;
        }
    }
}

/* vertical blank: IRQ 22 at ~60 Hz (the firmware waits on it for "frame done") */
void irq_raise(int n);
static double vs_frac;
void lcd_tick(int cycles) {
    vs_frac += cycles * (59.94 / 108000000.0);
    if (vs_frac >= 1.0) {
        vs_frac -= 1.0;
        /* 0x94FC: the 1 MHz system time latched at vertical blank. drvvout (LVC 0x17cac / 0x1811e)
           compares it with frame timestamps to phase-lock the display to the console ("timesync"). */
        extern uint32_t timer_us_count(void);
        lcd_stub.regs[0xfc / 4] = timer_us_count();
        irq_raise(22);
    }
}
