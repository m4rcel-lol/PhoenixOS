/* mount.c — PhoenixOS mount utility */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mount.h>

#define PROC_MOUNTS "/proc/mounts"

static int list_mounts(void) {
    FILE *f = fopen(PROC_MOUNTS, "r");
    if (!f) {
        fprintf(stderr, "mount: cannot open " PROC_MOUNTS ": %s\n", strerror(errno));
        return 1;
    }
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        fputs(line, stdout);
    }
    fclose(f);
    return 0;
}

int main(int argc, char *argv[]) {
    const char *fstype  = NULL;
    const char *options = NULL;
    int i;

    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
        for (int j = 1; argv[i][j]; j++) {
            if (argv[i][j] == 't') {
                if (argv[i][j + 1] != '\0') {
                    fstype = argv[i] + j + 1;
                    break;
                } else if (++i < argc) {
                    fstype = argv[i];
                    break;
                }
            } else if (argv[i][j] == 'o') {
                if (argv[i][j + 1] != '\0') {
                    options = argv[i] + j + 1;
                    break;
                } else if (++i < argc) {
                    options = argv[i];
                    break;
                }
            } else {
                fprintf(stderr, "mount: invalid option -- '%c'\n", argv[i][j]);
                return 1;
            }
        }
    }

    if (i >= argc) return list_mounts();

    if (argc - i < 2) {
        fprintf(stderr, "Usage: mount [-t fstype] [-o options] DEVICE MOUNTPOINT\n");
        return 1;
    }

    const char *device = argv[i];
    const char *target = argv[i + 1];
    (void)options; /* mount flags parsing omitted for brevity */

    unsigned long flags = 0;
    if (mount(device, target, fstype ? fstype : "", flags, NULL) != 0) {
        fprintf(stderr, "mount: %s\n", strerror(errno));
        return 1;
    }
    return 0;
}
