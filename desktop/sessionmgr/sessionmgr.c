/* sessionmgr.c — AshDE Session Manager */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>

#define SESSION_LOG  "/var/log/asde-session.log"

static FILE *log_fp   = NULL;
static pid_t wm_pid   = -1;
static pid_t panel_pid = -1;
static bool  running  = true;

/* ── Logging ──────────────────────────────────────────────────────────────── */

static void slog(const char *msg) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char ts[32];
    strftime(ts, sizeof(ts), "%H:%M:%S", tm);
    fprintf(stderr, "[%s] sessionmgr: %s\n", ts, msg);
    if (log_fp) { fprintf(log_fp, "[%s] %s\n", ts, msg); fflush(log_fp); }
}

/* ── Launch a process ─────────────────────────────────────────────────────── */

static pid_t launch(const char *path, char *const argv[]) {
    pid_t pid = fork();
    if (pid == 0) {
        execv(path, argv);
        fprintf(stderr, "sessionmgr: execv(%s): %s\n", path, strerror(errno));
        _exit(1);
    }
    return pid;
}

/* ── Set up session environment ───────────────────────────────────────────── */

static void setup_env(void) {
    setenv("DISPLAY",      ":0",                        1);
    setenv("DESKTOP",      "AshDE",                     1);
    setenv("SESSION_TYPE", "asde",                      1);
    setenv("XDG_SESSION",  "asde",                      1);
    setenv("HOME",         getenv("HOME") ? getenv("HOME") : "/root", 0);
    setenv("PATH",         "/bin:/usr/bin:/usr/local/bin:/usr/lib/asde", 1);
}

/* ── SIGCHLD handler ──────────────────────────────────────────────────────── */

static void on_child(int sig) {
    (void)sig;
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Child %d exited with status %d",
                 pid, WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        slog(msg);

        /* Restart critical components */
        if (pid == wm_pid) {
            slog("WM died — restarting");
            char *wm_argv[] = {"/usr/lib/asde/wm", NULL};
            wm_pid = launch("/usr/lib/asde/wm", wm_argv);
        }
    }
}

/* ── SIGTERM/SIGINT: logout ───────────────────────────────────────────────── */

static void on_term(int sig) {
    (void)sig;
    slog("Logout requested");
    running = false;
}

/* ── Shutdown helper ──────────────────────────────────────────────────────── */

static void do_shutdown(bool reboot) {
    slog(reboot ? "System reboot requested" : "System shutdown requested");
    /* Signal all children */
    if (wm_pid    > 0) kill(wm_pid,    SIGTERM);
    if (panel_pid > 0) kill(panel_pid, SIGTERM);
    sleep(2);
    if (reboot) execlp("reboot", "reboot", NULL);
    else        execlp("poweroff", "poweroff", NULL);
    _exit(0);
}

/* ── Main ─────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    mkdir("/var/log", 0755);
    log_fp = fopen(SESSION_LOG, "a");

    slog("AshDE Session Manager starting");

    setup_env();

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_child;
    sa.sa_flags   = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    sa.sa_handler = on_term;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);

    /* Start window manager */
    char *wm_argv[] = {"/usr/lib/asde/wm", NULL};
    wm_pid = launch("/usr/lib/asde/wm", wm_argv);
    if (wm_pid > 0) slog("WM started");
    else            slog("WM failed to start");
    sleep(1);

    /* Start panel */
    char *panel_argv[] = {"/usr/lib/asde/panel", NULL};
    panel_pid = launch("/usr/lib/asde/panel", panel_argv);
    if (panel_pid > 0) slog("Panel started");

    slog("Session ready");

    /* Wait for logout/shutdown */
    while (running) sleep(1);

    slog("Session ending — stopping desktop components");
    if (wm_pid    > 0) { kill(wm_pid,    SIGTERM); waitpid(wm_pid,    NULL, 0); }
    if (panel_pid > 0) { kill(panel_pid, SIGTERM); waitpid(panel_pid, NULL, 0); }

    slog("Session ended");
    if (log_fp) fclose(log_fp);
    return 0;
}
