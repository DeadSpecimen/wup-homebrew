#pragma once
#include <netinet/in.h>
#include <lwip/inet.h>                 /* inet_addr, inet_aton, inet_ntoa (macros), INET_ADDRSTRLEN */
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif
#ifdef __cplusplus
extern "C" {
#endif
int inet_pton(int af, const char *s, void *dst);
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
#ifdef __cplusplus
}
#endif
