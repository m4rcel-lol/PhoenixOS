/* mkdir.c — PhoenixOS mkdir utility */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

static int opt_parents = 0;
static mode_t opt_mode = 0755;

static int make_parents(const char *path) {
    char tmp[4096];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, opt_mode) != 0 && errno != EEXIST) {
                fprintf(stderr, "mkdir: cannot create directory '%s': %s\n",
                        tmp, strerror(errno));
                return 1;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, opt_mode) != 0 && errno != EEXIST) {
        fprintf(stderr, "mkdir: cannot create directory '%s': %s\n",
                tmp, strerror(errno));
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    int i;
    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
        for (int j = 1; argv[i][j]; j++) {
            if (argv[i][j] == 'p') opt_parents = 1;
            else if (argv[i][j] == 'm') {
                if (argv[i][j + 1] != '\0') {
                    opt_mode = (mode_t)strtoul(argv[i] + j + 1, NULL, 8);
                    break;
                } else if (++i < argc) {
                    opt_mode = (mode_t)strtoul(argv[i], NULL, 8);
                    break;
                }
            } else {
                fprintf(stderr, "mkdir: invalid option -- '%c'\n", argv[i][j]);
                return 1;
            }
        }
    }

    if (i >= argc) {
        fprintf(stderr, "Usage: mkdir [-p] [-m mode] DIRECTORY...\n");
        return 1;
    }

    int ret = 0;
    for (; i < argc; i++) {
        if (opt_parents) {
            ret |= make_parents(argv[i]);
        } else {
            if (mkdir(argv[i], opt_mode) != 0) {
                fprintf(stderr, "mkdir: cannot create directory '%s': %s\n",
                        argv[i], strerror(errno));
                ret = 1;
            }
        }
    }
    return ret;
}
