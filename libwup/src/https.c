/* https: HTTPS GET for apps (include/wup_https.h).
 *   TLS        BearSSL, in a small profile (profile(): TLS 1.2, ECDHE with ChaCha20-Poly1305 or AES-GCM, RSA and ECDSA
 *              certificates, BearSSL's compact i15/m15/ct code), checked against the roots and the clock
 *   HTTP       picohttpparser: the status line, the headers, chunked bodies; redirects followed (up to 5)
 *   TCP        lwIP's sockets (the app's network: net = yes)
 *   randomness the microsecond timer read around sleeps and every network event, mixed with SHA-256 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#include <unistd.h>
#include "wup.h"
#include "wup_https.h"
#include "picohttpparser.h"
#ifdef WUP_HTTPS_TEST_ROOTS                           /* (a test build: the default roots and a test server's CA) */
#include "../build/https/wup_roots_test.h"
#else
#include "wup_roots.h"
#endif

static br_ssl_client_context sc;
static br_x509_minimal_context xc;
static br_sslio_context ioc;
static int sock = -1, timeout, last_tls_error;
static void (*idle)(void);
int wup_https_tls_error(void) { return last_tls_error; }

/* ---- randomness: a pool stirred with the free-running microsecond timer */
static br_sha256_context pool;
static int pool_on;
static void stir(const void *p, size_t n) {
    if (!pool_on) { br_sha256_init(&pool); pool_on = 1; }
    uint32_t t = wup_us();
    br_sha256_update(&pool, &t, sizeof t);
    if (p) br_sha256_update(&pool, p, n);
}
static void seed(uint8_t out[32]) {
    for (int i = 0; i < 64; i++) {                    /* the timer around short sleeps: the scheduler's jitter */
        uint32_t a = wup_us(); wup_thread_sleep(1); uint32_t d = wup_us() - a; stir(&d, sizeof d);
    }
    time_t now = time(NULL); stir(&now, sizeof now);
    br_sha256_context c = pool; br_sha256_out(&c, out);
    stir(out, 32);                                    /* (the next seed differs) */
}

/* ---- TCP, and BearSSL's I/O on it (blocking: bytes, or -1 for an error or the end) */
static int tcp_open(const char *host, int port) {
    char ps[8]; struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM }, *ai;
    snprintf(ps, sizeof ps, "%d", port);
    if (getaddrinfo(host, ps, &hints, &ai) || !ai) return WUP_HTTPS_EDNS;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    int r = sock < 0 || connect(sock, ai->ai_addr, ai->ai_addrlen) ? WUP_HTTPS_ECONNECT : 0;
    freeaddrinfo(ai);
    if (r && sock >= 0) { close(sock); sock = -1; }
    stir(&r, sizeof r);
    return r;
}
static void tcp_close(void) { if (sock >= 0) { close(sock); sock = -1; } }
static int low_read(void *ctx, unsigned char *buf, size_t len) {
    (void)ctx;
    struct pollfd p = { sock, POLLIN, 0 };
    for (int waited = 0;; waited += 20) {            /* (in short waits: the caller's idle runs between them) */
        int r = poll(&p, 1, idle ? 20 : timeout);
        if (r > 0) break;
        if (r < 0 || !idle || waited >= timeout) return -1;
        idle();
    }
    int r = (int)recv(sock, buf, len, 0);
    stir(&r, sizeof r);
    if (idle && r > 0) idle();
    return r > 0 ? r : -1;
}
static int low_write(void *ctx, const unsigned char *buf, size_t len) {
    (void)ctx;
    int r = (int)send(sock, buf, len, 0);
    stir(&r, sizeof r);
    return r > 0 ? r : -1;
}

