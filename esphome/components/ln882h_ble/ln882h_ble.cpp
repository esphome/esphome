// ln882h_ble.cpp
//
// BLE controller support for the LN882H (LibreTiny lightning-ln882h family) —
// the platform analog of esp32_ble / rp2040_ble. Owns the one-time stack
// bring-up (rw_init + the ln_* app init sequence) and the controller BLE
// address (persistent KV entry, WiFi-MAC-derived once). Consumers
// (ln882h_ble_tracker) build on this component and contain no SDK calls of
// their own.
//
// BLE stack init and scan lifecycle mirror the SDK's ble_app usage. The BLE
// stack itself is compiled and linked by the LibreTiny lightning-ln882h builder
// (CFG_SUPPORT_BLE=1 via custom_options.proj_config#h; prebuilt
// libln882h_ble_full_stack.a).

#include "ln882h_ble.h"  // pulls esphome/core/defines.h for USE_LN882H_BLE

#ifdef USE_LN882H_BLE

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"  // get_mac_address_raw()
#include "esphome/core/log.h"

// ---------------------------------------------------------------------------
// LN882H BLE SDK — forward declarations
// ---------------------------------------------------------------------------
extern "C" {

struct ln_bd_addr_v_t {  // NOLINT(readability-identifier-naming) - mirrors the SDK type name
  uint8_t addr[6];
};  // ABI-identical to ln_bd_addr_t
void ln_kv_ble_app_init(void);
struct ln_bd_addr_v_t *ln_kv_ble_pub_addr_get(void);
int ln_kv_ble_addr_store(struct ln_bd_addr_v_t addr);
void soc_module_clk_gate_enable(uint32_t clk);

void rw_init(uint8_t mac[6]);
void ln_gap_app_init(void);
void ln_gatt_app_init(void);
void ln_ble_conn_mgr_init(void);
void ln_ble_evt_mgr_init(void);
void ln_ble_smp_init(void);
void ln_ble_scan_mgr_init(void);
void ln_rw_app_task_init(void);
void ln_gap_reset(void);

void ln_ble_scan_actv_creat(void);
void ln_ble_scan_start(void *scan_param);
void ln_ble_scan_stop(void);

}  // extern "C"

// ln_bd_addr_v_t mirrors the SDK's ln_bd_addr_t (ln_ble_app_defines.h) and is
// passed to ln_kv_ble_addr_store() by value, so its size and alignment are part
// of the calling convention.
static_assert(sizeof(struct ln_bd_addr_v_t) == 6, "ln_bd_addr_v_t must match the SDK's ln_bd_addr_t layout");
static_assert(alignof(struct ln_bd_addr_v_t) == 1, "ln_bd_addr_v_t must stay byte-aligned like the SDK type");

// ---------------------------------------------------------------------------
// LN882H SDK constants
//   CLK_G_BLE              — hal/hal_clock.h clock gate bit for the BLE block
//   BLE_EVT_ID_SCAN_REPORT — ble/ble_evt.h event id for scan reports
//   GAPM_*                 — ble/mac/ble/hl/api/gapm_task.h, enums gapm_scan_type /
//                            gapm_dup_filter_pol / gapm_scan_prop
// ---------------------------------------------------------------------------
static constexpr uint32_t CLK_G_BLE = 1u << 0;

// WiFi/BLE packet-traffic-indication (PTI) arbitration register. The LN882H SDK
// exposes no symbolic name for this register; the address and value replicate
// the SDK reference bring-up. 0x003F sets all six PTI priority bits so the
// arbiter can pre-empt WiFi for BLE traffic.
static constexpr uint32_t BLE_COEX_PTI_REG_ADDR = 0x400121F8;
static constexpr uint32_t BLE_COEX_PTI_ENABLE_ALL = 0x003F;

// ble_app_default_cfg.h BLE_DEFAULT_PUBLIC_ADDR, in the SDK's ln_bd_addr_t
// array order — least-significant octet first, the BLE/HCI convention (the
// SDK's own AT commands print addr[5]..addr[0]). Printable form:
// 00:FF:03:12:34:56.
static constexpr uint8_t BLE_DEFAULT_ADDR[6] = {0x56, 0x34, 0x12, 0x03, 0xFF, 0x00};

