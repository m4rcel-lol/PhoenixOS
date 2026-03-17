#include "../include/kernel.h"
#include "../include/fs.h"
#include "../include/mm.h"

/* ── ext2 on-disk structures ──────────────────────────────────────────────── */

struct ext2_superblock {
    u32 s_inodes_count;
    u32 s_blocks_count;
    u32 s_r_blocks_count;
    u32 s_free_blocks_count;
    u32 s_free_inodes_count;
    u32 s_first_data_block;
    u32 s_log_block_size;    /* block_size = 1024 << s_log_block_size */
    u32 s_log_frag_size;
    u32 s_blocks_per_group;
    u32 s_frags_per_group;
    u32 s_inodes_per_group;
    u32 s_mtime;
    u32 s_wtime;
    u16 s_mnt_count;
    u16 s_max_mnt_count;
    u16 s_magic;             /* 0xEF53 */
    u16 s_state;
    u16 s_errors;
    u16 s_minor_rev_level;
    u32 s_lastcheck;
    u32 s_checkinterval;
    u32 s_creator_os;
    u32 s_rev_level;
    u16 s_def_resuid;
    u16 s_def_resgid;
    /* EXT2_DYNAMIC_REV fields */
    u32 s_first_ino;
    u16 s_inode_size;
    u16 s_block_group_nr;
    u32 s_feature_compat;
    u32 s_feature_incompat;
    u32 s_feature_ro_compat;
} __packed;

struct ext2_group_desc {
    u32 bg_block_bitmap;
    u32 bg_inode_bitmap;
    u32 bg_inode_table;
    u16 bg_free_blocks_count;
    u16 bg_free_inodes_count;
    u16 bg_used_dirs_count;
    u16 bg_pad;
    u8  bg_reserved[12];
} __packed;

struct ext2_inode {
    u16 i_mode;
    u16 i_uid;
    u32 i_size;
    u32 i_atime;
    u32 i_ctime;
    u32 i_mtime;
    u32 i_dtime;
    u16 i_gid;
    u16 i_links_count;
    u32 i_blocks;
    u32 i_flags;
    u32 i_osd1;
    u32 i_block[15];    /* direct [0-11], single ind [12], double [13], triple [14] */
    u32 i_generation;
    u32 i_file_acl;
    u32 i_dir_acl;
    u32 i_faddr;
    u8  i_osd2[12];
} __packed;

struct ext2_dir_entry {
    u32 inode;
    u16 rec_len;
    u8  name_len;
    u8  file_type;
    char name[0];
} __packed;

#define EXT2_MAGIC        0xEF53
#define EXT2_ROOT_INO     2
#define EXT2_S_IFDIR      0x4000
#define EXT2_S_IFREG      0x8000

/* ── ext2 mount data ──────────────────────────────────────────────────────── */

struct ext2_fs {
    u8  *disk;        /* pointer to raw disk image in memory */
    u32  block_size;
    u32  inodes_per_group;
    u32  inode_size;
    struct ext2_superblock sb;
};

/* ── Block read helper ────────────────────────────────────────────────────── */

static void *ext2_read_block(struct ext2_fs *fs, u32 block_no) {
    u64 offset = (u64)block_no * fs->block_size;
    return fs->disk + offset;
}

/* ── Read inode ───────────────────────────────────────────────────────────── */

static struct ext2_inode *ext2_get_inode(struct ext2_fs *fs, u32 ino) {
    if (ino == 0) return NULL;
    u32 group   = (ino - 1) / fs->inodes_per_group;
    u32 index   = (ino - 1) % fs->inodes_per_group;

    /* Group descriptor table starts at block 2 (for 1KB blocks) or 1 (for larger) */
    u32 gdt_block = fs->block_size == 1024 ? 2 : 1;
    struct ext2_group_desc *gd =
        (struct ext2_group_desc *)ext2_read_block(fs, gdt_block);
    gd += group;

    u8 *inode_table = (u8 *)ext2_read_block(fs, gd->bg_inode_table);
    return (struct ext2_inode *)(inode_table + index * fs->inode_size);
}

/* ── Read data from inode ─────────────────────────────────────────────────── */

static usize ext2_read_inode_data(struct ext2_fs *fs, struct ext2_inode *in,
                                  u64 offset, u8 *buf, usize len) {
    usize read = 0;

    while (read < len && offset < in->i_size) {
        u32 block_idx = (u32)(offset / fs->block_size);
        u32 block_off = (u32)(offset % fs->block_size);

        u32 block_no;
        if (block_idx < 12) {
            block_no = in->i_block[block_idx];
        } else {
            /* Single indirect */
            u32 *ind = (u32 *)ext2_read_block(fs, in->i_block[12]);
            block_no = ind[block_idx - 12];
        }

        if (block_no == 0) break;

        u8 *block = (u8 *)ext2_read_block(fs, block_no);
        usize to_copy = fs->block_size - block_off;
        if (to_copy > len - read) to_copy = len - read;
        if (offset + to_copy > in->i_size) to_copy = (usize)(in->i_size - offset);

        for (usize i = 0; i < to_copy; i++) buf[read + i] = block[block_off + i];

        read   += to_copy;
        offset += to_copy;
    }
    return read;
}

