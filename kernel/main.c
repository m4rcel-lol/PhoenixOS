#include "include/kernel.h"
#include "arch/x86_64/include/gdt.h"
#include "arch/x86_64/include/idt.h"
#include "include/mm.h"
#include "include/sched.h"
#include "include/fs.h"
#include "include/syscall.h"

/* ── Boot info passed by GRUB (multiboot2) ────────────────────────────────── */

#define MB2_MAGIC            0x36d76289U
#define MB2_TAG_END          0
#define MB2_TAG_MMAP         6
#define MB2_TAG_FRAMEBUFFER  8
#define MB2_TAG_CMDLINE      1
#define MB2_TAG_BOOTLOADER   2
#define MB2_MMAP_AVAILABLE   1

struct mb2_tag {
    u32 type;
    u32 size;
};

struct mb2_tag_mmap {
    u32 type;
    u32 size;
    u32 entry_size;
    u32 entry_version;
    /* followed by mmap entries */
};

struct mb2_mmap_entry {
    u64 base_addr;
    u64 length;
    u32 type;
    u32 reserved;
};

struct mb2_tag_fb {
    u32 type;
    u32 size;
    u64 fb_addr;
    u32 fb_pitch;
    u32 fb_width;
    u32 fb_height;
    u8  fb_bpp;
    u8  fb_type;
    u16 reserved;
};

struct mb2_tag_string {
    u32  type;
    u32  size;
    char string[0];
};

/* ── Version string ───────────────────────────────────────────────────────── */

#define EMBER_BANNER \
    "\n" \
    "  ███████╗███╗   ███╗██████╗ ███████╗██████╗ \n" \
    "  ██╔════╝████╗ ████║██╔══██╗██╔════╝██╔══██╗\n" \
    "  █████╗  ██╔████╔██║██████╔╝█████╗  ██████╔╝\n" \
    "  ██╔══╝  ██║╚██╔╝██║██╔══██╗██╔══╝  ██╔══██╗\n" \
    "  ███████╗██║ ╚═╝ ██║██████╔╝███████╗██║  ██║\n" \
    "  ╚══════╝╚═╝     ╚═╝╚═════╝ ╚══════╝╚═╝  ╚═╝\n" \
    "  EmberKernel v" EMBER_VERSION_STR " — PhoenixOS\n\n"

/* ── Forward declarations for drivers ────────────────────────────────────────*/

void serial_init(void);
void console_init(void);
void fb_init(u32 *addr, u32 width, u32 height, u32 pitch, u32 bpp);
void pit_init(u32 frequency);
void kb_init(void);
void ext2_register(void);

/* ── Idle task entry ──────────────────────────────────────────────────────── */

static void idle_task(void *arg) {
    (void)arg;
    for (;;) {
        __asm__ volatile("hlt");
    }
}

/* ── Kernel entry point ───────────────────────────────────────────────────── */

void kernel_start(u32 mb2_magic, u64 mb2_info) {
    /* Serial must come first — it's always available */
    serial_init();
    early_printk(EMBER_BANNER);
    early_printk("[boot] EmberKernel starting...\n");

    /* Validate multiboot2 magic */
    if (mb2_magic != MB2_MAGIC) {
        early_printk("[boot] FATAL: Invalid multiboot2 magic: 0x%x\n", mb2_magic);
        for (;;) __asm__("hlt");
    }

    /* ── Parse multiboot2 tags ── */
    struct mb2_mmap_entry  *mmap_entries = NULL;
    u32                     mmap_count   = 0;
    u64                     fb_addr      = 0;
    u32                     fb_width     = 0, fb_height = 0;
    u32                     fb_pitch     = 0, fb_bpp    = 0;

    u64 tag_ptr = PHYS_TO_VIRT(mb2_info + 8);  /* skip total_size + reserved */
    struct mb2_tag *tag = (struct mb2_tag *)tag_ptr;

    while (tag->type != MB2_TAG_END) {
        if (tag->type == MB2_TAG_MMAP) {
            struct mb2_tag_mmap *mt = (struct mb2_tag_mmap *)tag;
            mmap_entries = (struct mb2_mmap_entry *)((u8 *)mt + sizeof(*mt));
            mmap_count   = (mt->size - sizeof(*mt)) / mt->entry_size;
        } else if (tag->type == MB2_TAG_FRAMEBUFFER) {
            struct mb2_tag_fb *ft = (struct mb2_tag_fb *)tag;
            fb_addr   = ft->fb_addr;
            fb_width  = ft->fb_width;
            fb_height = ft->fb_height;
            fb_pitch  = ft->fb_pitch;
            fb_bpp    = ft->fb_bpp;
        } else if (tag->type == MB2_TAG_CMDLINE) {
            struct mb2_tag_string *st = (struct mb2_tag_string *)tag;
            early_printk("[boot] cmdline: %s\n", st->string);
        }
        /* Tags are 8-byte aligned */
        tag_ptr += ALIGN_UP(tag->size, 8);
        tag = (struct mb2_tag *)tag_ptr;
    }

    /* ── GDT / IDT ── */
    early_printk("[gdt ] Initializing Global Descriptor Table...\n");
    gdt_init();
    tss_init();

    early_printk("[idt ] Initializing Interrupt Descriptor Table...\n");
    idt_init();

    /* ── Physical memory manager ── */
    early_printk("[pmm ] Initializing physical memory manager...\n");
    if (mmap_entries) {
        pmm_init((struct multiboot_mmap_entry *)mmap_entries, mmap_count);
        pmm_print_stats();
    } else {
        early_printk("[pmm ] WARNING: No memory map from bootloader!\n");
    }

    /* ── Virtual memory manager ── */
    early_printk("[vmm ] Initializing virtual memory manager...\n");
    vmm_init();

    /* ── Framebuffer ── */
    if (fb_addr && fb_bpp == 32) {
        early_printk("[fb  ] Framebuffer at 0x%lx  %dx%d  %dbpp  pitch=%d\n",
            fb_addr, fb_width, fb_height, fb_bpp, fb_pitch);
        fb_init((u32 *)PHYS_TO_VIRT(fb_addr),
                fb_width, fb_height, fb_pitch, fb_bpp);
    }

    /* ── Console ── */
    console_init();
    printk("[con ] Console initialized\n");

    /* ── Timer (PIT at 100 Hz) ── */
    printk("[pit ] Initializing PIT at 100 Hz...\n");
    pit_init(100);

    /* ── Keyboard ── */
    printk("[kb  ] Initializing PS/2 keyboard...\n");
    kb_init();

    /* ── Syscall interface ── */
    printk("[sysc] Setting up SYSCALL/SYSRET...\n");
    syscall_init();

    /* ── VFS + ext2 ── */
    printk("[vfs ] Initializing VFS...\n");
    vfs_init();
    ext2_register();

    /* ── Scheduler ── */
    printk("[sched] Initializing scheduler...\n");
    sched_init();

    /* Enable interrupts */
    __asm__ volatile("sti");

    printk("[boot] Kernel initialization complete. Launching PID 1 (Kindle)...\n");

    /* Create PID 1 — the Kindle init process */
    /* In a real system this would execve /sbin/kindle */
    /* For now we spin up a dummy task as placeholder */
    struct task_struct *init = task_create("kindle", idle_task, NULL, 8);
    if (!init) {
        panic("Failed to create init process!\n");
    }
    init->pid = 1;

    printk("[boot] PID 1 created. Entering idle loop.\n");

    /* The boot CPU becomes the idle task */
    for (;;) {
        schedule();
        __asm__ volatile("hlt");
    }
}