/* the TLS client: like BearSSL's br_ssl_client_init_full (src/ssl/ssl_client_full.c), with only the modern suites */
static void profile(const br_x509_trust_anchor *tas, size_t ntas) {
    static const uint16_t suites[] = {
        BR_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, BR_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
        BR_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, BR_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
        BR_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384, BR_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
    };
    static const br_hash_class *const hashes[] = { &br_sha256_vtable, &br_sha384_vtable, &br_sha512_vtable };
    br_ssl_client_zero(&sc);
    br_ssl_engine_set_versions(&sc.eng, BR_TLS12, BR_TLS12);
    br_x509_minimal_init(&xc, &br_sha256_vtable, tas, ntas);
    br_ssl_engine_set_suites(&sc.eng, suites, sizeof suites / sizeof suites[0]);
    br_ssl_engine_set_ec(&sc.eng, &br_ec_all_m15);
    br_ssl_engine_set_rsavrfy(&sc.eng, br_rsa_i15_pkcs1_vrfy);
    br_ssl_engine_set_ecdsa(&sc.eng, br_ecdsa_i15_vrfy_asn1);
    br_x509_minimal_set_rsa(&xc, br_rsa_i15_pkcs1_vrfy);
    br_x509_minimal_set_ecdsa(&xc, &br_ec_all_m15, br_ecdsa_i15_vrfy_asn1);
    for (size_t i = 0; i < sizeof hashes / sizeof hashes[0]; i++) {
        int id = (int)((hashes[i]->desc >> BR_HASHDESC_ID_OFF) & BR_HASHDESC_ID_MASK);
        br_ssl_engine_set_hash(&sc.eng, id, hashes[i]); br_x509_minimal_set_hash(&xc, id, hashes[i]);
    }
    br_ssl_engine_set_x509(&sc.eng, &xc.vtable);
    br_ssl_engine_set_prf_sha256(&sc.eng, &br_tls12_sha256_prf);
    br_ssl_engine_set_prf_sha384(&sc.eng, &br_tls12_sha384_prf);
    br_ssl_engine_set_gcm(&sc.eng, &br_sslrec_in_gcm_vtable, &br_sslrec_out_gcm_vtable);
    br_ssl_engine_set_aes_ctr(&sc.eng, &br_aes_ct_ctr_vtable);
    br_ssl_engine_set_ghash(&sc.eng, br_ghash_ctmul32);
    br_ssl_engine_set_chapol(&sc.eng, &br_sslrec_in_chapol_vtable, &br_sslrec_out_chapol_vtable);
    br_ssl_engine_set_chacha20(&sc.eng, br_chacha20_ct_run);
    br_ssl_engine_set_poly1305(&sc.eng, br_poly1305_ctmul32_run);
}

/* https://host[:port]/path -> its parts; 0 = it is one */
static int split(const char *url, char *host, size_t nh, int *port, const char **path) {
    if (strncmp(url, "https://", 8)) return -1;
    const char *h = url + 8, *e = h;
    while (*e && *e != '/' && *e != ':' && *e != '?') e++;
    if (e == h || (size_t)(e - h) >= nh) return -1;
    memcpy(host, h, (size_t)(e - h)); host[e - h] = 0;
    *port = 443;
    if (*e == ':') { *port = atoi(e + 1); while (*e && *e != '/' && *e != '?') e++; }
    *path = *e ? e : "/";
    return 0;
}
static const struct phr_header *find(const struct phr_header *h, size_t n, const char *name) {
    size_t k = strlen(name);
    for (size_t i = 0; i < n; i++) if (h[i].name_len == k && !strncasecmp(h[i].name, name, k)) return &h[i];
    return 0;
}

