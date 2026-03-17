/* rm.c — PhoenixOS rm utility */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>

static int opt_recursive = 0;
static int opt_force     = 0;

static int remove_path(const char *path);

static int remove_dir(const char *path) {
    DIR *d = opendir(path);
    if (!d) {
        if (opt_force && errno == ENOENT) return 0;
        fprintf(stderr, "rm: cannot open directory '%s': %s\n", path, strerror(errno));
        return 1;
    }

    int ret = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        char child[4096];
        snprintf(child, sizeof(child), "%s/%s", path, de->d_name);
        ret |= remove_path(child);
    }
    closedir(d);

    if (ret == 0 && rmdir(path) != 0) {
        fprintf(stderr, "rm: cannot remove '%s': %s\n", path, strerror(errno));
        ret = 1;
    }
    return ret;
}

static int remove_path(const char *path) {
    /* Try unlinking first; if it's a directory, handle that case. */
    if (unlink(path) == 0) return 0;

    if (errno == ENOENT) {
        if (opt_force) return 0;
        fprintf(stderr, "rm: cannot remove '%s': %s\n", path, strerror(errno));
        return 1;
    }

    if (errno == EISDIR || errno == EPERM) {
        if (!opt_recursive) {
            fprintf(stderr, "rm: cannot remove '%s': Is a directory\n", path);
            return 1;
        }
        return remove_dir(path);
    }

    fprintf(stderr, "rm: cannot remove '%s': %s\n", path, strerror(errno));
    return 1;
}

int main(int argc, char *argv[]) {
    int i;
    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
        for (int j = 1; argv[i][j]; j++) {
            if (argv[i][j] == 'r' || argv[i][j] == 'R') opt_recursive = 1;
            else if (argv[i][j] == 'f') opt_force = 1;
            else {
                fprintf(stderr, "rm: invalid option -- '%c'\n", argv[i][j]);
                return 1;
            }
        }
    }

    if (i >= argc) {
        if (!opt_force) {
            fprintf(stderr, "Usage: rm [-rfR] FILE...\n");
            return 1;
        }
        return 0;
    }

    int ret = 0;
    for (; i < argc; i++)
        ret |= remove_path(argv[i]);
    return ret;
}
