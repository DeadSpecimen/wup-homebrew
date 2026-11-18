#pragma once
#include <sys/socket.h>                /* (struct sockaddr_in, in_addr, INADDR_*, IPPROTO_*: lwIP's) */
static inline uint16_t htons(uint16_t x) { return (uint16_t)(x << 8 | x >> 8); }
static inline uint16_t ntohs(uint16_t x) { return htons(x); }
static inline uint32_t htonl(uint32_t x) { return __builtin_bswap32(x); }
static inline uint32_t ntohl(uint32_t x) { return __builtin_bswap32(x); }
