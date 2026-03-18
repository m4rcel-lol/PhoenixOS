#ifndef KERNEL_ACPI_H
#define KERNEL_ACPI_H

#include "kernel.h"

/* ── ACPI RSDP (Root System Description Pointer) ─────────────────────────── */

struct acpi_rsdp {
    char  signature[8];   /* "RSD PTR " */
    u8    checksum;
    char  oem_id[6];
    u8    revision;       /* 0 = ACPI 1.0, 2 = ACPI 2.0+ */
    u32   rsdt_addr;
    /* ACPI 2.0+ fields */
    u32   length;
    u64   xsdt_addr;
    u8    ext_checksum;
    u8    reserved[3];
} __packed;

/* ── ACPI SDT header ──────────────────────────────────────────────────────── */

struct acpi_sdt_header {
    char  signature[4];
    u32   length;
    u8    revision;
    u8    checksum;
    char  oem_id[6];
    char  oem_table_id[8];
    u32   oem_revision;
    u32   creator_id;
    u32   creator_revision;
} __packed;

/* ── FADT (Fixed ACPI Description Table) ─────────────────────────────────── */

struct acpi_fadt {
    struct acpi_sdt_header header;
    u32   firmware_ctrl;
    u32   dsdt;
    u8    reserved0;
    u8    preferred_pm_profile;
    u16   sci_interrupt;
    u32   smi_cmd;
    u8    acpi_enable;
    u8    acpi_disable;
    u8    s4bios_req;
    u8    pstate_cnt;
    u32   pm1a_event_blk;
    u32   pm1b_event_blk;
    u32   pm1a_ctrl_blk;
    u32   pm1b_ctrl_blk;
    u32   pm2_ctrl_blk;
    u32   pm_timer_blk;
    u32   gpe0_blk;
    u32   gpe1_blk;
    u8    pm1_evt_len;
    u8    pm1_ctrl_len;
    u8    pm2_ctrl_len;
    u8    pm_timer_len;
    u8    gpe0_blk_len;
    u8    gpe1_blk_len;
    u8    gpe1_base;
    u8    cst_cnt;
    u16   c2_latency;
    u16   c3_latency;
    u16   flush_size;
    u16   flush_stride;
    u8    duty_offset;
    u8    duty_width;
    u8    day_alarm;
    u8    mon_alarm;
    u8    century;
    u16   iapc_boot_arch;
    u8    reserved1;
    u32   flags;
} __packed;

/* ── Public API ───────────────────────────────────────────────────────────── */

void acpi_init(void);
void acpi_shutdown(void);
void acpi_reboot(void);
bool acpi_available(void);

#endif /* KERNEL_ACPI_H */
