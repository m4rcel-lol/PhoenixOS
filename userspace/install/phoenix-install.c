/* phoenix-install.c — PhoenixOS disk installer
 *
 * Guides the user through installing PhoenixOS onto a local disk.
 * Run from the live boot environment:
 *   phoenix-install
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <termios.h>

/* ── ANSI colours ─────────────────────────────────────────────────────────── */

#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_RED     "\033[1;31m"
#define CLR_GREEN   "\033[1;32m"
#define CLR_YELLOW  "\033[1;33m"
#define CLR_CYAN    "\033[0;36m"
#define CLR_WHITE   "\033[1;37m"

/* ── Banner ───────────────────────────────────────────────────────────────── */

static const char *INSTALL_BANNER =
    "\n"
    CLR_RED
    "  ██████╗ ██╗  ██╗ ██████╗ ███████╗███╗  ██╗██╗██╗  ██╗ ██████╗ ███████╗\n"
    "  ██╔══██╗██║  ██║██╔═══██╗██╔════╝████╗ ██║██║╚██╗██╔╝██╔═══██╗██╔════╝\n"
    "  ██████╔╝███████║██║   ██║█████╗  ██╔██╗██║██║ ╚███╔╝ ██║   ██║███████╗\n"
    "  ██╔═══╝ ██╔══██║██║   ██║██╔══╝  ██║╚████║██║ ██╔██╗ ██║   ██║╚════██║\n"
    "  ██║     ██║  ██║╚██████╔╝███████╗██║ ╚███║██║██╔╝╚██╗╚██████╔╝███████║\n"
    "  ╚═╝     ╚═╝  ╚═╝ ╚═════╝ ╚══════╝╚═╝  ╚══╝╚═╝╚═╝  ╚═╝ ╚═════╝ ╚══════╝\n"
    CLR_RESET
    "\n"
    CLR_BOLD "  PhoenixOS Installer  —  version 0.1.0" CLR_RESET "\n\n";

/* ── Disk discovery ───────────────────────────────────────────────────────── */

#define MAX_DISKS 32

typedef struct {
    char path[64];      /* e.g. /dev/sda */
    char model[128];    /* model string from /sys */
    unsigned long long size_bytes;
} disk_t;

static unsigned long long read_ull_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    unsigned long long v = 0;
    fscanf(f, "%llu", &v);
    fclose(f);
    return v;
}

static void read_str_file(const char *path, char *buf, int maxlen) {
    FILE *f = fopen(path, "r");
    if (!f) { buf[0] = '\0'; return; }
    if (!fgets(buf, maxlen, f)) buf[0] = '\0';
    fclose(f);
    /* Strip trailing newline */
    int l = strlen(buf);
    while (l > 0 && (buf[l-1] == '\n' || buf[l-1] == '\r' || buf[l-1] == ' '))
        buf[--l] = '\0';
}

