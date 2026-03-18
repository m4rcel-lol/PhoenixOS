/* stat.c — PhoenixOS stat utility */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

static void print_stat(const char *path, const struct stat *st) {
    printf("  File: %s\n", path);
    printf("  Size: %lld\t", (long long)st->st_size);
    printf("Blocks: %lld\t", (long long)st->st_blocks);
    printf("IO Block: %ld\t", (long)st->st_blksize);

    if (S_ISREG(st->st_mode))       printf("regular file\n");
    else if (S_ISDIR(st->st_mode))  printf("directory\n");
    else if (S_ISLNK(st->st_mode))  printf("symbolic link\n");
    else if (S_ISCHR(st->st_mode))  printf("character device\n");
    else if (S_ISBLK(st->st_mode))  printf("block device\n");
    else if (S_ISFIFO(st->st_mode)) printf("fifo\n");
    else if (S_ISSOCK(st->st_mode)) printf("socket\n");
    else                            printf("unknown\n");

    printf("Device: %lu\t", (unsigned long)st->st_dev);
    printf("Inode: %lu\t", (unsigned long)st->st_ino);
    printf("Links: %lu\n", (unsigned long)st->st_nlink);

    /* Permissions */
    char mode_str[11];
    mode_str[0]  = S_ISDIR(st->st_mode) ? 'd' : (S_ISLNK(st->st_mode) ? 'l' : '-');
    mode_str[1]  = (st->st_mode & S_IRUSR) ? 'r' : '-';
    mode_str[2]  = (st->st_mode & S_IWUSR) ? 'w' : '-';
    mode_str[3]  = (st->st_mode & S_IXUSR) ? 'x' : '-';
    mode_str[4]  = (st->st_mode & S_IRGRP) ? 'r' : '-';
    mode_str[5]  = (st->st_mode & S_IWGRP) ? 'w' : '-';
    mode_str[6]  = (st->st_mode & S_IXGRP) ? 'x' : '-';
    mode_str[7]  = (st->st_mode & S_IROTH) ? 'r' : '-';
    mode_str[8]  = (st->st_mode & S_IWOTH) ? 'w' : '-';
    mode_str[9]  = (st->st_mode & S_IXOTH) ? 'x' : '-';
    mode_str[10] = '\0';

    char owner[32] = "?", group[32] = "?";
    struct passwd *pw = getpwuid(st->st_uid);
    struct group  *gr = getgrgid(st->st_gid);
    if (pw) snprintf(owner, sizeof(owner), "%s", pw->pw_name);
    else    snprintf(owner, sizeof(owner), "%u", st->st_uid);
    if (gr) snprintf(group, sizeof(group), "%s", gr->gr_name);
    else    snprintf(group, sizeof(group), "%u", st->st_gid);

    printf("Access: (%04o/%s)  Uid: (%5u/%8s)  Gid: (%5u/%8s)\n",
           st->st_mode & 07777, mode_str,
           st->st_uid, owner, st->st_gid, group);

    char tbuf[64];
    struct tm *tm;

    tm = localtime(&st->st_atime);
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm);
    printf("Access: %s\n", tbuf);

    tm = localtime(&st->st_mtime);
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm);
    printf("Modify: %s\n", tbuf);

    tm = localtime(&st->st_ctime);
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm);
    printf("Change: %s\n", tbuf);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: stat FILE...\n");
        return 1;
    }

    int ret = 0;
    for (int i = 1; i < argc; i++) {
        struct stat st;
        if (lstat(argv[i], &st) != 0) {
            fprintf(stderr, "stat: cannot stat '%s': %s\n", argv[i], strerror(errno));
            ret = 1;
            continue;
        }
        print_stat(argv[i], &st);
    }
    return ret;
}
