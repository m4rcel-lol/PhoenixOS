/* test_drivers.c — Unit tests for PhoenixOS new hardware drivers */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

/* ── Assert framework ─────────────────────────────────────────────────────── */

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond) do { \
    tests_run++; \
    if (cond) { \
        tests_passed++; \
        printf("  PASS: %s\n", #cond); \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s  (line %d)\n", #cond, __LINE__); \
    } \
} while (0)

#define TEST(name) do { printf("\n[TEST] %s\n", name); } while(0)

/* ── Kernel type shims ────────────────────────────────────────────────────── */

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;

/* ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ──── */
/* PCI config-space stub                                                      */
/* ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ──── */

/* Simulate a small PCI device table used by the tests */
typedef struct {
    u8  bus; u8 dev; u8 func;
    u16 vendor_id; u16 device_id;
    u8  class_code; u8 subclass;
    u8  irq_line;
} fake_pci_entry_t;

#define FAKE_PCI_MAX 8
static fake_pci_entry_t fake_pci[FAKE_PCI_MAX];
static int              fake_pci_count = 0;

static void fake_pci_reset(void) { fake_pci_count = 0; }

static void fake_pci_add(u8 bus, u8 dev, u16 vid, u16 did,
                         u8 cls, u8 sub, u8 irq) {
    if (fake_pci_count >= FAKE_PCI_MAX) return;
    fake_pci[fake_pci_count++] = (fake_pci_entry_t){
        bus, dev, 0, vid, did, cls, sub, irq
    };
}

/* Simplified pci_find_by_class used by the test stubs below */
static bool fake_pci_find_by_class(u8 cls, u8 sub,
                                   fake_pci_entry_t *out) {
    for (int i = 0; i < fake_pci_count; i++) {
        if (fake_pci[i].class_code == cls && fake_pci[i].subclass == sub) {
            *out = fake_pci[i];
            return true;
        }
    }
    return false;
}

/* ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ──── */
/* PS/2 Mouse packet decoder (extracted logic, testable without hardware)     */
/* ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ──── */

typedef struct {
    s8   dx;
    s8   dy;
    bool btn_left;
    bool btn_right;
    bool btn_middle;
    bool overflow;
} mouse_event_t;

/* Decode a 3-byte PS/2 mouse packet into a mouse_event_t.
 * Returns false if the first byte fails the sync check or if an
 * overflow bit is set (packet should be discarded).                         */
static bool mouse_decode_packet(const u8 pkt[3], mouse_event_t *evt) {
    /* Byte 0, bit 3 must always be 1 for a valid first byte */
    if (!(pkt[0] & 0x08))
        return false;

    u8 flags = pkt[0];
    s8 dx    = (s8)pkt[1];
    s8 dy    = (s8)pkt[2];

    /* Overflow bits */
    if ((flags & 0x40) || (flags & 0x80)) {
        evt->overflow = true;
        return false;
    }

    evt->overflow   = false;
    evt->dx         = dx;
    evt->dy         = (s8)(-dy);   /* invert y-axis for screen coords */
    evt->btn_left   = (flags & 0x01) != 0;
    evt->btn_right  = (flags & 0x02) != 0;
    evt->btn_middle = (flags & 0x04) != 0;
    return true;
}

/* ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ──── */
/* GPU vendor classification stub                                             */
/* ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ──── */

#define GPU_PCI_VENDOR_INTEL   0x8086
#define GPU_PCI_VENDOR_AMD     0x1002
#define GPU_PCI_VENDOR_NVIDIA  0x10DE
#define GPU_PCI_VENDOR_VMWARE  0x15AD
#define GPU_PCI_VENDOR_BOCHS   0x1234

typedef enum {
    GPU_VENDOR_UNKNOWN = 0,
    GPU_VENDOR_INTEL, GPU_VENDOR_AMD, GPU_VENDOR_NVIDIA,
    GPU_VENDOR_VMWARE, GPU_VENDOR_BOCHS,
} gpu_vendor_t;

static gpu_vendor_t gpu_classify_vendor(u16 vid) {
    switch (vid) {
        case GPU_PCI_VENDOR_INTEL:  return GPU_VENDOR_INTEL;
        case GPU_PCI_VENDOR_AMD:    return GPU_VENDOR_AMD;
        case GPU_PCI_VENDOR_NVIDIA: return GPU_VENDOR_NVIDIA;
        case GPU_PCI_VENDOR_VMWARE: return GPU_VENDOR_VMWARE;
        case GPU_PCI_VENDOR_BOCHS:  return GPU_VENDOR_BOCHS;
        default:                    return GPU_VENDOR_UNKNOWN;
    }
}

/* ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ──── */
/* Net state helpers                                                          */
/* ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ──── */

typedef enum {
    NET_STATE_NOT_FOUND = 0,
    NET_STATE_DISABLED,
    NET_STATE_ENABLED,
    NET_STATE_CONNECTED,
} net_state_t;

/* WiFi ID table (mirrors the one in wifi.c) */
typedef struct { u16 vendor; u16 device; } net_id_t;

static const net_id_t wifi_known[] = {
    { 0x8086, 0x08B1 }, { 0x8086, 0x095A },
    { 0x8086, 0x24F3 }, { 0x8086, 0x2526 },
    { 0x8086, 0x02F0 }, { 0x168C, 0x003E },
    { 0x168C, 0x0042 }, { 0x10EC, 0xB723 },
    { 0x10EC, 0xC821 }, { 0x14E4, 0x43A0 },
    { 0x14E4, 0x43EC },
};
#define WIFI_ID_COUNT ((int)(sizeof(wifi_known)/sizeof(wifi_known[0])))

static bool wifi_id_known(u16 vid, u16 did) {
    for (int i = 0; i < WIFI_ID_COUNT; i++)
        if (wifi_known[i].vendor == vid && wifi_known[i].device == did)
            return true;
    return false;
}

/* Bluetooth ID table (mirrors the one in bluetooth.c) */
static const net_id_t bt_known[] = {
    { 0x8086, 0x0A2A }, { 0x8086, 0x3198 },
    { 0x8086, 0x02D0 }, { 0x8086, 0x0026 },
    { 0x168C, 0x0036 }, { 0x10EC, 0xB009 },
    { 0x0A5C, 0x216C }, { 0x0A5C, 0x21E6 },
};
#define BT_ID_COUNT ((int)(sizeof(bt_known)/sizeof(bt_known[0])))

static bool bt_id_known(u16 vid, u16 did) {
    for (int i = 0; i < BT_ID_COUNT; i++)
        if (bt_known[i].vendor == vid && bt_known[i].device == did)
            return true;
    return false;
}

/* ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ──── */
/* Tests                                                                      */
/* ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ── ──── */

/* ---- Mouse: packet decoder ---- */

static void test_mouse_basic_move(void) {
    TEST("Mouse: basic movement packet");
    u8 pkt[3] = { 0x08, 5, 10 };   /* bit3 set, dx=5, dy=10 */
    mouse_event_t evt;
    ASSERT(mouse_decode_packet(pkt, &evt) == true);
    ASSERT(evt.dx == 5);
    ASSERT(evt.dy == -10);   /* y-axis inverted */
    ASSERT(evt.btn_left   == false);
    ASSERT(evt.btn_right  == false);
    ASSERT(evt.btn_middle == false);
}

static void test_mouse_buttons(void) {
    TEST("Mouse: button flags");
    /* Left + right pressed, bit 3 always set */
    u8 pkt[3] = { 0x08 | 0x01 | 0x02, 0, 0 };
    mouse_event_t evt;
    ASSERT(mouse_decode_packet(pkt, &evt) == true);
    ASSERT(evt.btn_left  == true);
    ASSERT(evt.btn_right == true);
    ASSERT(evt.btn_middle == false);
}

static void test_mouse_middle_button(void) {
    TEST("Mouse: middle button");
    u8 pkt[3] = { 0x08 | 0x04, 0, 0 };
    mouse_event_t evt;
    ASSERT(mouse_decode_packet(pkt, &evt) == true);
    ASSERT(evt.btn_middle == true);
}

static void test_mouse_bad_sync(void) {
    TEST("Mouse: reject packet with bit 3 clear");
    u8 pkt[3] = { 0x00, 5, 5 };    /* bit 3 not set — invalid sync */
    mouse_event_t evt;
    ASSERT(mouse_decode_packet(pkt, &evt) == false);
}

static void test_mouse_overflow(void) {
    TEST("Mouse: overflow bits cause packet discard");
    u8 pkt_xov[3] = { 0x08 | 0x40, 0, 0 };   /* X overflow */
    u8 pkt_yov[3] = { 0x08 | 0x80, 0, 0 };   /* Y overflow */
    mouse_event_t evt;
    ASSERT(mouse_decode_packet(pkt_xov, &evt) == false);
    ASSERT(mouse_decode_packet(pkt_yov, &evt) == false);
}

static void test_mouse_negative_delta(void) {
    TEST("Mouse: negative delta values");
    u8 pkt[3] = { 0x08, (u8)(-3), (u8)(-7) };
    mouse_event_t evt;
    ASSERT(mouse_decode_packet(pkt, &evt) == true);
    ASSERT(evt.dx == -3);
    ASSERT(evt.dy == 7);   /* -(-7) = 7 */
}

/* ---- GPU: vendor classification ---- */

static void test_gpu_vendor_intel(void) {
    TEST("GPU: Intel vendor classification");
    ASSERT(gpu_classify_vendor(0x8086) == GPU_VENDOR_INTEL);
}

static void test_gpu_vendor_amd(void) {
    TEST("GPU: AMD vendor classification");
    ASSERT(gpu_classify_vendor(0x1002) == GPU_VENDOR_AMD);
}

static void test_gpu_vendor_nvidia(void) {
    TEST("GPU: NVIDIA vendor classification");
    ASSERT(gpu_classify_vendor(0x10DE) == GPU_VENDOR_NVIDIA);
}

static void test_gpu_vendor_vmware(void) {
    TEST("GPU: VMware vendor classification");
    ASSERT(gpu_classify_vendor(0x15AD) == GPU_VENDOR_VMWARE);
}

static void test_gpu_vendor_bochs(void) {
    TEST("GPU: QEMU/Bochs vendor classification");
    ASSERT(gpu_classify_vendor(0x1234) == GPU_VENDOR_BOCHS);
}

static void test_gpu_vendor_unknown(void) {
    TEST("GPU: unknown vendor returns UNKNOWN");
    ASSERT(gpu_classify_vendor(0xBEEF) == GPU_VENDOR_UNKNOWN);
    ASSERT(gpu_classify_vendor(0x0000) == GPU_VENDOR_UNKNOWN);
}

/* ---- WiFi: PCI detection stub ---- */

static void test_wifi_known_device(void) {
    TEST("WiFi: known Intel AX200 device ID recognised");
    ASSERT(wifi_id_known(0x8086, 0x2526) == true);
}

static void test_wifi_known_realtek(void) {
    TEST("WiFi: known Realtek RTL8821CE recognised");
    ASSERT(wifi_id_known(0x10EC, 0xC821) == true);
}

static void test_wifi_unknown_device(void) {
    TEST("WiFi: unknown device ID not recognised");
    ASSERT(wifi_id_known(0xDEAD, 0xBEEF) == false);
}

static void test_wifi_pci_class_detection(void) {
    TEST("WiFi: PCI class/subclass detection (0x02/0x80)");
    fake_pci_reset();
    fake_pci_add(0, 1, 0x8086, 0x2526, 0x02, 0x80, 5);

    fake_pci_entry_t found;
    bool ok = fake_pci_find_by_class(0x02, 0x80, &found);
    ASSERT(ok == true);
    ASSERT(found.vendor_id == 0x8086);
}

static void test_wifi_not_found(void) {
    TEST("WiFi: no adapter in empty PCI table");
    fake_pci_reset();
    fake_pci_entry_t found;
    ASSERT(fake_pci_find_by_class(0x02, 0x80, &found) == false);
}

/* ---- Bluetooth: PCI detection stub ---- */

static void test_bt_known_device(void) {
    TEST("Bluetooth: known Intel AX200 BT device recognised");
    ASSERT(bt_id_known(0x8086, 0x0026) == true);
}

static void test_bt_known_broadcom(void) {
    TEST("Bluetooth: known Broadcom BCM20702 recognised");
    ASSERT(bt_id_known(0x0A5C, 0x21E6) == true);
}

static void test_bt_unknown_device(void) {
    TEST("Bluetooth: unknown device not recognised");
    ASSERT(bt_id_known(0xDEAD, 0xBEEF) == false);
}

static void test_bt_pci_class_detection(void) {
    TEST("Bluetooth: PCI class/subclass detection (0x0C/0x11)");
    fake_pci_reset();
    fake_pci_add(0, 2, 0x8086, 0x02D0, 0x0C, 0x11, 11);

    fake_pci_entry_t found;
    bool ok = fake_pci_find_by_class(0x0C, 0x11, &found);
    ASSERT(ok == true);
    ASSERT(found.vendor_id == 0x8086);
    ASSERT(found.device_id == 0x02D0);
}

static void test_bt_not_found(void) {
    TEST("Bluetooth: no adapter in empty PCI table");
    fake_pci_reset();
    fake_pci_entry_t found;
    ASSERT(fake_pci_find_by_class(0x0C, 0x11, &found) == false);
}

/* ── Main ─────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("=== PhoenixOS Driver Unit Tests ===\n");

    /* Mouse */
    test_mouse_basic_move();
    test_mouse_buttons();
    test_mouse_middle_button();
    test_mouse_bad_sync();
    test_mouse_overflow();
    test_mouse_negative_delta();

    /* GPU */
    test_gpu_vendor_intel();
    test_gpu_vendor_amd();
    test_gpu_vendor_nvidia();
    test_gpu_vendor_vmware();
    test_gpu_vendor_bochs();
    test_gpu_vendor_unknown();

    /* WiFi */
    test_wifi_known_device();
    test_wifi_known_realtek();
    test_wifi_unknown_device();
    test_wifi_pci_class_detection();
    test_wifi_not_found();

    /* Bluetooth */
    test_bt_known_device();
    test_bt_known_broadcom();
    test_bt_unknown_device();
    test_bt_pci_class_detection();
    test_bt_not_found();

    printf("\n=== Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0)
        printf(", %d FAILED", tests_failed);
    printf(" ===\n");

    return tests_failed == 0 ? 0 : 1;
}
