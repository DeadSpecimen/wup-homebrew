/* the transport menu/ftpd.c uses when built with -DFTP_HOST (tools/ftptest/ftphost.c): plain sockets on 127.0.0.1 */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define QCAP 65536
struct ep { int fd, lst, closing; unsigned char q[QCAP]; int qn; };
static struct ep *all[64];
static ep_t *mk(int fd, int lst) {
    for (int i = 0; i < 64; i++) if (!all[i]) { ep_t *e = calloc(1, sizeof *e); e->fd = fd; e->lst = lst; fcntl(fd, F_SETFL, O_NONBLOCK); return all[i] = e; }
    close(fd); return 0;
}
static ep_t *io_listen(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0), one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons((unsigned short)port), .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
    if (bind(fd, (struct sockaddr *)&a, sizeof a) || listen(fd, 4)) { close(fd); return 0; }
    return mk(fd, 1);
}
static ep_t *io_accept(ep_t *l) { int fd = accept(l->fd, 0, 0); return fd < 0 ? 0 : mk(fd, 0); }
static int io_recv(ep_t *e, void *buf, int max) {
    int n = (int)recv(e->fd, buf, (size_t)max, 0);
    return n > 0 ? n : n == 0 ? -1 : (errno == EAGAIN || errno == EWOULDBLOCK) ? 0 : -1;
}
static int io_room(ep_t *e) { return QCAP - e->qn; }
static int io_send(ep_t *e, const void *d, int n) { if (n > QCAP - e->qn) n = QCAP - e->qn; memcpy(e->q + e->qn, d, (size_t)n); e->qn += n; return n; }
static void pump1(ep_t *e) {
    while (e->qn) { int n = (int)send(e->fd, e->q, (size_t)e->qn, 0); if (n <= 0) break; memmove(e->q, e->q + n, (size_t)(e->qn - n)); e->qn -= n; }
}
static void io_flush(ep_t *e) { pump1(e); }
static void io_close(ep_t *e) { if (e->lst) e->qn = 0; e->closing = 1; }
static u32 io_local_ip(ep_t *e) { struct sockaddr_in a; socklen_t n = sizeof a; getsockname(e->fd, (struct sockaddr *)&a, &n); return a.sin_addr.s_addr; }
static u32 io_ms(void) { struct timeval t; gettimeofday(&t, 0); return (u32)(t.tv_sec * 1000 + t.tv_usec / 1000); }
void ftp_host_pump(void) {                         /* queued data out; closed endpoints go once it is sent */
    for (int i = 0; i < 64; i++) if (all[i]) {
        ep_t *e = all[i]; pump1(e);
        if (e->closing && !e->qn) { close(e->fd); free(e); all[i] = 0; }
    }
}