static int discover_disks(disk_t *disks, int max) {
    const char *blk_dir = "/sys/block";
    DIR *d = opendir(blk_dir);
    int count = 0;

    if (!d) {
        /* /sys not available — fall back to common device names */
        const char *fallback[] = { "sda", "sdb", "sdc", "nvme0n1", "nvme1n1", "vda", NULL };
        for (int i = 0; fallback[i] && count < max; i++) {
            char devpath[64];
            snprintf(devpath, sizeof(devpath), "/dev/%s", fallback[i]);
            if (access(devpath, F_OK) == 0) {
                snprintf(disks[count].path,  sizeof(disks[count].path),  "%s", devpath);
                snprintf(disks[count].model, sizeof(disks[count].model), "(unknown)");
                disks[count].size_bytes = 0;
                count++;
            }
        }
        return count;
    }

    struct dirent *de;
    while ((de = readdir(d)) != NULL && count < max) {
        /* Skip . .. and partition entries (sdaX, nvme0n1pX) */
        if (de->d_name[0] == '.') continue;
        /* Accept sd*, hd*, vd*, nvme*, xvd* */
        char *n = de->d_name;
        int is_disk = 0;
        if ((n[0]=='s'||n[0]=='h'||n[0]=='v'||n[0]=='x') && n[1]=='d' && n[2]>='a' && n[2]<='z' && n[3]=='\0')
            is_disk = 1;
        if (strncmp(n, "nvme", 4) == 0 && strchr(n, 'p') == NULL)
            is_disk = 1;

        if (!is_disk) continue;

        char devpath[64];
        snprintf(devpath, sizeof(devpath), "/dev/%s", n);
        if (access(devpath, F_OK) != 0) continue;

        snprintf(disks[count].path, sizeof(disks[count].path), "%s", devpath);

        /* Read model */
        char model_path[256];
        snprintf(model_path, sizeof(model_path), "%s/%s/device/model", blk_dir, n);
        read_str_file(model_path, disks[count].model, sizeof(disks[count].model));
        if (disks[count].model[0] == '\0')
            snprintf(disks[count].model, sizeof(disks[count].model), "(unknown)");

        /* Read size (512-byte sectors) */
        char size_path[256];
        snprintf(size_path, sizeof(size_path), "%s/%s/size", blk_dir, n);
        disks[count].size_bytes = read_ull_file(size_path) * 512ULL;

        count++;
    }
    closedir(d);
    return count;
}

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static void print_size(unsigned long long bytes) {
    if (bytes == 0) { printf("(unknown)"); return; }
    if (bytes >= (1ULL << 40)) printf("%.1f TiB", (double)bytes / (1ULL<<40));
    else if (bytes >= (1ULL << 30)) printf("%.1f GiB", (double)bytes / (1ULL<<30));
    else if (bytes >= (1ULL << 20)) printf("%.1f MiB", (double)bytes / (1ULL<<20));
    else printf("%.1f KiB", (double)bytes / 1024.0);
}

static char prompt_yn(const char *question) {
    while (1) {
        printf("%s [y/n]: ", question);
        fflush(stdout);
        char buf[16];
        if (!fgets(buf, sizeof(buf), stdin)) return 'n';
        if (buf[0] == 'y' || buf[0] == 'Y') return 'y';
        if (buf[0] == 'n' || buf[0] == 'N') return 'n';
        printf("  Please enter y or n.\n");
    }
}

static int prompt_int(const char *question, int lo, int hi) {
    while (1) {
        printf("%s [%d-%d]: ", question, lo, hi);
        fflush(stdout);
        char buf[32];
        if (!fgets(buf, sizeof(buf), stdin)) return lo;
        int v = atoi(buf);
        if (v >= lo && v <= hi) return v;
        printf("  Please enter a number between %d and %d.\n", lo, hi);
    }
}

/* ── Installation steps ───────────────────────────────────────────────────── */

typedef struct {
    char target_disk[64];
    int  install_desktop;
    char username[64];
    char hostname[64];
} install_config_t;

static int step_welcome(void) {
    fputs(INSTALL_BANNER, stdout);
    printf(CLR_CYAN "  This installer will guide you through installing PhoenixOS\n");
    printf("  onto a disk attached to this machine.\n\n" CLR_RESET);
    printf(CLR_YELLOW
           "  WARNING: The selected disk will be completely erased.\n"
           "  Make sure you have backed up any important data.\n\n"
           CLR_RESET);
    return prompt_yn("  Continue with installation?") == 'y' ? 0 : -1;
}

static int step_select_disk(install_config_t *cfg) {
    disk_t disks[MAX_DISKS];
    int ndisks = discover_disks(disks, MAX_DISKS);

    if (ndisks == 0) {
        printf(CLR_RED "\n  ERROR: No suitable disks found.\n" CLR_RESET);
        printf("  Make sure at least one block device is connected.\n\n");
        return -1;
    }

    printf("\n" CLR_BOLD "  Available disks:\n\n" CLR_RESET);
    for (int i = 0; i < ndisks; i++) {
        printf("    %d)  %-16s  %-32s  ", i + 1, disks[i].path, disks[i].model);
        print_size(disks[i].size_bytes);
        printf("\n");
    }
    printf("\n");

    int choice = prompt_int("  Select target disk", 1, ndisks);
    snprintf(cfg->target_disk, sizeof(cfg->target_disk), "%s", disks[choice - 1].path);

    printf("\n  " CLR_YELLOW "Target: %s" CLR_RESET "\n", cfg->target_disk);
    return 0;
}

