/* the host side of source/fwpatch.c: fwpatch_cli ORIG PAYLOAD OUT */
#include <stdio.h>
#include <stdlib.h>
#include "../source/fwpatch.h"

static unsigned char *slurp(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb"); if (!f) { perror(path); exit(2); }
    fseek(f, 0, SEEK_END); *n = (size_t)ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc(*n); if (fread(b, 1, *n, f) != *n) exit(2); fclose(f); return b;
}
int main(int argc, char **argv) {
    if (argc != 4) { fprintf(stderr, "fwpatch_cli ORIG PAYLOAD OUT\n"); return 2; }
    size_t on, pn, n; const char *err = 0; unsigned char *out;
    unsigned char *o = slurp(argv[1], &on), *p = slurp(argv[2], &pn);
    if (fwpatch(o, on, p, pn, &out, &n, &err)) { printf("refused: %s\n", err); return 1; }
    FILE *f = fopen(argv[3], "wb"); fwrite(out, 1, n, f); fclose(f);
    printf("%s: %zu bytes\n", argv[3], n);
    return 0;
}
