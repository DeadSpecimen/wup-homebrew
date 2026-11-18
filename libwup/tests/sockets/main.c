/* Sockets test: BSD sockets (lwIP's, src/net.c) with threads, over 127.0.0.1: works in the emulator (which has no
 * Wi-Fi) and on the pad. One line per check. */
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include "wup.h"

WUP_APP("Sockets Test");
static int row, failed;
static void check(const char *what, int ok) {
    char l[120]; snprintf(l, sizeof l, "%s  %s", ok ? "ok  " : "FAIL", what);
    gfx_text(&font_body, 40, 50 + 30 * row++, l, ok ? RGB(200, 230, 200) : RGB(255, 120, 120));
    printf("sockets test: %s\n", l);
    gfx_present_all();
    if (!ok) failed++;
}
static struct sockaddr_in local(int port) {
    struct sockaddr_in a; memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_port = htons((uint16_t)port); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return a;
}
static int srv_ready, srv_bytes; static unsigned srv_sum;
static void *echo_server(void *arg) {                     /* accepts once: echoes until the peer closes its side */
    int one = 1, ls = socket(AF_INET, SOCK_STREAM, 0); struct sockaddr_in a = local((int)(intptr_t)arg); static char buf[2048];
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    if (ls < 0 || bind(ls, (struct sockaddr *)&a, sizeof a) || listen(ls, 2)) { srv_ready = -1; return 0; }
    srv_ready = 1;
    struct sockaddr_in peer; socklen_t pl = sizeof peer;
    int c = accept(ls, (struct sockaddr *)&peer, &pl);
    setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);   /* (else each 1500-byte echo waits for a delayed ACK) */
    for (ssize_t n; c >= 0 && (n = recv(c, buf, sizeof buf, 0)) > 0; ) {
        for (ssize_t i = 0; i < n; i++) srv_sum = srv_sum * 31 + (unsigned char)buf[i];
        srv_bytes += (int)n;
        for (ssize_t o = 0, w; o < n; o += w) if ((w = send(c, buf + o, (size_t)(n - o), 0)) <= 0) { n = 0; break; }
    }
    close(c); close(ls);
    return (void *)(intptr_t)(peer.sin_addr.s_addr == htonl(INADDR_LOOPBACK));
}
static void *udp_echo(void *arg) {
    int s = socket(AF_INET, SOCK_DGRAM, 0); struct sockaddr_in a = local((int)(intptr_t)arg), from; char b[64];
    if (bind(s, (struct sockaddr *)&a, sizeof a)) { srv_ready = -1; return 0; }
    srv_ready = 2;
    for (int i = 0; i < 20; i++) {
        socklen_t fl = sizeof from; ssize_t n = recvfrom(s, b, sizeof b, 0, (struct sockaddr *)&from, &fl);
        if (n > 0) sendto(s, b, (size_t)n, 0, (struct sockaddr *)&from, fl);
    }
    close(s);
    return 0;
}

