/* ps.c — PhoenixOS ps utility */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>

static int is_pid_dir(const char *name) {
    for (int i = 0; name[i]; i++)
        if (!isdigit((unsigned char)name[i])) return 0;
    return name[0] != '\0';
}

static void read_stat(int pid, char *comm, char *state,
                      int *ppid, unsigned long *vsize) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f) { comm[0] = '?'; comm[1] = '\0'; *state = '?'; *ppid = 0; *vsize = 0; return; }
    int dummy_pid;
    /* comm may contain spaces, enclosed in () */
    int r = fscanf(f, "%d (%63[^)]) %c %d", &dummy_pid, comm, state, ppid);
    (void)r;
    /* skip fields 5-22 to reach vsize at field 23 */
    for (int i = 5; i <= 22; i++) {
        long tmp;
        r = fscanf(f, " %ld", &tmp);
        (void)r;
    }
    r = fscanf(f, " %lu", vsize);
    (void)r;
    fclose(f);
}

static uid_t read_uid(int pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE *f = fopen(path, "r");
    if (!f) return (uid_t)-1;
    char line[256];
    uid_t uid = (uid_t)-1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "Uid:", 4) == 0) {
            sscanf(line + 4, "%u", &uid);
            break;
        }
    }
    fclose(f);
    return uid;
}

int main(void) {
    DIR *d = opendir("/proc");
    if (!d) {
        fprintf(stderr, "ps: cannot open /proc: %s\n", strerror(errno));
        return 1;
    }

    printf("%-8s %-8s %-5s %-6s %s\n", "USER", "PID", "STAT", "VSZ", "COMMAND");

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (!is_pid_dir(de->d_name)) continue;
        int pid = atoi(de->d_name);

        char comm[64] = "?";
        char state = '?';
        int ppid = 0;
        unsigned long vsize = 0;
        read_stat(pid, comm, &state, &ppid, &vsize);

        uid_t uid = read_uid(pid);
        char user[32] = "?";
        if (uid != (uid_t)-1) {
            struct passwd *pw = getpwuid(uid);
            if (pw) snprintf(user, sizeof(user), "%s", pw->pw_name);
            else    snprintf(user, sizeof(user), "%u", uid);
        }

        printf("%-8s %-8d %-5c %-6lu %s\n",
               user, pid, state, vsize / 1024, comm);
    }
    closedir(d);
    return 0;
}
