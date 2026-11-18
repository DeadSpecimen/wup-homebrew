/* firmware.h: tiny runtime for code running inside the stock GamePad firmware (LVC rev 7677,
 * NR4_12_20140805_release): the loader and the Homebrew Menu.
 *
 * Every address below was derived from that firmware in the emulator (see docs/ARCHITECTURE.md);
 * other firmware revisions will have different addresses.
 *
 * Context: we run in the app manager's task, after the firmware has brought up the LCD controller,
 * the panel (DCS display-on sent over I2C) and the UIC input polling. Other firmware tasks keep
 * running, so long loops must call fw_sleep() to let them (input polling in particular) run. */
#pragma once
#include <stdint.h>

typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;

/* ---- firmware entry points (thumb, hence +1) */
typedef int (*flash_read_fn)(u32 handle, u32 flash_addr, void *dst, u32 len, u32 *done);
#define FW_FLASH_READ   ((flash_read_fn)0x00009141)      /* SPI flash read: 0 = ok (takes a semaphore: task context) */
#define FW_FLASH_HANDLE (*(volatile u32 *)0x000bb5f4)    /* handle used by the firmware's own INDX reads */
typedef void (*dly_tsk_fn)(int ms);
#define FW_DLY_TSK      ((dly_tsk_fn)0x00089ae4)         /* RTOS task delay in ms (callers pass 1..200); ARM code (the kernel is called with blx), so no thumb bit */

/* ---- input: the firmware keeps the latest UIC input report (UIC command 0x07) in RAM */
#define FW_BUTTONS      ((volatile u8 *)0x000bdab0)      /* report bytes 2..3 */
#define FW_REPORT       ((volatile u8 *)0x00178d38)      /* full 128-byte report (latest) */
enum {  /* bit = 8 * (byte - 2 ^ 1) ...: here as a 16-bit word, byte 2 high */
    BTN_A = 0x8000, BTN_B = 0x4000, BTN_X = 0x2000, BTN_Y = 0x1000,
    BTN_LEFT = 0x0800, BTN_RIGHT = 0x0400, BTN_UP = 0x0200, BTN_DOWN = 0x0100,
    BTN_ZL = 0x0080, BTN_ZR = 0x0040, BTN_L = 0x0020, BTN_R = 0x0010,
    BTN_PLUS = 0x0008, BTN_MINUS = 0x0004, BTN_HOME = 0x0002, BTN_SYNC = 0x0001,
};
static inline u16 buttons(void) { return (u16)(FW_BUTTONS[0] << 8 | FW_BUTTONS[1]); }
static inline int16_t report_s16(int off) { return (int16_t)(FW_REPORT[off] | FW_REPORT[off + 1] << 8); }

static inline void fw_sleep(int ms) { FW_DLY_TSK(ms); }
/* The flash driver DMAs straight into the destination, and the DMA setup (LVC 0x7cd0) only accepts
   0x178000..0x7ffffff: lower RAM (stacks, firmware data) is refused and the read then never completes.
   So small reads into such memory go through a bounce buffer in our own image (0x34xxxx). */
#define DMA_OK(p) ((u32)(p) >= 0x178000u)
static inline int flash_read(u32 addr, void *dst, u32 len) {
    static u8 bounce[512] __attribute__((aligned(32)));
    u32 n = 0;
    if (DMA_OK(dst)) return FW_FLASH_READ(FW_FLASH_HANDLE, addr, dst, len, &n);
    u8 *d = dst;
    while (len) {
        u32 k = len < sizeof bounce ? len : sizeof bounce;
        int r = FW_FLASH_READ(FW_FLASH_HANDLE, addr, bounce, k, &n);
        if (r) return r;
        for (u32 i = 0; i < k; i++) d[i] = bounce[i];
        d += k; addr += k; len -= k;
    }
    return 0;
}

/* ---- debug output on UART1 (the battery-cover header, 115200 8N1), set up like wupbootdump/wuptool.
   The stock firmware does not use UART1. Every wait has a timeout, so nothing can hang on it. */
