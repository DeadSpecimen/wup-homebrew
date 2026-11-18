/* SDK self-test: what apps rely on from the C library (picolibc) and libwup's system layer (sys.c), checked on the
 * pad or in the emulator. One line per check; B returns to the menu. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <errno.h>
#include <time.h>
#include "wup.h"

WUP_APP("SDK Test");
static int row, failed;
static void check(const char *what, int ok) {
    char l[96]; snprintf(l, sizeof l, "%s  %s", ok ? "ok  " : "FAIL", what);
    gfx_text(&font_body, 40, 60 + 30 * row++, l, ok ? RGB(200, 230, 200) : RGB(255, 120, 120));
    printf("sdk test: %s\n", l);
    if (!ok) failed++;
}
static int cmp(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }
static jmp_buf jb;
static volatile double half = 0.5;

int main(void) {
    wup_screen();
    gfx_fill(0, 0, GFX_W, GFX_H, RGB(16, 20, 32));
    char b[64];
    void *p = malloc(100000), *q = malloc(200000);
    check("malloc: two big blocks, apart", p && q && (char *)q >= (char *)p + 100000);
    if (p) memset(p, 0x5a, 100000);
    free(p); p = realloc(q, 300000); check("realloc grows", p != 0); free(p);
    snprintf(b, sizeof b, "%d %s %05u %llx", -42, "pad", 7u, 0x123456789abcdefull);
    check("snprintf: int, string, padding, 64-bit", !strcmp(b, "-42 pad 00007 123456789abcdef"));
    volatile unsigned long long big = 1000000007ull * 1000000009ull; volatile unsigned d = 1000000007u;
    check("64-bit multiply and divide", big / d == 1000000009ull && big % d == 0);
    volatile double x = 3.0;
    check("soft-float double", (int)(x * half * 1000.0) == 1500 && x / 4.0 > 0.74 && x / 4.0 < 0.76);
    int v[] = { 5, 3, 9, 1, 7 }; qsort(v, 5, sizeof *v, cmp);
    check("qsort", v[0] == 1 && v[2] == 5 && v[4] == 9);
    check("strtol, strstr", strtol("-0x1f", 0, 16) == -31 && strstr("gamepad", "mep") != 0);
    if (!setjmp(jb)) longjmp(jb, 1); else check("setjmp / longjmp", 1);
    FILE *f = fopen("/sdktest.tmp", "wb");
    int wrote = f && fwrite("hello pad\n", 1, 10, f) == 10 && fprintf(f, "%d\n", 1234) > 0;
    if (f) fclose(f);
    check("fopen, fwrite, fprintf", wrote);
    char l1[32] = "", l2[32] = "";
    f = fopen("/sdktest.tmp", "rb");
    int rd = f && fgets(l1, sizeof l1, f) && fgets(l2, sizeof l2, f) && !fgets(b, sizeof b, f) && feof(f);
    if (f) { fseek(f, 0, SEEK_END); rd = rd && ftell(f) == 15; fclose(f); }
    check("fgets, feof, fseek, ftell", rd && !strcmp(l1, "hello pad\n") && atoi(l2) == 1234);
    check("remove, then fopen fails with ENOENT", !remove("/sdktest.tmp") && !fopen("/sdktest.tmp", "rb") && errno == ENOENT);
    {   /* time zones with summer time (POSIX TZ rules, as the menu's clock sets them) */
        struct tm w, su; time_t winter = 1763424000, summer = 1752796800;   /* 2025-11-18 and 2025-07-18, 00:00 UTC */
        wup_time_zone("PST8PDT,M3.2.0,M11.1.0");
        localtime_r(&winter, &w); localtime_r(&summer, &su);
        char l[96]; snprintf(l, sizeof l, "localtime with a TZ rule (Los Angeles: %02d:00 %s, %02d:00 %s)", w.tm_hour, w.tm_isdst ? "DST" : "std", su.tm_hour, su.tm_isdst ? "DST" : "std");
        check(l, w.tm_hour == 16 && !w.tm_isdst && su.tm_hour == 17 && su.tm_isdst);
        wup_time_set(1763424000, 0); time_t now = time(NULL);
        check("wup_time_set: time() has the date", wup_time_known() && now >= 1763424000 && now < 1763424005);
    }
    gfx_text(&font_body, 40, 440, failed ? "Some checks failed.   B returns to the menu" : "All checks passed.   B returns to the menu", RGB(255, 255, 255));
    gfx_present_all();
    while (wup_buttons() & WUP_B) wup_sleep(16);
    while (!(wup_buttons() & WUP_B)) wup_sleep(16);
    return failed;
}
