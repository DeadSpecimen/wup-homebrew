/* POSIX threads test: <pthread.h> and friends (src/pthread.c), on the pad or in the emulator. One line per check. */
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "wup.h"

WUP_APP("Pthread Test");
static int row, failed;
static void check(const char *what, int ok) {
    char l[120]; snprintf(l, sizeof l, "%s  %s", ok ? "ok  " : "FAIL", what);
    gfx_text(&font_body, 40, 50 + 30 * row++, l, ok ? RGB(200, 230, 200) : RGB(255, 120, 120));
    printf("pthread test: %s\n", l);
    gfx_present_all();
    if (!ok) failed++;
}
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER, qm = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t qc = PTHREAD_COND_INITIALIZER;
static pthread_once_t once = PTHREAD_ONCE_INIT;
static pthread_key_t key;
static int shared, onced, queue, taken, freed, bad;
static void *adder(void *arg) {
    for (int i = 0; i < 2000; i++) { pthread_mutex_lock(&m); int v = shared; if (!(i & 255)) usleep(1000); shared = v + (int)(intptr_t)arg; pthread_mutex_unlock(&m); }
    return (void *)((intptr_t)arg * 10);
}
static void *consumer(void *arg) {
    (void)arg;
    for (;;) {
        pthread_mutex_lock(&qm);
        while (!queue) pthread_cond_wait(&qc, &qm);
        int v = queue; queue = 0;
        pthread_cond_broadcast(&qc);
        pthread_mutex_unlock(&qm);
        if (v < 0) return 0;
        taken += v;
    }
}
static void init_once(void) { onced++; }
static void key_free(void *p) { freed += *(int *)p; free(p); }
static void *keyed(void *arg) {
    pthread_once(&once, init_once);
    int *p = malloc(sizeof *p); *p = (int)(intptr_t)arg;
    pthread_setspecific(key, p);
    usleep(3000);
    if (*(int *)pthread_getspecific(key) != (int)(intptr_t)arg) bad++;
    if ((intptr_t)arg == 2) pthread_exit((void *)77);
    return (void *)(intptr_t)pthread_self();
}
static void *hammer(void *arg) {                          /* the C library from several threads at once */
    char *p[16] = { 0 }, t[32]; unsigned x = (unsigned)(intptr_t)arg;
    for (int i = 0; i < 3000; i++) {
        x = x * 1103515245u + 12345u; int k = (x >> 16) & 15;
        if (p[k]) { for (unsigned j = 0; j < 8; j++) if ((unsigned char)p[k][j] != (unsigned char)(k + (intptr_t)arg)) bad++; free(p[k]); p[k] = 0; }
        else { p[k] = malloc(16 + ((x >> 8) & 255)); if (p[k]) memset(p[k], k + (int)(intptr_t)arg, 16); }
        if (!(i & 63)) { snprintf(t, sizeof t, "%d-%d", (int)(intptr_t)arg, i); if (atoi(t) != (int)(intptr_t)arg) bad++; usleep(1000); }
    }
    for (int k = 0; k < 16; k++) free(p[k]);
    return 0;
}

