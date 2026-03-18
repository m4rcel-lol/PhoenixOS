#include "../include/kernel.h"

/* ── Memory utilities ─────────────────────────────────────────────────────── */

void *kmemset(void *dst, int c, usize n) {
    u8 *p = (u8 *)dst;
    while (n--) *p++ = (u8)c;
    return dst;
}

void *kmemcpy(void *dst, const void *src, usize n) {
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    while (n--) *d++ = *s++;
    return dst;
}

int kmemcmp(const void *a, const void *b, usize n) {
    const u8 *p = (const u8 *)a;
    const u8 *q = (const u8 *)b;
    while (n--) {
        if (*p < *q) return -1;
        if (*p > *q) return  1;
        p++; q++;
    }
    return 0;
}

/* ── String utilities ─────────────────────────────────────────────────────── */

usize kstrlen(const char *s) {
    usize n = 0;
    while (*s++) n++;
    return n;
}

char *kstrcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++));
    return dst;
}

char *kstrncpy(char *dst, const char *src, usize n) {
    char *d = dst;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = '\0';
    return dst;
}

int kstrcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int kstrncmp(const char *a, const char *b, usize n) {
    while (n && *a && (*a == *b)) { a++; b++; n--; }
    if (!n) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

char *kstrchr(const char *s, int c) {
    for (; *s; s++)
        if ((unsigned char)*s == (unsigned char)c) return (char *)s;
    if (c == '\0') return (char *)s;
    return NULL;
}

char *kstrrchr(const char *s, int c) {
    const char *last = NULL;
    for (; *s; s++)
        if ((unsigned char)*s == (unsigned char)c) last = s;
    if (c == '\0') return (char *)s;
    return (char *)last;
}