#define HEAD_MAX 8192
int wup_https_get(const char *url, const wup_https_opts *o, wup_https_body_fn body, void *arg) {
    static const wup_https_opts none;
    if (!o) o = &none;
    if (!wup_time_known()) return WUP_HTTPS_ENOTIME;
    timeout = o->timeout_ms ? o->timeout_ms : 20000;
    idle = o->idle;
    const br_x509_trust_anchor *tas = o->tas ? o->tas : wup_roots;
    size_t ntas = o->tas ? o->ntas : WUP_ROOTS_NUM;
    int status = WUP_HTTPS_EREDIRECT;
    char *where = malloc(1024), *head = malloc(HEAD_MAX);
    unsigned char *iobuf = malloc(BR_SSL_BUFSIZE_MONO);       /* (half duplex: a request, then its response) */
    if (!where || !head || !iobuf) { free(where); free(head); free(iobuf); return WUP_HTTPS_ENOMEM; }
    snprintf(where, 1024, "%s", url);
    for (int hop = 0; hop < 6; hop++) {
        char host[256]; int port; const char *path;
        if (split(where, host, sizeof host, &port, &path)) { status = WUP_HTTPS_EURL; break; }
        stir(host, strlen(host));
        int r = tcp_open(host, port);
        if (r) { status = r; break; }
        /* TLS: the profile, the roots, the clock (days since year 0); then the request */
        profile(tas, ntas);
        time_t now = time(NULL);
        br_x509_minimal_set_time(&xc, (uint32_t)(now / 86400 + 719528), (uint32_t)(now % 86400));
        br_ssl_engine_set_buffer(&sc.eng, iobuf, BR_SSL_BUFSIZE_MONO, 0);
        uint8_t s[32]; seed(s); br_ssl_engine_inject_entropy(&sc.eng, s, sizeof s); memset(s, 0, sizeof s);
        br_ssl_client_reset(&sc, host, 0);
        br_sslio_init(&ioc, &sc.eng, low_read, 0, low_write, 0);
        int n = snprintf(head, HEAD_MAX, "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: wup-homebrew\r\nAccept-Encoding: identity\r\n"
                         "Connection: close\r\n%s\r\n", path, host, o->headers ? o->headers : "");
        if (br_sslio_write_all(&ioc, head, (size_t)n) || br_sslio_flush(&ioc)) {
            last_tls_error = br_ssl_engine_last_error(&sc.eng); tcp_close(); status = WUP_HTTPS_ETLS; break;
        }
        /* the status line and the headers */
        size_t have = 0; int hl = -2, minor, code = 0; const char *msg; size_t msg_len;
        struct phr_header hs[48]; size_t nh = 48;
        while (hl == -2 && have < HEAD_MAX) {
            int k = br_sslio_read(&ioc, head + have, HEAD_MAX - have);
            if (k < 0) break;
            have += (size_t)k; nh = 48;
            hl = phr_parse_response(head, have, &minor, &code, &msg, &msg_len, hs, &nh, have - (size_t)k);
        }
        if (hl < 0) {
            last_tls_error = br_ssl_engine_last_error(&sc.eng);
            status = last_tls_error ? WUP_HTTPS_ETLS : WUP_HTTPS_EHTTP; tcp_close(); break;
        }
        const struct phr_header *loc = find(hs, nh, "Location"), *te = find(hs, nh, "Transfer-Encoding"), *cl = find(hs, nh, "Content-Length");
        if (code >= 300 && code < 400 && loc) {        /* redirected: an absolute URL, or a path on the same host */
            char next[1024];
            if (loc->value_len >= 8 && !strncmp(loc->value, "https://", 8)) snprintf(next, sizeof next, "%.*s", (int)loc->value_len, loc->value);
            else snprintf(next, sizeof next, "https://%s:%d%.*s", host, port, (int)loc->value_len, loc->value);
            snprintf(where, 1024, "%s", next);
            tcp_close();
            continue;
        }
        /* the body: chunked, a known length, or up to the end */
        int chunked = te && te->value_len >= 7 && !strncasecmp(te->value, "chunked", 7);
        long long total = cl && !chunked ? atoll(cl->value) : -1, got = 0;
        struct phr_chunked_decoder dec; memset(&dec, 0, sizeof dec); dec.consume_trailer = 1;
        status = code;
        size_t k = have - (size_t)hl;                  /* (what came with the headers) */
        memmove(head, head + hl, k);
        for (int done = 0;;) {
            if (k && chunked) {
                ssize_t c = phr_decode_chunked(&dec, head, &k);
                if (c == -1) { status = WUP_HTTPS_EHTTP; break; }
                done = c >= 0;
            }
            if (k && body && body(arg, (const uint8_t *)head, k, total)) { status = WUP_HTTPS_ESTOPPED; break; }
            got += (long long)k;
            if (!chunked && total >= 0 && got >= total) done = 1;
            if (done) break;
            int r2 = br_sslio_read(&ioc, head, HEAD_MAX);
            if (r2 < 0) {                              /* the end: fine without a length, else cut short */
                if (chunked || (total >= 0 && got < total)) status = WUP_HTTPS_ETIMEOUT;
                break;
            }
            k = (size_t)r2;
        }
        tcp_close();
        break;
    }
    free(where); free(head); free(iobuf);
    return status;
}
