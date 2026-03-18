/* df.c — PhoenixOS df utility */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/statvfs.h>

static int opt_human = 0;

static void format_blocks(unsigned long long blks, unsigned long bsz, char *buf, int bufsz) {
    unsigned long long kb = (blks * bsz + 512) / 1024;
    if (!opt_human || kb < 1024) {
        snprintf(buf, bufsz, "%llu", kb);
        return;
    }
    const char *units[] = {"K", "M", "G", "T"};
    double v = (double)kb;
    int u = 0;
    while (v >= 1024.0 && u < 3) { v /= 1024.0; u++; }
    snprintf(buf, bufsz, "%.1f%s", v, units[u]);
}

static int show_fs(const char *path) {
    struct statvfs sv;
    if (statvfs(path, &sv) != 0) {
        fprintf(stderr, "df: %s: %s\n", path, strerror(errno));
        return 1;
    }

    char total[24], used[24], avail[24];
    format_blocks(sv.f_blocks, sv.f_frsize, total, sizeof(total));
    format_blocks(sv.f_blocks - sv.f_bfree, sv.f_frsize, used, sizeof(used));
    format_blocks(sv.f_bavail, sv.f_frsize, avail, sizeof(avail));

    unsigned long long pct = 0;
    if (sv.f_blocks > 0)
        pct = (unsigned long long)((sv.f_blocks - sv.f_bfree) * 100 / sv.f_blocks);

    printf("%-20s %10s %10s %10s %4llu%% %s\n",
           path, total, used, avail, pct, path);
    return 0;
}

int main(int argc, char *argv[]) {
    int i;
    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
        for (int j = 1; argv[i][j]; j++) {
            if (argv[i][j] == 'h') opt_human = 1;
            else {
                fprintf(stderr, "df: invalid option -- '%c'\n", argv[i][j]);
                return 1;
            }
        }
    }

    printf("%-20s %10s %10s %10s %5s %s\n",
           "Filesystem", "1K-blocks", "Used", "Available", "Use%", "Mounted on");

    if (i >= argc) {
        return show_fs(".");
    }

    int ret = 0;
    for (; i < argc; i++)
        ret |= show_fs(argv[i]);
    return ret;
}
