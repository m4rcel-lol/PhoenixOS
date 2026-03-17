#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include "kernel.h"

/* ── Syscall numbers ──────────────────────────────────────────────────────── */

#define SYS_READ      0
#define SYS_WRITE     1
#define SYS_OPEN      2
#define SYS_CLOSE     3
#define SYS_STAT      4
#define SYS_FSTAT     5
#define SYS_LSTAT     6
#define SYS_SEEK      8
#define SYS_MMAP      9
#define SYS_MUNMAP    11
#define SYS_BRK       12
#define SYS_IOCTL     16
#define SYS_READV     19
#define SYS_WRITEV    20
#define SYS_YIELD     24
#define SYS_SLEEP     35
#define SYS_GETPID    39
#define SYS_CLONE     56
#define SYS_FORK      57
#define SYS_EXECVE    59
#define SYS_EXIT      60
#define SYS_WAIT      61
#define SYS_KILL      62
#define SYS_GETPPID   110
#define SYS_GETUID    102
#define SYS_GETGID    104
#define SYS_MKDIR     83
#define SYS_RMDIR     84
#define SYS_UNLINK    87
#define SYS_RENAME    82
#define SYS_PIPE      22
#define SYS_DUP       32
#define SYS_DUP2      33
#define SYS_CHDIR     80
#define SYS_GETCWD    79
#define SYS_GETDENTS  78
#define SYS_UNAME     63
#define SYS_GETTIMEOFDAY 96
#define SYS_SOCKET    41
#define SYS_CONNECT   42
#define SYS_ACCEPT    43
#define SYS_SEND      44
#define SYS_RECV      45

/* ── EFER MSR bits ────────────────────────────────────────────────────────── */

#define MSR_EFER          0xC0000080
#define MSR_STAR          0xC0000081
#define MSR_LSTAR         0xC0000082
#define MSR_CSTAR         0xC0000083
#define MSR_SFMASK        0xC0000084
#define MSR_KERNEL_GS_BASE 0xC0000102

#define EFER_SCE  (1 << 0)   /* syscall enable */
#define EFER_LME  (1 << 8)
#define EFER_LMA  (1 << 10)
#define EFER_NXE  (1 << 11)

/* ── Syscall handler ──────────────────────────────────────────────────────── */

void syscall_init(void);
u64  syscall_handler(u64 num, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5);

/* Entry point from asm */
extern void syscall_entry(void);

#endif /* KERNEL_SYSCALL_H */
