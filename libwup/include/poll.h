#pragma once
#include <sys/socket.h>                /* struct pollfd, POLLIN..., nfds_t: lwIP's */
#ifdef __cplusplus
extern "C" {
#endif
int poll(struct pollfd *fds, nfds_t n, int timeout_ms);   /* -1: wait for ever */
#ifdef __cplusplus
}
#endif
