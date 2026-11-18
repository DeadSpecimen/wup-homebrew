/* bundle: the apps a Wii U install brings (App Store, Software Update), put on the pad by the menu. The installer's
 * firmware carries them in its bundle, after the menu (tools/mkflash.py build_bundle): a table {"WAPS", count, then per
 * app: id[32], version[16], offset from the bundle's start, size, crc32} on a 4 KB boundary, then the .wup files. Each
 * app goes to /apps/<id>/<id>.wup once (CRC checked), recorded as the App Store records what it installs
 * (/.get/packages/<id>/), so Software Update can update it. /.get/bundled/<id> remembers the version offered: an
 * update from Software Update is never replaced by the older bundled one. They are the system's own: one missing from
 * the pad (deleted over FTP) is put back at the next start. Pads installed over UART have no bundle: Software Update
 * installs them. */
#include <stdio.h>
#include <string.h>
#include "menu.h"
#include "fs.h"
#include "littlefs/lfs.h"
#include "armcpu.h"
#include "../common/version.h"

#ifndef BUNDLE_OFF
#error BUNDLE_OFF (as tools/mkflash.py)
#endif
typedef struct { char id[32], version[16]; uint32_t off, size, crc; } entry_t;

static int note(const char *path, const char *text) { return fs_write(path, text, (uint32_t)strlen(text)) == (int)strlen(text) ? 0 : -1; }
static int copy_app(uint32_t base, const entry_t *e) {
    static uint8_t buf[4096];
    char dir[48], path[96];
    snprintf(dir, sizeof dir, "/apps/%s", e->id); lfs_mkdir(&fs_lfs, dir);
    snprintf(path, sizeof path, "/apps/%s/%s.wup", e->id, e->id);
    if (fs_stream_open(path)) return -1;
    uint32_t crc = 0;
    for (uint32_t at = 0; at < e->size; at += sizeof buf) {
        uint32_t k = e->size - at < sizeof buf ? e->size - at : sizeof buf;
        if (fs_flash_read(base + e->off + at, buf, k) || fs_stream_write(buf, k)) { fs_stream_close(); fs_remove(path); return -1; }
        crc = crc32_update(crc, buf, k);
    }
    if (fs_stream_close() || crc != e->crc) { fs_remove(path); return -1; }
    char t[96];
    lfs_mkdir(&fs_lfs, "/.get"); lfs_mkdir(&fs_lfs, "/.get/packages");
    snprintf(dir, sizeof dir, "/.get/packages/%s", e->id); lfs_mkdir(&fs_lfs, dir);
    snprintf(path, sizeof path, "/.get/packages/%s/info.json", e->id);
    snprintf(t, sizeof t, "{\"version\": \"%s\"}\n", e->version); note(path, t);
    snprintf(path, sizeof path, "/.get/packages/%s/manifest.install", e->id);
    snprintf(t, sizeof t, "U: apps/%s/%s.wup\n", e->id, e->id); note(path, t);
    return 0;
}
int bundle_apps(void) {
    static uint32_t h[5]; static uint8_t sel[4], table[4096];
    if (!M.fs_ok || fs_flash_read(0xF000, sel, 4)) return 0;
    uint32_t base = (sel[0] == 1 ? 0x500000u : 0x100000u) + BUNDLE_OFF;
    if (fs_flash_read(base, h, 4) || h[0] != 0x31574643u) return 0;          /* "CFW1": a bundle here */
    if (fs_flash_read(base + 0x2000, h, 20) || h[0] != 0x554e4d57u) return 0; /* "WMNU" */
    uint32_t at = (0x3000 + h[1] + 0xfff) & ~0xfffu;
    if (fs_flash_read(base + at, table, sizeof table) || memcmp(table, "WAPS", 4)) return 0;
    uint32_t count, added = 0; memcpy(&count, table + 4, 4);
    lfs_mkdir(&fs_lfs, "/.get"); lfs_mkdir(&fs_lfs, "/.get/bundled");
    for (uint32_t i = 0; i < count && 8 + (i + 1) * sizeof(entry_t) <= sizeof table; i++) {
        entry_t e; memcpy(&e, table + 8 + i * sizeof e, sizeof e);
        e.id[31] = e.version[15] = 0;
        if (!e.id[0] || strchr(e.id, '/') || strchr(e.id, '.')) continue;
        char marker[64], had[16] = "";
        snprintf(marker, sizeof marker, "/.get/bundled/%s", e.id);
        int n = fs_read(marker, 0, had, sizeof had - 1); if (n > 0) had[n] = 0;
        char app[96]; snprintf(app, sizeof app, "/apps/%s/%s.wup", e.id, e.id);
        if (n > 0 && !version_newer(e.version, had) && fs_filesize(app) > 0) continue;   /* (there already) */
        if (!copy_app(base, &e)) { note(marker, e.version); added++; printf("[menu] %s %s from the bundle\n", e.id, e.version); }
    }
    return (int)added;
}
