/* tail.c — PhoenixOS tail utility */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define DEFAULT_LINES 10

static int tail_file(FILE *f, const char *name, long nlines) {
    /* Collect last nlines lines using a circular buffer of pointers */
    char **ring = NULL;
    size_t *sizes = NULL;
    long  *lens  = NULL;
    int ret = 0;

    if (nlines <= 0) {
        if (ferror(f)) {
            fprintf(stderr, "tail: %s: %s\n", name, strerror(errno));
            return 1;
        }
        return 0;
    }

    ring  = calloc((size_t)nlines, sizeof(char *));
    sizes = calloc((size_t)nlines, sizeof(size_t));
    lens  = calloc((size_t)nlines, sizeof(long));
    if (!ring || !sizes || !lens) {
        fprintf(stderr, "tail: out of memory\n");
        ret = 1;
        goto done;
    }

    long idx = 0;
    ssize_t len;
    while ((len = getline(&ring[idx % nlines], &sizes[idx % nlines], f)) != -1) {
        lens[idx % nlines] = (long)len;
        idx++;
    }
    if (ferror(f)) {
        fprintf(stderr, "tail: %s: %s\n", name, strerror(errno));
        ret = 1;
        goto done;
    }

    long start = (idx >= nlines) ? idx - nlines : 0;
    for (long j = start; j < idx; j++) {
        long slot = j % nlines;
        if (fwrite(ring[slot], 1, (size_t)lens[slot], stdout) != (size_t)lens[slot]) {
            fprintf(stderr, "tail: write error\n");
            ret = 1;
            break;
        }
    }

done:
    if (ring) {
        for (long j = 0; j < nlines; j++) free(ring[j]);
        free(ring);
    }
    free(sizes);
    free(lens);
    return ret;
}

int main(int argc, char *argv[]) {
    long nlines = DEFAULT_LINES;
    int i = 1;

    if (i < argc && argv[i][0] == '-' && argv[i][1] == 'n') {
        if (argv[i][2] != '\0') {
            nlines = strtol(argv[i] + 2, NULL, 10);
        } else if (++i < argc) {
            nlines = strtol(argv[i], NULL, 10);
        }
        i++;
    } else if (i < argc && argv[i][0] == '-' &&
               (argv[i][1] >= '0' && argv[i][1] <= '9')) {
        nlines = strtol(argv[i] + 1, NULL, 10);
        i++;
    }

    if (nlines < 0) nlines = 0;

    if (i >= argc) return tail_file(stdin, "<stdin>", nlines);

    int ret = 0;
    int multi = (argc - i > 1);
    for (; i < argc; i++) {
        FILE *f = fopen(argv[i], "r");
        if (!f) {
            fprintf(stderr, "tail: cannot open '%s': %s\n", argv[i], strerror(errno));
            ret = 1;
            continue;
        }
        if (multi) printf("==> %s <==\n", argv[i]);
        ret |= tail_file(f, argv[i], nlines);
        fclose(f);
        if (multi && i < argc - 1) putchar('\n');
    }
    return ret;
}
