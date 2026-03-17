/* kill.c — PhoenixOS kill utility */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>

static struct { const char *name; int num; } sigmap[] = {
    {"HUP",  SIGHUP},  {"INT",  SIGINT},  {"QUIT", SIGQUIT},
    {"ILL",  SIGILL},  {"ABRT", SIGABRT}, {"FPE",  SIGFPE},
    {"KILL", SIGKILL}, {"SEGV", SIGSEGV}, {"PIPE", SIGPIPE},
    {"ALRM", SIGALRM}, {"TERM", SIGTERM}, {"USR1", SIGUSR1},
    {"USR2", SIGUSR2}, {"CHLD", SIGCHLD}, {"CONT", SIGCONT},
    {"STOP", SIGSTOP}, {"TSTP", SIGTSTP}, {"TTIN", SIGTTIN},
    {"TTOU", SIGTTOU}, {NULL,   0}
};

static int name_to_sig(const char *name) {
    /* Strip optional "SIG" prefix */
    const char *n = (strncmp(name, "SIG", 3) == 0) ? name + 3 : name;
    for (int i = 0; sigmap[i].name; i++)
        if (strcasecmp(sigmap[i].name, n) == 0) return sigmap[i].num;
    return -1;
}

int main(int argc, char *argv[]) {
    int signum = SIGTERM;
    int i = 1;

    if (i < argc && argv[i][0] == '-') {
        const char *spec = argv[i] + 1;
        /* numeric: -9 */
        char *end;
        long n = strtol(spec, &end, 10);
        if (*end == '\0') {
            signum = (int)n;
        } else {
            signum = name_to_sig(spec);
            if (signum < 0) {
                fprintf(stderr, "kill: invalid signal '%s'\n", spec);
                return 1;
            }
        }
        i++;
    }

    if (i >= argc) {
        fprintf(stderr, "Usage: kill [-signal] PID...\n");
        return 1;
    }

    int ret = 0;
    for (; i < argc; i++) {
        char *end;
        long pid = strtol(argv[i], &end, 10);
        if (*end != '\0') {
            fprintf(stderr, "kill: invalid pid '%s'\n", argv[i]);
            ret = 1;
            continue;
        }
        if (kill((pid_t)pid, signum) != 0) {
            fprintf(stderr, "kill: (%ld) - %s\n", pid, strerror(errno));
            ret = 1;
        }
    }
    return ret;
}
