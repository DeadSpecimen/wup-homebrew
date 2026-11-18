/* CATALOG_PREVIEW: pictures and videos from wup-emu, which has no Wi-Fi. Never in a release. The real screens with a
 * built-in catalogue: loading takes a moment, installing copies /.get/preview/<name>.wup (put there with the emulator
 * image) to where a real install puts it, at a download's pace. (Included by catalog.c.) */
static const struct { const char *name, *title, *category, *description; uint32_t kb; } preview_pkgs[] = {
    { "app_store", "App Store", "system", "Get apps for your GamePad", 322 },
    { "software_update", "Software Update", "system", "Keep the Homebrew Menu up to date", 320 },
    { "homebrew_menu", "Homebrew Menu", "system", "The Homebrew Menu", 264 },
    { "wup_stream", "WUP Stream", "tool", "Play from a computer on your network", 103 },
};
static void preview_wait(int ms) { for (int t = 0; t < ms; t += 10) { if (catalog_idle) catalog_idle(); wup_sleep(10); } }
static int preview_load(void) {
    preview_wait(900);
    catalog_count = 0;
    for (unsigned i = 0; i < sizeof preview_pkgs / sizeof preview_pkgs[0]; i++) {
        catalog_pkg *p = &catalog[catalog_count++]; memset(p, 0, sizeof *p);
        snprintf(p->name, sizeof p->name, "%s", preview_pkgs[i].name); snprintf(p->title, sizeof p->title, "%s", preview_pkgs[i].title);
        snprintf(p->author, sizeof p->author, "wup-homebrew"); snprintf(p->version, sizeof p->version, "1.0.0");
        snprintf(p->category, sizeof p->category, "%s", preview_pkgs[i].category);
        snprintf(p->description, sizeof p->description, "%s", preview_pkgs[i].description);
        memset(p->md5, '0', 32); p->filesize_kb = preview_pkgs[i].kb;
        installed_version(p);
    }
    return 0;
}
static int preview_install(const catalog_pkg *p, void (*progress)(int)) {
    static uint8_t buf[4096];
    char from[96], dir[64], to[112], t[128];
    snprintf(from, sizeof from, "/.get/preview/%s.wup", p->name);
    int size = fs_filesize(from);
    if (size <= 0) return CATALOG_ENET;
    snprintf(dir, sizeof dir, "/apps/%s", p->name); lfs_mkdir(&fs_lfs, dir);
    snprintf(to, sizeof to, "/apps/%s/%s.wup", p->name, p->name);
    for (int at = 0; at < size; at += (int)sizeof buf) {           /* (read it all, then write: one file open at a time) */
        if (progress) progress(at * 100 / size);
        preview_wait(2400 * (int)sizeof buf / size);
    }
    if (fs_stream_open(to)) return CATALOG_EWRITE;
    for (int at = 0; at < size; at += (int)sizeof buf) {
        int k = size - at < (int)sizeof buf ? size - at : (int)sizeof buf;
        if (fs_read(from, (uint32_t)at, buf, (uint32_t)k) != k || fs_stream_write(buf, (uint32_t)k)) { fs_stream_close(); return CATALOG_EWRITE; }
    }
    if (fs_stream_close()) return CATALOG_EWRITE;
    if (progress) progress(100);
    lfs_mkdir(&fs_lfs, "/.get"); lfs_mkdir(&fs_lfs, "/.get/packages");
    snprintf(dir, sizeof dir, "/.get/packages/%s", p->name); lfs_mkdir(&fs_lfs, dir);
    snprintf(to, sizeof to, "/.get/packages/%s/info.json", p->name);
    int k = snprintf(t, sizeof t, "{\"version\": \"%s\"}\n", p->version); fs_write(to, t, (uint32_t)k);
    snprintf(to, sizeof to, "/.get/packages/%s/manifest.install", p->name);
    k = snprintf(t, sizeof t, "U: apps/%s/%s.wup\n", p->name, p->name); fs_write(to, t, (uint32_t)k);
    preview_wait(300);
    return 0;
}
