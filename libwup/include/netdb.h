#pragma once
#include <netinet/in.h>
#include <lwip/netdb.h>                /* struct addrinfo, struct hostent, AI_*, EAI_* */
#ifdef __cplusplus
extern "C" {
#endif
int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
void freeaddrinfo(struct addrinfo *res);
const char *gai_strerror(int err);
struct hostent *gethostbyname(const char *name);       /* not reentrant */
#ifdef __cplusplus
}
#endif
