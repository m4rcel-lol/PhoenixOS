#include "../include/kernel.h"
#include "../include/sched.h"
#include "../include/mm.h"

/* ── Task table ───────────────────────────────────────────────────────────── */

static struct task_struct task_pool[MAX_TASKS];
static u32 next_pid = 0;

struct task_struct *current_task = NULL;

/* Ready queue (simple circular doubly linked list head) */
static struct task_struct *ready_head = NULL;

/* ── Idle task ────────────────────────────────────────────────────────────── */

static void idle_fn(void *arg) {
    (void)arg;
    for (;;) __asm__ volatile("hlt");
}

/* ── Allocate a task_struct slot ──────────────────────────────────────────── */

static struct task_struct *alloc_task(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_pool[i].state == TASK_DEAD || task_pool[i].pid == 0) {
            struct task_struct *t = &task_pool[i];
            /* zero it out */
            u8 *p = (u8 *)t;
            for (usize j = 0; j < sizeof(*t); j++) p[j] = 0;
            return t;
        }
    }
    return NULL;
}

/* ── Enqueue to ready list ────────────────────────────────────────────────── */

static void enqueue_task(struct task_struct *t) {
    if (!ready_head) {
        ready_head  = t;
        t->next     = t;
        t->prev     = t;
    } else {
        struct task_struct *tail = ready_head->prev;
        tail->next       = t;
        t->prev          = tail;
        t->next          = ready_head;
        ready_head->prev = t;
    }
}

static void dequeue_task(struct task_struct *t) {
    if (t->next == t) {
        /* Only element */
        ready_head = NULL;
    } else {
        if (ready_head == t) ready_head = t->next;
        t->prev->next = t->next;
        t->next->prev = t->prev;
    }
    t->next = NULL;
    t->prev = NULL;
}

/* ── Switch context (defined in startup.asm) ──────────────────────────────── */

extern void switch_context(u64 **old_rsp, u64 *new_rsp);

/* ── task_create ──────────────────────────────────────────────────────────── */

struct task_struct *task_create(const char *name, void (*entry)(void *),
                                void *arg, u8 priority) {
    struct task_struct *t = alloc_task();
    if (!t) return NULL;

    t->pid      = ++next_pid;
    t->ppid     = current_task ? current_task->pid : 0;
    t->priority = priority;
    t->state    = TASK_READY;
    t->time_slice = 10 + priority;   /* higher priority = more ticks */

    /* Copy name */
    usize i = 0;
    while (name[i] && i < TASK_NAME_LEN - 1) { t->name[i] = name[i]; i++; }
    t->name[i] = '\0';

    /* Allocate kernel stack (8 KB) */
    t->kernel_stack = (virt_addr_t)kmalloc(KERNEL_STACK_SZ);
    if (!t->kernel_stack) {
        t->pid = 0;
        return NULL;
    }

    /* Set up initial kernel stack frame so switch_context "returns" to entry */
    u64 *sp = (u64 *)(t->kernel_stack + KERNEL_STACK_SZ);

    /* Simulate what switch_context restore expects:
       It pops: r15, r14, r13, r12, rbx, rbp, then rets.
       We put a task_trampoline address as the "return address". */

    /* Push argument and entry function for trampoline */
    *(--sp) = (u64)arg;
    *(--sp) = (u64)entry;

    /* Push saved callee-saved registers (all zero) */
    *(--sp) = 0;  /* rbp */
    *(--sp) = 0;  /* rbx */
    *(--sp) = 0;  /* r12 */
    *(--sp) = 0;  /* r13 */
    *(--sp) = 0;  /* r14 */
    *(--sp) = 0;  /* r15 */

    /* "return address" for switch_context's ret */
    extern void task_trampoline(void);
    *(--sp) = (u64)task_trampoline;

    t->kernel_rsp = sp;

    /* Use kernel PML4 */
    extern u64 *get_pml4(void);
    extern phys_addr_t alloc_page(void);
    t->cr3 = (u64)get_pml4();  /* simplified: share kernel address space */

    enqueue_task(t);
    return t;
}

/* ── Trampoline: called when a task first runs ────────────────────────────── */

/* Defined in C with __attribute__((naked)) to avoid prologue clobbering stack */
__attribute__((naked)) void task_trampoline(void) {
    __asm__ volatile(
        "pop %rsi\n"   /* entry function */
        "pop %rdi\n"   /* arg */
        "sti\n"
        "call *%rsi\n"
        /* If entry returns, call task_exit(0) */
        "xor %edi, %edi\n"
        "call task_exit\n"
        "ud2\n"
    );
}

