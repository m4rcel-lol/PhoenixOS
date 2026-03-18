#include "../include/kernel.h"
#include "../include/syscall.h"
#include "../include/sched.h"
#include "../include/fs.h"
#include "../include/mm.h"
#include "../include/timer.h"
#include "../arch/x86_64/include/asm.h"

/* ── Syscall dispatch table ───────────────────────────────────────────────── */

typedef u64 (*syscall_fn_t)(u64, u64, u64, u64, u64, u64);

/* ── Individual syscall implementations ──────────────────────────────────── */

static u64 sys_read(u64 fd, u64 buf, u64 count, u64 a4, u64 a5, u64 a6) {
    (void)a4; (void)a5; (void)a6;
    return vfs_read((int)fd, (void *)buf, (usize)count);
}

static u64 sys_write(u64 fd, u64 buf, u64 count, u64 a4, u64 a5, u64 a6) {
    (void)a4; (void)a5; (void)a6;
    const char *s = (const char *)buf;
    if (fd == 1 || fd == 2) {
        for (usize i = 0; i < (usize)count; i++) {
            extern void console_putc(char c);
            extern void serial_putc(char c);
            console_putc(s[i]);
            serial_putc(s[i]);
        }
        return (u64)count;
    }
    return vfs_write((int)fd, (const void *)buf, (usize)count);
}

static u64 sys_open(u64 path, u64 flags, u64 mode, u64 a4, u64 a5, u64 a6) {
    (void)mode; (void)a4; (void)a5; (void)a6;
    return (u64)vfs_open((const char *)path, (int)flags);
}

static u64 sys_close(u64 fd, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return (u64)vfs_close((int)fd);
}

static u64 sys_exit(u64 code, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    task_exit((u32)code);
    return 0; /* unreachable */
}

static u64 sys_getpid(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    struct task_struct *t = task_get_current();
    return t ? t->pid : 0;
}

static u64 sys_getppid(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    struct task_struct *t = task_get_current();
    return t ? t->ppid : 0;
}

static u64 sys_yield(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    task_yield();
    return 0;
}

static u64 sys_sleep(u64 ms, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    task_sleep((u32)ms);
    return 0;
}

static u64 sys_kill(u64 pid, u64 sig, u64 a3, u64 a4, u64 a5, u64 a6) {
    (void)sig; (void)a3; (void)a4; (void)a5; (void)a6;
    task_kill((u32)pid);
    return 0;
}

static u64 sys_enosys(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return (u64)-(s64)ENOSYS;
}

/* ── sys_brk: bump the userspace heap break ───────────────────────────────── */

static virt_addr_t user_brk = 0x400000000ULL;  /* 16 GB mark */
#define USER_BRK_MAX (user_brk + 64ULL * 1024 * 1024)

static u64 sys_brk(u64 new_brk, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (new_brk == 0) return user_brk;
    if (new_brk < user_brk) return user_brk;  /* can't shrink past start */
    if (new_brk > USER_BRK_MAX) return user_brk; /* refuse oversized request */
    /* Map any new pages needed */
    virt_addr_t cur = ALIGN_UP(user_brk, PAGE_SIZE);
    virt_addr_t end = ALIGN_UP(new_brk,  PAGE_SIZE);
    for (virt_addr_t va = cur; va < end; va += PAGE_SIZE) {
        phys_addr_t pa = alloc_page();
        if (!pa) return user_brk;
        map_page(pa, va, PF_WRITE | PF_USER | VM_EXEC);
    }
    user_brk = new_brk;
    return user_brk;
}

/* ── sys_uname: return system / release info ──────────────────────────────── */

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

