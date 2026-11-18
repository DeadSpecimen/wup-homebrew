/* POSIX on libwup's threads (kernel.c): <pthread.h>, <semaphore.h>, the sleeps, and the C library's
 * own locks (picolibc calls __retarget_lock_*: with them malloc and the rest are safe from several threads). */
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <sys/lock.h>
#include <sys/time.h>
#include <unistd.h>
#include "wup.h"

unsigned wup__task(void);

uint64_t wup__now_us(void);                        /* (time.c) */
static void sleep_ms(int ms) { wup__now_us(); if (pthread_self()) wup_thread_sleep(ms); else wup_sleep(ms); }
int nanosleep(const struct timespec *t, struct timespec *left) {
    uint64_t ms = (uint64_t)t->tv_sec * 1000u + (uint64_t)(t->tv_nsec + 999999) / 1000000u;
    while (ms > 60000) { sleep_ms(60000); ms -= 60000; }
    sleep_ms(ms ? (int)ms : 1);
    if (left) { left->tv_sec = 0; left->tv_nsec = 0; }
    return 0;
}
int usleep(useconds_t us) { sleep_ms((int)((us + 999) / 1000)); return 0; }
unsigned sleep(unsigned s) { while (s--) sleep_ms(1000); return 0; }
int sched_yield(void) { sleep_ms(1); return 0; }
static int ms_until(const struct timespec *until) {         /* >= 0; until: CLOCK_REALTIME, as POSIX has it */
    struct timespec now; clock_gettime(CLOCK_REALTIME, &now);
    int64_t d = ((int64_t)(until->tv_sec - now.tv_sec) * 1000000 + (until->tv_nsec - now.tv_nsec) / 1000 + 999) / 1000;
    return d < 0 ? 0 : d > 0x3fffffff ? 0x3fffffff : (int)d;
}

/* ---- mutexes: a semaphore of one, made at the first lock (so the static initialisers work) */
static int sem_of(int *sem, int count, int max) {
    if (*sem <= 0) { unsigned st = wup_irq_off(); if (*sem <= 0) *sem = wup_sem_new(count, max); wup_irq_restore(st); }
    return *sem;
}
static int lock(pthread_mutex_t *m, int type, int ms) {
    unsigned self = wup__task();
    if (sem_of(&m->sem, 1, 1) <= 0) return ENOMEM;
    if (m->count && m->owner == self) {
        if (type == PTHREAD_MUTEX_RECURSIVE) { m->count++; return 0; }
        if (type == PTHREAD_MUTEX_ERRORCHECK || !ms) return ms ? EDEADLK : EBUSY;
    }
    if (wup_sem_wait(m->sem, ms)) return EBUSY;
    m->owner = self; m->count = 1;
    return 0;
}
static int unlock(pthread_mutex_t *m) {
    if (!m->count || m->owner != wup__task()) return EPERM;
    if (--m->count) return 0;
    m->owner = 0;
    wup_sem_signal(m->sem);
    return 0;
}
int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a) { memset(m, 0, sizeof *m); m->type = a ? a->type : PTHREAD_MUTEX_DEFAULT; return 0; }
int pthread_mutex_destroy(pthread_mutex_t *m) { if (m->sem > 0) wup_sem_free(m->sem); m->sem = 0; return 0; }
int pthread_mutex_lock(pthread_mutex_t *m) { return lock(m, m->type, -1); }
int pthread_mutex_trylock(pthread_mutex_t *m) { return lock(m, m->type, 0); }
int pthread_mutex_unlock(pthread_mutex_t *m) { return unlock(m); }
int pthread_mutexattr_init(pthread_mutexattr_t *a) { a->type = PTHREAD_MUTEX_DEFAULT; return 0; }
int pthread_mutexattr_destroy(pthread_mutexattr_t *a) { (void)a; return 0; }
int pthread_mutexattr_settype(pthread_mutexattr_t *a, int type) { a->type = type; return 0; }

