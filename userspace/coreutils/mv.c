/* mv.c — PhoenixOS mv utility */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <libgen.h>

#define CHUNK_SIZE 4096

static int move_file(const char *src, const char *dst) {
    /* Try a simple rename first */
    if (rename(src, dst) == 0) return 0;

    if (errno != EXDEV) {
        fprintf(stderr, "mv: cannot move '%s' to '%s': %s\n", src, dst, strerror(errno));
        return 1;
    }

    /* Cross-device: copy then unlink */
    int src_fd = open(src, O_RDONLY);
    if (src_fd < 0) {
        fprintf(stderr, "mv: cannot open '%s': %s\n", src, strerror(errno));
        return 1;
    }

    struct stat st;
    if (fstat(src_fd, &st) < 0) {
        fprintf(stderr, "mv: cannot stat '%s': %s\n", src, strerror(errno));
        close(src_fd);
        return 1;
    }

    int dst_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
    if (dst_fd < 0) {
        fprintf(stderr, "mv: cannot create '%s': %s\n", dst, strerror(errno));
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
                fprintf(stderr, "mv: write error on '%s': %s\n", dst, strerror(errno));
                ret = 1;
                goto done;
            }
            written += w;
        }
    }
    if (n < 0) {
        fprintf(stderr, "mv: read error on '%s': %s\n", src, strerror(errno));
        ret = 1;
    }

done:
    close(src_fd);
    close(dst_fd);
    if (ret == 0) {
        if (unlink(src) != 0) {
            fprintf(stderr, "mv: cannot remove '%s': %s\n", src, strerror(errno));
            ret = 1;
        }
    }
    return ret;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: mv SOURCE... DEST\n");
        return 1;
    }

    const char *dst = argv[argc - 1];
    int nsrc = argc - 2;

    struct stat dst_st;
    int dst_is_dir = (stat(dst, &dst_st) == 0 && S_ISDIR(dst_st.st_mode));

    if (nsrc > 1 && !dst_is_dir) {
        fprintf(stderr, "mv: target '%s' is not a directory\n", dst);
        return 1;
    }

    int ret = 0;
    for (int i = 1; i < argc - 1; i++) {
        if (dst_is_dir) {
            char tmp[4096];
            strncpy(tmp, argv[i], sizeof(tmp) - 1);
            tmp[sizeof(tmp) - 1] = '\0';
            char *base = basename(tmp);
            char dst_path[4096];
            snprintf(dst_path, sizeof(dst_path), "%s/%s", dst, base);
            ret |= move_file(argv[i], dst_path);
        } else {
            ret |= move_file(argv[i], dst);
        }
    }
    return ret;
}
