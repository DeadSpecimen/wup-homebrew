/* Kernel test: libwup's threads and semaphores (kernel.c) on the pad or in the emulator. One line per check. */
#include <stdio.h>
#include "wup.h"

WUP_APP("Kernel Test");
static int row, failed;
static void check(const char *what, int ok) {
    char l[96]; snprintf(l, sizeof l, "%s  %s", ok ? "ok  " : "FAIL", what);
    gfx_text(&font_body, 40, 60 + 30 * row++, l, ok ? RGB(200, 230, 200) : RGB(255, 120, 120));
    printf("kernel test: %s\n", l);
    gfx_present_all();
    if (!ok) failed++;
}
static int ping, pong, count, lock, shared;
static void ponger(void *arg) { for (int i = 0; i < (int)(intptr_t)arg; i++) { wup_sem_wait(ping, -1); count++; wup_sem_signal(pong); } }
static void adder(void *arg) {
    for (int i = 0; i < 2000; i++) { wup_sem_wait(lock, -1); int v = shared; if (!(i & 255)) wup_thread_sleep(1); shared = v + (int)(intptr_t)arg; wup_sem_signal(lock); }
}
static void waiter(void *arg) { wup_sem_wait((int)(intptr_t)arg, -1); unsigned st = wup_irq_off(); shared++; wup_irq_restore(st); }
static void quick(void *arg) { *(int *)arg = wup_thread_self(); }

int main(void) {
    wup_screen();
    gfx_fill(0, 0, GFX_W, GFX_H, RGB(16, 20, 32));
    ping = wup_sem_new(0, 1); pong = wup_sem_new(0, 1); lock = wup_sem_new(1, 1);
    check("semaphores made", ping > 0 && pong > 0 && lock > 0 && ping != pong);
    uint32_t t0 = wup_us(); int r = wup_sem_wait(ping, 60); uint32_t dt = (wup_us() - t0) / 1000;
    check("a wait times out after its time", r < 0 && dt >= 50 && dt < 200);
    check("a poll on an empty semaphore fails at once", wup_sem_wait(ping, 0) < 0);
    char l[120];
    { uint32_t ker = 0xc7a88, tt = *(volatile uint32_t *)(ker + 0x50), cur = *(volatile uint32_t *)(ker + 0x34); int dorm = 0, used = 0;
      for (unsigned id = 1; id <= *(volatile uint16_t *)tt; id++) { uint32_t tc = *(volatile uint32_t *)(tt + 4 * id); if (tc) { used++; if (*(volatile uint8_t *)(tc + 0x12) == 0x10) dorm++; } }
      snprintf(l, sizeof l, "kernel: tasks max %u, count %u, in table %d, dormant %d; my priority %u, highest number %u", *(volatile uint16_t *)tt, *(volatile uint16_t *)(tt + 2), used, dorm,
               cur ? *(volatile uint16_t *)(cur + 0x2e) : 0, *(volatile uint16_t *)*(volatile uint32_t *)(ker + 0x38));
      printf("kernel test: %s\n", l); }
    int t = wup_thread_start(ponger, (void *)1000, 4096, 0);
    snprintf(l, sizeof l, "a thread starts (wup_thread_start = %d)", t);
    check(l, t >= 0);
    t0 = wup_us();
    for (int i = 0; i < 1000 && t >= 0; i++) { wup_sem_signal(ping); if (wup_sem_wait(pong, 2000)) break; }
    dt = (wup_us() - t0) / 1000;
    snprintf(l, sizeof l, "1000 hand-overs between two threads (%u ms)", (unsigned)dt);
    check(l, count == 1000);
    check("join", wup_thread_join(t, 2000) == 0);
    int a = wup_thread_start(adder, (void *)1, 4096, 0), b = wup_thread_start(adder, (void *)-1, 4096, 0), c = wup_thread_start(adder, (void *)3, 4096, 0);
    { uint32_t tt = *(volatile uint32_t *)(0xc7a88 + 0x50), st = *(volatile uint32_t *)(0xc7a88 + 0x54); int dorm = 0;
      for (unsigned id = 1; id <= *(volatile uint16_t *)tt; id++) { uint32_t tc = *(volatile uint32_t *)(tt + 4 * id); if (tc && *(volatile uint8_t *)(tc + 0x12) == 0x10) dorm++; }
      snprintf(l, sizeof l, "a %d b %d c %d; tasks max %u used %u dormant %d; sems max %u used %u", a, b, c, *(volatile uint16_t *)tt, *(volatile uint16_t *)(tt + 2), dorm, *(volatile uint16_t *)st, *(volatile uint16_t *)(st + 2)); check(l, 1); }
    check("three threads at once", a >= 0 && b >= 0 && c >= 0);
    check("they end", !wup_thread_join(a, 20000) && !wup_thread_join(b, 20000) && !wup_thread_join(c, 20000));
    check("a lock keeps a shared count right (6000)", shared == 6000);
    int who = -9, ok = 1;
    for (int i = 0; i < 20 && ok; i++) { int q = wup_thread_start(quick, &who, 2048, 1); ok = q >= 0 && !wup_thread_join(q, 2000); }
    extern int wup__kernel_tasks(void); int tasks = wup__kernel_tasks();
    snprintf(l, sizeof l, "20 more threads, one after another (%d kernel tasks left)", tasks);
    check(l, ok && who >= 0 && tasks == 0);
    check("the main thread is not one of them", wup_thread_self() == -1);
    int gate = wup_sem_new(0, 10), ten[10], started = 0, joined = 0; shared = 0;
    for (int i = 0; i < 10; i++) { ten[i] = wup_thread_start(waiter, (void *)(intptr_t)gate, 2048, 0); started += ten[i] >= 0; }
    wup_sleep(20);
    for (int i = 0; i < 10; i++) wup_sem_signal(gate);
    for (int i = 0; i < 10; i++) joined += ten[i] >= 0 && !wup_thread_join(ten[i], 3000);
    snprintf(l, sizeof l, "ten threads wait on one semaphore and all wake (%d started, %d done)", started, shared);
    check(l, started == 10 && joined == 10 && shared == 10);
    snprintf(l, sizeof l, "their kernel tasks are deleted again (%d left)", wup__kernel_tasks());
    check(l, wup__kernel_tasks() == 0);
    gfx_text(&font_body, 400, 60, failed ? "Some checks failed.   B returns to the menu" : "All checks passed.   B returns to the menu", RGB(255, 255, 255));
    gfx_present_all();
    while (wup_buttons() & WUP_B) wup_sleep(16);
    while (!(wup_buttons() & WUP_B)) wup_sleep(16);
    return failed;
}
