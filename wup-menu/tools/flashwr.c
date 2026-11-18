/* on-pad helper for tools/menu_install.py: program one 4 KB flash sector from RAM through the stock
 * firmware's driver. args (at the address passed): flash addr (4 KB aligned), source RAM (uncached,
 * DMA-reachable), length (<= 4096), a read-back buffer (uncached, 1 KB). Erases the sector, writes, reads back and
 * compares. */
typedef unsigned u32; typedef unsigned char u8;
typedef int (*rw_fn)(u32 h, u32 addr, void *buf, u32 len, u32 *done);
typedef int (*er_fn)(u32 h, u32 first, u32 count);
#define FL_READ  ((rw_fn)0x00009141)
#define FL_WRITE ((rw_fn)0x00008fb3)
#define FL_ERASE4K ((er_fn)0x000092b9)
#define HANDLE (*(volatile u32 *)0x000bb5f4)
__attribute__((section(".text.entry"))) int flashwr(const u32 *a) {
    u32 fa = a[0], len = a[2], done = 0; const u8 *src = (const u8 *)a[1]; u8 *CHECK = (u8 *)a[3];
    if ((fa & 0xfff) || len == 0 || len > 4096) return -1;
    if (FL_ERASE4K(HANDLE, fa >> 12, 1)) return -2;
    if (FL_WRITE(HANDLE, fa, (void *)src, len, &done)) return -3;
    for (u32 o = 0; o < len; o += 1024) {
        u32 k = len - o < 1024 ? len - o : 1024;
        if (FL_READ(HANDLE, fa + o, CHECK, k, &done)) return -4;
        for (u32 i = 0; i < k; i++) if (CHECK[i] != src[o + i]) return -5;
    }
    return 0;
}