// The SDK's KV loader treats an all-zero address as unset and substitutes the
// default; resolve_mac_() applies the same rule to a stored entry.
static bool is_unset_addr(const uint8_t (&addr)[6]) {
  return std::all_of(std::begin(addr), std::end(addr), [](uint8_t b) { return b == 0; });
}

// gapm_scan_type: GEN_DISC = 0, LIM_DISC = 1, OBSERVER = 2. Observer reports every
// advertisement without filtering — what a tracker wants.
static constexpr uint8_t GAPM_SCAN_TYPE_OBSERVER = 2;
static constexpr uint8_t GAPM_DUP_FILT_DIS = 0;
// gapm_scan_prop bits: PHY_1M = 1<<0, PHY_CODED = 1<<1, ACTIVE_1M = 1<<2, ACTIVE_CODED = 1<<3.
static constexpr uint8_t GAPM_SCAN_PROP_PHY_1M_BIT = 1 << 0;

// Scan parameter block passed to ln_ble_scan_start(); mirrors the SDK layout,
// with the pad byte explicit so the whole block zero-initialises.
struct le_scan_parameters_t {  // NOLINT(readability-identifier-naming) - mirrors the SDK type name
  uint8_t type;
  uint8_t prop;
  uint8_t dup_filt_pol;
  uint8_t pad;
  uint16_t scan_intv;
  uint16_t scan_wd;
};
// Pin the compiler's layout decisions for the hand-mirrored SDK struct: it is
// passed to ln_ble_scan_start() as void*, so a padding drift would silently
// feed garbage scan parameters to the controller.
static_assert(sizeof(le_scan_parameters_t) == 8, "le_scan_parameters_t must match the SDK layout");
static_assert(offsetof(le_scan_parameters_t, scan_intv) == 4, "unexpected padding in le_scan_parameters_t");

// ---------------------------------------------------------------------------
// __sprintf weak stub
//
// The LN882H BLE SDK objects reference __sprintf (a Beken/LN libc alias) that
// LibreTiny's newlib does not provide. Supply a weak fallback so linking
// succeeds; a real definition, if one is ever provided, takes precedence.
// ---------------------------------------------------------------------------
#include <cstdarg>
#include <cstdio>
extern "C" __attribute__((weak)) int
__sprintf(  // NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
    char *str, const char *format, ...) {
  va_list args;
  va_start(args, format);
  int ret = vsprintf(str, format, args);  // NOLINT
  va_end(args);
  return ret;
}

