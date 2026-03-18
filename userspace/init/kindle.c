/* kindle.c — Kindle init system (PID 1) for PhoenixOS */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

/* ── ANSI colour helpers ──────────────────────────────────────────────────── */

#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_RED     "\033[1;31m"
#define ANSI_GREEN   "\033[1;32m"
#define ANSI_YELLOW  "\033[1;33m"
#define ANSI_CYAN    "\033[1;36m"
#define ANSI_WHITE   "\033[1;37m"

/* Status tags — same visual style as OpenRC */
#define TAG_OK    " " ANSI_GREEN  "[ ok ]" ANSI_RESET
#define TAG_FAIL  " " ANSI_RED    "[FAIL]" ANSI_RESET
#define TAG_WARN  " " ANSI_YELLOW "[ !! ]" ANSI_RESET
#define TAG_START       ANSI_CYAN " [ * ]" ANSI_RESET " "

/* Boot splash printed once at startup */
#define KINDLE_SPLASH \
    "\n" \
    ANSI_BOLD \
    "  ██████╗ ██╗  ██╗ ██████╗ ███████╗███╗  ██╗██╗██╗  ██╗ ██████╗ ███████╗\n" \
    "  ██╔══██╗██║  ██║██╔═══██╗██╔════╝████╗ ██║██║╚██╗██╔╝██╔═══██╗██╔════╝\n" \
    "  ██████╔╝███████║██║   ██║█████╗  ██╔██╗██║██║ ╚███╔╝ ██║   ██║███████╗\n" \
    "  ██╔═══╝ ██╔══██║██║   ██║██╔══╝  ██║╚████║██║ ██╔██╗ ██║   ██║╚════██║\n" \
    "  ██║     ██║  ██║╚██████╔╝███████╗██║ ╚███║██║██╔╝╚██╗╚██████╔╝███████║\n" \
    "  ╚═╝     ╚═╝  ╚═╝ ╚═════╝ ╚══════╝╚═╝  ╚══╝╚═╝╚═╝  ╚═╝ ╚═════╝ ╚══════╝\n" \
    ANSI_RESET \
    ANSI_CYAN "  PhoenixOS — Kindle init v" KINDLE_VERSION "   (custom UNIX-like OS)\n" ANSI_RESET \
    "\n"

/* Label column width for "Starting foo-service ..." before the tag */
#define LABEL_WIDTH 52

/*
 * After fork()ing a service we wait this long before checking whether the
 * child already exited (indicating an exec failure).  20 ms is long enough
 * for a typical execv() to fail with ENOENT but short enough that it does
 * not meaningfully delay the startup sequence.
 */
#define SERVICE_STARTUP_CHECK_DELAY_US 20000

/* Kindle init version — update when the init protocol changes */
#define KINDLE_VERSION "1.0"

/* ── Service descriptor ───────────────────────────────────────────────────── */

#define MAX_SERVICES  64
#define NAME_MAX_LEN  64
#define EXEC_MAX_LEN  256
#define DEP_MAX       8

typedef enum { RESTART_NEVER = 0, RESTART_ALWAYS = 1, RESTART_ON_FAIL = 2 } restart_t;
typedef enum { SVC_STOPPED=0, SVC_STARTING=1, SVC_RUNNING=2, SVC_FAILED=3 } svc_state_t;

struct service {
    char        name[NAME_MAX_LEN];
    char        exec[EXEC_MAX_LEN];
    restart_t   restart;
    char        depends[DEP_MAX][NAME_MAX_LEN];
    int         dep_count;
    pid_t       pid;
    svc_state_t state;
    int         restarts;
    time_t      last_start;
};

static struct service services[MAX_SERVICES];
static int service_count = 0;
static FILE *log_file = NULL;

/* ── Logging ──────────────────────────────────────────────────────────────── */

static void kindle_log(const char *level, const char *fmt, ...) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%H:%M:%S", tm);

    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    /* Plain log line to stderr (no colour codes — keeps log file clean) */
    fprintf(stderr, "[%s] kindle/%s: %s\n", timebuf, level, msg);
    if (log_file) {
        fprintf(log_file, "[%s] kindle/%s: %s\n", timebuf, level, msg);
        fflush(log_file);
    }
}

/* ── Boot-status animation helpers (OpenRC-style) ────────────────────────── */

/*
 * Print the "starting" line with a trailing ellipsis and no newline so the
 * tag printed by boot_status_result() lands on the same line.
 *
 *   " [ * ] Starting dbus ...                         "
 */
