#ifndef KERNEL_SCHED_H
#define KERNEL_SCHED_H

#include "kernel.h"
#include "mm.h"

/* ── Task limits ──────────────────────────────────────────────────────────── */

#define MAX_TASKS        256
#define TASK_NAME_LEN    64
#define KERNEL_STACK_SZ  8192
#define USER_STACK_SZ    65536

/* ── Task states ──────────────────────────────────────────────────────────── */

typedef enum {
    TASK_READY    = 0,
    TASK_RUNNING  = 1,
    TASK_SLEEPING = 2,
    TASK_BLOCKED  = 3,
    TASK_ZOMBIE   = 4,
    TASK_DEAD     = 5,
} task_state_t;

/* ── Saved CPU context (callee-saved registers + RIP/RSP) ─────────────────── */

struct cpu_context {
    u64 r15, r14, r13, r12;
    u64 rbx, rbp;
    /* RIP is implicit from switch_context's call/ret */
};

/* ── Task descriptor ──────────────────────────────────────────────────────── */

struct task_struct {
    /* Identity */
    u32           pid;
    u32           ppid;
    char          name[TASK_NAME_LEN];

    /* State */
    task_state_t  state;
    u8            priority;          /* 0 = lowest, 15 = highest */
    u32           exit_code;

    /* CPU context */
    u64          *kernel_rsp;        /* saved kernel stack pointer */
    u64           cr3;               /* page table root (physical) */

    /* Stacks */
    virt_addr_t   kernel_stack;      /* kernel stack base */
    virt_addr_t   user_stack;        /* user stack top */

    /* Timing */
    u64           ticks_total;       /* total CPU ticks used */
    u64           wake_tick;         /* tick to wake from sleep */
    u32           time_slice;        /* remaining ticks in current slice */

    /* File descriptors */
    struct file  *fd_table[256];
    u32           fd_count;

    /* Memory regions */
    struct vm_area *vm_areas;

    /* Scheduler list links */
    struct task_struct *next;
    struct task_struct *prev;
};

/* ── Scheduler interface ──────────────────────────────────────────────────── */

void  sched_init(void);
void  schedule(void);
void  sched_tick(void);

struct task_struct *task_create(const char *name,
                                void (*entry)(void *),
                                void *arg,
                                u8    priority);

void  task_exit(u32 exit_code);
void  task_sleep(u32 ms);
void  task_yield(void);
struct task_struct *task_get_current(void);
struct task_struct *task_find_by_pid(u32 pid);
void  task_kill(u32 pid);

/* ── Global current task pointer ─────────────────────────────────────────── */

extern struct task_struct *current_task;

#endif /* KERNEL_SCHED_H */
