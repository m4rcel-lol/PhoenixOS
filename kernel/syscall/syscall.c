#include "../include/kernel.h"
#include "../include/syscall.h"
#include "../include/sched.h"
#include "../include/fs.h"
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
