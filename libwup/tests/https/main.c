/* HTTPS test: GETs from real sites through libwup's HTTPS client (wup_https.h): the status, the bytes received and the
 * time, one line per site. Needs the menu connected to Wi-Fi and its clock set. HTTPS_TEST_URL (a define) adds one. */
#include <stdio.h>
#include <string.h>
#include "wup.h"
#include "wup_https.h"

WUP_APP("HTTPS Test");
static int row, failed;
static void line(const char *s, int ok) {
    gfx_text(&font_body, 40, 50 + 30 * row++, s, ok ? RGB(200, 230, 200) : RGB(255, 120, 120));
    printf("https test: %s\n", s);
    gfx_present_all();
    if (!ok) failed++;
}
static int count(void *arg, const uint8_t *d, size_t n, long long total) { (void)d; (void)total; *(long long *)arg += (long long)n; return 0; }
static void get(const char *url) {
    long long got = 0; uint32_t t0 = wup_us();
    int r = wup_https_get(url, NULL, count, &got);
    char l[200];
    if (r == WUP_HTTPS_ETLS) snprintf(l, sizeof l, "%s  TLS error %d", url, wup_https_tls_error());
    else snprintf(l, sizeof l, "%s  %d, %lld bytes, %u ms", url, r, got, (unsigned)((wup_us() - t0) / 1000));
    line(l, r == 200 && got > 0);
}

int main(void) {
    wup_screen();
    gfx_fill(0, 0, GFX_W, GFX_H, RGB(16, 20, 32));
    if (wup_net_init()) { line("not connected: join Wi-Fi in the Homebrew Menu first", 0); }
    else if (!wup_time_known()) { line("the clock is not set yet (the menu sets it once Wi-Fi is joined)", 0); }
    else {
        get("https://api.github.com/repos/fortheusers/libget/releases/latest");   /* Sectigo (USERTrust ECC), chunked */
        get("https://github.com/");                                                /* redirects nowhere, big page */
        get("https://example.com/");                                               /* DigiCert */
        get("https://letsencrypt.org/");                                           /* ISRG */
#ifdef HTTPS_TEST_URL
        get(HTTPS_TEST_URL);
#endif
    }
    gfx_text(&font_body, 40, 60 + 30 * row, failed ? "Some checks failed.   B returns to the menu" : "All checks passed.   B returns to the menu", RGB(255, 255, 255));
    gfx_present_all();
    while (wup_buttons() & WUP_B) wup_sleep(16);
    while (!(wup_buttons() & WUP_B)) wup_sleep(16);
    return failed;
}
