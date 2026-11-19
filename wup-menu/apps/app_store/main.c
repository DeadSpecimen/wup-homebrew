/* App Store: the catalogue's apps (common/catalog.c) to install, update and remove, in the Homebrew Menu's look
 * (libwup's kit). Remove asks first, and what could not be done is said, in alerts.
 * The system's own packages (the Homebrew Menu, App Store, Software Update) are Software Update's: not listed here, so
 * never removed here.
 *   home   Loading... -> the apps as the menu lists them (icon, name, description, version or Update), the storage
 *          left; or why not
 *   app    Install / Update / Remove, the developer and version, the description;
 *          Installing... (with a bar) / Removing... -> back to the list (Installed under the version), or why not */
#include <stdio.h>
#include <string.h>
#include "wup_kit.h"
#include "catalog.h"
#include "fs.h"

WUP_APP("App Store");

static int loaded, load_err, pick;
static enum { VIEW, WORKING } app_state;
static int removing, act_err;
static lv_obj_t *bar;

static char icons[CATALOG_MAX][64];
static int shown(const catalog_pkg *p) { return !catalog_system(p); }
static int has_update(const catalog_pkg *p) { return p->installed[0] && catalog_newer(p->version, p->installed); }
static void size_text(uint32_t kb, char *out, size_t n) {
    if (kb >= 1024) snprintf(out, n, "%u.%u MB", (unsigned)(kb / 1024), (unsigned)(kb % 1024 * 10 / 1024));
    else snprintf(out, n, "%u KB", (unsigned)kb);
}

/* ---- one app */
static void progress(int pct) { if (bar) lv_bar_set_value(bar, pct, LV_ANIM_OFF); }
static void act(void) {
    catalog_pkg *p = &catalog[pick];
    act_err = removing ? catalog_remove(p) : catalog_install(p, progress);
    if (!act_err) { if (removing) p->installed[0] = 0; else snprintf(p->installed, sizeof p->installed, "%s", p->version); }
    bar = 0; kit_hold(0); app_state = VIEW;
    if (!act_err) {                                          /* done: back to the list, where the app shows Installed, and a word */
        static char t[72]; snprintf(t, sizeof t, "\"%s\" %s", p->title, removing ? "Removed" : "Installed");
        kit_pop(); wup_notice(t, 1);
        return;
    }
    kit_show();                                              /* (not done: its page again, and an alert saying why) */
    wup_alert(removing ? "Couldn't Remove" : "Couldn't Install", catalog_why(act_err), "OK", NULL, NULL, 0);
}
static void start(int remove) { removing = remove; app_state = WORKING; kit_hold(1); kit_show(); kit_defer(act); }
static void on_install(lv_event_t *e) { (void)e; start(0); }
static void remove_go(void) { start(1); }
static void on_remove(lv_event_t *e) {
    (void)e; static char t[72]; snprintf(t, sizeof t, "Remove \"%s\"", catalog[pick].title);
    wup_alert(t, "The app is removed from the GamePad", "Remove", "Cancel", remove_go, 1);
}
static void app_screen(lv_obj_t *scr) {
    catalog_pkg *p = &catalog[pick];
    char t[24];
    switch (app_state) {
    case WORKING: {
        lv_obj_t *l = kit_page(scr, p->title, "");
        kit_busy(l, removing ? "Removing\xE2\x80\xA6" : "Installing\xE2\x80\xA6", "Don't turn off the GamePad", NULL);
        if (!removing) bar = kit_bar(l);
        break;
    }
    case VIEW: {
        lv_obj_t *l = kit_page(scr, p->title, "B Back    A Select");
        if (!p->installed[0]) { size_text(p->filesize_kb, t, sizeof t); kit_row(l, LV_SYMBOL_DOWNLOAD, "Install", t, on_install, 0); }
        else if (has_update(p)) kit_row(l, LV_SYMBOL_DOWNLOAD, "Update", p->version, on_install, 0);
        if (p->installed[0]) kit_row(l, LV_SYMBOL_TRASH, "Remove", NULL, on_remove, 0);
        kit_row(l, NULL, "Developer", p->author, NULL, NULL);
        kit_row(l, NULL, "Version", p->installed[0] ? p->installed : p->version, NULL, NULL);
        break;
    }
    }
}

/* ---- the apps */
static void load(void) {                         /* (the icons too: fetched once, then kept on the pad) */
    load_err = catalog_load();
    for (int i = 0; !load_err && i < catalog_count; i++) {
        const char *s = shown(&catalog[i]) ? catalog_icon(&catalog[i]) : NULL;
        snprintf(icons[i], sizeof icons[i], "%s", s ? s : "");
    }
    loaded = 1; kit_show();
}
static void on_app(lv_event_t *e) { pick = (int)(intptr_t)lv_event_get_user_data(e); app_state = VIEW; kit_push(app_screen); }
static void home(lv_obj_t *scr) {
    if (!loaded) { kit_busy(kit_page(scr, "App Store", ""), "Loading\xE2\x80\xA6", "", NULL); kit_defer(load); return; }
    if (load_err) { kit_outcome(kit_page(scr, "App Store", "B Back"), "Can't Load Apps", catalog_why(load_err)); return; }
    int any = 0;
    for (int i = 0; i < catalog_count; i++) any |= shown(&catalog[i]);
    if (!any) { kit_empty(kit_page(scr, "App Store", "B Back"), "No Apps Yet", "New apps appear here"); return; }
    lv_obj_t *l = kit_page(scr, "App Store", "B Back    A Select");
    char t[24], note[48];
    int n = 0;
    for (int i = 0; i < catalog_count; i++) {
        const catalog_pkg *p = &catalog[i];
        if (!shown(p)) continue;
        char text[220];                              /* as the menu lists apps: the name, its description under it, the version */
        if (p->description[0]) snprintf(text, sizeof text, "%s\n%s", p->title, p->description); else snprintf(text, sizeof text, "%s", p->title);
        char v[48];                                  /* the version, and under it Installed or Update */
        snprintf(v, sizeof v, "%s%s", p->version, has_update(p) ? "\nUpdate" : p->installed[0] ? "\nInstalled" : "");
        kit_row(l, icons[i][0] ? icons[i] : LV_SYMBOL_FILE, text, v, on_app, (void *)(intptr_t)i);
        n++;
    }
    size_text(fs_size_kb() - fs_used_kb(), t, sizeof t);
    (void)n; snprintf(note, sizeof note, "%s available on the GamePad.", t);
    kit_note(l, note);
}

int main(void) {
    catalog_idle = kit_idle;
    kit_start(home);
    while (kit_step()) {}
    return 0;
}
