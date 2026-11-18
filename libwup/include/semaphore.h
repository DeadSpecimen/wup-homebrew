#pragma once
/* <semaphore.h>: POSIX counting semaphores (unnamed), on libwup's (kernel.c). */
#include <time.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef int sem_t;
int sem_init(sem_t *s, int shared, unsigned value);
int sem_destroy(sem_t *s);
int sem_wait(sem_t *s);
int sem_trywait(sem_t *s);
int sem_timedwait(sem_t *s, const struct timespec *until);    /* CLOCK_REALTIME */
int sem_post(sem_t *s);
#ifdef __cplusplus
}
#endif