/* ── schedule ─────────────────────────────────────────────────────────────── */

void schedule(void) {
    if (!ready_head) return;

    struct task_struct *prev = current_task;
    struct task_struct *next = ready_head;

    /* Find next READY task */
    struct task_struct *start = next;
    do {
        if (next->state == TASK_READY && next != prev) break;
        next = next->next;
    } while (next != start);

    if (next == prev || next->state != TASK_READY) return;

    /* Advance ready_head round-robin */
    ready_head = next->next;
    current_task = next;
    next->state = TASK_RUNNING;

    if (prev && prev->state == TASK_RUNNING) prev->state = TASK_READY;

    if (prev)
        switch_context(&prev->kernel_rsp, (u64)next->kernel_rsp);
    else {
        /* First schedule — fake old RSP */
        u64 *dummy = NULL;
        switch_context(&dummy, (u64)next->kernel_rsp);
    }
}

/* ── sched_tick (called from timer IRQ) ──────────────────────────────────── */

void sched_tick(void) {
    extern volatile u64 timer_ticks;

    /* Wake sleeping tasks */
    for (int i = 0; i < MAX_TASKS; i++) {
        struct task_struct *t = &task_pool[i];
        if (t->state == TASK_SLEEPING && t->wake_tick <= timer_ticks) {
            t->state = TASK_READY;
        }
    }

    if (current_task) {
        if (current_task->time_slice > 0)
            current_task->time_slice--;
        if (current_task->time_slice == 0) {
            current_task->time_slice = 10 + current_task->priority;
            schedule();
        }
    }
}

/* ── task_exit ────────────────────────────────────────────────────────────── */

void task_exit(u32 exit_code) {
    __asm__ volatile("cli");
    current_task->exit_code = exit_code;
    current_task->state     = TASK_ZOMBIE;
    dequeue_task(current_task);
    schedule();
    for (;;) __asm__("hlt");
}

/* ── task_sleep ───────────────────────────────────────────────────────────── */

void task_sleep(u32 ms) {
    extern volatile u64 timer_ticks;
    extern u32 ticks_per_ms;
    if (!current_task) return;
    current_task->wake_tick = timer_ticks + (u64)ms;
    current_task->state     = TASK_SLEEPING;
    schedule();
}

void task_yield(void) {
    current_task->time_slice = 0;
    schedule();
}

/* ── sched_init ───────────────────────────────────────────────────────────── */

void sched_init(void) {
    /* Mark all slots dead */
    for (int i = 0; i < MAX_TASKS; i++) {
        task_pool[i].state = TASK_DEAD;
        task_pool[i].pid   = 0;
    }
    /* Create idle task (PID 0) */
    struct task_struct *idle = alloc_task();
    idle->pid      = 0;
    idle->ppid     = 0;
    idle->priority = 0;
    idle->state    = TASK_READY;

    /* Copy name */
    const char *idlename = "idle";
    for (int i = 0; idlename[i]; i++) idle->name[i] = idlename[i];

    idle->kernel_stack = (virt_addr_t)kmalloc(KERNEL_STACK_SZ);
    u64 *sp = (u64 *)(idle->kernel_stack + KERNEL_STACK_SZ);
    *(--sp) = 0;  /* arg */
    *(--sp) = (u64)idle_fn;
    *(--sp) = 0; *(--sp) = 0; *(--sp) = 0;
    *(--sp) = 0; *(--sp) = 0; *(--sp) = 0;
    extern void task_trampoline(void);
    *(--sp) = (u64)task_trampoline;
    idle->kernel_rsp = sp;

    enqueue_task(idle);
    current_task = idle;
}

struct task_struct *task_get_current(void) { return current_task; }

struct task_struct *task_find_by_pid(u32 pid) {
    for (int i = 0; i < MAX_TASKS; i++)
        if (task_pool[i].pid == pid && task_pool[i].state != TASK_DEAD)
            return &task_pool[i];
    return NULL;
}

void task_kill(u32 pid) {
    struct task_struct *t = task_find_by_pid(pid);
    if (t && t != current_task) {
        t->state = TASK_ZOMBIE;
        dequeue_task(t);
    }
}
