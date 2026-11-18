/* fsimage: put files into the pad's littlefs inside a 32 MB flash image (for the emulator, or to
 * prepare a flash). Same littlefs and configuration as libwup/src/fs.c, same flash extents.
 *   cc -O2 -I cfw/littlefs -o build/fsimage tools/fsimage.c cfw/littlefs/lfs.c cfw/littlefs/lfs_util.c
 *   build/fsimage flash.bin [--format] /apps/x.wup=local.wup /roms/y.gbc=local.gbc ...
 *   build/fsimage flash.bin --ls /apps/game_boy/saves
 * Without --format an existing filesystem is kept (it is formatted only if none mounts). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lfs.h"
static const struct { unsigned lo, hi; } ext[] = {
    /* as libwup/src/fs.c (keep both the same) */
    { 0x1100000, 0x1900000 },                         /* Lang2 */
};
#define NEXT (int)(sizeof ext / sizeof ext[0])
static unsigned char *flash;
static unsigned block_addr(lfs_block_t b) {
    for (int i = 0; i < NEXT; i++) { unsigned n = (ext[i].hi - ext[i].lo) >> 12; if (b < n) return ext[i].lo + (b << 12); b -= n; }
    return 0xffffffffu;
}
static int rd(const struct lfs_config *c, lfs_block_t b, lfs_off_t o, void *buf, lfs_size_t n) { memcpy(buf, flash + block_addr(b) + o, n); return 0; }
static int pr(const struct lfs_config *c, lfs_block_t b, lfs_off_t o, const void *buf, lfs_size_t n) {
    unsigned char *d = flash + block_addr(b) + o; const unsigned char *s = buf;
    for (lfs_size_t i = 0; i < n; i++) d[i] &= s[i];                 /* NOR: programming only clears bits */
    return 0;
}
static int er(const struct lfs_config *c, lfs_block_t b) { memset(flash + block_addr(b), 0xff, 4096); return 0; }
static int sy(const struct lfs_config *c) { return 0; }
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s flash.bin [--format] /dest=src ...\n", argv[0]); return 1; }
    FILE *f = fopen(argv[1], "rb"); if (!f) { perror(argv[1]); return 1; }
    flash = malloc(0x2000000); size_t got = fread(flash, 1, 0x2000000, f); fclose(f);
    if (got != 0x2000000) { fprintf(stderr, "%s: not a 32 MB flash image\n", argv[1]); return 1; }
    unsigned blocks = 0; for (int i = 0; i < NEXT; i++) blocks += (ext[i].hi - ext[i].lo) >> 12;
    struct lfs_config cfg = { .read = rd, .prog = pr, .erase = er, .sync = sy, .read_size = 16, .prog_size = 16,
                              .block_size = 4096, .block_count = blocks, .cache_size = 256, .lookahead_size = 64, .block_cycles = 500 };
    lfs_t fs; int fmt = argc > 2 && !strcmp(argv[2], "--format");
    if (fmt || lfs_mount(&fs, &cfg)) {
        if (lfs_format(&fs, &cfg) || lfs_mount(&fs, &cfg)) { fprintf(stderr, "format failed\n"); return 1; }
        printf("formatted (%u blocks)\n", blocks);
    }
    lfs_mkdir(&fs, "/apps"); lfs_mkdir(&fs, "/save"); lfs_mkdir(&fs, "/roms");
    if (argc == 4 && !strcmp(argv[2], "--ls")) {                     /* list a folder */
        lfs_dir_t d; struct lfs_info inf;
        if (lfs_dir_open(&fs, &d, argv[3])) { fprintf(stderr, "%s: no such folder\n", argv[3]); return 1; }
        while (lfs_dir_read(&fs, &d, &inf) > 0) printf("%s%s  %u\n", inf.name, inf.type == LFS_TYPE_DIR ? "/" : "", (unsigned)inf.size);
        return 0;
    }
    for (int i = 2 + fmt; i < argc; i++) {
        char *eq = strchr(argv[i], '='); if (!eq) { fprintf(stderr, "%s: want /dest=src\n", argv[i]); return 1; }
        *eq = 0;
        FILE *s = fopen(eq + 1, "rb"); if (!s) { perror(eq + 1); return 1; }
        fseek(s, 0, SEEK_END); long n = ftell(s); fseek(s, 0, SEEK_SET);
        char *d = malloc(n); fread(d, 1, n, s); fclose(s);
        for (char *q = argv[i] + 1; (q = strchr(q, '/')); q++) { *q = 0; lfs_mkdir(&fs, argv[i]); *q = '/'; }   /* parents */
        lfs_file_t lf;
        if (lfs_file_open(&fs, &lf, argv[i], LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) || lfs_file_write(&fs, &lf, d, n) != n || lfs_file_close(&fs, &lf)) {
            fprintf(stderr, "%s: write failed\n", argv[i]); return 1;
        }
        printf("%s: %ld bytes\n", argv[i], n); free(d);
    }
    lfs_unmount(&fs);
    f = fopen(argv[1], "wb"); fwrite(flash, 1, 0x2000000, f); fclose(f);
    return 0;
}
