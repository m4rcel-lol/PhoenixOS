/* pyre.c — PyreShell: the PhoenixOS command shell */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <termios.h>
#include <signal.h>

#define MAX_LINE      2048
#define MAX_ARGS      128
#define MAX_HISTORY   50
#define MAX_PATH_DIRS 32

static char  history[MAX_HISTORY][MAX_LINE];
static int   hist_count = 0;
static int   hist_idx   = 0;
static char  cwd[4096]  = "/";
static char  hostname[256] = "phoenix";

/* ── History ──────────────────────────────────────────────────────────────── */

static void hist_push(const char *line) {
    if (line[0] == '\0') return;
    if (hist_count > 0 && strcmp(history[(hist_idx - 1 + MAX_HISTORY) % MAX_HISTORY], line) == 0)
        return;
    strncpy(history[hist_idx], line, MAX_LINE - 1);
    hist_idx = (hist_idx + 1) % MAX_HISTORY;
    if (hist_count < MAX_HISTORY) hist_count++;
}

static const char *hist_get(int offset) {
    if (hist_count == 0) return NULL;
    int idx = (hist_idx - 1 - offset + MAX_HISTORY * 2) % MAX_HISTORY;
    return history[idx];
}

/* ── Readline (with basic line editing) ───────────────────────────────────── */

static int pyre_readline(char *buf, int maxlen) {
    struct termios old, raw;
    tcgetattr(STDIN_FILENO, &old);
    raw = old;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    int pos = 0, len = 0;
    int hist_offset = -1;
    buf[0] = '\0';

    while (1) {
        char c;
        int n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) break;

        if (c == '\n' || c == '\r') {
            write(STDOUT_FILENO, "\n", 1);
            buf[len] = '\0';
            break;
        } else if (c == 3) {  /* Ctrl-C */
            write(STDOUT_FILENO, "^C\n", 3);
            buf[0] = '\0'; len = 0; pos = 0;
            break;
        } else if (c == 4 && len == 0) {  /* Ctrl-D on empty line = exit */
            tcsetattr(STDIN_FILENO, TCSANOW, &old);
            write(STDOUT_FILENO, "\n", 1);
            return -1;
        } else if (c == '\b' || c == 127) {  /* backspace */
            if (pos > 0) {
                memmove(buf + pos - 1, buf + pos, len - pos);
                pos--; len--;
                buf[len] = '\0';
                write(STDOUT_FILENO, "\b \b", 3);
            }
        } else if (c == 1) {  /* Ctrl-A = home */
            while (pos > 0) { write(STDOUT_FILENO, "\b", 1); pos--; }
        } else if (c == 5) {  /* Ctrl-E = end */
            while (pos < len) {
                write(STDOUT_FILENO, &buf[pos], 1); pos++;
            }
        } else if (c == 27) {  /* escape sequence */
            char seq[3];
            if (read(STDIN_FILENO, &seq[0], 1) <= 0) continue;
            if (read(STDIN_FILENO, &seq[1], 1) <= 0) continue;
            if (seq[0] == '[') {
                if (seq[1] == 'A') {  /* up arrow = history back */
                    hist_offset++;
                    const char *h = hist_get(hist_offset);
                    if (!h) { hist_offset--; continue; }
                    while (pos > 0) { write(STDOUT_FILENO, "\b \b", 3); pos--; }
                    strncpy(buf, h, maxlen - 1);
                    len = pos = strlen(buf);
                    write(STDOUT_FILENO, buf, len);
                } else if (seq[1] == 'B') {  /* down arrow = history forward */
                    hist_offset--;
                    if (hist_offset < 0) {
                        hist_offset = -1;
                        while (pos > 0) { write(STDOUT_FILENO, "\b \b", 3); pos--; }
                        buf[0] = '\0'; len = 0;
                        continue;
                    }
                    const char *h = hist_get(hist_offset);
                    if (!h) continue;
                    while (pos > 0) { write(STDOUT_FILENO, "\b \b", 3); pos--; }
                    strncpy(buf, h, maxlen - 1);
                    len = pos = strlen(buf);
                    write(STDOUT_FILENO, buf, len);
                } else if (seq[1] == 'C') {  /* right */
                    if (pos < len) { write(STDOUT_FILENO, &buf[pos], 1); pos++; }
                } else if (seq[1] == 'D') {  /* left */
                    if (pos > 0) { write(STDOUT_FILENO, "\b", 1); pos--; }
                }
            }
        } else if (c >= 0x20 && c < 0x7f) {
            if (len < maxlen - 1) {
                memmove(buf + pos + 1, buf + pos, len - pos);
                buf[pos] = c;
                pos++; len++;
                buf[len] = '\0';
                write(STDOUT_FILENO, &c, 1);
            }
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &old);
    return len;
}

