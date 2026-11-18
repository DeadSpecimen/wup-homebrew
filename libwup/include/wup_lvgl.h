/* LVGL on the GamePad (app.cfg: lvgl = yes). The standard GUI library, with the pad's screen, touch screen and
 * buttons wired in: after wup_lvgl_init(), everything in <lvgl.h> works as in its documentation (docs.lvgl.io).
 *
 *     wup_lvgl_init();
 *     lv_obj_t *b = lv_button_create(lv_screen_active());  ...
 *     for (;;) wup_lvgl_run(5);
 *
 * Screen: 854x480, RGB565, drawn straight into the framebuffer. Touch: a pointer. Buttons: a keypad in the default
 * group (lv_group_get_default()): up/down move the focus (inside a keyboard, list of choices or text box all four
 * directions go to it), A = LV_KEY_ENTER, B = LV_KEY_ESC. The other buttons: wup_buttons().
 * Fonts: inter_16, inter_20 (default), inter_24, inter_32, with LV_SYMBOL_* icons (and inter_hud: button letters). Files: "A:/apps/x/icon.png". */
#pragma once
#include <lvgl.h>
#include "wup.h"

/* Font Awesome symbols in the fonts beside LVGL's own LV_SYMBOL_* */
#define WUP_SYMBOL_SUN       "\xEF\x86\x85"     /* f185 a sun */
#define WUP_SYMBOL_CLOCK     "\xEF\x80\x97"     /* f017 a clock */
#define WUP_SYMBOL_TERMINAL  "\xEF\x84\xA0"     /* f120 a terminal prompt */
#define WUP_SYMBOL_CHIP      "\xEF\x8B\x9B"     /* f2db a microchip */
#define WUP_SYMBOL_DRIVE     "\xEF\x82\xA0"     /* f0a0 a drive */

lv_display_t *wup_lvgl_init(void);
void wup_lvgl_run(int ms);                 /* draw and handle input for about ms milliseconds (the system runs too) */
lv_indev_t *wup_lvgl_touch(void);
lv_indev_t *wup_lvgl_keys(void);
