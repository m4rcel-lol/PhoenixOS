/* touch.c — PhoenixOS touch utility */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <utime.h>

int main(int argc, char *argv[]) {
    int opt_nocreate = 0;
    int i;

    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
        for (int j = 1; argv[i][j]; j++) {
            if (argv[i][j] == 'c') opt_nocreate = 1;
            else {
                fprintf(stderr, "touch: invalid option -- '%c'\n", argv[i][j]);
                return 1;
            }
        }
    }

    if (i >= argc) {
        fprintf(stderr, "Usage: touch [-c] FILE...\n");
        return 1;
    }

    int ret = 0;
    for (; i < argc; i++) {
        struct stat st;
        if (stat(argv[i], &st) != 0) {
            if (errno != ENOENT) {
                fprintf(stderr, "touch: %s: %s\n", argv[i], strerror(errno));
                ret = 1;
                continue;
            }
            if (opt_nocreate) continue;
            /* Create the file */
            int fd = open(argv[i], O_CREAT | O_WRONLY, 0666);
            if (fd < 0) {
                fprintf(stderr, "touch: cannot create '%s': %s\n", argv[i], strerror(errno));
                ret = 1;
                continue;
            }
            close(fd);
        } else {
            /* Update timestamps */
            if (utime(argv[i], NULL) != 0) {
                fprintf(stderr, "touch: %s: %s\n", argv[i], strerror(errno));
                ret = 1;
            }
        }
    }
    return ret;
}
