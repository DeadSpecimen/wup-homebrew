/* mem test: libwup's memset, memcpy, memmove and bzero (src/mem.S) against byte loops the compiler cannot turn into
 * calls to them (volatile): lengths 0..300, every alignment of source and destination, overlaps both ways, and that
 * nothing outside the range is touched. The result goes to the log and the screen: "mem: PASS" or the first failure. */
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "wup.h"
WUP_APP("Mem Test");

static uint8_t a[1024], b[1024], ra[1024], rb[1024];
static void fill(void) { for (int i = 0; i < 1024; i++) { a[i] = ra[i] = (uint8_t)(i * 7 + 3); b[i] = rb[i] = (uint8_t)(i * 13 + 1); } }
static void ref_set(volatile uint8_t *d, int c, int n) { for (int i = 0; i < n; i++) d[i] = (uint8_t)c; }
static void ref_cpy(volatile uint8_t *d, const volatile uint8_t *s, int n) { for (int i = 0; i < n; i++) d[i] = s[i]; }
static void ref_move(volatile uint8_t *d, const volatile uint8_t *s, int n) {
    if (d <= s) for (int i = 0; i < n; i++) d[i] = s[i]; else for (int i = n - 1; i >= 0; i--) d[i] = s[i];
}
static int same(void) { for (int i = 0; i < 1024; i++) if (a[i] != ra[i] || b[i] != rb[i]) return 0; return 1; }
static char fail[80];
static int run(void) {
    long checks = 0;
    for (int n = 0; n <= 300; n++)
        for (int da = 0; da < 4; da++) {
            fill(); if (memset(a + 64 + da, n, (size_t)n) != a + 64 + da) return snprintf(fail, sizeof fail, "memset return n=%d", n);
            ref_set(ra + 64 + da, n, n); if (!same()) return snprintf(fail, sizeof fail, "memset n=%d align=%d", n, da);
            fill(); bzero(a + 64 + da, (size_t)n); ref_set(ra + 64 + da, 0, n); if (!same()) return snprintf(fail, sizeof fail, "bzero n=%d align=%d", n, da);
            for (int sa = 0; sa < 4; sa++) {
                fill(); if (memcpy(a + 64 + da, b + 128 + sa, (size_t)n) != a + 64 + da) return snprintf(fail, sizeof fail, "memcpy return n=%d", n);
                ref_cpy(ra + 64 + da, rb + 128 + sa, n); if (!same()) return snprintf(fail, sizeof fail, "memcpy n=%d dst=%d src=%d", n, da, sa);
                for (int off = -9; off <= 9; off += 3) {            /* overlapping: dst before, at and after src */
                    fill(); memmove(a + 400 + da + off, a + 400 + sa, (size_t)n); ref_move(ra + 400 + da + off, ra + 400 + sa, n);
                    if (!same()) return snprintf(fail, sizeof fail, "memmove n=%d dst=%d src=%d off=%d", n, da, sa, off);
                    checks++;
                }
            }
        }
    printf("mem: PASS (%ld overlapping moves, lengths 0..300, all alignments)\n", checks);
    return 0;
}
int main(void) {
    int r = run();
    if (r) printf("mem: FAIL %s\n", fail);
    wup_screen(); gfx_fill(0, 0, GFX_W, GFX_H, r ? RGB(160, 30, 30) : RGB(20, 120, 60));
    gfx_text_c(&font_title, GFX_W / 2, 250, r ? fail : "mem: PASS", RGB(255, 255, 255)); gfx_present_all();
    for (int i = 0; i < 300 && !(wup_buttons() & (WUP_B | WUP_HOME)); i++) wup_sleep(16);
    return 0;
}
