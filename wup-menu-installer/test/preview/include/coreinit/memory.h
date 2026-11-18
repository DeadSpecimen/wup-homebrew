#pragma once
/* the console's system font (shared data) -> a font file on the host: PREVIEW_FONT */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
enum { OS_SHAREDDATATYPE_FONT_STANDARD };
static inline int OSGetSharedData(int, uint32_t, void **data, uint32_t *size) {
    FILE *f = fopen(PREVIEW_FONT, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    *data = malloc(n); *size = (uint32_t)n; size_t r = fread(*data, 1, n, f); fclose(f); return r == (size_t)n;
}