static void boot_status_begin(const char *svc_name) {
    char label[LABEL_WIDTH + 1];
    int n = snprintf(label, sizeof(label), "Starting %s ...", svc_name);
    /* Pad to LABEL_WIDTH with spaces */
    while (n < LABEL_WIDTH) label[n++] = ' ';
    label[LABEL_WIDTH] = '\0';

    fprintf(stdout, TAG_START "%s", label);
    fflush(stdout);
}

/*
 * Print the result tag and newline after boot_status_begin().
 *   ok == 1  → "[ ok ]\n"
 *   ok == 0  → "[FAIL]\n"
 *   ok == -1 → "[ !! ]\n"  (warning / skipped)
 */
static void boot_status_result(int ok) {
    if (ok > 0)
        fprintf(stdout, "%s\n", TAG_OK);
    else if (ok == 0)
        fprintf(stdout, "%s\n", TAG_FAIL);
    else
        fprintf(stdout, "%s\n", TAG_WARN);
    fflush(stdout);
}

/*
 * Print a one-shot informational status line (no result tag).
 */
static void boot_status_info(const char *fmt, ...) {
    char msg[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    fprintf(stdout, ANSI_BOLD "  ~~  " ANSI_RESET " %s\n", msg);
    fflush(stdout);
}

/* ── Parse a .svc file ────────────────────────────────────────────────────── */

static int parse_svc_file(const char *path, struct service *svc) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    memset(svc, 0, sizeof(*svc));
    svc->restart = RESTART_NEVER;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        /* Strip newline */
        int len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';

        /* Skip comments and blank lines */
        if (line[0] == '#' || line[0] == '\0') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line, *val = eq + 1;

        /* Trim whitespace */
        while (*key == ' ' || *key == '\t') key++;
        while (*val == ' ' || *val == '\t') val++;

        if (strcmp(key, "name") == 0)
            strncpy(svc->name, val, NAME_MAX_LEN - 1);
        else if (strcmp(key, "exec") == 0)
            strncpy(svc->exec, val, EXEC_MAX_LEN - 1);
        else if (strcmp(key, "restart") == 0) {
            if (strcmp(val, "always") == 0)   svc->restart = RESTART_ALWAYS;
            else if (strcmp(val, "on-fail") == 0) svc->restart = RESTART_ON_FAIL;
            else svc->restart = RESTART_NEVER;
        } else if (strcmp(key, "depends") == 0 && svc->dep_count < DEP_MAX) {
            strncpy(svc->depends[svc->dep_count++], val, NAME_MAX_LEN - 1);
        }
    }
    fclose(f);
    return (svc->name[0] && svc->exec[0]) ? 0 : -1;
}

/* ── Load all services from /etc/kindle/services/ ────────────────────────── */

static void load_services(void) {
    const char *dir_path = "/etc/kindle/services";
    DIR *d = opendir(dir_path);
    if (!d) {
        kindle_log("WARN", "Cannot open %s: %s", dir_path, strerror(errno));
        return;
    }

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        int nlen = strlen(de->d_name);
        if (nlen < 5) continue;
        if (strcmp(de->d_name + nlen - 4, ".svc") != 0) continue;

        char full[512];
        snprintf(full, sizeof(full), "%s/%s", dir_path, de->d_name);

        if (service_count >= MAX_SERVICES) {
            kindle_log("WARN", "Too many services, skipping %s", de->d_name);
            break;
        }

        if (parse_svc_file(full, &services[service_count]) == 0) {
            kindle_log("INFO", "Loaded service: %s -> %s",
                       services[service_count].name,
                       services[service_count].exec);
            service_count++;
        } else {
            kindle_log("WARN", "Failed to parse %s", full);
        }
    }
    closedir(d);
}

/* ── Check if a service's dependencies are all running ───────────────────── */

static int deps_satisfied(struct service *svc) {
    for (int i = 0; i < svc->dep_count; i++) {
        bool found = false;
        for (int j = 0; j < service_count; j++) {
            if (strcmp(services[j].name, svc->depends[i]) == 0) {
                if (services[j].state == SVC_RUNNING) { found = true; break; }
            }
        }
        if (!found) return 0;
    }
    return 1;
}

/* ── Start a service ──────────────────────────────────────────────────────── */

