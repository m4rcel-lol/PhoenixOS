#include "include/libflame.h"

/* ── strlen ───────────────────────────────────────────────────────────────── */
size_t strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

/* ── strcpy ───────────────────────────────────────────────────────────────── */
char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++))
        ;
    return dst;
}

/* ── strncpy ──────────────────────────────────────────────────────────────── */
char *strncpy(char *dst, const char *src, size_t n) {
    size_t i = 0;
    while (i < n && src[i]) { dst[i] = src[i]; i++; }
    while (i < n) dst[i++] = '\0';
    return dst;
}

/* ── strcmp ───────────────────────────────────────────────────────────────── */
int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

/* ── strncmp ──────────────────────────────────────────────────────────────── */
int strncmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && *a == *b) { a++; b++; }
    if (n == (size_t)-1) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

/* ── strcat ───────────────────────────────────────────────────────────────── */
char *strcat(char *dst, const char *src) {
    char *d = dst;
    while (*d) d++;
    while ((*d++ = *src++))
        ;
    return dst;
}

/* ── strncat ──────────────────────────────────────────────────────────────── */
char *strncat(char *dst, const char *src, size_t n) {
    char *d = dst;
    while (*d) d++;
    while (n-- && *src) *d++ = *src++;
    *d = '\0';
    return dst;
}

/* ── strchr ───────────────────────────────────────────────────────────────── */
char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (c == '\0') ? (char *)s : NULL;
}

/* ── strrchr ──────────────────────────────────────────────────────────────── */
char *strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if (c == '\0') return (char *)s;
    return (char *)last;
}

/* ── strstr ───────────────────────────────────────────────────────────────── */
char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    size_t nlen = strlen(needle);
    while (*haystack) {
        if (strncmp(haystack, needle, nlen) == 0) return (char *)haystack;
        haystack++;
    }
    return NULL;
}

/* ── strtok_r ─────────────────────────────────────────────────────────────── */
char *strtok_r(char *s, const char *delim, char **saveptr) {
    if (s == NULL) s = *saveptr;
    /* Skip leading delimiters */
    while (*s && strchr(delim, *s)) s++;
    if (*s == '\0') { *saveptr = s; return NULL; }
    char *start = s;
    while (*s && !strchr(delim, *s)) s++;
    if (*s) { *s++ = '\0'; }
    *saveptr = s;
    return start;
}

/* ── strtok ───────────────────────────────────────────────────────────────── */
char *strtok(char *s, const char *delim) {
    static char *saved = NULL;
    return strtok_r(s, delim, &saved);
}

/* ── strtol ───────────────────────────────────────────────────────────────── */
long strtol(const char *s, char **endptr, int base) {
    while (*s == ' ' || *s == '\t') s++;
    int neg = 0;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;

    if (base == 0) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; s += 2; }
        else if (s[0] == '0') { base = 8; s++; }
        else base = 10;
    } else if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;

    long result = 0;
    const char *start = s;
    while (*s) {
        int digit;
        if (*s >= '0' && *s <= '9')      digit = *s - '0';
        else if (*s >= 'a' && *s <= 'f') digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') digit = *s - 'A' + 10;
        else break;
        if (digit >= base) break;
        result = result * base + digit;
        s++;
    }
    if (s == start) s = (const char *)start;  /* no conversion */
    if (endptr) *endptr = (char *)s;
    return neg ? -result : result;
}

/* ── strtoul ──────────────────────────────────────────────────────────────── */
unsigned long strtoul(const char *s, char **endptr, int base) {
    return (unsigned long)strtol(s, endptr, base);
}

/* ── atoi / atol ──────────────────────────────────────────────────────────── */
int  atoi(const char *s) { return (int)strtol(s, NULL, 10); }
long atol(const char *s) { return strtol(s, NULL, 10); }

/* ── itoa ─────────────────────────────────────────────────────────────────── */
char *itoa(int val, char *buf, int base) {
    if (base < 2 || base > 36) { buf[0] = '\0'; return buf; }
    int neg = 0;
    if (val < 0 && base == 10) { neg = 1; val = -val; }
    char tmp[64];
    int i = 0;
    unsigned int uval = (unsigned int)val;
    if (uval == 0) tmp[i++] = '0';
    while (uval > 0) {
        int rem = uval % base;
        tmp[i++] = rem < 10 ? '0' + rem : 'a' + rem - 10;
        uval /= base;
    }
    if (neg) tmp[i++] = '-';
    int j = 0;
    while (i-- > 0) buf[j++] = tmp[i];
    buf[j] = '\0';
    return buf;
}

/* ── strdup ───────────────────────────────────────────────────────────────── */
char *strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *copy = (char *)malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}
