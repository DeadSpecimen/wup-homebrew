/* Audio output (0xF0005400): DMA ring buffer feeding the I2S codec.
 *   +0x00 control (bit0 enable)          +0x08 ring start    +0x0C ring end
 *   +0x10 write pointer (software)       +0x14 read pointer (hardware)
 *   +0x24 low-water threshold (8-byte units)                 +0x28 queued data (8-byte units)
 *   +0x2C interrupt control              +0x30 interrupt status (bit3: below threshold)
 * Samples: 16-bit signed little endian stereo at 48 kHz, or 24 kHz when control bit 22 is set. Consumed frames go to a host
 * ring buffer read by the SDL audio callback. */
#include "dev.h"
#include "audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void irq_raise(int n);
#define AUDIO_IRQ 23
#define CPU_HZ 108000000.0

static uint32_t r[0x40];
static uint32_t rp;               /* hardware read pointer */
static double frac;
static bool low_latched;

/* host ring buffer (int16 stereo frames) */
#define HOST_FRAMES 16384
static int16_t host[HOST_FRAMES * 2];
static volatile unsigned host_w, host_r;
static FILE *wav; static uint32_t wav_bytes;

static uint32_t queued_bytes(void) {
    uint32_t start = r[0x08 / 4], end = r[0x0c / 4], wp = r[0x10 / 4];
    if (end <= start || wp < start || wp > end) return 0;
    uint32_t size = end - start;
    if (wp == end) return size;        /* loop mode (write pointer = ring end): plays the ring endlessly,
                                          as on the real pad (hb_pong, padnet, libpad audio) */
    return (wp + size - rp) % size;
}

static uint32_t aout_read(void *ctx, uint32_t off, int size) {
    switch (off) {
    case 0x14: return rp;
    case 0x28: return queued_bytes() / 8;
    case 0x30: return r[0x30 / 4];
    case 0x34: return (r[0x20 / 4] & 0x04) ? 0x04 : 0;           /* stop acknowledged */
    }
    return r[(off & 0xff) / 4];
}

static void aout_write(void *ctx, uint32_t off, uint32_t val, int size) {
    if (off == 0x30) { r[0x30 / 4] &= ~val; return; }            /* write-1-to-clear */
    r[(off & 0xff) / 4] = val;
    if (off == 0x08) rp = val;                                     /* ring start resets the read pointer */
    if (off == 0x10) low_latched = false;
    if (off == 0x20 && !(val & 4) && (val & 1)) low_latched = false; /* (re)start */
}

static void acap_tick(int cycles);
/* TLV320AIC3012 DAC: page 0 reg 0x40 bit3/bit2 = left/right mute, regs 0x41/0x42 = left/right
 * digital volume (signed, 0.5 dB steps). The firmware programs these from the volume slider. */
uint8_t i2c_codec_reg(int page, int reg);
static float dac_gain(int reg, int mutebit) {
    static float tab[256]; static bool init;
    if (!init) { for (int i = 0; i < 256; i++) tab[i] = powf(10.0f, (int8_t)i * 0.5f / 20.0f); init = true; }
    if (i2c_codec_reg(0, 0x40) & mutebit) return 0;
    return tab[i2c_codec_reg(0, reg)];
}
static int16_t sat16(float v) { return v > 32767 ? 32767 : v < -32768 ? -32768 : (int16_t)v; }
static void push_frame(int16_t l, int16_t rr) {
    /* reg 0x40 bits 1:0: 01 = left volume follows right, 10 = right follows left */
    int follow = i2c_codec_reg(0, 0x40) & 3;
    l  = sat16(l  * dac_gain(follow == 1 ? 0x42 : 0x41, 0x08));
    rr = sat16(rr * dac_gain(follow == 2 ? 0x41 : 0x42, 0x04));
    if (wav) { int16_t s[2] = { l, rr }; fwrite(s, 4, 1, wav); wav_bytes += 4; }
    unsigned w = host_w;
    if (((w + 1) % HOST_FRAMES) == host_r) return;                  /* host buffer full: drop */
    host[w * 2] = l; host[w * 2 + 1] = rr;
    host_w = (w + 1) % HOST_FRAMES;
}

