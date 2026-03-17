#include "include/libflame.h"

/* ── memcpy ───────────────────────────────────────────────────────────────── */
void *memcpy(void *dst, const void *src, size_t n) {
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    while (n--) *d++ = *s++;
    return dst;
}

/* ── memmove ──────────────────────────────────────────────────────────────── */
void *memmove(void *dst, const void *src, size_t n) {
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    if (d < s || d >= s + n) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

/* ── memset ───────────────────────────────────────────────────────────────── */
void *memset(void *s, int c, size_t n) {
    u8 *p = (u8 *)s;
    while (n--) *p++ = (u8)c;
    return s;
}

/* ── memcmp ───────────────────────────────────────────────────────────────── */
int memcmp(const void *a, const void *b, size_t n) {
    const u8 *pa = (const u8 *)a;
    const u8 *pb = (const u8 *)b;
    while (n--) {
        if (*pa != *pb) return *pa - *pb;
        pa++; pb++;
    }
    return 0;
}

/* ── Simple heap allocator using sbrk ────────────────────────────────────── */

#define HEAP_MAGIC  0xF1A6E3D0UL
#define MIN_ALLOC   16

typedef struct block_hdr {
    u32            magic;
    u32            size;     /* usable bytes */
    u32            free;
    struct block_hdr *next;
    struct block_hdr *prev;
} block_hdr_t;

static block_hdr_t *heap_head = NULL;

static void *sbrk_wrapper(ssize_t inc) {
    /* On PhoenixOS we call the SYS_MMAP syscall to get memory */
    extern long syscall(long num, ...);
#define SYS_MMAP 9
#define PROT_RW  3
#define MAP_ANON 0x22
    if (inc <= 0) return (void *)-1;
    void *ptr = (void *)syscall(SYS_MMAP, 0, (size_t)inc, PROT_RW, MAP_ANON, -1, 0);
    if ((long)ptr == -1) return (void *)-1;
    return ptr;
}

void *malloc(size_t size) {
    if (size == 0) return NULL;
    size = (size + MIN_ALLOC - 1) & ~(MIN_ALLOC - 1);

    /* Find free block */
    block_hdr_t *b = heap_head;
    while (b) {
        if (b->free && b->size >= size) {
            /* Split if large enough */
            if (b->size >= size + sizeof(block_hdr_t) + MIN_ALLOC) {
                block_hdr_t *nb = (block_hdr_t *)((u8 *)b + sizeof(block_hdr_t) + size);
                nb->magic = HEAP_MAGIC;
                nb->size  = b->size - size - sizeof(block_hdr_t);
                nb->free  = 1;
                nb->next  = b->next;
                nb->prev  = b;
                if (b->next) b->next->prev = nb;
                b->next   = nb;
                b->size   = size;
            }
            b->free = 0;
            return (u8 *)b + sizeof(block_hdr_t);
        }
        b = b->next;
    }

    /* Expand heap */
    size_t expand = sizeof(block_hdr_t) + size;
    if (expand < 4096) expand = 4096;
    block_hdr_t *nb = (block_hdr_t *)sbrk_wrapper((ssize_t)expand);
    if (nb == (void *)-1) return NULL;
    nb->magic = HEAP_MAGIC;
    nb->size  = expand - sizeof(block_hdr_t);
    nb->free  = 1;
    nb->next  = NULL;
    nb->prev  = NULL;

    if (!heap_head) {
        heap_head = nb;
    } else {
        block_hdr_t *last = heap_head;
        while (last->next) last = last->next;
        last->next = nb;
        nb->prev   = last;
    }
    return malloc(size);
}

void free(void *ptr) {
    if (!ptr) return;
    block_hdr_t *b = (block_hdr_t *)((u8 *)ptr - sizeof(block_hdr_t));
    if (b->magic != HEAP_MAGIC) return;
    b->free = 1;
    /* Coalesce */
    if (b->next && b->next->free) {
        b->size += sizeof(block_hdr_t) + b->next->size;
        b->next  = b->next->next;
        if (b->next) b->next->prev = b;
    }
    if (b->prev && b->prev->free) {
        b->prev->size += sizeof(block_hdr_t) + b->size;
        b->prev->next  = b->next;
        if (b->next) b->next->prev = b->prev;
    }
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (!size) { free(ptr); return NULL; }
    block_hdr_t *b = (block_hdr_t *)((u8 *)ptr - sizeof(block_hdr_t));
    if (b->size >= size) return ptr;
    void *np = malloc(size);
    if (!np) return NULL;
    memcpy(np, ptr, b->size);
    free(ptr);
    return np;
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *ptr = malloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}