static void copy_str(char *dst, const char *src, usize max) {
    usize i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static u64 sys_uname(u64 buf, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    struct utsname *u = (struct utsname *)buf;
    if (!u) return (u64)-(s64)EINVAL;
    copy_str(u->sysname,    "PhoenixOS",       65);
    copy_str(u->nodename,   "phoenix",          65);
    copy_str(u->release,    "0.0.2",            65);
    copy_str(u->version,    "EmberKernel 0.1.0",65);
    copy_str(u->machine,    "x86_64",           65);
    copy_str(u->domainname, "(none)",            65);
    return 0;
}

/* ── sys_gettimeofday: return seconds since boot ──────────────────────────── */

struct timeval {
    u64 tv_sec;
    u64 tv_usec;
};

static u64 sys_gettimeofday(u64 tv_ptr, u64 tz_ptr, u64 a3, u64 a4, u64 a5, u64 a6) {
    (void)tz_ptr; (void)a3; (void)a4; (void)a5; (void)a6;
    if (!tv_ptr) return (u64)-(s64)EINVAL;
    struct timeval *tv = (struct timeval *)tv_ptr;
    u64 ticks = timer_ticks;           /* ticks since boot (100 Hz) */
    tv->tv_sec  = ticks / 100;
    tv->tv_usec = (ticks % 100) * 10000;
    return 0;
}

/* ── sys_stat: fill struct stat for a path ────────────────────────────────── */

static u64 sys_stat_impl(u64 path, u64 st_ptr, u64 a3, u64 a4, u64 a5, u64 a6) {
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (!path || !st_ptr) return (u64)-(s64)EINVAL;
    struct stat *st = (struct stat *)st_ptr;
    int r = vfs_stat((const char *)path, st);
    return r < 0 ? (u64)(s64)r : 0;
}

/* ── sys_mmap: anonymous page allocation ──────────────────────────────────── */

static u64 sys_mmap(u64 addr, u64 length, u64 prot, u64 flags, u64 fd_u, u64 offset) {
    (void)prot; (void)flags; (void)fd_u; (void)offset;
    if (!length) return (u64)-(s64)EINVAL;
    usize pages = ALIGN_UP(length, PAGE_SIZE) / PAGE_SIZE;
    virt_addr_t va_start = addr ? (virt_addr_t)addr : user_brk;
    va_start = ALIGN_UP(va_start, PAGE_SIZE);
    for (usize i = 0; i < pages; i++) {
        phys_addr_t pa = alloc_page();
        if (!pa) return (u64)-(s64)ENOMEM;
        map_page(pa, va_start + i * PAGE_SIZE, PF_WRITE | PF_USER | VM_EXEC);
    }
    if (!addr) user_brk = va_start + pages * PAGE_SIZE;
    return va_start;
}

/* ── sys_fstat ────────────────────────────────────────────────────────────── */

static u64 sys_fstat_impl(u64 fd, u64 st_ptr, u64 a3, u64 a4, u64 a5, u64 a6) {
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (!st_ptr) return (u64)-(s64)EINVAL;
    struct stat *st = (struct stat *)st_ptr;
    int r = vfs_fstat((int)fd, st);
    return r < 0 ? (u64)(s64)r : 0;
}

/* ── Dispatch table ───────────────────────────────────────────────────────── */

#define SYSCALL_MAX  256
static syscall_fn_t syscall_table[SYSCALL_MAX];

static void init_table(void) {
    for (int i = 0; i < SYSCALL_MAX; i++)
        syscall_table[i] = sys_enosys;

    syscall_table[SYS_READ]   = sys_read;
    syscall_table[SYS_WRITE]  = sys_write;
    syscall_table[SYS_OPEN]   = sys_open;
    syscall_table[SYS_CLOSE]  = sys_close;
    syscall_table[SYS_EXIT]   = sys_exit;
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_GETPPID]= sys_getppid;
    syscall_table[SYS_YIELD]  = sys_yield;
    syscall_table[SYS_SLEEP]  = sys_sleep;
    syscall_table[SYS_KILL]   = sys_kill;
    syscall_table[SYS_BRK]    = sys_brk;
    syscall_table[SYS_UNAME]  = sys_uname;
    syscall_table[SYS_GETTIMEOFDAY] = sys_gettimeofday;
    syscall_table[SYS_STAT]   = sys_stat_impl;
    syscall_table[SYS_FSTAT]  = sys_fstat_impl;
    syscall_table[SYS_MMAP]   = sys_mmap;
}

/* ── syscall_init: configure SYSCALL/SYSRET MSRs ─────────────────────────── */

void syscall_init(void) {
    init_table();

    /* Enable SCE in EFER */
    u64 efer = rdmsr(MSR_EFER);
    efer |= EFER_SCE;
    wrmsr(MSR_EFER, efer);

    /* STAR: bits[47:32] = kernel CS (SYSRET will use CS+16, DS+8) */
    /* STAR: bits[63:48] = user CS base for SYSRET */
    u64 star = ((u64)0x10 << 48) | ((u64)0x08 << 32);
    wrmsr(MSR_STAR, star);

    /* LSTAR: address of syscall entry point */
    wrmsr(MSR_LSTAR, (u64)syscall_entry);

    /* SFMASK: mask RFLAGS.IF on entry (disable interrupts during syscall) */
    wrmsr(MSR_SFMASK, (1 << 9));

    printk("[sysc] SYSCALL ready. %d entries registered.\n", SYSCALL_MAX);
}

/* ── Main handler (called from syscall_entry asm) ────────────────────────── */

u64 syscall_handler(u64 num, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    if (num >= SYSCALL_MAX) return (u64)-(s64)ENOSYS;
    return syscall_table[num](a1, a2, a3, a4, a5, 0);
}
