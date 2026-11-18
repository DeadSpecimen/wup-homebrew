/* lwIP's operating-system layer on libwup's threads and semaphores (src/sys_arch.c). */
#pragma once
typedef int sys_sem_t;                     /* libwup's handles: > 0 */
typedef int sys_mutex_t;
typedef struct wup_mbox *sys_mbox_t;
typedef int sys_thread_t;
typedef unsigned sys_prot_t;
#define sys_sem_valid(s)          (*(s) > 0)
#define sys_sem_set_invalid(s)    (*(s) = 0)
#define sys_mutex_valid(m)        (*(m) > 0)
#define sys_mutex_set_invalid(m)  (*(m) = 0)
#define sys_mbox_valid(b)         (*(b) != 0)
#define sys_mbox_set_invalid(b)   (*(b) = 0)
