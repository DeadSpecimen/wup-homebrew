/* version: "1.10.2", "v2", "1.00" compared number by number (the menu's bundle.c, the catalogue) */
#pragma once
#include <stdlib.h>
static inline int version_newer(const char *a, const char *b) {   /* is a newer than b? */
    for (int i = 0; i < 4; i++) {
        while (*a && (*a < '0' || *a > '9')) a++;
        while (*b && (*b < '0' || *b > '9')) b++;
        long x = strtol(a, (char **)&a, 10), y = strtol(b, (char **)&b, 10);
        if (x != y) return x > y;
        if (!*a && !*b) break;
    }
    return 0;
}
