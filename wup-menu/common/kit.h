/* kit: the Homebrew Menu's look for its apps (App Store, Software Update): the same bars, status, rows, notes, button
 * hints, spinner and clicks as the menu (menu/ui.c), its theme and sound setting from the hand-off block. LVGL.
 *
 *   static void home(lv_obj_t *scr) { lv_obj_t *l = kit_page(scr, "Title", "B Back    A Select"); kit_row(l, ...); }
 *   int main(void) { kit_start(home); while (kit_step()) {} return 0; }      B at the first screen ends the app */
#pragma once
#include "wup.h"
#include "wup_lvgl.h"

typedef void (*kit_screen)(lv_obj_t *scr);
void kit_start(kit_screen first);            /* LVGL, the theme, the first screen */
int kit_step(void);                          /* one turn of the loop; 0: B at the first screen, the app ends */
void kit_push(kit_screen f), kit_pop(void), kit_show(void);   /* another screen; back; build this one again */
void kit_defer(void (*f)(void));             /* run f once the screen is drawn (work behind a spinner) */
void kit_on_a(void (*f)(void));             /* A does f on this screen (one without rows: "A Install" in its hints) */
void kit_idle(void);                         /* call during long work (catalog_idle): the screen keeps moving */
void kit_hold(int on);                       /* 1: B does nothing (while something must not be interrupted) */
kit_screen kit_current(void);

lv_obj_t *kit_page(lv_obj_t *scr, const char *title, const char *hints);   /* the bars; -> the list to fill */
lv_obj_t *kit_row(lv_obj_t *list, const void *icon, const char *text, const char *value, lv_event_cb_t cb, void *arg);
void kit_note(lv_obj_t *list, const char *text);
lv_obj_t *kit_busy(lv_obj_t *list, const char *title, const char *line, lv_obj_t **sub);   /* the spinner, a title, a line */
void kit_outcome(lv_obj_t *list, const char *title, const char *line);   /* a title and a quieter line, centred */
lv_obj_t *kit_bar(lv_obj_t *list);           /* a progress bar under kit_busy (0..100) */
void kit_click(void);
int kit_dark(void);
