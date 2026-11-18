/* BSD sockets for GamePad apps: lwIP's sockets (IPv4 TCP and UDP over the pad's Wi-Fi, and 127.0.0.1) under their
 * standard names. app.cfg: net = yes; call wup_net_init() once first (wup.h). Usable from any thread. */
#pragma once
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <lwip/sockets.h>              /* the structures and constants */
#ifdef __cplusplus
extern "C" {
#endif
#if !LWIP_IPV6                         /* IPv4 only, but portable code keeps addresses in the IPv6 structure (it is the larger) */
struct sockaddr_in6 {
    u8_t sin6_len; sa_family_t sin6_family; in_port_t sin6_port; u32_t sin6_flowinfo; struct in6_addr sin6_addr; u32_t sin6_scope_id;
};
#endif
#ifndef SOMAXCONN
#define SOMAXCONN 4
#endif
int socket(int domain, int type, int protocol);
int bind(int fd, const struct sockaddr *addr, socklen_t len);
int listen(int fd, int backlog);
int accept(int fd, struct sockaddr *addr, socklen_t *len);
int connect(int fd, const struct sockaddr *addr, socklen_t len);
ssize_t send(int fd, const void *buf, size_t n, int flags);
ssize_t recv(int fd, void *buf, size_t n, int flags);
ssize_t sendto(int fd, const void *buf, size_t n, int flags, const struct sockaddr *to, socklen_t tolen);
ssize_t recvfrom(int fd, void *buf, size_t n, int flags, struct sockaddr *from, socklen_t *fromlen);
int shutdown(int fd, int how);
int setsockopt(int fd, int level, int name, const void *val, socklen_t len);
int getsockopt(int fd, int level, int name, void *val, socklen_t *len);
int getsockname(int fd, struct sockaddr *addr, socklen_t *len);
int getpeername(int fd, struct sockaddr *addr, socklen_t *len);
int ioctl(int fd, long cmd, void *arg);          /* FIONBIO, FIONREAD */
#ifdef __cplusplus
}
#endif
