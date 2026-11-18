/* ftphost: the pad's FTP server (menu/ftpd.c, the same code) on the Mac, over a littlefs in RAM configured
 * like the pad's. For testing with curl, ftp, FileZilla or Finder (port 2121, data ports 50100..).
 *   cc -O1 -g -DFTP_HOST -DFTP_PORT=2121 -I tools/ftptest -I menu -I libwup/src -I third_party/littlefs -o build/ftphost \
 *      tools/ftptest/ftphost.c menu/ftpd.c third_party/littlefs/lfs.c third_party/littlefs/lfs_util.c
 *   build/ftphost [--password PW] */
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "lfs.h"
#include "ftpd.h"
void ftp_host_pump(void);

#define BLOCKS 172                                  /* 688 KB, as on the pad */
static unsigned char disk[BLOCKS * 4096];
static int rd(const struct lfs_config *c, lfs_block_t b, lfs_off_t o, void *buf, lfs_size_t n) { (void)c; memcpy(buf, disk + b * 4096 + o, n); return 0; }
static int pr(const struct lfs_config *c, lfs_block_t b, lfs_off_t o, const void *buf, lfs_size_t n) {
    (void)c; unsigned char *d = disk + b * 4096 + o; const unsigned char *s = buf;
    for (lfs_size_t i = 0; i < n; i++) d[i] &= s[i];         /* NOR: programming only clears bits */
    return 0;
}
static int er(const struct lfs_config *c, lfs_block_t b) { (void)c; memset(disk + b * 4096, 0xff, 4096); return 0; }
static int sy(const struct lfs_config *c) { (void)c; return 0; }
static struct lfs_config cfg = { .read = rd, .prog = pr, .erase = er, .sync = sy, .read_size = 16, .prog_size = 16,
                                 .block_size = 4096, .block_count = BLOCKS, .cache_size = 256, .lookahead_size = 64, .block_cycles = 500 };
lfs_t fs_lfs;

int main(int argc, char **argv) {
    for (int i = 1; i + 1 < argc; i++) if (!strcmp(argv[i], "--password")) { unsigned char h[16]; ftpd_hash(argv[i + 1], h); ftpd_set_auth(1, h); }
    signal(SIGPIPE, SIG_IGN);
    if (lfs_format(&fs_lfs, &cfg) || lfs_mount(&fs_lfs, &cfg)) { fprintf(stderr, "littlefs failed\n"); return 1; }
    lfs_mkdir(&fs_lfs, "/apps"); lfs_mkdir(&fs_lfs, "/config");
    if (ftpd_open()) { perror("listen"); return 1; }
    fprintf(stderr, "ftphost on ftp://127.0.0.1:%d\n", FTP_PORT);
    for (;;) { ftpd_step(); ftp_host_pump(); usleep(500); }
}
