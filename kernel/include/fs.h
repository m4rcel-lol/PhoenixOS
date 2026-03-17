#ifndef KERNEL_FS_H
#define KERNEL_FS_H

#include "kernel.h"

/* ── File type constants ──────────────────────────────────────────────────── */

#define S_IFMT    0xF000
#define S_IFREG   0x8000   /* regular file */
#define S_IFDIR   0x4000   /* directory */
#define S_IFCHR   0x2000   /* character device */
#define S_IFBLK   0x6000   /* block device */
#define S_IFLNK   0xA000   /* symbolic link */
#define S_IFIFO   0x1000   /* FIFO / pipe */

#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)

/* ── Open flags ───────────────────────────────────────────────────────────── */

#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0040
#define O_TRUNC     0x0200
#define O_APPEND    0x0400
#define O_NONBLOCK  0x0800

/* ── Seek whence ──────────────────────────────────────────────────────────── */

#define SEEK_SET  0
#define SEEK_CUR  1
#define SEEK_END  2

/* ── Max name lengths ─────────────────────────────────────────────────────── */

#define NAME_MAX  255
#define PATH_MAX  4096

/* ── Inode ────────────────────────────────────────────────────────────────── */

struct inode {
    u64   ino;           /* inode number */
    u32   mode;          /* file type + permissions */
    u32   uid, gid;
    u64   size;
    u64   atime, mtime, ctime;
    u32   nlink;
    void *fs_data;       /* filesystem-private pointer */
    struct super_block   *sb;
    struct inode_ops     *ops;
};

/* ── Directory entry ──────────────────────────────────────────────────────── */

struct dentry {
    char            name[NAME_MAX + 1];
    struct inode   *inode;
    struct dentry  *parent;
    struct dentry  *children;
    struct dentry  *next_sibling;
};

/* ── Open file descriptor ─────────────────────────────────────────────────── */

struct file {
    struct inode     *inode;
    struct dentry    *dentry;
    u64               offset;
    u32               flags;
    u32               ref_count;
    struct file_ops  *ops;
};

/* ── File operations ──────────────────────────────────────────────────────── */

struct file_ops {
    ssize (*read)(struct file *f, void *buf, usize len);
    ssize (*write)(struct file *f, const void *buf, usize len);
    int   (*seek)(struct file *f, s64 offset, int whence);
    int   (*ioctl)(struct file *f, u32 cmd, u64 arg);
    int   (*close)(struct file *f);
    int   (*readdir)(struct file *f, struct dirent *de);
};

/* ── Inode operations ─────────────────────────────────────────────────────── */

struct inode_ops {
    struct inode *(*lookup)(struct inode *dir, const char *name);
    int           (*create)(struct inode *dir, const char *name, u32 mode);
    int           (*mkdir)(struct inode *dir, const char *name, u32 mode);
    int           (*unlink)(struct inode *dir, const char *name);
    int           (*rmdir)(struct inode *dir, const char *name);
};

/* ── Superblock ───────────────────────────────────────────────────────────── */

struct filesystem;

struct super_block {
    struct filesystem  *fs;
    struct dentry      *root;
    void               *fs_data;
    u32                 block_size;
};

/* ── Filesystem type registration ─────────────────────────────────────────── */

struct filesystem {
    const char          *name;
    struct super_block *(*mount)(void *device);
    void                (*umount)(struct super_block *sb);
    struct filesystem   *next;
};

/* ── Directory entry (user-facing) ───────────────────────────────────────── */

struct dirent {
    u64   d_ino;
    u32   d_type;
    char  d_name[NAME_MAX + 1];
};

/* ── Stat structure ───────────────────────────────────────────────────────── */

struct stat {
    u64  st_ino;
    u32  st_mode;
    u32  st_nlink;
    u32  st_uid;
    u32  st_gid;
    u64  st_size;
    u64  st_atime;
    u64  st_mtime;
    u64  st_ctime;
    u32  st_blksize;
    u64  st_blocks;
};

/* ── VFS interface ────────────────────────────────────────────────────────── */

void  vfs_init(void);
int   vfs_register_fs(struct filesystem *fs);
int   vfs_mount(const char *path, const char *fsname, void *device);
int   vfs_umount(const char *path);

int   vfs_open(const char *path, int flags);
ssize vfs_read(int fd, void *buf, usize len);
ssize vfs_write(int fd, const void *buf, usize len);
int   vfs_close(int fd);
int   vfs_stat(const char *path, struct stat *st);
int   vfs_fstat(int fd, struct stat *st);
int   vfs_mkdir(const char *path, u32 mode);
int   vfs_unlink(const char *path);
int   vfs_readdir(int fd, struct dirent *de);
int   vfs_seek(int fd, s64 offset, int whence);

#endif /* KERNEL_FS_H */
