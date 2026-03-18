/* s.h — S Language Standard Prelude
 *
 * This header is automatically included at the top of every S program.
 * It provides:
 *   - Portable integer type aliases  (i8, i16, i32, i64, u8, u16, u32, u64)
 *   - Boolean type                   (bool, true, false)
 *   - Sized types                    (usize, isize)
 *   - String type alias              (str = char*)
 *   - Memory helpers                 (s_new, s_free, s_zero)
 *   - Useful macros                  (S_ARRAY_LEN, S_UNUSED, S_MIN, S_MAX, S_CLAMP)
 *   - println / print / eprint macros
 *   - Panic / assert macros
 *
 * All definitions guard-wrapped so this can be included alongside stdint.h etc.
 */

#ifndef S_LANG_PRELUDE_H
#define S_LANG_PRELUDE_H

/* Enable POSIX extensions for ssize_t and other POSIX types */
#if !defined(_POSIX_C_SOURCE) && !defined(_GNU_SOURCE)
#  define _POSIX_C_SOURCE 200809L
#endif

/* ── Standard C headers ───────────────────────────────────────────────────── */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

/* ── Integer type aliases ─────────────────────────────────────────────────── */

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float    f32;
typedef double   f64;

typedef size_t   usize;
typedef ssize_t  isize;

/* ── String type alias (char*) ────────────────────────────────────────────── */

typedef char * str;
typedef const char * cstr;

/* ── Null convenience ─────────────────────────────────────────────────────── */

/* null is already mapped to NULL by the transpiler */

/* ── Memory helpers ───────────────────────────────────────────────────────── */

/* Allocate N elements of type T (zero-initialised) */
#define s_new(T, n)   ((T *)calloc((n), sizeof(T)))

/* Free memory (sets pointer to NULL after free) */
#define s_free(p)     do { free(p); (p) = NULL; } while (0)

/* Zero a variable / buffer */
#define s_zero(x)     memset(&(x), 0, sizeof(x))

/* ── Array length ─────────────────────────────────────────────────────────── */

#define S_ARRAY_LEN(a)  ((int)(sizeof(a) / sizeof((a)[0])))

/* ── Suppress unused-variable warnings ───────────────────────────────────── */

#define S_UNUSED(x)   ((void)(x))

/* ── Min / Max / Clamp ────────────────────────────────────────────────────── */

#define S_MIN(a, b)      ((a) < (b) ? (a) : (b))
#define S_MAX(a, b)      ((a) > (b) ? (a) : (b))
#define S_CLAMP(x, lo, hi) S_MAX((lo), S_MIN((x), (hi)))

/* ── Alignment helpers ────────────────────────────────────────────────────── */

#define S_ALIGN_UP(x, align)   (((x) + (align) - 1) & ~((align) - 1))
#define S_ALIGN_DOWN(x, align) ((x) & ~((align) - 1))
#define S_IS_POW2(x)           ((x) && !((x) & ((x) - 1)))

/* ── I/O convenience macros ───────────────────────────────────────────────── */

/* println(fmt, ...) — printf to stdout with trailing newline */
#define println(fmt, ...)   (printf(fmt "\n", ##__VA_ARGS__))

/* print(fmt, ...) — printf to stdout (no newline) */
#define print(fmt, ...)     (printf(fmt, ##__VA_ARGS__))

/* eprint(fmt, ...) — printf to stderr (no newline) */
#define eprint(fmt, ...)    (fprintf(stderr, fmt, ##__VA_ARGS__))

/* eprintln(fmt, ...) — printf to stderr with trailing newline */
#define eprintln(fmt, ...)  (fprintf(stderr, fmt "\n", ##__VA_ARGS__))

/* ── Panic / assertion ────────────────────────────────────────────────────── */

/* panic(msg) — print message and abort */
#define panic(msg)          do { \
    fprintf(stderr, "panic at %s:%d: %s\n", __FILE__, __LINE__, (msg)); \
    abort(); \
} while (0)

/* panic_if(cond, msg) — panic if condition is true */
#define panic_if(cond, msg) do { if (cond) panic(msg); } while (0)

/* unreachable() — mark unreachable code; aborts in debug, UB hint in release */
#ifdef NDEBUG
#  if defined(__GNUC__) || defined(__clang__)
#    define unreachable()  __builtin_unreachable()
#  else
#    define unreachable()  ((void)0)
#  endif
#else
#  define unreachable()    panic("unreachable code reached")
#endif

/* ── Bit manipulation ─────────────────────────────────────────────────────── */

#define S_BIT(n)           (1U << (n))
#define S_BIT64(n)         (1ULL << (n))
#define S_BITMASK(lo, hi)  (((1U << ((hi) - (lo) + 1)) - 1U) << (lo))

/* ── Swap ─────────────────────────────────────────────────────────────────── */

#define S_SWAP(T, a, b)    do { T _tmp = (a); (a) = (b); (b) = _tmp; } while (0)

/* ── Stringification ─────────────────────────────────────────────────────── */

#define S_STRINGIFY(x)     #x
#define S_TOSTRING(x)      S_STRINGIFY(x)

/* ── Compiler attributes ──────────────────────────────────────────────────── */

#if defined(__GNUC__) || defined(__clang__)
#  define S_NORETURN      __attribute__((noreturn))
#  define S_PACKED        __attribute__((packed))
#  define S_PRINTF(f, a)  __attribute__((format(printf, f, a)))
#  define S_LIKELY(x)     __builtin_expect(!!(x), 1)
#  define S_UNLIKELY(x)   __builtin_expect(!!(x), 0)
#  define S_INLINE        static inline __attribute__((always_inline))
#else
#  define S_NORETURN
#  define S_PACKED
#  define S_PRINTF(f, a)
#  define S_LIKELY(x)     (x)
#  define S_UNLIKELY(x)   (x)
#  define S_INLINE        static inline
#endif

/* ── Result type helper (simple OK / ERR) ─────────────────────────────────── */

typedef enum { S_OK = 0, S_ERR = -1 } SResult;

#define s_ok(r)   ((r) == S_OK)
#define s_err(r)  ((r) == S_ERR)

#endif /* S_LANG_PRELUDE_H */