int main(void) {
    wup_screen();
    gfx_fill(0, 0, GFX_W, GFX_H, RGB(16, 20, 32));
    char l[110]; pthread_t t; void *ret = 0;
    int wifi = wup_net_init();
    snprintf(l, sizeof l, "the stack starts (Wi-Fi: %s)", wifi ? "not connected, 127.0.0.1 only" : wup_net_ip_str());
    check(l, 1);

    pthread_create(&t, 0, echo_server, (void *)7000);
    for (int i = 0; i < 200 && !srv_ready; i++) usleep(5000);
    int c = socket(AF_INET, SOCK_STREAM, 0), one = 1; struct sockaddr_in a = local(7000);
    int r = connect(c, (struct sockaddr *)&a, sizeof a);
    snprintf(l, sizeof l, "a TCP connection to a server thread (socket %d, connect %d, errno %d)", c, r, r ? errno : 0);
    check(l, srv_ready == 1 && c >= 3 && !r);
    setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);

    static char out[1500], in[1500]; unsigned sum = 0, back = 0; int sent = 0, got = 0; uint32_t t0 = wup_us();
    for (int k = 0; k < 140; k++) {                       /* 210 KB there and back */
        for (unsigned i = 0; i < sizeof out; i++) { out[i] = (char)(k * 7 + i * 13); sum = sum * 31 + (unsigned char)out[i]; }
        for (size_t o = 0; o < sizeof out; ) { ssize_t w = send(c, out + o, sizeof out - o, 0); if (w <= 0) break; o += (size_t)w; sent += (int)w; }
        for (size_t o = 0; o < sizeof in; ) { ssize_t n = recv(c, in + o, sizeof in - o, 0); if (n <= 0) break; for (ssize_t i = 0; i < n; i++) back = back * 31 + (unsigned char)in[o + (size_t)i]; o += (size_t)n; got += (int)n; }
    }
    unsigned ms = (wup_us() - t0) / 1000;
    snprintf(l, sizeof l, "%d KB echoed without a wrong byte (%u ms)", got / 1024, ms);
    check(l, sent == 210000 && got == sent && back == sum);

    struct pollfd pf = { c, POLLIN, 0 }; t0 = wup_us();
    r = poll(&pf, 1, 60); ms = (wup_us() - t0) / 1000;
    fd_set rs; FD_ZERO(&rs); FD_SET(c, &rs); struct timeval tv = { 0, 30000 };
    int r2 = select(c + 1, &rs, 0, 0, &tv);
    send(c, "x", 1, 0); pf.revents = 0;
    int r3 = poll(&pf, 1, 1000);
    snprintf(l, sizeof l, "poll and select: nothing to read times out (%u ms of 60), then data is seen", ms);
    check(l, r == 0 && r2 == 0 && ms >= 50 && ms < 120 && r3 == 1 && (pf.revents & POLLIN) && recv(c, in, 1, 0) == 1);

    fcntl(c, F_SETFL, fcntl(c, F_GETFL, 0) | O_NONBLOCK);
    ssize_t n = recv(c, in, 1, 0); int e = errno;
    fcntl(c, F_SETFL, fcntl(c, F_GETFL, 0) & ~O_NONBLOCK);
    struct timeval rt = { 0, 50000 }; setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &rt, sizeof rt);
    t0 = wup_us(); ssize_t n2 = recv(c, in, 1, 0); int e2 = errno; ms = (wup_us() - t0) / 1000;
    snprintf(l, sizeof l, "a non-blocking read says EAGAIN; SO_RCVTIMEO ends a read (%u ms of 50)", ms);
    check(l, n == -1 && (e == EAGAIN || e == EWOULDBLOCK) && n2 == -1 && (e2 == EAGAIN || e2 == EWOULDBLOCK) && ms >= 40 && ms < 110);

    shutdown(c, SHUT_WR);
    rt.tv_usec = 0; rt.tv_sec = 2; setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &rt, sizeof rt);
    n = recv(c, in, 1, 0);
    pthread_join(t, &ret); close(c);
    snprintf(l, sizeof l, "closing: the server saw %d bytes, the same ones, from 127.0.0.1", srv_bytes);
    check(l, n == 0 && srv_bytes == sent + 1 && ret == (void *)1);

    srv_ready = 0; pthread_create(&t, 0, udp_echo, (void *)7001);
    for (int i = 0; i < 200 && !srv_ready; i++) usleep(5000);
    int u = socket(AF_INET, SOCK_DGRAM, 0), okn = 0; a = local(7001);
    rt.tv_sec = 1; setsockopt(u, SOL_SOCKET, SO_RCVTIMEO, &rt, sizeof rt);
    for (int i = 0; i < 20; i++) {
        char m[32], b[32]; int len = snprintf(m, sizeof m, "datagram %d", i);
        sendto(u, m, (size_t)len, 0, (struct sockaddr *)&a, sizeof a);
        struct sockaddr_in from; socklen_t fl = sizeof from;
        if (recvfrom(u, b, sizeof b, 0, (struct sockaddr *)&from, &fl) == len && !memcmp(m, b, (size_t)len) && from.sin_port == htons(7001)) okn++;
    }
    pthread_join(t, 0); close(u);
    snprintf(l, sizeof l, "UDP: %d of 20 datagrams there and back", okn);
    check(l, okn == 20);

    struct addrinfo hints, *ai = 0; memset(&hints, 0, sizeof hints); hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    r = getaddrinfo("192.168.4.20", "47989", &hints, &ai); char txt[INET_ADDRSTRLEN] = "";
    if (!r && ai) inet_ntop(AF_INET, &((struct sockaddr_in *)ai->ai_addr)->sin_addr, txt, sizeof txt);
    int port = !r && ai ? ntohs(((struct sockaddr_in *)ai->ai_addr)->sin_port) : 0;
    if (ai) freeaddrinfo(ai);
    struct in_addr ia; int p1 = inet_pton(AF_INET, "10.1.2.3", &ia);
    snprintf(l, sizeof l, "getaddrinfo, inet_ntop, inet_pton (%s:%d)", txt, port);
    check(l, !r && !strcmp(txt, "192.168.4.20") && port == 47989 && p1 == 1 && ia.s_addr == htonl(0x0a010203));

    int fds[10], made = 0; for (int i = 0; i < 10; i++) { fds[i] = socket(AF_INET, i & 1 ? SOCK_DGRAM : SOCK_STREAM, 0); made += fds[i] >= 0; }
    for (int i = 0; i < 10; i++) close(fds[i]);
    FILE *f = fopen("sockets.tmp", "wb"); int fok = f && fputs("files still work", f) >= 0; if (f) fclose(f);
    remove("sockets.tmp");
    int again = socket(AF_INET, SOCK_STREAM, 0); close(again);
    snprintf(l, sizeof l, "ten sockets opened and closed (%d), the first number again (%d); files beside them", made, again);
    check(l, made == 10 && again == fds[0] && fok);

    gfx_text(&font_body, 40, 60 + 30 * row, failed ? "Some checks failed.   B returns to the menu" : "All checks passed.   B returns to the menu", RGB(255, 255, 255));
    gfx_present_all();
    printf("sockets test: %s\n", failed ? "FAILED" : "all passed");
    while (wup_buttons() & WUP_B) wup_sleep(16);
    while (!(wup_buttons() & WUP_B)) wup_sleep(16);
    return failed;
}
