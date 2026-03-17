#include "../include/kernel.h"
#include "../include/fs.h"
#include "../include/mm.h"
#include "../include/sched.h"

/* ── Mount table ──────────────────────────────────────────────────────────── */

#define MAX_MOUNTS  16

struct mount_entry {
    char             path[PATH_MAX];
    struct super_block *sb;
    bool             used;
};

static struct mount_entry mount_table[MAX_MOUNTS];
static struct filesystem *fs_list = NULL;

/* ── Root dentry ──────────────────────────────────────────────────────────── */

static struct dentry *vfs_root = NULL;

/* ── vfs_init ─────────────────────────────────────────────────────────────── */

void vfs_init(void) {
    for (int i = 0; i < MAX_MOUNTS; i++) mount_table[i].used = false;
}

/* ── Register filesystem ──────────────────────────────────────────────────── */

int vfs_register_fs(struct filesystem *fs) {
    fs->next = fs_list;
    fs_list  = fs;
    printk("[vfs ] Registered filesystem: %s\n", fs->name);
    return 0;
}

/* ── Mount ────────────────────────────────────────────────────────────────── */

int vfs_mount(const char *path, const char *fsname, void *device) {
    /* Find filesystem */
    struct filesystem *fs = fs_list;
    while (fs) {
        bool match = true;
        const char *a = fs->name, *b = fsname;
        while (*a && *b) { if (*a++ != *b++) { match = false; break; } }
        if (match && *a == '\0' && *b == '\0') break;
        fs = fs->next;
    }
    if (!fs) {
        printk("[vfs ] Unknown filesystem: %s\n", fsname);
        return -ENODEV;
    }

    /* Find free slot */
    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (!mount_table[i].used) {
            struct super_block *sb = fs->mount(device);
            if (!sb) return -ENOMEM;

            /* Copy path */
            usize j = 0;
            while (path[j] && j < PATH_MAX - 1) {
                mount_table[i].path[j] = path[j]; j++;
            }
            mount_table[i].path[j] = '\0';
            mount_table[i].sb   = sb;
            mount_table[i].used = true;

            if (path[0] == '/' && path[1] == '\0')
                vfs_root = sb->root;

            printk("[vfs ] Mounted %s at %s\n", fsname, path);
            return 0;
        }
    }
    return -ENOMEM;
}

/* ── Path resolution ──────────────────────────────────────────────────────── */

static struct dentry *lookup_path(const char *path) {
    if (!path || path[0] != '/') return NULL;
    if (!vfs_root) return NULL;

    struct dentry *cur = vfs_root;
    if (path[1] == '\0') return cur;

    /* Walk each path component */
    char component[NAME_MAX + 1];
    const char *p = path + 1;

    while (*p) {
        usize len = 0;
        while (*p && *p != '/') component[len++] = *p++;
        component[len] = '\0';
        if (len == 0) { if (*p) p++; continue; }

        /* Look in current dir's children */
        struct dentry *child = cur->children;
        bool found = false;
        while (child) {
            bool match = true;
            const char *a = child->name, *b = component;
            while (*a && *b) { if (*a++ != *b++) { match = false; break; } }
            if (match && *a == '\0' && *b == '\0') { cur = child; found = true; break; }
            child = child->next_sibling;
        }

        if (!found) {
            /* Try inode lookup via filesystem */
            if (cur->inode && cur->inode->ops && cur->inode->ops->lookup) {
                struct inode *in = cur->inode->ops->lookup(cur->inode, component);
                if (in) {
                    struct dentry *de = (struct dentry *)kzmalloc(sizeof(*de));
                    usize ni = 0;
                    while (component[ni] && ni < NAME_MAX) {
                        de->name[ni] = component[ni]; ni++;
                    }
                    de->inode        = in;
                    de->parent       = cur;
                    de->next_sibling = cur->children;
                    cur->children    = de;
                    cur = de;
                    found = true;
                }
            }
            if (!found) return NULL;
        }
        if (*p == '/') p++;
    }
    return cur;
}

/* ── Allocate a file descriptor ───────────────────────────────────────────── */

