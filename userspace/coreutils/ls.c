/* ls.c — PhoenixOS ls utility */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <errno.h>

static int opt_long = 0;
static int opt_all  = 0;
static int opt_human = 0;

static void format_size(off_t size, char *buf, int bufsz) {
    if (!opt_human || size < 1024) {
        snprintf(buf, bufsz, "%lld", (long long)size);
        return;
    }
    const char *units[] = {"B", "K", "M", "G", "T"};
    double s = (double)size;
    int u = 0;
    while (s >= 1024.0 && u < 4) { s /= 1024.0; u++; }
    snprintf(buf, bufsz, "%.1f%s", s, units[u]);
}

static void format_mode(mode_t mode, char *buf) {
    buf[0] = S_ISDIR(mode) ? 'd' : (S_ISLNK(mode) ? 'l' : '-');
    buf[1] = (mode & S_IRUSR) ? 'r' : '-';
    buf[2] = (mode & S_IWUSR) ? 'w' : '-';
    buf[3] = (mode & S_IXUSR) ? 'x' : '-';
    buf[4] = (mode & S_IRGRP) ? 'r' : '-';
    buf[5] = (mode & S_IWGRP) ? 'w' : '-';
    buf[6] = (mode & S_IXGRP) ? 'x' : '-';
    buf[7] = (mode & S_IROTH) ? 'r' : '-';
    buf[8] = (mode & S_IWOTH) ? 'w' : '-';
    buf[9] = (mode & S_IXOTH) ? 'x' : '-';
    buf[10] = '\0';
}

static void print_entry(const char *dir, const char *name) {
    if (!opt_all && name[0] == '.') return;

    if (!opt_long) {
        printf("%s\n", name);
        return;
    }

    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", dir, name);

    struct stat st;
    if (lstat(path, &st) != 0) {
        printf("%-30s  (stat error: %s)\n", name, strerror(errno));
        return;
    }

    char mode_str[11];
    format_mode(st.st_mode, mode_str);

    char sz[16];
    format_size(st.st_size, sz, sizeof(sz));

    /* Time */
    char timebuf[32];
    struct tm *tm = localtime(&st.st_mtime);
    strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", tm);

    /* Owner / group */
    char owner[32] = "?";
    char group[32] = "?";
    struct passwd *pw = getpwuid(st.st_uid);
    struct group  *gr = getgrgid(st.st_gid);
    if (pw) strncpy(owner, pw->pw_name, 31);
    else    snprintf(owner, sizeof(owner), "%d", st.st_uid);
    if (gr) strncpy(group, gr->gr_name, 31);
    else    snprintf(group, sizeof(group), "%d", st.st_gid);

    printf("%s %3lu %-8s %-8s %8s %s %s\n",
           mode_str, (unsigned long)st.st_nlink,
           owner, group, sz, timebuf, name);
}

static int compare_names(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

static void list_dir(const char *path) {
    DIR *d = opendir(path);
    if (!d) { fprintf(stderr, "ls: %s: %s\n", path, strerror(errno)); return; }

    char *names[4096];
    int count = 0;

    struct dirent *de;
    while ((de = readdir(d)) != NULL && count < 4095) {
        names[count++] = strdup(de->d_name);
    }
    closedir(d);

    qsort(names, count, sizeof(char *), compare_names);

    for (int i = 0; i < count; i++) {
        print_entry(path, names[i]);
        free(names[i]);
    }
}

int main(int argc, char *argv[]) {
    int path_start = 1;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (int j = 1; argv[i][j]; j++) {
                if (argv[i][j] == 'l') opt_long  = 1;
                if (argv[i][j] == 'a') opt_all   = 1;
                if (argv[i][j] == 'h') opt_human = 1;
            }
            path_start = i + 1;
        }
    }

    if (path_start >= argc) {
        list_dir(".");
    } else {
        for (int i = path_start; i < argc; i++) {
            if (argc - path_start > 1) printf("%s:\n", argv[i]);
            struct stat st;
            if (stat(argv[i], &st) != 0) {
                fprintf(stderr, "ls: %s: %s\n", argv[i], strerror(errno));
                continue;
            }
            if (S_ISDIR(st.st_mode)) list_dir(argv[i]);
            else print_entry(".", argv[i]);
        }
    }
    return 0;
}
