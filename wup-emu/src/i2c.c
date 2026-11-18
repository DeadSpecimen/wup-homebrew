/* I2C controllers at 0xF0005800.
 *   0xF0005800 +0x00 pending (bit per channel), +0x04 irq enable, +0x08 write-1-to-clear
 *   Renesas-style channels 2,3,4 at 0xF0005800 + ch*0x400:
 *     +0x00 ctrl, +0x04 data, +0x08 command (b0 STOP, b1 START, b2 ACK, b5 RX next byte),
 *     +0x18 status (b0 stopped, b2 ACK received, b3 transmitter), +0x1c error, +0x20 bus state
 *   Samsung-style channels 0,1 at 0xF0005900 + ch*0x400 (not modelled in detail yet).
 * All transfers complete instantly; IRQ 15 signals the CPU. */
#include "dev.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void irq_raise(int n);
int i2c_log = -1;

/* ---- generic I2C slave devices: an 8-bit register pointer + 256 registers ---- */
typedef struct {
    uint8_t pages[64][256];  /* paged devices (TI codecs) use register 0 as page select */
    uint8_t ptr, page;
    bool ptr_set;
    bool paged;
} I2cDev;
static I2cDev devs[128];
#define REGS(a) (devs[a].pages[devs[a].paged ? (devs[a].page & 63) : 0])

static void dev_start(int addr) { devs[addr].ptr_set = false; }
static void dev_write(int addr, uint8_t b) {
    I2cDev *d = &devs[addr];
    if (!d->ptr_set) { d->ptr = b; d->ptr_set = true; return; }
    if (d->paged && d->ptr == 0) d->page = b;
    REGS(addr)[d->ptr++] = b;
}
static uint8_t dev_read(int addr) {
    I2cDev *d = &devs[addr];
    return REGS(addr)[d->ptr++];
}

/* ---- controller ---- */
typedef struct {
    uint32_t regs[0x40 / 4];
    int state;          /* 0 idle, 1 expect address, 2 tx, 3 rx */
    int addr;
    uint8_t log[64]; int nlog; bool rx;
} Chan;
static Chan ch[5];
static uint32_t pending, enable;

static void ch_irq(int c) {
    pending |= 1u << c;
    if (enable & (1u << c)) irq_raise(15);
}

static void log_flush(int c) {
    Chan *h = &ch[c];
    if (i2c_log < 0) i2c_log = getenv("DRC_DEBUG") != NULL || getenv("I2C_LOG") != NULL;
    if (!i2c_log || h->addr < 0) return;
    fprintf(stderr, "[i2c%d] %s 0x%02x:", c, h->rx ? "read " : "write", h->addr);
    for (int i = 0; i < h->nlog; i++) fprintf(stderr, " %02x", h->log[i]);
    if (getenv("I2C_LOG_PC")) {                                  /* who is talking: pc, lr, stack words */
        fprintf(stderr, "   pc %08x lr %08x sp:", cpu.r[15], cpu.r[14]);
        for (int i = 0; i < 24; i++) fprintf(stderr, " %08x", cpu_peek32(&cpu, cpu.r[13] + 4 * i));
    }
    fprintf(stderr, "\n");
    h->nlog = 0;
}

static uint32_t i2c_read(void *ctx, uint32_t off, int size) {
    if (off < 0x100) {
        switch (off) {
        case 0x00: return pending;
        case 0x04: return enable;
        }
        return 0;
    }
    int c = off >> 10;
    uint32_t r = off & 0x3ff;
    if (c > 4) return 0;
    if (c >= 2 && r < 0x40) return ch[c].regs[r / 4];
    if (c < 2 && r >= 0x100 && r < 0x140) return ch[c].regs[(r - 0x100) / 4];
    return 0;
}

static void renesas_write(int c, uint32_t r, uint32_t val) {
    Chan *h = &ch[c];
    uint32_t *R = h->regs;
    switch (r) {
    case 0x04: /* data */
        if (h->state == 1) {
            if (h->nlog && h->addr >= 0) log_flush(c);
            h->addr = (val >> 1) & 0x7f;
            h->rx = val & 1;
            if (!h->rx) dev_start(h->addr);
            h->state = h->rx ? 3 : 2;
            R[0x18 / 4] = 0x04 | (h->rx ? 0 : 0x08);   /* ACK, transmitter if writing */
            ch_irq(c);
        } else if (h->state == 2) {
            dev_write(h->addr, val);
            if (h->nlog < 64) h->log[h->nlog++] = val;
            R[0x18 / 4] = 0x0c;
            ch_irq(c);
        }
        R[1] = val;
        return;
    case 0x08: {
        uint32_t v = val;
        if (v & 0x02) {          /* START / repeated start */
            h->state = 1;
            v &= ~0x02u;
        }
        if ((v & 0x20) && h->state == 3) {   /* receive next byte */
            uint8_t b = dev_read(h->addr);
            if (h->nlog < 64) h->log[h->nlog++] = b;
            R[1] = b;
            R[0x18 / 4] = 0x04;
            v &= ~0x20u;
            ch_irq(c);
        }
        if (v & 0x01) {          /* STOP */
            log_flush(c);
            h->state = 0;
            v &= ~0x01u;
            R[0x18 / 4] = 0x01;
            ch_irq(c);
        }
        R[2] = v;
        return;
    }
    default:
        R[r / 4] = val;
    }
}

static void i2c_write(void *ctx, uint32_t off, uint32_t val, int size) {
    if (off < 0x100) {
        switch (off) {
        case 0x04: enable = val; if (pending & enable) irq_raise(15); break;
        case 0x08: pending &= ~val; break;
        }
        return;
    }
    int c = off >> 10;
    uint32_t r = off & 0x3ff;
    if (c > 4) return;
    if (c >= 2 && r < 0x40) { renesas_write(c, r, val); return; }
    if (c < 2 && r >= 0x100 && r < 0x140) {
        static int warned;
        if (!warned++ && getenv("DRC_DEBUG")) fprintf(stderr, "[i2c] samsung-type channel %d accessed (+%03x <- %08x), not modelled\n", c, r, val);
        ch[c].regs[(r - 0x100) / 4] = val;
    }
}

void i2c_init(void) {
    for (int i = 0; i < 5; i++) ch[i].addr = -1;
    /* 0x39: LCD panel driver IC (Renesas R69208 / JDI panel); device-code register 0xBF */
    static const uint8_t r69208_id[5] = { 0x01, 0x22, 0x92, 0x08, 0x00 };
    memcpy(&devs[0x39].pages[0][0xbf], r69208_id, 5);
    /* 0x21: OmniVision OV7740 camera (SCCB): PID/VER, manufacturer ID */
    devs[0x21].pages[0][0x0a] = 0x77; devs[0x21].pages[0][0x0b] = 0x42;
    devs[0x21].pages[0][0x1c] = 0x7f; devs[0x21].pages[0][0x1d] = 0xa2;
    /* 0x18: TI TLV320AIC3012 audio codec, paged */
    devs[0x18].paged = true;
    devs[0x18].pages[1][0x30] = 0x80;
    bus_register(0xf0005800, 0x1400, "i2c", NULL, i2c_read, i2c_write);
}

/* codec state for the audio output path */
uint8_t i2c_codec_reg(int page, int reg) { return devs[0x18].pages[page & 63][reg & 0xff]; }
