/* apps: the installed apps on the filesystem (libwup/src/fs.c), format libwup/include/wupfmt.h. Same rules as the Switch's
 * nx-hbmenu for sdmc:/switch:
 *   /apps/<x>.wup                  a single-file app
 *   /apps/<dir>/<dir>.wup          an app with its own folder (its data lives next to it), or, if that
 *   /apps/<dir>/<one>.wup          name does not exist, the only .wup in the folder
 * The app is told its path (the hand-off block app_path, like argv[0]) and finds its folder from it. */
#include <string.h>
#include "fs.h"
#include "wupfmt.h"
#include "apps.h"
#include "armcpu.h"
typedef uint32_t u32; typedef uint8_t u8;

app_entry_t apps_list[APPS_MAX];
int apps_count;

static int ends_wup(const char *n) { int l = (int)strlen(n); return l > 4 && !strcmp(n + l - 4, ".wup"); }

static void add(const char *path, u32 size) {
    if (apps_count >= APPS_MAX || strlen(path) >= sizeof apps_list[0].path) return;
    app_entry_t *a = &apps_list[apps_count];
    strcpy(a->path, path);
    wup_hdr_t h;
    if (fs_read(a->path, 0, &h, sizeof h) != (int)sizeof h || h.magic != WUP_MAGIC) return;
    if (!h.code_size || h.code_size > APP_CODE_MAX || h.code_off + h.code_size > size) return;
    memcpy(a->name, h.name, 32); a->name[31] = 0;
    memcpy(a->author, h.author, 32); a->author[31] = 0;
    memcpy(a->version, h.version, 16); a->version[15] = 0;
    if (h.hdr_size >= WUP_HDR_V2) { memcpy(a->description, h.description, 96); a->description[95] = 0; } else a->description[0] = 0;
    if (!a->name[0]) { const char *n = path; for (const char *q = path; *q; q++) if (*q == '/') n = q + 1; strncpy(a->name, n, 31); a->name[31] = 0; }
    a->code_off = h.code_off; a->code_size = h.code_size; a->code_crc = h.code_crc;
    a->icon_off = h.icon_w == WUP_ICON && h.icon_h == WUP_ICON && h.icon_off + WUP_ICON * WUP_ICON * 2 <= size ? h.icon_off : 0;
    apps_count++;
}
/* a folder: <dir>/<dir>.wup, else its only .wup */
static void add_folder(const char *dir) {
    static char p[96]; struct lfs_info info;
    if (strlen(dir) > 40) return;
    strcpy(p, "/apps/"); strcat_(p, dir); strcat_(p, "/"); strcat_(p, dir); strcat_(p, ".wup");
    if (!lfs_stat(&fs_lfs, p, &info) && info.type == LFS_TYPE_REG) { add(p, info.size); return; }
    strcpy(p, "/apps/"); strcat_(p, dir);
    lfs_dir_t d; static char one[64]; int n = 0; u32 sz = 0;
    if (lfs_dir_open(&fs_lfs, &d, p)) return;
    while (lfs_dir_read(&fs_lfs, &d, &info) > 0)
        if (info.type == LFS_TYPE_REG && ends_wup(info.name) && strlen(info.name) < sizeof one) { if (++n > 1) break; strcpy(one, info.name); sz = info.size; }
    lfs_dir_close(&fs_lfs, &d);
    if (n != 1) return;
    strcat_(p, "/"); strcat_(p, one);
    add(p, sz);
}
int apps_scan(void) {
    apps_count = 0;
    if (!fs_ok()) return 0;
    lfs_dir_t d; struct lfs_info info;
    if (lfs_dir_open(&fs_lfs, &d, "/apps")) return 0;
    static char p[96];
    while (apps_count < APPS_MAX && lfs_dir_read(&fs_lfs, &d, &info) > 0) {
        if (info.name[0] == '.') continue;
        if (info.type == LFS_TYPE_DIR) { add_folder(info.name); continue; }
        if (!ends_wup(info.name) || strlen(info.name) > 80) continue;
        strcpy(p, "/apps/"); strcat_(p, info.name);
        add(p, info.size);
    }
    lfs_dir_close(&fs_lfs, &d);
    return apps_count;
}
int apps_load_icon(int i, void *dst) {
    const app_entry_t *a = &apps_list[i];
    if (!a->icon_off) return 0;
    return fs_read(a->path, a->icon_off, dst, WUP_ICON * WUP_ICON * 2) == WUP_ICON * WUP_ICON * 2;
}
int apps_remove(int i) { return fs_remove(apps_list[i].path); }