void audio_tick(int cycles) {
    acap_tick(cycles);
    if (!(r[0] & 1)) return;
    /* control bit 22 selects 24 kHz (the driver's "rate 0"); otherwise 48 kHz. The firmware picks
       24 or 48 kHz sound-effect clips to match and copies them 1:1, so the rate sets the pitch. */
    bool half = r[0] & 0x400000;
    frac += cycles * ((half ? 24000.0 : 48000.0) / CPU_HZ);
    int n = (int)frac;
    frac -= n;
    uint32_t start = r[0x08 / 4], end = r[0x0c / 4];
    static int16_t prev[2];
    for (int i = 0; i < n; i++) {
        int16_t s[2] = { 0, 0 };
        if (queued_bytes() >= 4) {
            uint8_t *p = bus_ptr(rp);
            if (p) memcpy(s, p, 4);
            rp += 4;
            if (rp >= end) rp = start;
        }
        if (half)   /* host runs at 48 kHz: insert the midpoint */
            push_frame((prev[0] + s[0]) / 2, (prev[1] + s[1]) / 2);
        push_frame(s[0], s[1]);
        prev[0] = s[0]; prev[1] = s[1];
    }
    uint32_t thr = r[0x24 / 4] * 8;
    if (queued_bytes() <= thr) {
        if (!low_latched) {
            low_latched = true;
            r[0x30 / 4] |= 0x08;
            if (r[0x2c / 4] & 0x08) irq_raise(AUDIO_IRQ);
        }
    } else low_latched = false;
}

/* SDL callback side */
void audio_fill(int16_t *out, int frames) {
    for (int i = 0; i < frames; i++) {
        if (host_r != host_w) {
            out[i * 2] = host[host_r * 2];
            out[i * 2 + 1] = host[host_r * 2 + 1];
            host_r = (host_r + 1) % HOST_FRAMES;
        } else {
            out[i * 2] = out[i * 2 + 1] = 0;
        }
    }
}
/* frames waiting for the host device (used to drop/skip when the emulator runs ahead) */
int audio_backlog(void) { return (int)((host_w + HOST_FRAMES - host_r) % HOST_FRAMES); }
void audio_discard(void) { host_r = host_w; }

static void wav_close(void) {
    if (!wav) return;
    uint32_t hdr[11] = { 0x46464952, 36 + wav_bytes, 0x45564157, 0x20746d66, 16, 0x00020001, 48000, 48000 * 4, 0x00100004, 0x61746164, wav_bytes };
    fseek(wav, 0, SEEK_SET); fwrite(hdr, 4, 11, wav); fclose(wav); wav = NULL;
}

/* ---- audio capture block (0xF0005480): microphone DMA + the ~180 Hz frame tick (IRQ 30)
 * that paces input polling and event dispatch in the firmware ---- */
static uint32_t ac[0x20];
static double ac_frac;
static uint32_t acap_read(void *ctx, uint32_t off, int size) { return ac[(off & 0x7f) / 4]; }
static void acap_write(void *ctx, uint32_t off, uint32_t val, int size) {
    if (off == 0x48) { ac[0x40 / 4] &= ~val; return; }            /* 0x54C8: status clear */
    ac[(off & 0x7f) / 4] = val;
}
static void acap_tick(int cycles) {
    ac_frac += cycles * (180.0 / CPU_HZ);
    if (ac_frac >= 1.0) { ac_frac -= 1.0; irq_raise(30); }
}

void audio_init(void) {
    bus_register(0xf0005400, 0x80, "aout", NULL, aout_read, aout_write);
    bus_register(0xf0005480, 0x80, "acap", NULL, acap_read, acap_write);
    const char *w = getenv("AUDIO_WAV");
    if (w && (wav = fopen(w, "wb"))) { uint8_t z[44] = { 0 }; fwrite(z, 1, 44, wav); atexit(wav_close); }
}