/* ── Tokenizer (handles single and double quotes) ─────────────────────────── */

static int tokenize(char *line, char **argv, int maxargs) {
    int argc = 0;
    char *p = line;

    while (*p && argc < maxargs - 1) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        char *start;
        if (*p == '"') {
            p++;
            start = p;
            while (*p && *p != '"') p++;
            if (*p) *p++ = '\0';
        } else if (*p == '\'') {
            p++;
            start = p;
            while (*p && *p != '\'') p++;
            if (*p) *p++ = '\0';
        } else {
            start = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '|' &&
                   *p != '>' && *p != '<') p++;
            if (*p == ' ' || *p == '\t') *p++ = '\0';
            else if (*p) break;  /* special char — stop tokenizing */
        }
        argv[argc++] = start;
    }
    argv[argc] = NULL;
    return argc;
}

/* ── PATH search ──────────────────────────────────────────────────────────── */

static int find_executable(const char *name, char *out, int outlen) {
    if (name[0] == '/' || name[0] == '.') {
        strncpy(out, name, outlen - 1);
        return access(out, X_OK) == 0 ? 0 : -1;
    }
    const char *path_env = getenv("PATH");
    if (!path_env) path_env = "/bin:/usr/bin:/sbin";

    char path_copy[4096];
    strncpy(path_copy, path_env, sizeof(path_copy) - 1);

    char *dir = strtok(path_copy, ":");
    while (dir) {
        snprintf(out, outlen, "%s/%s", dir, name);
        if (access(out, X_OK) == 0) return 0;
        dir = strtok(NULL, ":");
    }
    return -1;
}

/* ── Built-in: cd ─────────────────────────────────────────────────────────── */

static int builtin_cd(char **argv) {
    const char *target = argv[1] ? argv[1] : getenv("HOME");
    if (!target) target = "/";
    if (chdir(target) != 0) {
        fprintf(stderr, "pyre: cd: %s: %s\n", target, strerror(errno));
        return 1;
    }
    getcwd(cwd, sizeof(cwd));
    return 0;
}

static int builtin_pwd(char **argv) {
    (void)argv;
    puts(cwd);
    return 0;
}

static int builtin_exit(char **argv) {
    int code = argv[1] ? atoi(argv[1]) : 0;
    exit(code);
}

static int builtin_echo(char **argv) {
    for (int i = 1; argv[i]; i++) {
        if (i > 1) putchar(' ');
        fputs(argv[i], stdout);
    }
    putchar('\n');
    return 0;
}

static int builtin_help(char **argv) {
    (void)argv;
    puts("PyreShell built-in commands:");
    puts("  cd [dir]     Change directory");
    puts("  pwd          Print working directory");
    puts("  echo [args]  Print arguments");
    puts("  env          Show environment variables");
    puts("  set VAR=VAL  Set environment variable");
    puts("  history      Show command history");
    puts("  exit [n]     Exit shell with code n");
    puts("  help         Show this help");
    return 0;
}

static int builtin_env(char **argv) {
    (void)argv;
    extern char **environ;
    for (char **e = environ; *e; e++) puts(*e);
    return 0;
}

static int builtin_set(char **argv) {
    if (!argv[1]) return builtin_env(argv);
    char *eq = strchr(argv[1], '=');
    if (!eq) { fprintf(stderr, "pyre: set: usage: set VAR=VALUE\n"); return 1; }
    return putenv(argv[1]);
}

static int builtin_history(char **argv) {
    (void)argv;
    int start = hist_idx - hist_count;
    for (int i = 0; i < hist_count; i++) {
        int idx = (start + i + MAX_HISTORY * 2) % MAX_HISTORY;
        printf("%4d  %s\n", i + 1, history[idx]);
    }
    return 0;
}

