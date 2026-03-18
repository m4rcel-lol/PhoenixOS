/* hostname.c — PhoenixOS hostname utility */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define HOST_MAX 256

int main(int argc, char *argv[]) {
    if (argc == 1) {
        char name[HOST_MAX];
        if (gethostname(name, sizeof(name)) != 0) {
            fprintf(stderr, "hostname: %s\n", strerror(errno));
            return 1;
        }
        puts(name);
        return 0;
    }

    if (argc == 2) {
        if (sethostname(argv[1], strlen(argv[1])) != 0) {
            fprintf(stderr, "hostname: %s\n", strerror(errno));
            return 1;
        }
        return 0;
    }

    fprintf(stderr, "Usage: hostname [NAME]\n");
    return 1;
}
