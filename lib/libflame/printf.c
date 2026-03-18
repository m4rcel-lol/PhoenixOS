#include "include/libflame.h"
#include <stdint.h>

/* syscall is declared in libflame.h; used below to write to stdout */
extern long syscall(long number, ...);

typedef struct {
    char    *buf;   /* NULL = stdout */
    char    *end;   /* one past last writable */
    int      count;
} fmt_ctx;

static void ctx_emit(fmt_ctx *ctx, char c) {
    if (ctx->buf) {
        if (ctx->buf < ctx->end)
            *ctx->buf++ = c;
    } else {
        /* write to fd 1 */
        char ch = c;
        syscall(1 /* SYS_WRITE */, 1, &ch, 1);
    }
    ctx->count++;
}

static void ctx_emit_str(fmt_ctx *ctx, const char *s, int width, int left) {
    int len = 0;
    const char *p = s;
    while (*p++) len++;
    if (!left) for (int i = len; i < width; i++) ctx_emit(ctx, ' ');
    for (int i = 0; i < len; i++) ctx_emit(ctx, s[i]);
    if (left) for (int i = len; i < width; i++) ctx_emit(ctx, ' ');
}

static void ctx_emit_uint(fmt_ctx *ctx, unsigned long long val, int base,
                          int upper, int width, char pad, int left) {
    const char *lo = "0123456789abcdef";
    const char *hi = "0123456789ABCDEF";
    const char *digits = upper ? hi : lo;
    char tmp[64];
    int len = 0;
    if (val == 0) { tmp[len++] = '0'; }
    else { while (val) { tmp[len++] = digits[val % base]; val /= base; } }
    if (!left) for (int i = len; i < width; i++) ctx_emit(ctx, pad);
    for (int i = len - 1; i >= 0; i--) ctx_emit(ctx, tmp[i]);
    if (left) for (int i = len; i < width; i++) ctx_emit(ctx, ' ');
}

static int vformat(fmt_ctx *ctx, const char *fmt, __builtin_va_list ap) {
    while (*fmt) {
        if (*fmt != '%') { ctx_emit(ctx, *fmt++); continue; }
        fmt++;  /* skip '%' */

        /* Flags */
        int left = 0, zero_pad = 0;
        while (*fmt == '-' || *fmt == '0') {
            if (*fmt == '-') left    = 1;
            if (*fmt == '0') zero_pad = 1;
            fmt++;
        }

        /* Width */
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');

        /* Precision (ignored for non-strings for now) */
        int prec = -1;
        if (*fmt == '.') {
            fmt++;
            prec = 0;
            while (*fmt >= '0' && *fmt <= '9') prec = prec * 10 + (*fmt++ - '0');
        }

        /* Length modifier */
        int is_long = 0, is_llong = 0;
        if (*fmt == 'l') {
            is_long = 1; fmt++;
            if (*fmt == 'l') { is_llong = 1; fmt++; }
        }

        char pad = (zero_pad && !left) ? '0' : ' ';

        switch (*fmt++) {
        case 'd': case 'i': {
            long long v = is_llong ? (long long)__builtin_va_arg(ap, long long) :
                          is_long  ? (long long)__builtin_va_arg(ap, long) :
                                     (long long)__builtin_va_arg(ap, int);
            if (v < 0) { ctx_emit(ctx, '-'); v = -v; if (width > 0) width--; }
            ctx_emit_uint(ctx, (unsigned long long)v, 10, 0, width, pad, left);
            break;
        }
        case 'u': {
            unsigned long long v = is_llong ? __builtin_va_arg(ap, unsigned long long) :
                                   is_long  ? (unsigned long long)__builtin_va_arg(ap, unsigned long) :
                                              (unsigned long long)__builtin_va_arg(ap, unsigned int);
            ctx_emit_uint(ctx, v, 10, 0, width, pad, left);
            break;
        }
        case 'x': {
            unsigned long long v = is_llong ? __builtin_va_arg(ap, unsigned long long) :
                                   is_long  ? (unsigned long long)__builtin_va_arg(ap, unsigned long) :
                                              (unsigned long long)__builtin_va_arg(ap, unsigned int);
            ctx_emit_uint(ctx, v, 16, 0, width, pad, left);
            break;
        }
        case 'X': {
            unsigned long long v = is_llong ? __builtin_va_arg(ap, unsigned long long) :
                                   is_long  ? (unsigned long long)__builtin_va_arg(ap, unsigned long) :
                                              (unsigned long long)__builtin_va_arg(ap, unsigned int);
            ctx_emit_uint(ctx, v, 16, 1, width, pad, left);
            break;
        }
        case 'p': {
            unsigned long long v = (unsigned long long)(uintptr_t)__builtin_va_arg(ap, void *);
            ctx_emit(ctx, '0'); ctx_emit(ctx, 'x');
            ctx_emit_uint(ctx, v, 16, 0, width ? width : 16, '0', 0);
            break;
        }
        case 's': {
            const char *s = __builtin_va_arg(ap, const char *);
            if (!s) s = "(null)";
            if (prec >= 0) {
                /* Print at most prec chars */
                int len = 0;
                const char *t = s;
                while (*t++ && len < prec) len++;
                if (!left) for (int i = len; i < width; i++) ctx_emit(ctx, ' ');
                for (int i = 0; i < len; i++) ctx_emit(ctx, s[i]);
                if (left)  for (int i = len; i < width; i++) ctx_emit(ctx, ' ');
            } else {
                ctx_emit_str(ctx, s, width, left);
            }
            break;
        }
        case 'c': {
            char c = (char)__builtin_va_arg(ap, int);
            ctx_emit(ctx, c);
            break;
        }
        case '%':
            ctx_emit(ctx, '%');
            break;
        case 'n': {
            int *n = __builtin_va_arg(ap, int *);
            if (n) *n = ctx->count;
            break;
        }
        default:
            ctx_emit(ctx, '%');
            ctx_emit(ctx, *(fmt-1));
            break;
        }
    }
    return ctx->count;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

int vsnprintf(char *buf, size_t n, const char *fmt, __builtin_va_list ap) {
    fmt_ctx ctx;
    ctx.buf   = buf;
    ctx.end   = buf + (n > 0 ? n - 1 : 0);
    ctx.count = 0;
    int ret = vformat(&ctx, fmt, ap);
    if (buf && n > 0) *ctx.buf = '\0';
    return ret;
}

int vsprintf(char *buf, const char *fmt, __builtin_va_list ap) {
    /* INT_MAX is a safe upper bound; no realistic sprintf fills 2 GiB */
    return vsnprintf(buf, (size_t)0x7fffffff, fmt, ap);
}

int vprintf(const char *fmt, __builtin_va_list ap) {
    fmt_ctx ctx;
    ctx.buf   = NULL;
    ctx.end   = NULL;
    ctx.count = 0;
    return vformat(&ctx, fmt, ap);
}

int snprintf(char *buf, size_t n, const char *fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int ret = vsnprintf(buf, n, fmt, ap);
    __builtin_va_end(ap);
    return ret;
}

int sprintf(char *buf, const char *fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int ret = vsprintf(buf, fmt, ap);
    __builtin_va_end(ap);
    return ret;
}

int printf(const char *fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int ret = vprintf(fmt, ap);
    __builtin_va_end(ap);
    return ret;
}

int vfprintf(FILE *f, const char *fmt, __builtin_va_list ap) {
    (void)f;  /* simplified: always stdout for now */
    return vprintf(fmt, ap);
}

int fprintf(FILE *f, const char *fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int ret = vfprintf(f, fmt, ap);
    __builtin_va_end(ap);
    return ret;
}
