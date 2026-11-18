/* HTTPS GET for apps (app.cfg: https = yes, with net = yes). TLS is BearSSL's (a small modern profile: TLS 1.2, ECDHE,
 * ChaCha20-Poly1305 or AES-GCM, RSA and ECDSA certificates); the response is read with picohttpparser, redirects are
 * followed and chunked bodies decoded. Certificates are checked against Mozilla's major roots (wup_roots.h) or the
 * caller's, and the clock (wup_time_known(): the menu sets it from the network). One request at a time, blocking. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <bearssl.h>

/* the body as it arrives; total: its length, or -1 if the server did not say. 0 = go on, anything else stops */
typedef int (*wup_https_body_fn)(void *arg, const uint8_t *data, size_t n, long long total);

typedef struct {
    const br_x509_trust_anchor *tas;    /* the roots to trust (NULL: wup_roots.h) */
    size_t ntas;
    const char *headers;                /* extra request lines, each ending in "\r\n" (or NULL) */
    int timeout_ms;                     /* the longest wait for data (0: 20 s) */
    void (*idle)(void);                 /* called often while it waits (or NULL): keep a screen moving */
} wup_https_opts;

int wup_https_get(const char *url, const wup_https_opts *o, wup_https_body_fn body, void *arg);   /* o may be NULL */
/* -> the final HTTP status (200, 404...), or one of these: */
enum {
    WUP_HTTPS_EURL = -1,       /* not an https:// URL */
    WUP_HTTPS_ENOTIME = -2,    /* the clock is not set: certificates cannot be checked */
    WUP_HTTPS_EDNS = -3,       /* the name did not resolve */
    WUP_HTTPS_ECONNECT = -4,
    WUP_HTTPS_ETLS = -5,       /* the handshake or the certificate check failed (wup_https_tls_error: BearSSL's BR_ERR_*) */
    WUP_HTTPS_EHTTP = -6,      /* not an HTTP response */
    WUP_HTTPS_ETIMEOUT = -7,
    WUP_HTTPS_EREDIRECT = -8,  /* too many redirects */
    WUP_HTTPS_ESTOPPED = -9,   /* the body function said stop */
    WUP_HTTPS_ENOMEM = -10,
};
int wup_https_tls_error(void);
