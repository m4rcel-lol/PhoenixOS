/* uname.c — PhoenixOS uname utility */

#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

int main(int argc, char *argv[]) {
    int opt_all      = 0;
    int opt_sysname  = 0;
    int opt_nodename = 0;
    int opt_release  = 0;
    int opt_version  = 0;
    int opt_machine  = 0;

    if (argc == 1) {
        opt_sysname = 1;
    } else {
        for (int i = 1; i < argc; i++) {
            if (argv[i][0] != '-') {
                fprintf(stderr, "uname: invalid argument '%s'\n", argv[i]);
                return 1;
            }
            for (int j = 1; argv[i][j]; j++) {
                switch (argv[i][j]) {
                case 'a': opt_all      = 1; break;
                case 's': opt_sysname  = 1; break;
                case 'n': opt_nodename = 1; break;
                case 'r': opt_release  = 1; break;
                case 'v': opt_version  = 1; break;
                case 'm': opt_machine  = 1; break;
                default:
                    fprintf(stderr, "uname: invalid option -- '%c'\n", argv[i][j]);
                    return 1;
                }
            }
        }
    }

    struct utsname u;
    if (uname(&u) != 0) {
        perror("uname");
        return 1;
    }

    int printed = 0;
#define PRINT(val) do { if (printed++) putchar(' '); fputs((val), stdout); } while(0)

    if (opt_all || opt_sysname)  PRINT(u.sysname);
    if (opt_all || opt_nodename) PRINT(u.nodename);
    if (opt_all || opt_release)  PRINT(u.release);
    if (opt_all || opt_version)  PRINT(u.version);
    if (opt_all || opt_machine)  PRINT(u.machine);

    putchar('\n');
    return 0;
}