#define REG32(a) (*(volatile u32 *)(a))
static inline void dbg_init(void) {
    REG32(0xf00050d4) = 0x8001; REG32(0xf00050d8) = 0x0801;           /* pins: UART1 TX / RX */
    REG32(0xf0000058) |= 1u << 8;                                      /* reset UART1 */
    REG32(0xf0000044) = 8 - 1;                                         /* clock: 32 MHz / 8 */
    REG32(0xf0000058) &= ~(1u << 8);
    REG32(0xf0004c70) = 1;                                             /* enable */
    REG32(0xf0004c64) = 1; REG32(0xf0004c68) = 288; REG32(0xf0004c6c) = 625;   /* 115200 baud */
    REG32(0xf0004c50) = 0; REG32(0xf0004c54) = 3; REG32(0xf0004c58) = 0;      /* 8N1 */
    REG32(0xf0004c48) = 0;                                             /* no UART interrupts */
}
static inline void dbg_putc(char c) {
    for (int i = 0; i < 100000 && !(REG32(0xf0004c5c) & 0x20); i++) ;
    REG32(0xf0004c44) = (u8)c;
}
static inline void dbg(const char *s) { while (*s) { if (*s == '\n') dbg_putc('\r'); dbg_putc(*s++); } }
static inline void dbg_hex(u32 v) { for (int i = 28; i >= 0; i -= 4) { int d = v >> i & 15; dbg_putc(d < 10 ? '0' + d : 'a' + d - 10); } }

static inline void hex32(char *out, u32 v) { for (int i = 0; i < 8; i++) { int d = v >> (28 - 4 * i) & 15; out[i] = d < 10 ? '0' + d : 'a' + d - 10; } out[8] = 0; }
static inline void dec(char *out, int v) {
    char t[12]; int n = 0, neg = v < 0; unsigned u = neg ? -v : v;
    do { t[n++] = '0' + u % 10; u /= 10; } while (u);
    if (neg) *out++ = '-'; while (n) *out++ = t[--n]; *out = 0;
}

/* freestanding helpers the compiler may emit calls to (each image includes this header once) */
#include <stddef.h>
void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);
#ifndef CFWLIB_NO_AEABI   /* (an image that links its own, e.g. with libwup's networking, defines this) */
/* ARMv5 has no divide instruction: EABI division helpers (quotient in r0, remainder in r1) */
__attribute__((used)) unsigned long long __aeabi_uidivmod(unsigned n, unsigned d) {
    unsigned q = 0, r = 0;
    for (int i = 31; i >= 0; i--) { r = r << 1 | (n >> i & 1); if (r >= d) { r -= d; q |= 1u << i; } }
    return (unsigned long long)r << 32 | q;
}
__attribute__((used)) unsigned __aeabi_uidiv(unsigned n, unsigned d) { return (unsigned)__aeabi_uidivmod(n, d); }
__attribute__((used)) long long __aeabi_idivmod(int n, int d) {
    unsigned long long qr = __aeabi_uidivmod(n < 0 ? -n : n, d < 0 ? -d : d);
    int q = (int)(unsigned)qr, r = (int)(qr >> 32);
    if ((n < 0) != (d < 0)) q = -q;
    if (n < 0) r = -r;
    return (long long)(unsigned)r << 32 | (unsigned)q;
}
__attribute__((used)) int __aeabi_idiv(int n, int d) { return (int)__aeabi_idivmod(n, d); }
#endif

/* ---- the app image's header (libwup/include/wupfmt.h): linked at HB_RAM */
#ifndef HB_MAGIC
#define HB_MAGIC 0x57524248u   /* "HBRW" */
#endif
typedef struct { u32 magic, entry, size; char name[32]; } hb_header;
#define HB_RAM         0x00350000u
#define HB_MAX         0x00030000u   /* 0x350000..0x380000 is unused by the stock firmware in every boot mode */