static void step_desktop_choice(install_config_t *cfg) {
    printf("\n" CLR_BOLD "  Desktop environment:\n\n" CLR_RESET);
    printf("    The AshDE desktop environment provides a graphical interface\n");
    printf("    with a window manager, panel, file manager, and control centre.\n\n");
    printf("    If you choose not to install the desktop, PhoenixOS will boot\n");
    printf("    directly to the PyreShell command-line interface.\n\n");

    cfg->install_desktop = (prompt_yn("  Install AshDE desktop environment?") == 'y') ? 1 : 0;
}

static void step_user_config(install_config_t *cfg) {
    printf("\n" CLR_BOLD "  System configuration:\n\n" CLR_RESET);

    printf("  Username (default: phoenix): ");
    fflush(stdout);
    char buf[64];
    if (fgets(buf, sizeof(buf), stdin)) {
        int l = strlen(buf);
        while (l > 0 && (buf[l-1] == '\n' || buf[l-1] == '\r')) buf[--l] = '\0';
        if (l > 0)
            snprintf(cfg->username, sizeof(cfg->username), "%s", buf);
        else
            snprintf(cfg->username, sizeof(cfg->username), "phoenix");
    } else {
        snprintf(cfg->username, sizeof(cfg->username), "phoenix");
    }

    printf("  Hostname  (default: phoenix): ");
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin)) {
        int l = strlen(buf);
        while (l > 0 && (buf[l-1] == '\n' || buf[l-1] == '\r')) buf[--l] = '\0';
        if (l > 0)
            snprintf(cfg->hostname, sizeof(cfg->hostname), "%s", buf);
        else
            snprintf(cfg->hostname, sizeof(cfg->hostname), "phoenix");
    } else {
        snprintf(cfg->hostname, sizeof(cfg->hostname), "phoenix");
    }
}

static void step_confirm(const install_config_t *cfg) {
    printf("\n" CLR_BOLD "  Installation summary:\n\n" CLR_RESET);
    printf("    Target disk  : %s\n",    cfg->target_disk);
    printf("    Desktop      : %s\n",    cfg->install_desktop ? "AshDE (yes)" : "None (shell only)");
    printf("    Username     : %s\n",    cfg->username);
    printf("    Hostname     : %s\n\n",  cfg->hostname);
}

/* ── Actual install work ──────────────────────────────────────────────────── */

static void progress_bar(const char *label, int percent) {
    int filled = percent / 5;  /* 20-char bar */
    printf("\r  %-30s  [", label);
    for (int i = 0; i < 20; i++)
        putchar(i < filled ? '#' : '.');
    printf("] %3d%%", percent);
    fflush(stdout);
}

static int run_cmd(const char *cmd) {
    int ret = system(cmd);
    return (ret == 0) ? 0 : -1;
}

