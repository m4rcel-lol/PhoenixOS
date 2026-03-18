#include "../../include/kernel.h"
#include "../../include/acpi.h"
#include "../../arch/x86_64/include/asm.h"

/* ── ACPI state ───────────────────────────────────────────────────────────── */

static bool    acpi_enabled   = false;
static u16     pm1a_ctrl_port = 0;
static u16     pm1b_ctrl_port = 0;
static u16     slp_typa       = 0;
static u16     slp_typb       = 0;

/* ── RSDP scan range ──────────────────────────────────────────────────────── */

#define EBDA_BASE    0x0009FC00ULL
#define BIOS_BASE    0x000E0000ULL
#define BIOS_END     0x000FFFFFULL

static bool acpi_checksum(const u8 *ptr, u32 len) {
    u8 sum = 0;
    for (u32 i = 0; i < len; i++) sum += ptr[i];
    return sum == 0;
}

static struct acpi_rsdp *find_rsdp(u64 base, u64 end) {
    for (u64 addr = base; addr < end; addr += 16) {
        const char *p = (const char *)PHYS_TO_VIRT(addr);
        if (p[0] == 'R' && p[1] == 'S' && p[2] == 'D' && p[3] == ' ' &&
            p[4] == 'P' && p[5] == 'T' && p[6] == 'R' && p[7] == ' ') {
            struct acpi_rsdp *rsdp = (struct acpi_rsdp *)p;
            if (acpi_checksum((const u8 *)rsdp, 20))
                return rsdp;
        }
    }
    return NULL;
}

/* ── RSDT walk ────────────────────────────────────────────────────────────── */

static struct acpi_sdt_header *find_table(u32 rsdt_phys, const char sig[4]) {
    struct acpi_sdt_header *rsdt =
        (struct acpi_sdt_header *)PHYS_TO_VIRT((u64)rsdt_phys);
    if (!acpi_checksum((const u8 *)rsdt, rsdt->length)) return NULL;

    u32 entries = (rsdt->length - sizeof(*rsdt)) / 4;
    u32 *ptrs   = (u32 *)((u8 *)rsdt + sizeof(*rsdt));

    for (u32 i = 0; i < entries; i++) {
        struct acpi_sdt_header *hdr =
            (struct acpi_sdt_header *)PHYS_TO_VIRT((u64)ptrs[i]);
        if (hdr->signature[0] == sig[0] && hdr->signature[1] == sig[1] &&
            hdr->signature[2] == sig[2] && hdr->signature[3] == sig[3]) {
            if (acpi_checksum((const u8 *)hdr, hdr->length))
                return hdr;
        }
    }
    return NULL;
}

/* ── acpi_init ────────────────────────────────────────────────────────────── */

void acpi_init(void) {
    /* Search for RSDP in EBDA and BIOS area */
    struct acpi_rsdp *rsdp = find_rsdp(EBDA_BASE, EBDA_BASE + 1024);
    if (!rsdp)
        rsdp = find_rsdp(BIOS_BASE, BIOS_END);

    if (!rsdp) {
        printk("[acpi] RSDP not found — ACPI unavailable\n");
        return;
    }

    printk("[acpi] RSDP found (OEM: %.6s, rev %d)\n",
           rsdp->oem_id, rsdp->revision);

    /* Locate FADT */
    struct acpi_sdt_header *fadt_hdr =
        find_table(rsdp->rsdt_addr, "FACP");
    if (!fadt_hdr) {
        printk("[acpi] FADT not found\n");
        return;
    }

    struct acpi_fadt *fadt = (struct acpi_fadt *)fadt_hdr;

    pm1a_ctrl_port = (u16)fadt->pm1a_ctrl_blk;
    pm1b_ctrl_port = (u16)fadt->pm1b_ctrl_blk;

    /* S5 sleep type (power-off) encoded in DSDT — use common defaults */
    slp_typa = 0x1 << 10;   /* SLP_TYP for S5 on most BIOSes = 0b111 << 10 */
    slp_typb = 0;

    acpi_enabled = true;
    printk("[acpi] Initialized (PM1a=0x%x PM1b=0x%x)\n",
           pm1a_ctrl_port, pm1b_ctrl_port);
}

bool acpi_available(void) { return acpi_enabled; }

/* ── acpi_shutdown ────────────────────────────────────────────────────────── */

void acpi_shutdown(void) {
    printk("[acpi] System shutdown...\n");

    if (acpi_enabled && pm1a_ctrl_port) {
        /* SLP_EN (bit 13) | SLP_TYP for S5 */
        outw(pm1a_ctrl_port, slp_typa | (1 << 13));
        if (pm1b_ctrl_port)
            outw(pm1b_ctrl_port, slp_typb | (1 << 13));
    }

    /* QEMU-compatible shutdown via port 0x604 (ISA/PIIX) */
    outw(0x604, 0x2000);

    /* APM fallback */
    outb(0xB2, 0x00);

    /* If we get here, just halt */
    for (;;) __asm__ volatile("cli; hlt");
}

/* ── acpi_reboot ─────────────────────────────────────────────────────────── */

void acpi_reboot(void) {
    printk("[acpi] System reboot...\n");

    /* PS/2 keyboard controller reset line — most reliable on x86 */
    u8 val;
    /* Wait until input buffer is empty */
    do { val = inb(0x64); } while (val & 0x02);
    outb(0x64, 0xFE);   /* pulse reset line */

    /* If that didn't work, triple-fault via ACPI reset register */
    if (acpi_enabled) {
        /* Attempt ACPI reset (many modern systems support it) */
        outb(0xCF9, 0x06);
    }

    for (;;) __asm__ volatile("cli; hlt");
}
