/* lwIP's operating-system layer (sys_arch) on libwup's threads and semaphores (src/kernel.c): semaphores,
 * mutexes, mailboxes, threads, the clock. Only built for net = yes (lwIP's sockets). */
#include <stdlib.h>
#include "lwip/opt.h"
#if !NO_SYS
#include "lwip/sys.h"
#include "wup.h"

uint64_t wup__now_us(void);
void sys_init(void) { }
u32_t sys_now(void) { return (u32_t)(wup__now_us() / 1000u); }
sys_prot_t sys_arch_protect(void) { return wup_irq_off(); }
void sys_arch_unprotect(sys_prot_t p) { wup_irq_restore(p); }

err_t sys_sem_new(sys_sem_t *s, u8_t count) { *s = wup_sem_new(count, 30000); return *s > 0 ? ERR_OK : ERR_MEM; }
void sys_sem_free(sys_sem_t *s) { wup_sem_free(*s); }
void sys_sem_signal(sys_sem_t *s) { wup_sem_signal(*s); }
u32_t sys_arch_sem_wait(sys_sem_t *s, u32_t ms) { return wup_sem_wait(*s, ms ? (int)ms : -1) ? SYS_ARCH_TIMEOUT : 0; }
err_t sys_mutex_new(sys_mutex_t *m) { *m = wup_sem_new(1, 1); return *m > 0 ? ERR_OK : ERR_MEM; }
void sys_mutex_free(sys_mutex_t *m) { wup_sem_free(*m); }
void sys_mutex_lock(sys_mutex_t *m) { wup_sem_wait(*m, -1); }
void sys_mutex_unlock(sys_mutex_t *m) { wup_sem_signal(*m); }

/* a mailbox: a ring of pointers; "items" counts what is in it, "space" what still fits */
struct wup_mbox { int items, space, size, head, n; void *q[]; };
err_t sys_mbox_new(sys_mbox_t *b, int size) {
    if (size <= 0) size = 16;
    struct wup_mbox *m = malloc(sizeof *m + (size_t)size * sizeof(void *));
    if (!m) return ERR_MEM;
    m->items = wup_sem_new(0, size); m->space = wup_sem_new(size, size); m->size = size; m->head = m->n = 0;
    if (m->items <= 0 || m->space <= 0) { wup_sem_free(m->items); wup_sem_free(m->space); free(m); return ERR_MEM; }
    *b = m;
    return ERR_OK;
}
void sys_mbox_free(sys_mbox_t *b) { struct wup_mbox *m = *b; if (m) { wup_sem_free(m->items); wup_sem_free(m->space); free(m); } *b = 0; }
static void put(struct wup_mbox *m, void *msg) {
    unsigned st = wup_irq_off(); m->q[(m->head + m->n++) % m->size] = msg; wup_irq_restore(st);
    wup_sem_signal(m->items);
}
static void *take(struct wup_mbox *m) {
    unsigned st = wup_irq_off(); void *msg = m->q[m->head]; m->head = (m->head + 1) % m->size; m->n--; wup_irq_restore(st);
    wup_sem_signal(m->space);
    return msg;
}
void sys_mbox_post(sys_mbox_t *b, void *msg) { wup_sem_wait((*b)->space, -1); put(*b, msg); }
err_t sys_mbox_trypost(sys_mbox_t *b, void *msg) { if (wup_sem_wait((*b)->space, 0)) return ERR_MEM; put(*b, msg); return ERR_OK; }
err_t sys_mbox_trypost_fromisr(sys_mbox_t *b, void *msg) { return sys_mbox_trypost(b, msg); }
u32_t sys_arch_mbox_fetch(sys_mbox_t *b, void **msg, u32_t ms) {
    if (wup_sem_wait((*b)->items, ms ? (int)ms : -1)) { if (msg) *msg = 0; return SYS_ARCH_TIMEOUT; }
    void *v = take(*b); if (msg) *msg = v;
    return 0;
}
u32_t sys_arch_mbox_tryfetch(sys_mbox_t *b, void **msg) {
    if (wup_sem_wait((*b)->items, 0)) { if (msg) *msg = 0; return SYS_MBOX_EMPTY; }
    void *v = take(*b); if (msg) *msg = v;
    return 0;
}

/* lwIP's threads never return: they are stopped when the app ends (wup__net_threads_kill, from src/net.c) */
static int made[4], nmade;
sys_thread_t sys_thread_new(const char *name, lwip_thread_fn fn, void *arg, int stack, int prio) {
    (void)name;
    int t = wup_thread_start(fn, arg, (unsigned)stack, prio);
    if (t >= 0 && nmade < 4) made[nmade++] = t;
    return t;
}
void wup__net_threads_kill(void) { while (nmade) wup_thread_kill(made[--nmade]); }
#endif
