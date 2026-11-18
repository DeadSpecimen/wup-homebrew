/* LVGL test: the standard widgets with the pad's screen, touch and buttons. B on the first button returns. */
#include "wup_lvgl.h"

WUP_APP("LVGL Test");
static int quit;
static void on_quit(lv_event_t *e) { (void)e; quit = 1; }
static void on_slider(lv_event_t *e) { lv_label_set_text_fmt(lv_event_get_user_data(e), "Brightness %d", (int)lv_slider_get_value(lv_event_get_target(e))); }

int main(void) {
    wup_lvgl_init();
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(scr, 24, 0); lv_obj_set_style_pad_row(scr, 14, 0);

    lv_obj_t *t = lv_label_create(scr);
    lv_label_set_text(t, "LVGL on the GamePad " LV_SYMBOL_WIFI " " LV_SYMBOL_BATTERY_3);
    lv_obj_set_style_text_font(t, &inter_32, 0);

    lv_obj_t *row = lv_obj_create(scr);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT); lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *b = lv_button_create(row); lv_obj_add_event_cb(b, on_quit, LV_EVENT_CLICKED, 0);
    lv_label_set_text(lv_label_create(b), LV_SYMBOL_HOME "  Back to the menu");
    lv_obj_t *sw = lv_switch_create(row); lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_t *cb = lv_checkbox_create(row); lv_checkbox_set_text(cb, "Developer Mode");
    lv_obj_t *sp = lv_spinner_create(row); lv_obj_set_size(sp, 40, 40);

    lv_obj_t *l = lv_label_create(scr); lv_label_set_text(l, "Brightness 3");
    lv_obj_t *s = lv_slider_create(scr); lv_slider_set_range(s, 1, 5); lv_slider_set_value(s, 3, LV_ANIM_OFF);
    lv_obj_set_width(s, 400); lv_obj_add_event_cb(s, on_slider, LV_EVENT_VALUE_CHANGED, l);

    lv_obj_t *list = lv_list_create(scr); lv_obj_set_size(list, LV_PCT(100), 150);
    lv_list_add_button(list, LV_SYMBOL_FILE, "Moonlight");
    lv_list_add_button(list, LV_SYMBOL_FILE, "wupstream");
    lv_list_add_button(list, LV_SYMBOL_SETTINGS, "Settings");

    while (!quit) wup_lvgl_run(10);
    return 0;
}
