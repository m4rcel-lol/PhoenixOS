#ifndef LIBFLAME_H
#define LIBFLAME_H

/* libflame — PhoenixOS standard C-like library */

#ifdef __cplusplus
extern "C" {
#endif

/* ── Types ────────────────────────────────────────────────────────────────── */

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef signed char        s8;
typedef signed short       s16;
typedef signed int         s32;
typedef signed long long   s64;

typedef u64  size_t;
typedef s64  ssize_t;
typedef s32  pid_t;
typedef u32  uid_t;
typedef u32  gid_t;
typedef s64  off_t;
typedef s64  time_t;

typedef u8 bool;
#define true  1
#define false 0
#define NULL  ((void *)0)

/* ── Limits ───────────────────────────────────────────────────────────────── */

#define INT_MAX   0x7FFFFFFF
#define INT_MIN   (-INT_MAX - 1)
#define UINT_MAX  0xFFFFFFFFU
#define SIZE_MAX  ((size_t)-1)

/* ── String functions ─────────────────────────────────────────────────────── */

size_t  strlen(const char *s);
char   *strcpy(char *dst, const char *src);
char   *strncpy(char *dst, const char *src, size_t n);
int     strcmp(const char *a, const char *b);
int     strncmp(const char *a, const char *b, size_t n);
char   *strcat(char *dst, const char *src);
char   *strncat(char *dst, const char *src, size_t n);
char   *strchr(const char *s, int c);
char   *strrchr(const char *s, int c);
char   *strstr(const char *haystack, const char *needle);
char   *strtok(char *s, const char *delim);
char   *strtok_r(char *s, const char *delim, char **saveptr);
long    strtol(const char *s, char **endptr, int base);
unsigned long strtoul(const char *s, char **endptr, int base);
int     atoi(const char *s);
long    atol(const char *s);
char   *itoa(int val, char *buf, int base);
char   *strdup(const char *s);

/* ── Memory functions ─────────────────────────────────────────────────────── */

void   *memcpy(void *dst, const void *src, size_t n);
void   *memmove(void *dst, const void *src, size_t n);
void   *memset(void *s, int c, size_t n);
int     memcmp(const void *a, const void *b, size_t n);
void   *malloc(size_t size);
void    free(void *ptr);
void   *realloc(void *ptr, size_t size);
void   *calloc(size_t nmemb, size_t size);

/* ── I/O ──────────────────────────────────────────────────────────────────── */

typedef struct _FILE FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int     printf(const char *fmt, ...);
int     fprintf(FILE *f, const char *fmt, ...);
int     sprintf(char *buf, const char *fmt, ...);
int     snprintf(char *buf, size_t n, const char *fmt, ...);
int     vprintf(const char *fmt, __builtin_va_list ap);
int     vfprintf(FILE *f, const char *fmt, __builtin_va_list ap);
int     vsprintf(char *buf, const char *fmt, __builtin_va_list ap);
int     vsnprintf(char *buf, size_t n, const char *fmt, __builtin_va_list ap);

int     puts(const char *s);
int     putchar(int c);
int     getchar(void);
int     fputs(const char *s, FILE *f);
int     fputc(int c, FILE *f);
int     fgetc(FILE *f);
char   *fgets(char *buf, int n, FILE *f);

FILE   *fopen(const char *path, const char *mode);
int     fclose(FILE *f);
size_t  fread(void *buf, size_t size, size_t nmemb, FILE *f);
size_t  fwrite(const void *buf, size_t size, size_t nmemb, FILE *f);
int     fseek(FILE *f, long offset, int whence);
long    ftell(FILE *f);
int     fflush(FILE *f);
int     feof(FILE *f);
int     ferror(FILE *f);

#define EOF     (-1)
#define SEEK_SET  0
#define SEEK_CUR  1
#define SEEK_END  2

/* ── Process ──────────────────────────────────────────────────────────────── */

void    exit(int code);
void   _exit(int code);
pid_t   getpid(void);
pid_t   getppid(void);
pid_t   fork(void);
int     execv(const char *path, char *const argv[]);
int     execve(const char *path, char *const argv[], char *const envp[]);
int     waitpid(pid_t pid, int *status, int options);

/* ── Syscall numbers ──────────────────────────────────────────────────────── */

#define SYS_READ    0
#define SYS_WRITE   1
#define SYS_OPEN    2
#define SYS_CLOSE   3
#define SYS_MMAP    9
#define SYS_MUNMAP  11
#define SYS_IOCTL   16
#define SYS_YIELD   24
#define SYS_SLEEP   35
#define SYS_GETPID  39
#define SYS_FORK    57
#define SYS_EXECVE  59
#define SYS_EXIT    60
#define SYS_WAIT    61

long syscall(long num, ...);

/* ── Error codes ──────────────────────────────────────────────────────────── */

extern int errno;

#define EPERM   1
#define ENOENT  2
#define ESRCH   3
#define EINTR   4
#define EIO     5
#define ENOEXEC 8
#define EBADF   9
#define ECHILD  10
#define ENOMEM  12
#define EACCES  13
#define EFAULT  14
#define EBUSY   16
#define EEXIST  17
#define ENODEV  19
#define ENOTDIR 20
#define EISDIR  21
#define EINVAL  22
#define EMFILE  24
#define ENOSYS  38

#ifdef __cplusplus
}
#endif

#endif /* LIBFLAME_H */
