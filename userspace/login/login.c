/* login.c — PhoenixOS login manager */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <pwd.h>
#include <errno.h>
#include <shadow.h>

#define MAX_USERNAME  64
#define MAX_PASSWORD  256
#define PASSWD_FILE   "/etc/passwd"
#define MAX_ATTEMPTS  5

/* ── Phoenix ASCII art banner ─────────────────────────────────────────────── */

static const char *BANNER =
    "\033[0;33m"
    "           .  .  .\n"
    "         .   . . .  .\n"
    "     . .  .   ___   .  . .\n"
    "    .  . . . /   \\ . . .  .\n"
    "   . . .  . |  P  | .  . . .\n"
    "  .  . . .  | h o | . . .  .\n"
    "   . . .   .|  n  |. . . . .\n"
    "    .  .  .  \\___/  .  .  .\n"
    "     .  .   //|||\\\\  .  .\n"
    "      . . . ||||| . . . .\n"
    "       .  . ||||| .  . .\n"
    "        . .  |||  . . \n"
    "         .    |   .\n"
    "\033[0m\n"
    "\033[1;31m"
    "  ██████╗ ██╗  ██╗ ██████╗ ███████╗███╗  ██╗██╗██╗  ██╗ ██████╗ ███████╗\n"
    "  ██╔══██╗██║  ██║██╔═══██╗██╔════╝████╗ ██║██║╚██╗██╔╝██╔═══██╗██╔════╝\n"
    "  ██████╔╝███████║██║   ██║█████╗  ██╔██╗██║██║ ╚███╔╝ ██║   ██║███████╗\n"
    "  ██╔═══╝ ██╔══██║██║   ██║██╔══╝  ██║╚████║██║ ██╔██╗ ██║   ██║╚════██║\n"
    "  ██║     ██║  ██║╚██████╔╝███████╗██║ ╚███║██║██╔╝╚██╗╚██████╔╝███████║\n"
    "  ╚═╝     ╚═╝  ╚═╝ ╚═════╝ ╚══════╝╚═╝  ╚══╝╚═╝╚═╝  ╚═╝ ╚═════╝ ╚══════╝\n"
    "\033[0m\n"
    "  \033[0;36mEmberKernel — AshDE — PyreShell\033[0m\n\n";

/* ── Read password without echo ───────────────────────────────────────────── */

static void read_password(char *buf, int maxlen) {
    struct termios old, noecho;
    tcgetattr(STDIN_FILENO, &old);
    noecho = old;
    noecho.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &noecho);

    fgets(buf, maxlen, stdin);
    int len = strlen(buf);
    if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';

    tcsetattr(STDIN_FILENO, TCSANOW, &old);
    putchar('\n');
}

/* ── Simple passwd file lookup ────────────────────────────────────────────── */

struct phoenix_passwd {
    char username[MAX_USERNAME];
    char pass_hash[MAX_PASSWORD];
    uid_t uid;
    gid_t gid;
    char home[256];
    char shell[256];
};

static int lookup_user(const char *username, struct phoenix_passwd *out) {
    FILE *f = fopen(PASSWD_FILE, "r");
    if (!f) return -1;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue;
        int len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';

        /* Format: username:hash:uid:gid:home:shell */
        char *fields[6];
        int nfields = 0;
        char *p = line;
        char *tok;
        while ((tok = strsep(&p, ":")) != NULL && nfields < 6) {
            fields[nfields++] = tok;
        }
        if (nfields < 6) continue;

        if (strcmp(fields[0], username) == 0) {
            strncpy(out->username, fields[0], MAX_USERNAME - 1);
            strncpy(out->pass_hash, fields[1], MAX_PASSWORD - 1);
            out->uid = (uid_t)atoi(fields[2]);
            out->gid = (gid_t)atoi(fields[3]);
            strncpy(out->home, fields[4], 255);
            strncpy(out->shell, fields[5], 255);
            fclose(f);
            return 0;
        }
    }
    fclose(f);
    return -1;
}

/* ── Simple password verification (crypt-based if available) ─────────────── */

static int verify_password(const char *entered, const char *stored_hash) {
    /* Empty hash = no password */
    if (stored_hash[0] == '\0' || strcmp(stored_hash, "*") == 0) return 0;

    /* Use crypt() for proper hashing */
    const char *hashed = crypt(entered, stored_hash);
    if (!hashed) return -1;
    return strcmp(hashed, stored_hash) == 0 ? 0 : -1;
}

/* ── Launch session ───────────────────────────────────────────────────────── */

static int launch_session(struct phoenix_passwd *pw) {
    printf("\nWelcome to PhoenixOS, %s!\n\n", pw->username);

    if (setgid(pw->gid) != 0 || setuid(pw->uid) != 0) {
        fprintf(stderr, "login: failed to set UID/GID: %s\n", strerror(errno));
        return -1;
    }

    /* Set up environment */
    setenv("HOME",     pw->home,     1);
    setenv("USER",     pw->username, 1);
    setenv("LOGNAME",  pw->username, 1);
    setenv("SHELL",    pw->shell,    1);
    setenv("PATH",     "/bin:/usr/bin:/sbin:/usr/local/bin", 1);

    if (chdir(pw->home) != 0) chdir("/");

    /* Check if we should start desktop or shell */
    const char *display = getenv("DISPLAY");
    if (!display) {
        /* Console mode: run shell */
        char *shell_argv[] = { pw->shell, NULL };
        execv(pw->shell, shell_argv);
        fprintf(stderr, "login: execv(%s) failed: %s\n", pw->shell, strerror(errno));
        return -1;
    } else {
        /* Desktop mode: start session manager */
        char *sm_argv[] = { "/usr/lib/asde/sessionmgr", NULL };
        execv(sm_argv[0], sm_argv);
        /* Fall back to shell */
        char *shell_argv[] = { pw->shell, NULL };
        execv(pw->shell, shell_argv);
        return -1;
    }
}

/* ── Main ─────────────────────────────────────────────────────────────────── */

int main(void) {
    /* Clear screen */
    printf("\033[2J\033[H");
    printf("%s", BANNER);

    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];

    for (int attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
        printf("  login: ");
        fflush(stdout);
        if (!fgets(username, sizeof(username), stdin)) break;
        int len = strlen(username);
        if (len > 0 && username[len-1] == '\n') username[len-1] = '\0';
        if (len == 0) continue;

        printf("  Password: ");
        fflush(stdout);
        read_password(password, sizeof(password));

        struct phoenix_passwd pw;
        if (lookup_user(username, &pw) != 0) {
            printf("\n  Login incorrect.\n\n");
            sleep(2);
            continue;
        }

        if (verify_password(password, pw.pass_hash) != 0) {
            printf("\n  Login incorrect.\n\n");
            sleep(2);
            continue;
        }

        /* Clear password from memory */
        memset(password, 0, sizeof(password));

        /* Success */
        launch_session(&pw);
        /* If we get here, session ended — loop for re-login */
        printf("\033[2J\033[H%s", BANNER);
        attempt = -1;  /* reset attempt counter after successful login */
    }

    printf("\n  Too many failed login attempts.\n");
    return 1;
}
