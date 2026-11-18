/* The time. The pad has no clock that keeps the date: it has a free-running counter (0xF0000408, 997.3 kHz, measured
 * against the audio's 48 kHz) that wraps every 72 minutes. So:
 *   the monotonic clock  counts from the app's start (the counter, kept as 64 bits: asked at least every sleep);
 *   the date and time    are known once the menu has asked the network (SNTP) and set them (wup_time_set); they
 *                        pass from the menu to each app and back in the hand-off block, with the time zone (a POSIX
 *                        TZ rule, so the C library's localtime() knows summer time). Until then time() counts
 *                        from 1970 as before, and wup_time_known() says 0. */
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "wup.h"

#define HZ 997300u                                     /* the counter's rate */
static uint64_t ticks;                                 /* since the app started */
static uint32_t last;
static int64_t base_us;                                /* the date (µs since 1970) when ticks was 0; 0: not known */
uint64_t wup__now_us(void) {
    unsigned st = wup_irq_off();
    uint32_t t = wup_us(); ticks += (uint32_t)(t - last); last = t;
    uint64_t r = ticks * 1000000u / HZ;
    wup_irq_restore(st);
    return r;
}
static int64_t real_us(void) { return base_us + (int64_t)wup__now_us(); }
int clock_gettime(clockid_t id, struct timespec *ts) {
    int64_t t = id == CLOCK_MONOTONIC ? (int64_t)wup__now_us() : real_us();
    ts->tv_sec = (time_t)(t / 1000000); ts->tv_nsec = (long)(t % 1000000) * 1000;
    return 0;
}
int gettimeofday(struct timeval *restrict tv, void *restrict tz) {
    (void)tz;
    int64_t t = real_us();
    tv->tv_sec = (time_t)(t / 1000000); tv->tv_usec = (suseconds_t)(t % 1000000);
    return 0;
}

int wup_time_known(void) { return base_us != 0; }
void wup_time_set(int64_t unix_seconds, uint32_t us) { base_us = unix_seconds * 1000000 + us - (int64_t)wup__now_us(); }
static char zone[48];
void wup_time_zone(const char *posix_tz) {
    strncpy(zone, posix_tz ? posix_tz : "UTC0", sizeof zone - 1);
    setenv("TZ", zone, 1); tzset();
}

/* the hand-off: the date at a counter reading, checked (the block is not cleared at power-on beyond its older part) */
#define TMAGIC 0x454d4954u                             /* "TIME" */
static uint32_t check(volatile wup_handoff_t *h) { return TMAGIC ^ h->time_s_lo ^ h->time_s_hi ^ h->time_us ^ h->time_tick ^ 0x5a5a5a5au; }
void wup__time_start(void) {
    wup__now_us();                                     /* (the counter from here) */
    ticks = 0; last = wup_us();
    volatile wup_handoff_t *h = WUP_HANDOFF;
    if (h->magic != WUP_HANDOFF_MAGIC || h->time_magic != TMAGIC || h->time_check != check(h)) return;
    uint32_t since = wup_us() - h->time_tick;           /* (an app that ran longer than 72 minutes left its own time) */
    int64_t s = (int64_t)((uint64_t)h->time_s_hi << 32 | h->time_s_lo);
    base_us = s * 1000000 + h->time_us + (int64_t)((uint64_t)since * 1000000u / HZ);
    char z[sizeof zone]; for (unsigned i = 0; i < sizeof z; i++) z[i] = h->time_zone[i];
    z[sizeof z - 1] = 0;
    if (z[0]) wup_time_zone(z);
}
void wup__time_end(void) {
    volatile wup_handoff_t *h = WUP_HANDOFF;
    if (h->magic != WUP_HANDOFF_MAGIC) return;
    if (!base_us) { h->time_magic = 0; return; }
    int64_t t = real_us();
    h->time_tick = wup_us();
    h->time_s_lo = (uint32_t)(t / 1000000); h->time_s_hi = (uint32_t)((uint64_t)(t / 1000000) >> 32); h->time_us = (uint32_t)(t % 1000000);
    for (unsigned i = 0; i < sizeof zone; i++) h->time_zone[i] = zone[i];
    h->time_magic = TMAGIC; h->time_check = check(h);
}