/* ---- the C library's locks */
struct __lock { pthread_mutex_t m; };
struct __lock __lock___libc_recursive_mutex;
void __retarget_lock_init(_LOCK_T *l) { *l = calloc(1, sizeof **l); }
void __retarget_lock_init_recursive(_LOCK_T *l) { *l = calloc(1, sizeof **l); }
void __retarget_lock_close(_LOCK_T l) { if (l) { pthread_mutex_destroy(&l->m); free(l); } }
void __retarget_lock_close_recursive(_LOCK_T l) { __retarget_lock_close(l); }
void __retarget_lock_acquire(_LOCK_T l) { if (l) lock(&l->m, PTHREAD_MUTEX_NORMAL, -1); }
void __retarget_lock_acquire_recursive(_LOCK_T l) { if (l) lock(&l->m, PTHREAD_MUTEX_RECURSIVE, -1); }
void __retarget_lock_release(_LOCK_T l) { if (l) unlock(&l->m); }
void __retarget_lock_release_recursive(_LOCK_T l) { if (l) unlock(&l->m); }

/* ---- condition variables: who waits is counted; a signal gives the semaphore one for one of them */
static int cond_wait(pthread_cond_t *c, pthread_mutex_t *m, int ms) {
    if (sem_of(&c->sem, 0, 30000) <= 0) return ENOMEM;
    unsigned st = wup_irq_off(); c->waiters++; wup_irq_restore(st);
    int count = m->count, r = 0;
    m->count = 1; unlock(m);                           /* (all the way, were it locked several times) */
    if (wup_sem_wait(c->sem, ms)) {
        st = wup_irq_off();
        if (c->waiters > 0) { c->waiters--; r = ETIMEDOUT; }
        wup_irq_restore(st);
        if (!r) wup_sem_wait(c->sem, -1);              /* signalled at that very moment: it is on its way */
    }
    lock(m, PTHREAD_MUTEX_NORMAL, -1); m->count = count;
    return r;
}
int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a) { (void)a; c->sem = 0; c->waiters = 0; return 0; }
int pthread_cond_destroy(pthread_cond_t *c) { if (c->sem > 0) wup_sem_free(c->sem); c->sem = 0; return 0; }
int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) { return cond_wait(c, m, -1); }
int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *until) { int ms = ms_until(until); return cond_wait(c, m, ms ? ms : 1); }
int pthread_cond_signal(pthread_cond_t *c) {
    unsigned st = wup_irq_off(); int w = c->waiters; if (w) c->waiters--; wup_irq_restore(st);
    if (w) wup_sem_signal(c->sem);
    return 0;
}
int pthread_cond_broadcast(pthread_cond_t *c) {
    unsigned st = wup_irq_off(); int w = c->waiters; c->waiters = 0; wup_irq_restore(st);
    while (w-- > 0) wup_sem_signal(c->sem);
    return 0;
}
int pthread_condattr_init(pthread_condattr_t *a) { (void)a; return 0; }
int pthread_condattr_destroy(pthread_condattr_t *a) { (void)a; return 0; }

