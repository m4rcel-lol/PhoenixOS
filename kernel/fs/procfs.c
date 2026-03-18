#include "../include/kernel.h"
#include "../include/fs.h"
#include "../include/mm.h"
#include "../include/timer.h"

/* ── /proc virtual filesystem ────────────────────────────────────────────── */

/* Each /proc entry has a read callback that fills a static buffer. */

typedef void (*proc_fill_fn)(char *buf, usize *len);

struct proc_entry {
    const char    *name;
    proc_fill_fn   fill;
};

/* ── /proc/version ────────────────────────────────────────────────────────── */

static void fill_version(char *buf, usize *len) {
    /* Kernel version string similar to Linux /proc/version */
    *len = 0;
    const char *str =
        "EmberKernel 0.1.0 (PhoenixOS 0.0.2) "
        "(x86_64-elf-gcc) #1 SMP\n";
    const char *p = str;
    while (*p) buf[(*len)++] = *p++;
}

/* ── /proc/meminfo ────────────────────────────────────────────────────────── */

extern u64 pmm_get_free_pages(void);
extern u64 pmm_get_total_pages(void);

/* Simple integer-to-string helper (decimal) */
static usize u64_to_str(u64 v, char *out) {
    if (v == 0) { out[0] = '0'; return 1; }
    char tmp[24];
    usize n = 0;
    while (v) { tmp[n++] = '0' + (v % 10); v /= 10; }
    for (usize i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    return n;
}

static usize append_str(char *buf, usize pos, const char *str) {
    while (*str) buf[pos++] = *str++;
    return pos;
}

static usize append_u64(char *buf, usize pos, u64 v) {
    char tmp[24];
    usize n = u64_to_str(v, tmp);
    for (usize i = 0; i < n; i++) buf[pos++] = tmp[i];
    return pos;
}

static void fill_meminfo(char *buf, usize *len) {
    u64 total_pages = pmm_get_total_pages();
    u64 free_pages  = pmm_get_free_pages();
    u64 used_pages  = total_pages - free_pages;
    u64 page_kb     = PAGE_SIZE / 1024;

    usize p = 0;
    p = append_str(buf, p, "MemTotal:       ");
    p = append_u64(buf, p, total_pages * page_kb);
    p = append_str(buf, p, " kB\n");
    p = append_str(buf, p, "MemFree:        ");
    p = append_u64(buf, p, free_pages * page_kb);
    p = append_str(buf, p, " kB\n");
    p = append_str(buf, p, "MemUsed:        ");
    p = append_u64(buf, p, used_pages * page_kb);
    p = append_str(buf, p, " kB\n");
    p = append_str(buf, p, "PageSize:       ");
    p = append_u64(buf, p, PAGE_SIZE);
    p = append_str(buf, p, " B\n");
    *len = p;
}

/* ── /proc/uptime ─────────────────────────────────────────────────────────── */

static void fill_uptime(char *buf, usize *len) {
    u64 ticks = timer_ticks;
    u64 secs  = ticks / 100;  /* assumes 100 Hz PIT */
    usize p = 0;
    p = append_u64(buf, p, secs);
    p = append_str(buf, p, ".00 ");
    p = append_u64(buf, p, secs);
    p = append_str(buf, p, ".00\n");
    *len = p;
}

/* ── /proc/cpuinfo ────────────────────────────────────────────────────────── */

static void fill_cpuinfo(char *buf, usize *len) {
    usize p = 0;
    p = append_str(buf, p, "processor\t: 0\n");
    p = append_str(buf, p, "vendor_id\t: PhoenixOS x86_64\n");
    p = append_str(buf, p, "model name\t: EmberKernel Virtual CPU\n");
    p = append_str(buf, p, "arch\t\t: x86_64\n");
    p = append_str(buf, p, "bogomips\t: 1000.00\n");
    *len = p;
}

/* ── /proc/mounts ─────────────────────────────────────────────────────────── */

static void fill_mounts(char *buf, usize *len) {
    usize p = 0;
    p = append_str(buf, p, "proc /proc procfs rw 0 0\n");
    p = append_str(buf, p, "ext2 / ext2 rw 0 1\n");
    *len = p;
}

/* ── Entry table ──────────────────────────────────────────────────────────── */

#define PROC_BUF_SZ  4096

static const struct proc_entry proc_entries[] = {
    { "version", fill_version },
    { "meminfo", fill_meminfo },
    { "uptime",  fill_uptime  },
    { "cpuinfo", fill_cpuinfo },
    { "mounts",  fill_mounts  },
};
#define PROC_ENTRY_COUNT (sizeof(proc_entries) / sizeof(proc_entries[0]))

/* ── File-descriptor state ────────────────────────────────────────────────── */

struct procfs_file_data {
    char  buf[PROC_BUF_SZ];
    usize size;
};

/* ── File operations ──────────────────────────────────────────────────────── */

static ssize procfs_read(struct file *f, void *ubuf, usize len) {
    struct procfs_file_data *fd = (struct procfs_file_data *)f->inode->fs_data;
    if (!fd) return -EBADF;
    if (f->offset >= fd->size) return 0;
    usize avail = fd->size - (usize)f->offset;
    usize n     = avail < len ? avail : len;
    u8 *src = (u8 *)fd->buf + (usize)f->offset;
    u8 *dst = (u8 *)ubuf;
    for (usize i = 0; i < n; i++) dst[i] = src[i];
    f->offset += n;
    return (ssize)n;
}

static struct file_ops procfs_fops = {
    .read    = procfs_read,
    .write   = NULL,
    .seek    = NULL,
    .ioctl   = NULL,
    .close   = NULL,
    .readdir = NULL,
};

/* ── Inode operations ─────────────────────────────────────────────────────── */

static struct inode *procfs_lookup(struct inode *dir, const char *name) {
    (void)dir;
    for (usize i = 0; i < PROC_ENTRY_COUNT; i++) {
        /* strcmp equivalent */
        const char *a = proc_entries[i].name, *b = name;
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a == '\0' && *b == '\0') {
            struct inode *in = (struct inode *)kzmalloc(sizeof(*in));
            if (!in) return NULL;
            in->ino  = (u64)(i + 1);
            in->mode = S_IFREG | 0444;
            in->fops = &procfs_fops;

            /* Pre-fill the buffer */
            struct procfs_file_data *fd =
                (struct procfs_file_data *)kzmalloc(sizeof(*fd));
            if (!fd) { kfree(in); return NULL; }
            proc_entries[i].fill(fd->buf, &fd->size);
            in->size    = fd->size;
            in->fs_data = fd;
            return in;
        }
    }
    return NULL;
}

