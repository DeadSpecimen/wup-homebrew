/* armcpu: the ARM926's caches, and small helpers the boot code, the menu and libwup all need (one copy).
 * The D-cache is write-back: anything a DMA engine, the LCD, the video decoder or the radio reads must be cleaned
 * out of it first, and code written as data needs the I-cache dropped. CP15 needs ARM state, so these are ARM
 * functions (Thumb callers reach them through blx). */
#pragma once
#include <stdint.h>

/* write the D-cache back to RAM (for a DMA engine or the display reading it) */
__attribute__((target("arm"), noinline, unused)) static void cache_clean(const void *p, uint32_t n) {
    for (uint32_t a = (uint32_t)p & ~31u, e = (uint32_t)p + n; a < e; a += 32) __asm__ volatile("mcr p15, 0, %0, c7, c10, 1" :: "r"(a) : "memory");
    __asm__ volatile("mcr p15, 0, %0, c7, c10, 4" :: "r"(0) : "memory");
}
/* write back and drop it from the D-cache (for a buffer a driver may also write by DMA) */
__attribute__((target("arm"), noinline, unused)) static void cache_flush(const void *p, uint32_t n) {
    for (uint32_t a = (uint32_t)p & ~31u, e = (uint32_t)p + n; a < e; a += 32) __asm__ volatile("mcr p15, 0, %0, c7, c14, 1" :: "r"(a) : "memory");
    __asm__ volatile("mcr p15, 0, %0, c7, c10, 4" :: "r"(0) : "memory");
}
/* code written as data (an app loaded, a patched instruction): to RAM, and no stale instructions */
__attribute__((target("arm"), noinline, unused)) static void sync_code(uint32_t addr, uint32_t n) {
    for (uint32_t a = addr & ~31u, e = addr + n; a < e; a += 32) __asm__ volatile("mcr p15, 0, %0, c7, c14, 1" :: "r"(a) : "memory");
    __asm__ volatile("mcr p15, 0, %0, c7, c10, 4\n\tmcr p15, 0, %0, c7, c5, 0" :: "r"(0) : "memory");
}
__attribute__((target("arm"), noinline, unused)) static void icache_inval(void) {
    __asm__ volatile("mcr p15, 0, %0, c7, c5, 0" :: "r"(0) : "memory");
}

/* a Thumb 'bl' at `at` to `target`, as the two halfwords in one word (first in the low half) */
static inline uint32_t thumb_bl(uint32_t at, uint32_t target) {
    uint32_t off = target - (at + 4);
    return (0xf000 | ((off >> 12) & 0x7ff)) | (uint32_t)(0xf800 | ((off >> 1) & 0x7ff)) << 16;
}

/* CRC-32 (zlib's): crc32(p, n), or in pieces: c = crc32_update(c, p, n) starting from 0 */
__attribute__((unused)) static uint32_t crc32_update(uint32_t c, const void *p, uint32_t n) {
    const uint8_t *q = p; c = ~c;
    while (n--) { c ^= *q++; for (int k = 0; k < 8; k++) c = c & 1 ? (c >> 1) ^ 0xedb88320u : c >> 1; }
    return ~c;
}
static inline uint32_t crc32(const void *p, uint32_t n) { return crc32_update(0, p, n); }