int main(void) {
    wup_screen();
    gfx_fill(0, 0, GFX_W, GFX_H, RGB(16, 20, 32));
    char l[100]; pthread_t t[8]; void *ret[8] = { 0 }; int ok = 1;

    for (int i = 0; i < 3; i++) ok &= !pthread_create(&t[i], 0, adder, (void *)(intptr_t)(i + 1));
    for (int i = 0; i < 3; i++) ok &= !pthread_join(t[i], &ret[i]);
    snprintf(l, sizeof l, "three threads under a mutex (%d), their return values", shared);
    check(l, ok && shared == 12000 && ret[0] == (void *)10 && ret[1] == (void *)20 && ret[2] == (void *)30);

    pthread_mutexattr_t ma; pthread_mutex_t rm; pthread_mutexattr_init(&ma); pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_RECURSIVE); pthread_mutex_init(&rm, &ma);
    ok = !pthread_mutex_lock(&rm) && !pthread_mutex_lock(&rm) && !pthread_mutex_unlock(&rm) && !pthread_mutex_unlock(&rm) && pthread_mutex_unlock(&rm) == EPERM;
    ok &= !pthread_mutex_trylock(&m) && pthread_mutex_trylock(&m) == EBUSY && !pthread_mutex_unlock(&m);
    pthread_mutex_destroy(&rm);
    check("a recursive mutex; trylock on a held one says busy", ok);

    ok = !pthread_create(&t[0], 0, consumer, 0); int sent = 0;
    for (int i = 1; i <= 200; i++) {
        pthread_mutex_lock(&qm);
        while (queue) pthread_cond_wait(&qc, &qm);
        queue = i <= 199 ? i : -1; sent += i <= 199 ? i : 0;
        pthread_cond_broadcast(&qc);
        pthread_mutex_unlock(&qm);
    }
    ok &= !pthread_join(t[0], 0);
    snprintf(l, sizeof l, "a queue with a condition variable (%d of %d)", taken, sent);
    check(l, ok && taken == sent);

    struct timespec a, b, until;
    clock_gettime(CLOCK_REALTIME, &a); until = a; until.tv_nsec += 80000000; if (until.tv_nsec >= 1000000000) { until.tv_nsec -= 1000000000; until.tv_sec++; }
    pthread_mutex_lock(&qm); int r = pthread_cond_timedwait(&qc, &qm, &until); pthread_mutex_unlock(&qm);
    clock_gettime(CLOCK_REALTIME, &b);
    int ms = (int)((b.tv_sec - a.tv_sec) * 1000 + (b.tv_nsec - a.tv_nsec) / 1000000);
    snprintf(l, sizeof l, "a timed wait ends on time (%d ms of 80)", ms);
    check(l, r == ETIMEDOUT && ms >= 70 && ms < 140);

    pthread_key_create(&key, key_free); ok = 1;
    for (int i = 0; i < 4; i++) ok &= !pthread_create(&t[i], 0, keyed, (void *)(intptr_t)(i + 1));
    for (int i = 0; i < 4; i++) ok &= !pthread_join(t[i], &ret[i]);
    snprintf(l, sizeof l, "once (%d), a value per thread (freed %d), pthread_exit, pthread_self", onced, freed);
    check(l, ok && onced == 1 && freed == 10 && !bad && ret[1] == (void *)77 && ret[0] == (void *)(intptr_t)t[0] && pthread_self() == 0);

    sem_t s; sem_init(&s, 0, 2);
    ok = !sem_wait(&s) && !sem_trywait(&s) && sem_trywait(&s) == -1 && errno == EAGAIN && !sem_post(&s) && !sem_wait(&s);
    sem_destroy(&s);
    check("a POSIX semaphore", ok);

    ok = 1; bad = 0;
    for (int i = 0; i < 4; i++) ok &= !pthread_create(&t[i], 0, hammer, (void *)(intptr_t)(i + 1));
    for (int i = 0; i < 4; i++) ok &= !pthread_join(t[i], 0);
    snprintf(l, sizeof l, "malloc, free and snprintf from four threads at once (%d wrong)", bad);
    check(l, ok && !bad);

    pthread_attr_t at; pthread_attr_init(&at); pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED); pthread_attr_setstacksize(&at, 4096);
    ok = 1; shared = 0;
    for (int i = 0; i < 30; i++) { ok &= !pthread_create(&t[0], &at, adder, (void *)1); usleep(60000); }
    snprintf(l, sizeof l, "30 detached threads, one after another (%d)", shared);
    check(l, ok && shared == 60000);

    gfx_text(&font_body, 40, 60 + 30 * row, failed ? "Some checks failed.   B returns to the menu" : "All checks passed.   B returns to the menu", RGB(255, 255, 255));
    gfx_present_all();
    printf("pthread test: %s\n", failed ? "FAILED" : "all passed");
    while (wup_buttons() & WUP_B) wup_sleep(16);
    while (!(wup_buttons() & WUP_B)) wup_sleep(16);
    return failed;
}
