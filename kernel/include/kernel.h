#ifndef KERNEL_H
#define KERNEL_H

/* ── Primitive types ──────────────────────────────────────────────────────── */

typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;
typedef signed char         s8;
typedef signed short        s16;
typedef signed int          s32;
typedef signed long long    s64;

typedef u64  usize;
typedef s64  ssize;
typedef u64  uintptr_t;
typedef s64  intptr_t;
typedef u64  size_t;
typedef s64  ptrdiff_t;
typedef u64  phys_addr_t;
typedef u64  virt_addr_t;

/* ── Boolean ──────────────────────────────────────────────────────────────── */

typedef u8 bool;
#define true  ((bool)1)
#define false ((bool)0)

/* ── NULL ─────────────────────────────────────────────────────────────────── */

#define NULL ((void*)0)

/* ── Constants ────────────────────────────────────────────────────────────── */

#define PAGE_SIZE       4096UL
#define PAGE_SHIFT      12
#define PAGE_MASK       (~(PAGE_SIZE - 1))

#define KERNEL_BASE     0xFFFFFFFF80000000ULL
#define KERNEL_PHYS     0x0000000000100000ULL

#define EMBER_VERSION_MAJOR  0
#define EMBER_VERSION_MINOR  1
#define EMBER_VERSION_PATCH  0
#define EMBER_VERSION_STR    "0.1.0"

/* ── Alignment helpers ────────────────────────────────────────────────────── */

#define ALIGN_UP(x, a)    (((x) + (a) - 1) & ~((a) - 1))
#define ALIGN_DOWN(x, a)  ((x) & ~((a) - 1))
#define IS_ALIGNED(x, a)  (((x) & ((a) - 1)) == 0)

/* ── Min / Max ────────────────────────────────────────────────────────────── */

#define MIN(a, b)  ((a) < (b) ? (a) : (b))
#define MAX(a, b)  ((a) > (b) ? (a) : (b))

/* ── Array size ───────────────────────────────────────────────────────────── */

#define ARRAY_SIZE(arr)  (sizeof(arr) / sizeof((arr)[0]))

/* ── Compiler hints ───────────────────────────────────────────────────────── */

#define likely(x)    __builtin_expect(!!(x), 1)
#define unlikely(x)  __builtin_expect(!!(x), 0)
#define __noreturn   __attribute__((noreturn))
#define __packed     __attribute__((packed))
#define __aligned(n) __attribute__((aligned(n)))
#define __used       __attribute__((used))
#define __weak       __attribute__((weak))
#define __noinline   __attribute__((noinline))
#define __always_inline __attribute__((always_inline))

/* ── Offset of ────────────────────────────────────────────────────────────── */

#define offsetof(type, member) __builtin_offsetof(type, member)
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/* ── Bit operations ───────────────────────────────────────────────────────── */

#define BIT(n)       (1UL << (n))
#define BITS(h, l)   (((1UL << ((h) - (l) + 1)) - 1) << (l))
#define SET_BIT(x, n)    ((x) |=  BIT(n))
#define CLEAR_BIT(x, n)  ((x) &= ~BIT(n))
#define TEST_BIT(x, n)   (!!((x) & BIT(n)))

/* ── Error codes ──────────────────────────────────────────────────────────── */

#define ENOMEM    12
#define EINVAL    22
#define ENOENT     2
#define EBADF      9
#define ENOSYS    38
#define EPERM      1
#define EEXIST    17
#define ENODEV    19
#define EAGAIN    11
#define EBUSY     16

/* ── Function declarations ────────────────────────────────────────────────── */

/* printk - kernel formatted output (serial + console) */
void printk(const char *fmt, ...);

/* early_printk - serial only, safe before framebuffer init */
void early_printk(const char *fmt, ...);

/* panic - print message, dump registers, halt */
__noreturn void panic(const char *fmt, ...);

/* kernel assertion */
#define ASSERT(cond) \
    do { \
        if (unlikely(!(cond))) \
            panic("Assertion failed: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
    } while (0)

#define BUG_ON(cond) ASSERT(!(cond))

#endif /* KERNEL_H */
