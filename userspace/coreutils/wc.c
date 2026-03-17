/* wc.c — PhoenixOS wc utility */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

static void count(FILE *f, long *lines, long *words, long *bytes) {
    int c, in_word = 0;
    while ((c = fgetc(f)) != EOF) {
        (*bytes)++;
        if (c == '\n') (*lines)++;
        if (isspace((unsigned char)c)) {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            (*words)++;
        }
    }
}

int main(int argc, char *argv[]) {
    int opt_l = 0, opt_w = 0, opt_c = 0;
    int i;

    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
        for (int j = 1; argv[i][j]; j++) {
            if      (argv[i][j] == 'l') opt_l = 1;
            else if (argv[i][j] == 'w') opt_w = 1;
            else if (argv[i][j] == 'c') opt_c = 1;
            else {
                fprintf(stderr, "wc: invalid option -- '%c'\n", argv[i][j]);
                return 1;
            }
        }
    }

    if (!opt_l && !opt_w && !opt_c) opt_l = opt_w = opt_c = 1;

    long total_l = 0, total_w = 0, total_c = 0;
    int ret = 0;
    int nfiles = argc - i;

    if (nfiles == 0) {
        long l = 0, w = 0, c = 0;
        count(stdin, &l, &w, &c);
        if (opt_l) printf(" %ld", l);
        if (opt_w) printf(" %ld", w);
        if (opt_c) printf(" %ld", c);
        putchar('\n');
        return 0;
    }

    for (; i < argc; i++) {
        FILE *f = fopen(argv[i], "r");
        if (!f) {
            fprintf(stderr, "wc: %s: %s\n", argv[i], strerror(errno));
            ret = 1;
            continue;
        }
        long l = 0, w = 0, c = 0;
        count(f, &l, &w, &c);
        fclose(f);

        total_l += l; total_w += w; total_c += c;

        if (opt_l) printf(" %ld", l);
        if (opt_w) printf(" %ld", w);
        if (opt_c) printf(" %ld", c);
        printf(" %s\n", argv[i]);
    }

    if (nfiles > 1) {
        if (opt_l) printf(" %ld", total_l);
        if (opt_w) printf(" %ld", total_w);
        if (opt_c) printf(" %ld", total_c);
        printf(" total\n");
    }
    return ret;
}
