#include "../include/kernel.h"
#include "../include/fs.h"
#include "../include/mm.h"
#include "../include/sched.h"

/* ── Pipe structure ───────────────────────────────────────────────────────── */

#define PIPE_BUF_SIZE  4096

struct pipe {
    u8   buf[PIPE_BUF_SIZE];
    u32  read_pos;
    u32  write_pos;
    u32  used;
    bool read_closed;
    bool write_closed;
};

/* ── Pipe file operations ─────────────────────────────────────────────────── */

static ssize pipe_read(struct file *f, void *buf, usize len) {
    struct pipe *p = (struct pipe *)f->inode->fs_data;
    if (!p) return -EBADF;

    usize read = 0;
    u8 *dst = (u8 *)buf;

    while (read < len) {
        if (p->used == 0) {
            if (p->write_closed) break;  /* EOF */
            task_yield();               /* wait for data */
            continue;
        }
        dst[read++] = p->buf[p->read_pos];
        p->read_pos  = (p->read_pos + 1) % PIPE_BUF_SIZE;
        p->used--;
    }
    return (ssize)read;
}

static ssize pipe_write(struct file *f, const void *buf, usize len) {
    struct pipe *p = (struct pipe *)f->inode->fs_data;
    if (!p) return -EBADF;
    if (p->read_closed) return -EPERM;  /* broken pipe */

    const u8 *src = (const u8 *)buf;
    usize written = 0;

    while (written < len) {
        if (p->used == PIPE_BUF_SIZE) {
            task_yield();   /* buffer full, wait for reader */
            continue;
        }
        p->buf[p->write_pos] = src[written++];
        p->write_pos = (p->write_pos + 1) % PIPE_BUF_SIZE;
        p->used++;
    }
    return (ssize)written;
}

static int pipe_close_read(struct file *f) {
    struct pipe *p = (struct pipe *)f->inode->fs_data;
    if (p) p->read_closed = true;
    return 0;
}

static int pipe_close_write(struct file *f) {
    struct pipe *p = (struct pipe *)f->inode->fs_data;
    if (p) p->write_closed = true;
    return 0;
}

static struct file_ops pipe_read_ops = {
    .read  = pipe_read,
    .write = NULL,
    .close = pipe_close_read,
};

static struct file_ops pipe_write_ops = {
    .read  = NULL,
    .write = pipe_write,
    .close = pipe_close_write,
};

/* ── pipe_create ──────────────────────────────────────────────────────────── */

int pipe_create(int fds[2]) {
    struct pipe *p = (struct pipe *)kmalloc(sizeof(struct pipe));
    if (!p) return -ENOMEM;

    /* Zero the pipe */
    u8 *raw = (u8 *)p;
    for (usize i = 0; i < sizeof(*p); i++) raw[i] = 0;

    /* Create two inode stubs sharing the same pipe buffer */
    struct inode *ri = (struct inode *)kzmalloc(sizeof(struct inode));
    struct inode *wi = (struct inode *)kzmalloc(sizeof(struct inode));
    if (!ri || !wi) { kfree(p); return -ENOMEM; }

    ri->mode = S_IFIFO | 0666;
    ri->fs_data = p;
    wi->mode = S_IFIFO | 0666;
    wi->fs_data = p;

    struct file *rf = (struct file *)kzmalloc(sizeof(struct file));
    struct file *wf = (struct file *)kzmalloc(sizeof(struct file));
    if (!rf || !wf) { kfree(p); kfree(ri); kfree(wi); return -ENOMEM; }

    rf->inode     = ri;
    rf->flags     = 0;  /* O_RDONLY */
    rf->ref_count = 1;
    rf->ops       = &pipe_read_ops;

    wf->inode     = wi;
    wf->flags     = 1;  /* O_WRONLY */
    wf->ref_count = 1;
    wf->ops       = &pipe_write_ops;

    /* Assign file descriptors in current task */
    struct task_struct *t = task_get_current();
    if (!t) { kfree(p); kfree(ri); kfree(wi); kfree(rf); kfree(wf); return -EINVAL; }

    int fd_read = -1, fd_write = -1;
    for (int i = 3; i < 256; i++) {
        if (!t->fd_table[i]) {
            if (fd_read < 0)  { t->fd_table[i] = rf; fd_read  = i; }
            else              { t->fd_table[i] = wf; fd_write = i; break; }
        }
    }

    if (fd_read < 0 || fd_write < 0) return -ENOMEM;

    fds[0] = fd_read;
    fds[1] = fd_write;
    return 0;
}