/* ── Redirection and pipeline parser ─────────────────────────────────────── */

typedef struct {
    char  *argv[MAX_ARGS];
    int    argc;
    char  *redir_in;
    char  *redir_out;
    int    redir_append;
} cmd_t;

static int parse_pipeline(char *line, cmd_t *cmds, int max_cmds) {
    int ncmds = 0;
    char *seg = line;
    char *pipe_pos;

    while ((pipe_pos = strchr(seg, '|')) != NULL) {
        *pipe_pos = '\0';
        if (ncmds >= max_cmds - 1) break;
        cmd_t *c = &cmds[ncmds++];
        memset(c, 0, sizeof(*c));
        /* Look for redirections within segment */
        char *out_redir = strchr(seg, '>');
        char *in_redir  = strchr(seg, '<');
        if (out_redir) {
            *out_redir++ = '\0';
            c->redir_append = (*out_redir == '>');
            if (c->redir_append) out_redir++;
            while (*out_redir == ' ') out_redir++;
            c->redir_out = out_redir;
            char *end = out_redir;
            while (*end && *end != ' ') end++;
            *end = '\0';
        }
        if (in_redir) {
            *in_redir++ = '\0';
            while (*in_redir == ' ') in_redir++;
            c->redir_in = in_redir;
            char *end = in_redir;
            while (*end && *end != ' ') end++;
            *end = '\0';
        }
        c->argc = tokenize(seg, c->argv, MAX_ARGS);
        seg = pipe_pos + 1;
    }
    /* Last command */
    if (ncmds < max_cmds) {
        cmd_t *c = &cmds[ncmds++];
        memset(c, 0, sizeof(*c));
        char *out_redir = strchr(seg, '>');
        char *in_redir  = strchr(seg, '<');
        if (out_redir) {
            *out_redir++ = '\0';
            c->redir_append = (*out_redir == '>');
            if (c->redir_append) out_redir++;
            while (*out_redir == ' ') out_redir++;
            c->redir_out = out_redir;
        }
        if (in_redir) {
            *in_redir++ = '\0';
            while (*in_redir == ' ') in_redir++;
            c->redir_in = in_redir;
        }
        c->argc = tokenize(seg, c->argv, MAX_ARGS);
        if (c->argc == 0) ncmds--;
    }
    return ncmds;
}

/* ── Execute a pipeline ───────────────────────────────────────────────────── */

static int execute_pipeline(cmd_t *cmds, int ncmds) {
    if (ncmds == 0) return 0;

    int prev_fd = -1;
    pid_t pids[64];
    int pipes_fd[64][2];

    for (int i = 0; i < ncmds; i++) {
        cmd_t *c = &cmds[i];
        if (c->argc == 0) continue;

        /* Create pipe to next command */
        if (i < ncmds - 1)
            pipe(pipes_fd[i]);

        pid_t pid = fork();
        if (pid == 0) {
            /* Child */
            if (prev_fd != -1) { dup2(prev_fd, STDIN_FILENO); close(prev_fd); }
            if (i < ncmds - 1) {
                close(pipes_fd[i][0]);
                dup2(pipes_fd[i][1], STDOUT_FILENO);
                close(pipes_fd[i][1]);
            }
            /* Redirections */
            if (c->redir_in) {
                int fd = open(c->redir_in, O_RDONLY);
                if (fd < 0) { perror(c->redir_in); _exit(1); }
                dup2(fd, STDIN_FILENO); close(fd);
            }
            if (c->redir_out) {
                int flags = O_WRONLY | O_CREAT | (c->redir_append ? O_APPEND : O_TRUNC);
                int fd = open(c->redir_out, flags, 0644);
                if (fd < 0) { perror(c->redir_out); _exit(1); }
                dup2(fd, STDOUT_FILENO); close(fd);
            }

            char exec_path[4096];
            if (find_executable(c->argv[0], exec_path, sizeof(exec_path)) != 0) {
                fprintf(stderr, "pyre: %s: command not found\n", c->argv[0]);
                _exit(127);
            }
            execv(exec_path, c->argv);
            perror(exec_path);
            _exit(1);
        }
        pids[i] = pid;
        if (prev_fd != -1) close(prev_fd);
        if (i < ncmds - 1) {
            close(pipes_fd[i][1]);
            prev_fd = pipes_fd[i][0];
        }
    }

    int last_status = 0;
    for (int i = 0; i < ncmds; i++) {
        int status;
        waitpid(pids[i], &status, 0);
        if (i == ncmds - 1)
            last_status = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }
    return last_status;
}