/* ---- threads. pthread_t is libwup's handle + 1 (0: the main thread) */
static struct { void *(*fn)(void *); void *arg, *ret; jmp_buf out; volatile int ready; } pt[WUP_THREADS];
static struct { void (*destructor)(void *); int used; } keys[PTHREAD_KEYS_MAX];
static void *vals[WUP_THREADS + 1][PTHREAD_KEYS_MAX];
static void run(void *arg) {
    (void)arg;
    int i = wup_thread_self();
    while (!pt[i].ready) wup_thread_sleep(1);          /* (until pthread_create has written down what to run) */
    if (!setjmp(pt[i].out)) pt[i].ret = pt[i].fn(pt[i].arg);
    for (int k = 0; k < PTHREAD_KEYS_MAX; k++) {
        void *v = vals[i + 1][k]; vals[i + 1][k] = 0;
        if (v && keys[k].used && keys[k].destructor) keys[k].destructor(v);
    }
    pt[i].ready = 0;
}
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*fn)(void *), void *arg) {
    static pthread_mutex_t one;                        /* (a handle is free again, and its record ours, only under this) */
    lock(&one, PTHREAD_MUTEX_NORMAL, -1);
    int h = wup_thread_start(run, 0, attr && attr->stack_size ? attr->stack_size : 8192, 0);
    if (h >= 0) {
        pt[h].fn = fn; pt[h].arg = arg; pt[h].ret = 0;
        if (attr && attr->detached) wup_thread_detach(h);
        pt[h].ready = 1;
        *thread = (pthread_t)h + 1;
    }
    unlock(&one);
    return h < 0 ? EAGAIN : 0;
}
int pthread_join(pthread_t t, void **ret) {
    if (!t || t > WUP_THREADS) return ESRCH;
    if (wup_thread_join((int)t - 1, -1)) return ESRCH;
    if (ret) *ret = pt[t - 1].ret;
    return 0;
}
int pthread_detach(pthread_t t) { if (!t || t > WUP_THREADS) return ESRCH; wup_thread_detach((int)t - 1); return 0; }
pthread_t pthread_self(void) { return (pthread_t)(wup_thread_self() + 1); }
int pthread_equal(pthread_t a, pthread_t b) { return a == b; }
void pthread_exit(void *ret) {
    int i = wup_thread_self();
    if (i < 0) exit(0);
    pt[i].ret = ret;
    longjmp(pt[i].out, 1);
}
int pthread_attr_init(pthread_attr_t *a) { a->stack_size = 0; a->detached = 0; return 0; }
int pthread_attr_destroy(pthread_attr_t *a) { (void)a; return 0; }
int pthread_attr_setstacksize(pthread_attr_t *a, size_t size) { if (size < PTHREAD_STACK_MIN) return EINVAL; a->stack_size = (unsigned)size; return 0; }
int pthread_attr_getstacksize(const pthread_attr_t *a, size_t *size) { *size = a->stack_size ? a->stack_size : 8192; return 0; }
int pthread_attr_setdetachstate(pthread_attr_t *a, int state) { a->detached = state == PTHREAD_CREATE_DETACHED; return 0; }

int pthread_once(pthread_once_t *once, void (*fn)(void)) {
    if (once->state == 2) return 0;
    lock(&once->lock, PTHREAD_MUTEX_NORMAL, -1);
    if (once->state != 2) { fn(); once->state = 2; }
    unlock(&once->lock);
    return 0;
}
int pthread_key_create(pthread_key_t *key, void (*destructor)(void *)) {
    unsigned st = wup_irq_off(); int k = 0;
    while (k < PTHREAD_KEYS_MAX && keys[k].used) k++;
    if (k < PTHREAD_KEYS_MAX) { keys[k].used = 1; keys[k].destructor = destructor; for (int i = 0; i <= WUP_THREADS; i++) vals[i][k] = 0; }
    wup_irq_restore(st);
    if (k == PTHREAD_KEYS_MAX) return EAGAIN;
    *key = (pthread_key_t)k;
    return 0;
}
int pthread_key_delete(pthread_key_t k) { if (k >= PTHREAD_KEYS_MAX) return EINVAL; keys[k].used = 0; return 0; }
void *pthread_getspecific(pthread_key_t k) { return k < PTHREAD_KEYS_MAX ? vals[pthread_self()][k] : 0; }
int pthread_setspecific(pthread_key_t k, const void *v) { if (k >= PTHREAD_KEYS_MAX) return EINVAL; vals[pthread_self()][k] = (void *)v; return 0; }

/* ---- <semaphore.h> */
int sem_init(sem_t *s, int shared, unsigned value) { (void)shared; *s = wup_sem_new((int)value, 30000); if (*s <= 0) { errno = ENOSPC; return -1; } return 0; }
int sem_destroy(sem_t *s) { wup_sem_free(*s); *s = 0; return 0; }
int sem_wait(sem_t *s) { return wup_sem_wait(*s, -1) ? (errno = EINVAL, -1) : 0; }
int sem_trywait(sem_t *s) { return wup_sem_wait(*s, 0) ? (errno = EAGAIN, -1) : 0; }
int sem_timedwait(sem_t *s, const struct timespec *until) { int ms = ms_until(until); return wup_sem_wait(*s, ms ? ms : 1) ? (errno = ETIMEDOUT, -1) : 0; }
int sem_post(sem_t *s) { wup_sem_signal(*s); return 0; }