static int alloc_fd(struct task_struct *t, struct file *f) {
    for (int i = 0; i < 256; i++) {
        if (!t->fd_table[i]) {
            t->fd_table[i] = f;
            return i;
        }
    }
    return -ENOMEM;
}

/* ── vfs_open ─────────────────────────────────────────────────────────────── */

int vfs_open(const char *path, int flags) {
    struct dentry *de = lookup_path(path);
    if (!de) return -ENOENT;
    if (!de->inode) return -ENOENT;

    struct file *f = (struct file *)kzmalloc(sizeof(*f));
    if (!f) return -ENOMEM;

    f->inode     = de->inode;
    f->dentry    = de;
    f->flags     = flags;
    f->ref_count = 1;
    f->ops       = de->inode->fops;   /* set by FS driver via inode->fops */

    struct task_struct *t = task_get_current();
    if (!t) { kfree(f); return -EINVAL; }
    return alloc_fd(t, f);
}

/* ── vfs_read / write / close ─────────────────────────────────────────────── */

ssize vfs_read(int fd, void *buf, usize len) {
    struct task_struct *t = task_get_current();
    if (!t || fd < 0 || fd >= 256 || !t->fd_table[fd]) return -EBADF;
    struct file *f = t->fd_table[fd];
    if (!f->ops || !f->ops->read) return -EBADF;
    return f->ops->read(f, buf, len);
}

ssize vfs_write(int fd, const void *buf, usize len) {
    struct task_struct *t = task_get_current();
    if (!t || fd < 0 || fd >= 256 || !t->fd_table[fd]) return -EBADF;
    struct file *f = t->fd_table[fd];
    if (!f->ops || !f->ops->write) return -EBADF;
    return f->ops->write(f, buf, len);
}

int vfs_close(int fd) {
    struct task_struct *t = task_get_current();
    if (!t || fd < 0 || fd >= 256 || !t->fd_table[fd]) return -EBADF;
    struct file *f = t->fd_table[fd];
    f->ref_count--;
    if (f->ref_count == 0) {
        if (f->ops && f->ops->close) f->ops->close(f);
        kfree(f);
    }
    t->fd_table[fd] = NULL;
    return 0;
}

int vfs_stat(const char *path, struct stat *st) {
    struct dentry *de = lookup_path(path);
    if (!de || !de->inode) return -ENOENT;
    struct inode *in = de->inode;
    st->st_ino   = in->ino;
    st->st_mode  = in->mode;
    st->st_nlink = in->nlink;
    st->st_uid   = in->uid;
    st->st_gid   = in->gid;
    st->st_size  = in->size;
    st->st_mtime = in->mtime;
    return 0;
}

int vfs_fstat(int fd, struct stat *st) {
    struct task_struct *t = task_get_current();
    if (!t || fd < 0 || fd >= 256 || !t->fd_table[fd]) return -EBADF;
    struct inode *in = t->fd_table[fd]->inode;
    if (!in) return -EBADF;
    st->st_ino  = in->ino;
    st->st_mode = in->mode;
    st->st_size = in->size;
    return 0;
}

int vfs_readdir(int fd, struct dirent *de) {
    struct task_struct *t = task_get_current();
    if (!t || fd < 0 || fd >= 256 || !t->fd_table[fd]) return -EBADF;
    struct file *f = t->fd_table[fd];
    if (!f->ops || !f->ops->readdir) return -EBADF;
    return f->ops->readdir(f, de);
}

int vfs_seek(int fd, s64 offset, int whence) {
    struct task_struct *t = task_get_current();
    if (!t || fd < 0 || fd >= 256 || !t->fd_table[fd]) return -EBADF;
    struct file *f = t->fd_table[fd];
    if (!f->ops || !f->ops->seek) {
        /* Default seek for regular files */
        if (whence == SEEK_SET) f->offset = (u64)offset;
        else if (whence == SEEK_CUR) f->offset += (u64)offset;
        else if (whence == SEEK_END && f->inode) f->offset = f->inode->size + (u64)offset;
        return 0;
    }
    return f->ops->seek(f, offset, whence);
}