/* ── Execute a command line ───────────────────────────────────────────────── */

static int execute_line(char *line) {
    /* Trim leading/trailing whitespace */
    while (*line == ' ' || *line == '\t') line++;
    int len = strlen(line);
    while (len > 0 && (line[len-1] == ' ' || line[len-1] == '\t')) line[--len] = '\0';
    if (len == 0) return 0;
    if (line[0] == '#') return 0;

    hist_push(line);

    /* Parse pipeline */
    cmd_t cmds[16];
    int ncmds = parse_pipeline(line, cmds, 16);
    if (ncmds == 0 || cmds[0].argc == 0) return 0;

    /* Check built-ins (only for single commands) */
    if (ncmds == 1) {
        char *cmd = cmds[0].argv[0];
        if (strcmp(cmd, "cd")      == 0) return builtin_cd(cmds[0].argv);
        if (strcmp(cmd, "pwd")     == 0) return builtin_pwd(cmds[0].argv);
        if (strcmp(cmd, "exit")    == 0) return builtin_exit(cmds[0].argv);
        if (strcmp(cmd, "echo")    == 0) return builtin_echo(cmds[0].argv);
        if (strcmp(cmd, "help")    == 0) return builtin_help(cmds[0].argv);
        if (strcmp(cmd, "env")     == 0) return builtin_env(cmds[0].argv);
        if (strcmp(cmd, "set")     == 0) return builtin_set(cmds[0].argv);
        if (strcmp(cmd, "history") == 0) return builtin_history(cmds[0].argv);
    }

    return execute_pipeline(cmds, ncmds);
}

/* ── Source a startup file ────────────────────────────────────────────────── */

static void source_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        int l = strlen(line);
        if (l > 0 && line[l-1] == '\n') line[l-1] = '\0';
        execute_line(line);
    }
    fclose(f);
}

/* ── Main REPL ────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    /* Set up environment defaults */
    if (!getenv("PATH")) putenv("PATH=/bin:/usr/bin:/sbin:/usr/local/bin");
    if (!getenv("HOME")) putenv("HOME=/root");
    if (!getenv("SHELL")) putenv("SHELL=/bin/pyre");
    if (!getenv("TERM"))  putenv("TERM=ansi");
    setenv("SHELL_NAME", "pyre", 1);

    getcwd(cwd, sizeof(cwd));
    gethostname(hostname, sizeof(hostname));

    /* Source startup files */
    source_file("/etc/pyre/pyrerc");
    const char *home = getenv("HOME");
    if (home) {
        char rc[512];
        snprintf(rc, sizeof(rc), "%s/.pyrerc", home);
        source_file(rc);
    }

    /* Non-interactive: run a script file */
    if (argc > 1) {
        FILE *f = fopen(argv[1], "r");
        if (!f) { fprintf(stderr, "pyre: cannot open %s\n", argv[1]); return 1; }
        char line[MAX_LINE];
        int ret = 0;
        while (fgets(line, sizeof(line), f)) {
            int l = strlen(line);
            if (l > 0 && line[l-1] == '\n') line[l-1] = '\0';
            ret = execute_line(line);
        }
        fclose(f);
        return ret;
    }

    /* Interactive banner */
    printf("\n  PyreShell (pyre) — PhoenixOS  [type 'help' for commands]\n\n");

    signal(SIGINT,  SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    char line[MAX_LINE];
    int last_status = 0;

    while (1) {
        /* Prompt */
        const char *user = getenv("USER");
        if (!user) user = "user";
        printf("%s@%s:%s %s ", user, hostname, cwd, last_status ? "✗" : "pyre$");
        fflush(stdout);

        int n = pyre_readline(line, sizeof(line));
        if (n < 0) break;  /* Ctrl-D */
        last_status = execute_line(line);
    }

    printf("\nLogout.\n");
    return 0;
}