/* ── VFS inode operations ─────────────────────────────────────────────────── */

static struct inode *ext2_lookup(struct inode *dir_inode, const char *name) {
    struct ext2_fs *fs = (struct ext2_fs *)dir_inode->sb->fs_data;
    struct ext2_inode *in = (struct ext2_inode *)dir_inode->fs_data;
    if (!in || !(in->i_mode & EXT2_S_IFDIR)) return NULL;

    /* Read directory blocks */
    u8 block_buf[4096];
    u64 off = 0;
    while (off < in->i_size) {
        usize n = ext2_read_inode_data(fs, in, off, block_buf,
                                        MIN((usize)fs->block_size, sizeof(block_buf)));
        if (n == 0) break;

        u8 *p = block_buf;
        u8 *end = p + n;
        while (p < end) {
            struct ext2_dir_entry *de = (struct ext2_dir_entry *)p;
            if (de->rec_len == 0) break;

            if (de->inode != 0 && de->name_len > 0) {
                /* Compare name */
                bool match = true;
                for (u8 i = 0; i < de->name_len; i++) {
                    if (de->name[i] != name[i]) { match = false; break; }
                }
                if (match && name[de->name_len] == '\0') {
                    /* Found — build inode */
                    struct ext2_inode *ei = ext2_get_inode(fs, de->inode);
                    struct inode *vnode = (struct inode *)kzmalloc(sizeof(*vnode));
                    vnode->ino  = de->inode;
                    vnode->mode = ei->i_mode;
                    vnode->size = ei->i_size;
                    vnode->uid  = ei->i_uid;
                    vnode->gid  = ei->i_gid;
                    vnode->mtime = ei->i_mtime;
                    vnode->nlink = ei->i_links_count;
                    vnode->sb    = dir_inode->sb;
                    vnode->ops   = dir_inode->ops;
                    vnode->fs_data = ei;
                    return vnode;
                }
            }
            p += de->rec_len;
        }
        off += n;
    }
    return NULL;
}

static struct inode_ops ext2_inode_ops = {
    .lookup = ext2_lookup,
    .create = NULL,   /* read-only for Phase 1 */
    .mkdir  = NULL,
    .unlink = NULL,
    .rmdir  = NULL,
};

/* ── File operations (read) ───────────────────────────────────────────────── */

static ssize ext2_file_read(struct file *f, void *buf, usize len) {
    struct ext2_fs *fs = (struct ext2_fs *)f->inode->sb->fs_data;
    struct ext2_inode *in = (struct ext2_inode *)f->inode->fs_data;
    usize n = ext2_read_inode_data(fs, in, f->offset, (u8 *)buf, len);
    f->offset += n;
    return (ssize)n;
}

static struct file_ops ext2_file_ops = {
    .read  = ext2_file_read,
    .write = NULL,
    .close = NULL,
};

/* ── ext2_mount ───────────────────────────────────────────────────────────── */

static struct super_block *ext2_mount(void *device) {
    u8 *disk = (u8 *)device;

    /* Superblock is at offset 1024 */
    struct ext2_superblock *sb_disk = (struct ext2_superblock *)(disk + 1024);
    if (sb_disk->s_magic != EXT2_MAGIC) {
        printk("[ext2] Bad magic: 0x%x\n", sb_disk->s_magic);
        return NULL;
    }

    struct ext2_fs *fs = (struct ext2_fs *)kzmalloc(sizeof(*fs));
    fs->disk  = disk;
    fs->block_size = 1024U << sb_disk->s_log_block_size;
    fs->inodes_per_group = sb_disk->s_inodes_per_group;
    fs->inode_size = (sb_disk->s_rev_level >= 1) ? sb_disk->s_inode_size : 128;

    /* Copy superblock */
    u8 *s = (u8 *)sb_disk, *d = (u8 *)&fs->sb;
    for (usize i = 0; i < sizeof(fs->sb); i++) d[i] = s[i];

    /* Read root inode */
    struct ext2_inode *root_in = ext2_get_inode(fs, EXT2_ROOT_INO);

    struct inode *root_inode = (struct inode *)kzmalloc(sizeof(*inode));
    root_inode->ino   = EXT2_ROOT_INO;
    root_inode->mode  = root_in->i_mode;
    root_inode->size  = root_in->i_size;
    root_inode->ops   = &ext2_inode_ops;
    root_inode->fs_data = root_in;

    struct dentry *root_dentry = (struct dentry *)kzmalloc(sizeof(*root_dentry));
    root_dentry->name[0] = '/';
    root_dentry->inode   = root_inode;

    struct super_block *sblk = (struct super_block *)kzmalloc(sizeof(*sblk));
    sblk->root       = root_dentry;
    sblk->block_size = fs->block_size;
    sblk->fs_data    = fs;

    root_inode->sb = sblk;

    printk("[ext2] Mounted: block_size=%u  inodes=%u  blocks=%u\n",
           fs->block_size, sb_disk->s_inodes_count, sb_disk->s_blocks_count);
    return sblk;
}

static struct filesystem ext2_fs = {
    .name   = "ext2",
    .mount  = ext2_mount,
    .umount = NULL,
};

void ext2_register(void) {
    vfs_register_fs(&ext2_fs);
}
