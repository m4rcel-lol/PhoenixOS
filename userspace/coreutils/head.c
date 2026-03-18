/* head.c — PhoenixOS head utility */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define DEFAULT_LINES 10

static int head_file(FILE *f, const char *name, long nlines) {
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    long count = 0;
    int ret = 0;

    while (count < nlines && (len = getline(&line, &cap, f)) != -1) {
        if (fwrite(line, 1, (size_t)len, stdout) != (size_t)len) {
            fprintf(stderr, "head: write error\n");
            ret = 1;
            break;
        }
        count++;
    }
    if (ferror(f)) {
        fprintf(stderr, "head: %s: %s\n", name, strerror(errno));
        ret = 1;
    }
    free(line);
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

    if (i >= argc) return head_file(stdin, "<stdin>", nlines);

    int ret = 0;
    int multi = (argc - i > 1);
    for (; i < argc; i++) {
        FILE *f = fopen(argv[i], "r");
        if (!f) {
            fprintf(stderr, "head: cannot open '%s': %s\n", argv[i], strerror(errno));
            ret = 1;
            continue;
        }
        if (multi) printf("==> %s <==\n", argv[i]);
        ret |= head_file(f, argv[i], nlines);
        fclose(f);
        if (multi && i < argc - 1) putchar('\n');
    }
    return ret;
}
