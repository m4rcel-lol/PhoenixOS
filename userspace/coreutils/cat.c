/* cat.c — PhoenixOS cat utility */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define CHUNK_SIZE  4096

static int cat_fd(int fd, const char *name) {
    char buf[CHUNK_SIZE];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(STDOUT_FILENO, buf + written, (size_t)(n - written));
            if (w <= 0) {
                perror("cat: write");
                return 1;
            }
            written += w;
        }
    }
    if (n < 0) {
        fprintf(stderr, "cat: %s: %s\n", name, strerror(errno));
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc == 1) return cat_fd(STDIN_FILENO, "<stdin>");

    int ret = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-") == 0) {
            ret |= cat_fd(STDIN_FILENO, "<stdin>");
            continue;
        }
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "cat: %s: %s\n", argv[i], strerror(errno));
            ret = 1;
            continue;
        }
        ret |= cat_fd(fd, argv[i]);
        close(fd);
    }
    return ret;
}
