/* exec test: in the menu's place, it starts /apps/hello_world/hello_world.wup with A (wup_exec) and counts how
   often it has come back (the loader's run counter). */
#include "wup_lvgl.h"

WUP_APP("Exec Test");
static int go;
static void on_go(lv_event_t *e) { (void)e; go = 1; }
int main(void) {
    wup_lvgl_init();
    const wup_handoff_t *pb = wup_handoff();
    lv_obj_t *l = lv_label_create(lv_screen_active());
    lv_label_set_text_fmt(l, "Exec test: apps run so far %d\nfilesystem %s", pb ? (int)pb->runs : -1, wup_fs_ok() ? "mounted" : "NOT mounted");
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_t *b = lv_button_create(lv_screen_active()); lv_obj_center(b);
    lv_label_set_text(lv_label_create(b), "Start Hello World");
    lv_obj_add_event_cb(b, on_go, LV_EVENT_CLICKED, 0);
    while (!go) wup_lvgl_run(10);
    int r = wup_exec("/apps/hello_world/hello_world.wup", 0);
    if (r) { lv_label_set_text_fmt(l, "wup_exec failed: %d", r); for (;;) wup_lvgl_run(10); }
    return 0;
}