static struct inode_ops procfs_inode_ops = {
    .lookup = procfs_lookup,
    .create = NULL,
    .mkdir  = NULL,
    .unlink = NULL,
    .rmdir  = NULL,
};

/* ── Superblock / mount ───────────────────────────────────────────────────── */

static struct super_block procfs_sb;
static struct dentry      procfs_root_dentry;
static struct inode       procfs_root_inode;

static struct super_block *procfs_mount(void *device) {
    (void)device;

    procfs_root_inode.ino  = 0;
    procfs_root_inode.mode = S_IFDIR | 0555;
    procfs_root_inode.ops  = &procfs_inode_ops;
    procfs_root_inode.fops = NULL;

    procfs_root_dentry.inode       = &procfs_root_inode;
    procfs_root_dentry.parent      = NULL;
    procfs_root_dentry.children    = NULL;
    procfs_root_dentry.next_sibling= NULL;
    procfs_root_dentry.name[0]     = '/';
    procfs_root_dentry.name[1]     = '\0';

    procfs_sb.root       = &procfs_root_dentry;
    procfs_sb.block_size = PAGE_SIZE;
    procfs_sb.fs_data    = NULL;
    return &procfs_sb;
}

static struct filesystem procfs_fs = {
    .name   = "procfs",
    .mount  = procfs_mount,
    .umount = NULL,
    .next   = NULL,
};

/* ── Register procfs ──────────────────────────────────────────────────────── */

void procfs_register(void) {
    vfs_register_fs(&procfs_fs);
    vfs_mount("/proc", "procfs", NULL);
    printk("[proc] /proc mounted\n");
}