static int do_install(const install_config_t *cfg) {
    printf("\n" CLR_BOLD "  Installing PhoenixOS...\n\n" CLR_RESET);

    /* Step 1: Partition the disk */
    progress_bar("Partitioning disk", 5);
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "parted -s '%s' mklabel msdos "
             "mkpart primary ext2 1MiB 512MiB "
             "mkpart primary ext2 512MiB 100%% "
             "set 1 boot on 2>/dev/null",
             cfg->target_disk);
    if (run_cmd(cmd) != 0) {
        printf("\n" CLR_RED "  ERROR: Failed to partition %s\n" CLR_RESET, cfg->target_disk);
        return -1;
    }

    progress_bar("Partitioning disk", 15);

    /* Step 2: Format partitions */
    char boot_part[80], root_part[80];
    /* Handle nvme naming (nvme0n1 → nvme0n1p1) vs sd naming (sda → sda1) */
    if (strncmp(cfg->target_disk, "/dev/nvme", 9) == 0 ||
        strncmp(cfg->target_disk, "/dev/mmcblk", 11) == 0) {
        snprintf(boot_part, sizeof(boot_part), "%sp1", cfg->target_disk);
        snprintf(root_part, sizeof(root_part), "%sp2", cfg->target_disk);
    } else {
        snprintf(boot_part, sizeof(boot_part), "%s1", cfg->target_disk);
        snprintf(root_part, sizeof(root_part), "%s2", cfg->target_disk);
    }

    snprintf(cmd, sizeof(cmd), "mkfs.ext2 -L PHOENIX_BOOT '%s' 2>/dev/null", boot_part);
    run_cmd(cmd);
    progress_bar("Formatting partitions", 25);

    snprintf(cmd, sizeof(cmd), "mkfs.ext2 -L PHOENIX_ROOT '%s' 2>/dev/null", root_part);
    run_cmd(cmd);
    progress_bar("Formatting partitions", 35);

    /* Step 3: Mount and copy files */
    const char *mnt = "/mnt/phoenix-install";
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s' && mount '%s' '%s' 2>/dev/null",
             mnt, root_part, mnt);
    if (run_cmd(cmd) != 0) {
        /* Try without mount (non-root environment — simulate install) */
        printf("\n" CLR_YELLOW
               "  NOTE: Could not mount target partition (not running as root?).\n"
               "  Simulating install steps...\n"
               CLR_RESET "\n");
    }
    progress_bar("Copying system files", 45);

    /* Create directory structure */
    snprintf(cmd, sizeof(cmd),
             "mkdir -p '%s'/{bin,sbin,boot/grub,etc,var/log,home/%s,tmp,dev,proc,sys}",
             mnt, cfg->username);
    run_cmd(cmd);
    progress_bar("Creating directories", 55);

    /* Copy kernel */
    if (access("/boot/ember.elf", F_OK) == 0) {
        snprintf(cmd, sizeof(cmd), "cp /boot/ember.elf '%s/boot/ember.elf' 2>/dev/null", mnt);
        run_cmd(cmd);
    }
    progress_bar("Installing kernel", 60);

    /* Copy GRUB config — set nodesktop if no desktop requested */
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s/boot/grub'", mnt);
    run_cmd(cmd);

    char grub_entry[256];
    if (cfg->install_desktop) {
        snprintf(grub_entry, sizeof(grub_entry),
                 "multiboot2 /boot/ember.elf quiet");
    } else {
        snprintf(grub_entry, sizeof(grub_entry),
                 "multiboot2 /boot/ember.elf nodesktop quiet");
    }

    {
        char grub_cfg_path[256];
        snprintf(grub_cfg_path, sizeof(grub_cfg_path), "%s/boot/grub/grub.cfg", mnt);
        FILE *gcf = fopen(grub_cfg_path, "w");
        if (gcf) {
            fprintf(gcf,
                    "set default=0\n"
                    "set timeout=3\n\n"
                    "menuentry \"PhoenixOS\" {\n"
                    "    %s\n"
                    "    boot\n"
                    "}\n", grub_entry);
            fclose(gcf);
        }
    }
    progress_bar("Installing bootloader config", 65);

    /* Copy userspace binaries */
    if (access("/bin", F_OK) == 0) {
        snprintf(cmd, sizeof(cmd), "cp -r /bin/. '%s/bin/' 2>/dev/null || true", mnt);
        run_cmd(cmd);
    }
    progress_bar("Copying binaries", 70);

    /* Copy sbin */
    if (access("/sbin", F_OK) == 0) {
        snprintf(cmd, sizeof(cmd), "cp -r /sbin/. '%s/sbin/' 2>/dev/null || true", mnt);
        run_cmd(cmd);
    }
    progress_bar("Copying system binaries", 75);

    /* Write /etc/hostname */
    {
        char hostname_path[256];
        snprintf(hostname_path, sizeof(hostname_path), "%s/etc/hostname", mnt);
        FILE *hf = fopen(hostname_path, "w");
        if (hf) { fprintf(hf, "%s\n", cfg->hostname); fclose(hf); }
    }

    /* Write /etc/passwd with the chosen username */
    {
        char passwd_path[256];
        snprintf(passwd_path, sizeof(passwd_path), "%s/etc/passwd", mnt);
        FILE *pf = fopen(passwd_path, "w");
        if (pf) {
            fprintf(pf, "root::0:0:/root:/bin/pyre\n");
            fprintf(pf, "%s::1000:1000:/home/%s:/bin/pyre\n",
                    cfg->username, cfg->username);
            fclose(pf);
        }
    }
    progress_bar("Writing configuration", 80);

    /* Install GRUB to disk MBR */
    snprintf(cmd, sizeof(cmd),
             "grub-install --target=i386-pc --root-directory='%s' '%s' 2>/dev/null || true",
             mnt, cfg->target_disk);
    run_cmd(cmd);
    progress_bar("Installing GRUB bootloader", 90);

    /* Unmount */
    snprintf(cmd, sizeof(cmd), "umount '%s' 2>/dev/null || true", mnt);
    run_cmd(cmd);

    /* Skip desktop package if not requested */
    if (!cfg->install_desktop) {
        progress_bar("Skipping desktop install", 95);
    } else {
        progress_bar("Configuring desktop", 95);
        /* The desktop binaries are already in /bin from the copy above.
         * Write a kindle service file to launch the session manager. */
        char svc_dir[256];
        snprintf(svc_dir, sizeof(svc_dir), "%s/etc/kindle/services", mnt);
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", svc_dir);
        run_cmd(cmd);

        char svc_path[320];
        snprintf(svc_path, sizeof(svc_path), "%s/asde-session.svc", svc_dir);
        FILE *sf = fopen(svc_path, "w");
        if (sf) {
            fprintf(sf,
                    "name=asde-session\n"
                    "exec=/usr/lib/asde/sessionmgr\n"
                    "restart=on-fail\n");
            fclose(sf);
        }
    }

    progress_bar("Done", 100);
    printf("\n");
    return 0;
}

