/* pwd.c — PhoenixOS pwd utility */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    char *cwd = getcwd(NULL, 0);
    if (!cwd) {
        fprintf(stderr, "pwd: %s\n", strerror(errno));
        return 1;
    }
    puts(cwd);
    free(cwd);
    return 0;
}
