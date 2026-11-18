/* lwIP architecture glue for the GamePad (ARM926, little endian, picolibc). */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <fcntl.h>                         /* (O_NONBLOCK, F_GETFL: the C library's values, in lwIP's sockets too) */
#include <sys/time.h>                      /* struct timeval */
#include <sys/select.h>                    /* fd_set */
void wup_net_assert(const char *msg, const char *file, int line);
#define LWIP_PLATFORM_DIAG(x)              do { } while (0)
#define LWIP_PLATFORM_ASSERT(x)            wup_net_assert((x), __FILE__, __LINE__)