namespace esphome::ln882h_ble {

static const char *const TAG = "ln882h_ble";

// ---------------------------------------------------------------------------
// Component lifecycle
// ---------------------------------------------------------------------------

void LN882HBLE::setup() {
  // Resolve the MAC early so get_mac_lsb_first() is valid for consumers before
  // the stack is up. The KV load also happens here (no stack dependency).
  this->resolve_mac_();
  if (this->enable_on_boot_) {
    this->enable();
  }
}

// AFTER_WIFI, not BLUETOOTH: replicates the proven pre-split timing — the LN
// SDK's KV subsystem is first touched only once WiFi is up; earlier access
// destabilized the device in hardware testing.
float LN882HBLE::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

void LN882HBLE::enable() {
  if (this->state_ != BLEComponentState::STATE_OFF)
    return;
  this->state_ = BLEComponentState::ENABLING;

  *reinterpret_cast<volatile uint32_t *>(BLE_COEX_PTI_REG_ADDR) = BLE_COEX_PTI_ENABLE_ALL;
  soc_module_clk_gate_enable(CLK_G_BLE);

  rw_init(this->ble_mac_);
  ln_gap_app_init();
  ln_gatt_app_init();
  ln_ble_conn_mgr_init();
  ln_ble_evt_mgr_init();
  ln_ble_smp_init();
  ln_ble_scan_mgr_init();
  ln_rw_app_task_init();
  ln_gap_reset();

  delay(100);  // NOLINT — one-time BLE stack init; SDK requires this settle time

  ln_ble_scan_actv_creat();
  delay(10);

  // Prime the scan activity with a short probe start/stop — the SDK's scan
  // manager completes activity creation on the first start.
  // static: its address is handed to ln_ble_scan_start(void *), which may
  // retain it past this call.
  static le_scan_parameters_t probe_p{};
  probe_p.type = GAPM_SCAN_TYPE_OBSERVER;
  probe_p.prop = GAPM_SCAN_PROP_PHY_1M_BIT;
  probe_p.dup_filt_pol = GAPM_DUP_FILT_DIS;
  probe_p.scan_intv = 160;
  probe_p.scan_wd = 16;
  ln_ble_scan_start(&probe_p);
  delay(10);
  ln_ble_scan_stop();

  this->state_ = BLEComponentState::ACTIVE;
  ESP_LOGD(TAG, "BLE stack initialised");
}

void LN882HBLE::get_mac_lsb_first(uint8_t out[6]) const { memcpy(out, this->ble_mac_, sizeof(this->ble_mac_)); }

void LN882HBLE::dump_config() {
  ESP_LOGCONFIG(TAG,
                "LN882H BLE:\n"
                "  MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n"
                "  Active: %s",
                this->ble_mac_[5], this->ble_mac_[4], this->ble_mac_[3], this->ble_mac_[2], this->ble_mac_[1],
                this->ble_mac_[0], YESNO(this->is_active()));
}

// ---------------------------------------------------------------------------
// MAC resolution
// ---------------------------------------------------------------------------

void LN882HBLE::resolve_mac_() {
  ln_kv_ble_app_init();

  // ln_kv_ble_app_init() loads the persistent address from the "2_ble_addr" KV
  // entry, falling back to BLE_DEFAULT_ADDR when nothing is stored. A stored
  // address is preferred so boot does not write flash. Order is LSB-first
  // throughout (BLE/HCI convention); consumers reverse for printable form.
  ln_bd_addr_v_t bt_addr{};
  bool have_unique_addr = false;
  if (const ln_bd_addr_v_t *stored = ln_kv_ble_pub_addr_get(); stored != nullptr) {
    bt_addr = *stored;
    // All-zero is "unset", not a unique address: ln_kv_ble_addr_load() itself
    // substitutes the default for it, so programming it verbatim would give the
    // controller a null address. Treat it like the default and derive instead.
    have_unique_addr =
        memcmp(bt_addr.addr, BLE_DEFAULT_ADDR, sizeof(bt_addr.addr)) != 0 && !is_unset_addr(bt_addr.addr);
  } else {
    // KV subsystem down (wrong partition layout, corrupted region): derive
    // below instead of dereferencing null and boot-looping.
    ESP_LOGW(TAG, "BLE address KV unavailable; deriving address from WiFi MAC");
  }
  if (!have_unique_addr) {
    uint8_t wifi_mac[6] = {0};
    get_mac_address_raw(wifi_mac);  // MSB-first
    // Reverse into controller (LSB-first) order, then BLE = WiFi + 1: increment
    // the NIC low byte (addr[0] once reversed), no carry, OUI unchanged — the
    // Beken/Tuya factory pairing the bk72xx sibling also uses.
    for (int i = 0; i < 6; i++)
      bt_addr.addr[i] = wifi_mac[5 - i];
    bt_addr.addr[0] = static_cast<uint8_t>(bt_addr.addr[0] + 1);
    if (int err = ln_kv_ble_addr_store(bt_addr); err != 0) {
      ESP_LOGW(TAG, "Failed to persist derived BLE address (err %d); will re-derive next boot", err);
    } else {
      ESP_LOGD(TAG, "MAC derived (WiFi+1) and stored");
    }
  }
  memcpy(this->ble_mac_, bt_addr.addr, 6);
}

}  // namespace esphome::ln882h_ble

#endif  // USE_LN882H_BLE
