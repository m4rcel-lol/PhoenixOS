/* cp.c — PhoenixOS cp utility */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <libgen.h>

#define CHUNK_SIZE 4096

static int opt_recursive = 0;
static int opt_force     = 0;

static int copy_file(const char *src, const char *dst) {
    int src_fd = open(src, O_RDONLY);
    if (src_fd < 0) {
        fprintf(stderr, "cp: cannot open '%s': %s\n", src, strerror(errno));
        return 1;
    }

    struct stat st;
    if (fstat(src_fd, &st) < 0) {
        fprintf(stderr, "cp: cannot stat '%s': %s\n", src, strerror(errno));
        close(src_fd);
        return 1;
    }

    int flags = O_WRONLY | O_CREAT | O_TRUNC;
    if (!opt_force) flags |= O_EXCL;
    int dst_fd = open(dst, flags, st.st_mode & 0777);
    if (dst_fd < 0 && opt_force && errno == EEXIST) {
        dst_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
    }
    if (dst_fd < 0) {
        fprintf(stderr, "cp: cannot create '%s': %s\n", dst, strerror(errno));
        close(src_fd);
        return 1;
    }

    char buf[CHUNK_SIZE];
    ssize_t n;
    int ret = 0;
    while ((n = read(src_fd, buf, sizeof(buf))) > 0) {
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(dst_fd, buf + written, (size_t)(n - written));
            if (w <= 0) {
                fprintf(stderr, "cp: write error on '%s': %s\n", dst, strerror(errno));
                ret = 1;
                goto done;
            }
            written += w;
        }
    }
    if (n < 0) {
        fprintf(stderr, "cp: read error on '%s': %s\n", src, strerror(errno));
        ret = 1;
    }

done:
    close(src_fd);
    close(dst_fd);
    return ret;
}

static int copy_recursive(const char *src, const char *dst);

static int copy_entry(const char *src, const char *dst) {
    struct stat st;
    if (lstat(src, &st) < 0) {
        fprintf(stderr, "cp: cannot stat '%s': %s\n", src, strerror(errno));
        return 1;
    }
    if (S_ISDIR(st.st_mode)) {
        if (!opt_recursive) {
            fprintf(stderr, "cp: -r not specified; omitting directory '%s'\n", src);
            return 1;
        }
        return copy_recursive(src, dst);
    }
    return copy_file(src, dst);
}

static int copy_recursive(const char *src, const char *dst) {
    struct stat st;
    if (stat(src, &st) < 0) {
        fprintf(stderr, "cp: cannot stat '%s': %s\n", src, strerror(errno));
        return 1;
    }

    if (mkdir(dst, st.st_mode & 0777) < 0 && errno != EEXIST) {
        fprintf(stderr, "cp: cannot create directory '%s': %s\n", dst, strerror(errno));
        return 1;
    }

    DIR *d = opendir(src);
    if (!d) {
        fprintf(stderr, "cp: cannot open directory '%s': %s\n", src, strerror(errno));
        return 1;
    }

    int ret = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        char src_path[4096], dst_path[4096];
        snprintf(src_path, sizeof(src_path), "%s/%s", src, de->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst, de->d_name);
        ret |= copy_entry(src_path, dst_path);
    }
    closedir(d);
    return ret;
}

int main(int argc, char *argv[]) {
    int i;
    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
        for (int j = 1; argv[i][j]; j++) {
            if (argv[i][j] == 'r' || argv[i][j] == 'R') opt_recursive = 1;
            else if (argv[i][j] == 'f') opt_force = 1;
            else {
                fprintf(stderr, "cp: invalid option -- '%c'\n", argv[i][j]);
                return 1;
            }
        }
    }

    if (argc - i < 2) {
        fprintf(stderr, "Usage: cp [-rRf] SOURCE... DEST\n");
        return 1;
    }

    const char *dst = argv[argc - 1];
    int nsrc = argc - i - 1;

    struct stat dst_st;
    int dst_is_dir = (stat(dst, &dst_st) == 0 && S_ISDIR(dst_st.st_mode));

    if (nsrc > 1 && !dst_is_dir) {
        fprintf(stderr, "cp: target '%s' is not a directory\n", dst);
        return 1;
    }

    int ret = 0;
    for (; i < argc - 1; i++) {
        if (dst_is_dir) {
            char tmp[4096];
            strncpy(tmp, argv[i], sizeof(tmp) - 1);
            tmp[sizeof(tmp) - 1] = '\0';
            char *base = basename(tmp);
            char dst_path[4096];
            snprintf(dst_path, sizeof(dst_path), "%s/%s", dst, base);
            ret |= copy_entry(argv[i], dst_path);
        } else {
            ret |= copy_entry(argv[i], dst);
        }
    }
    return ret;
}