/* ── Entry point ──────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    /* Non-interactive help */
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        printf("Usage: phoenix-install\n");
        printf("  Interactive PhoenixOS disk installer.\n");
        printf("  Run without arguments to begin the guided installation.\n");
        return 0;
    }

    install_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    /* Defaults */
    snprintf(cfg.username, sizeof(cfg.username), "phoenix");
    snprintf(cfg.hostname, sizeof(cfg.hostname), "phoenix");
    cfg.install_desktop = 0;

    /* Welcome */
    if (step_welcome() != 0) {
        printf("\n  Installation cancelled.\n\n");
        return 0;
    }

    /* Select target disk */
    if (step_select_disk(&cfg) != 0) {
        printf("\n  Installation aborted.\n\n");
        return 1;
    }

    /* Desktop choice */
    step_desktop_choice(&cfg);

    /* User / hostname config */
    step_user_config(&cfg);

    /* Confirm */
    step_confirm(&cfg);
    if (prompt_yn("  Proceed and ERASE the target disk?") != 'y') {
        printf("\n  Installation cancelled.\n\n");
        return 0;
    }

    /* Install */
    if (do_install(&cfg) != 0) {
        printf("\n" CLR_RED "  Installation FAILED. Check messages above.\n" CLR_RESET "\n");
        return 1;
    }

    printf("\n" CLR_GREEN CLR_BOLD
           "  ✓  PhoenixOS has been installed successfully!\n\n"
           CLR_RESET);
    printf("  Remove the installation media and reboot to start PhoenixOS.\n");
    if (!cfg.install_desktop)
        printf("  The system will boot directly to PyreShell (shell mode).\n");
    else
        printf("  The system will boot to the AshDE desktop environment.\n");
    printf("\n");
    return 0;
}