static void start_service(struct service *svc) {
    kindle_log("INFO", "Starting service: %s (%s)", svc->name, svc->exec);
    boot_status_begin(svc->name);

    pid_t pid = fork();
    if (pid < 0) {
        kindle_log("ERROR", "fork() failed for %s: %s", svc->name, strerror(errno));
        svc->state = SVC_FAILED;
        boot_status_result(0);
        return;
    }
    if (pid == 0) {
        /* Child: tokenize exec string and execv */
        char exec_copy[EXEC_MAX_LEN];
        strncpy(exec_copy, svc->exec, EXEC_MAX_LEN - 1);
        char *argv[32];
        int argc = 0;
        char *tok = strtok(exec_copy, " \t");
        while (tok && argc < 31) {
            argv[argc++] = tok;
            tok = strtok(NULL, " \t");
        }
        argv[argc] = NULL;
        execv(argv[0], argv);
        fprintf(stderr, "kindle: execv(%s) failed: %s\n", argv[0], strerror(errno));
        _exit(1);
    }

    svc->pid        = pid;
    svc->state      = SVC_RUNNING;
    svc->last_start = time(NULL);
    svc->restarts++;

    /* Give the child a brief moment to detect an immediate exec failure */
    usleep(SERVICE_STARTUP_CHECK_DELAY_US);

    /* If the child already died (execv failure), waitpid will reap it */
    int wstatus = 0;
    pid_t ret = waitpid(pid, &wstatus, WNOHANG);
    if (ret == pid) {
        /* Process exited immediately — exec failure or instant crash */
        svc->pid   = 0;
        svc->state = SVC_FAILED;
        boot_status_result(0);
        kindle_log("ERROR", "Service %s exited immediately (code=%d)",
                   svc->name, WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : -1);
    } else {
        boot_status_result(1);
    }
}

/* ── SIGCHLD handler ──────────────────────────────────────────────────────── */

static void sigchld_handler(int sig) {
    (void)sig;
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < service_count; i++) {
            if (services[i].pid == pid) {
                int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                kindle_log("INFO", "Service %s exited (pid=%d, code=%d)",
                           services[i].name, pid, exit_code);
                services[i].pid   = 0;
                if (exit_code == 0) {
                    services[i].state = SVC_STOPPED;
                } else {
                    services[i].state = SVC_FAILED;
                    /* Print a failure notice on the console */
                    fprintf(stdout,
                            TAG_FAIL " Service " ANSI_BOLD "%s" ANSI_RESET
                            " exited with code %d\n",
                            services[i].name, exit_code);
                    fflush(stdout);
                }
                break;
            }
        }
    }
}

/* ── Main supervision loop ────────────────────────────────────────────────── */

static void supervise(void) {
    while (1) {
        for (int i = 0; i < service_count; i++) {
            struct service *svc = &services[i];
            if (svc->state == SVC_STOPPED || svc->state == SVC_FAILED) {
                bool should_start = false;
                if (svc->state == SVC_STOPPED && svc->pid == 0 && svc->restarts == 0)
                    should_start = true;
                if (svc->state == SVC_FAILED && svc->restart == RESTART_ALWAYS)
                    should_start = true;
                if (svc->state == SVC_FAILED && svc->restart == RESTART_ON_FAIL)
                    should_start = true;

                if (should_start && deps_satisfied(svc))
                    start_service(svc);
            }
        }
        sleep(1);
    }
}

/* ── Entry point ──────────────────────────────────────────────────────────── */

int main(void) {
    /* We are PID 1 */
    if (getpid() != 1)
        fprintf(stderr, "kindle: warning: not running as PID 1\n");

    /* Print boot splash to the console */
    fprintf(stdout, "%s", KINDLE_SPLASH);
    fflush(stdout);

    /* Mount essential filesystems */
    boot_status_begin("proc filesystem");
    system("mount -t proc proc /proc 2>/dev/null");
    boot_status_result(1);

    boot_status_begin("sysfs filesystem");
    system("mount -t sysfs sysfs /sys 2>/dev/null");
    boot_status_result(1);

    boot_status_begin("devtmpfs");
    system("mount -t devtmpfs devtmpfs /dev 2>/dev/null");
    boot_status_result(1);

    /* Open log */
    mkdir("/var/log", 0755);
    log_file = fopen("/var/log/kindle.log", "a");

    kindle_log("INFO", "Kindle init system starting (PhoenixOS)");
    kindle_log("INFO", "PID: %d", getpid());

    /* Handle zombie children */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sa.sa_flags   = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    /* Load and start services */
    load_services();

    boot_status_info("Loaded %d service(s) from /etc/kindle/services", service_count);
    kindle_log("INFO", "Loaded %d services", service_count);

    /* Initial start pass — start services with no deps first */
    for (int i = 0; i < service_count; i++)
        if (services[i].dep_count == 0)
            start_service(&services[i]);

    boot_status_info("Kindle init complete — system is up");

    supervise();  /* never returns */
    return 0;
}
