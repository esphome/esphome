// ln882h_ble.cpp
//
// BLE controller support for the LN882H (LibreTiny lightning-ln882h family) —
// the platform analog of esp32_ble / rp2040_ble. Owns everything that talks to
// the LN882H BLE SDK:
//   - one-time stack bring-up (rw_init + the ln_* app init sequence),
//   - the controller BLE address (persistent KV entry, WiFi-MAC-derived once),
//   - the raw controller scan primitives (ln_ble_scan_start/stop),
//   - the scan-report ring: the SDK's rw-task event callback decodes each
//     report (including the controller's RSSI sign quirk) into a fixed pool
//     and pushes it on a lock-free SPSC queue; loop() drains, dispatches on
//     the main task and returns reports to the pool — the same EventPool +
//     LockFreeQueue handoff esp32_ble uses, zero allocation at steady state.
// Consumers contain no SDK calls of their own.
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

using ble_evt_cb_t = void (*)(void *param);
void ln_ble_evt_mgr_reg_evt(int evt_id, ble_evt_cb_t cb);

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
//                            gapm_dup_filter_pol / gapm_scan_prop / gapm_adv_report_info
// ---------------------------------------------------------------------------
static constexpr uint32_t CLK_G_BLE = 1u << 0;
static constexpr int BLE_EVT_ID_SCAN_REPORT = 3;

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
static constexpr uint8_t GAPM_SCAN_PROP_ACTIVE_1M_BIT = 1 << 2;

// GAPM extended-advertising report types (bits 2:0 of ble_scan_report_t::info).
// 0 = ADV_EXT (extended advertisement), 1 = ADV_LEG (legacy advertisement),
// 2 = SCAN_RSP_EXT (scan response to extended adv), 3 = SCAN_RSP_LEG (scan response to legacy adv).
static constexpr uint8_t GAPM_REPORT_TYPE_ADV_LEG = 1;
static constexpr uint8_t GAPM_REPORT_TYPE_SCAN_RSP_LEG = 3;
// Bit 5 of ble_scan_report_t::info: the advertisement is scannable, i.e. a scan
// response may follow (enum gapm_adv_report_info, GAPM_REPORT_INFO_SCAN_ADV_BIT).
static constexpr uint8_t GAPM_REPORT_INFO_SCAN_ADV_BIT = 1u << 5;

// ---------------------------------------------------------------------------
// SDK struct layouts
// ---------------------------------------------------------------------------

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

// Scan report delivered by the BLE_EVT_ID_SCAN_REPORT event. Layout verified on
// hardware against the prebuilt BLE stack LibreTiny links: its report carries no
// PHY fields and stores the advertisement data inline (flexible array), unlike
// the newer upstream SDK header (which adds phy_prim/phy_second and a data pointer).
struct ble_scan_report_t {  // NOLINT(readability-identifier-naming) - mirrors the SDK type name
  uint8_t actv_idx;
  uint8_t info;
  uint8_t trans_addr_type;
  uint8_t trans_addr[6];
  uint8_t target_addr_type;
  uint8_t target_addr[6];
  int8_t tx_pwr;
  int8_t rssi;  // signed dBm, range -127..+20 (ble_evt_scan_report_t from ln_ble_event_manager.h)
  uint16_t length;
  uint8_t data[0];
};
// Pin the layout of the hand-mirrored report struct too: the comment above
// notes a newer SDK header uses a different layout (PHY fields + data pointer),
// so silent drift here would corrupt every decoded advertisement.
static_assert(sizeof(ble_scan_report_t) == 20, "ble_scan_report_t must match the linked BLE stack's layout");
static_assert(offsetof(ble_scan_report_t, length) == 18, "unexpected padding in ble_scan_report_t");
static_assert(offsetof(ble_scan_report_t, data) == 20, "advertisement data must follow the header inline");

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

// The SDK event callback is a plain C function pointer with no user argument,
// so it reaches the (single) component instance through a file-static pointer.
static LN882HBLE *s_ble = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Scan parameter blocks handed to ln_ble_scan_start(void *). static storage:
// the SDK may retain the pointer past the call (the block travels into a GAPM
// message consumed later by the rw task), so a stack-local would leave the
// controller reading a dead frame. Double-buffered: consecutive starts (the
// enable() probe followed by the first real scan, or a parameter restart)
// alternate blocks, so a rewrite can never race a previous block that is still
// in flight — correct under either reading of SDK retention. All writers run
// on the main task.
static le_scan_parameters_t s_scan_params[2]{};  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static uint8_t s_scan_params_idx = 0;            // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static le_scan_parameters_t *next_scan_params() {
  s_scan_params_idx ^= 1;
  return &s_scan_params[s_scan_params_idx];
}

// ---------------------------------------------------------------------------
// Scan-report event callback — runs in the SDK's rw task context.
// Decode the report (hardware-verified struct layout + the RSSI sign fix),
// copy it into the queue and return; all dispatch happens in loop() on the
// main task.
// ---------------------------------------------------------------------------
static void ble_scan_callback(void *param) {
  if (s_ble == nullptr || param == nullptr)
    return;
  const auto *info = reinterpret_cast<const ble_scan_report_t *>(param);

  // Only legacy framing is supported (see scan_start(): legacy 1M PHY only):
  // an extended report does not fit BLEScanReport::data and would reach
  // consumers as a truncated legacy frame. Reject before allocating so these
  // do not burn pool slots either.
  const uint8_t report_type = info->info & 0x07;
  if (report_type != GAPM_REPORT_TYPE_ADV_LEG && report_type != GAPM_REPORT_TYPE_SCAN_RSP_LEG) {
    s_ble->count_rejected_report();
    return;
  }

  // Fill the pool slot in place (the bk72xx_ble shape): no report on the rw
  // task's stack — its size is fixed by the prebuilt stack — one copy of the
  // payload instead of two, and only data_len bytes ever leave this frame.
  BLEScanReport *slot = s_ble->allocate_scan_report();
  if (slot == nullptr)
    return;  // no slot — counted as dropped in allocate_scan_report()

  // BLE RSSI sign fix. The LN882H controller intermittently reports the RSSI with
  // a flipped sign: a real -58 dBm arrives as +58, above the SDK's documented
  // -127..+20 dBm maximum. Recover it by negating any value above +20 (verified
  // on-device: the out-of-range positives cluster at the magnitude of each
  // device's real readings). This is the ONLY LN882H-specific RSSI handling —
  // downstream the value is used exactly like on ESP32.
  const int8_t raw = info->rssi;

  memcpy(slot->mac, info->trans_addr, MAC_ADDRESS_SIZE);
  slot->rssi = (raw > 20) ? static_cast<int8_t>(-raw) : raw;
  slot->addr_type = info->trans_addr_type;
  slot->is_scan_response = report_type == GAPM_REPORT_TYPE_SCAN_RSP_LEG;
  slot->scannable = (info->info & GAPM_REPORT_INFO_SCAN_ADV_BIT) != 0;
  slot->data_len = (info->length <= sizeof(slot->data)) ? static_cast<uint8_t>(info->length)
                                                        : static_cast<uint8_t>(sizeof(slot->data));
  memcpy(slot->data, info->data, slot->data_len);

  s_ble->push_scan_report(slot);
}

BLEScanReport *LN882HBLE::allocate_scan_report() {
  BLEScanReport *slot = this->report_pool_.allocate();
  if (slot == nullptr) {
    // No slot: pool exhausted (queue full) or the pool's on-demand RAM
    // allocation failed; count and drop either way.
    this->report_queue_.increment_dropped_count();
  }
  return slot;
}

void LN882HBLE::push_scan_report(BLEScanReport *report) {
  // Cannot fail: the pool is sized to the queue capacity.
  this->report_queue_.push(report);
}

// ---------------------------------------------------------------------------
// Component lifecycle
// ---------------------------------------------------------------------------

void LN882HBLE::setup() {
  s_ble = this;
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
  // manager completes activity creation on the first start. Uses the shared
  // static parameter block (see s_scan_params for the lifetime rationale).
  le_scan_parameters_t *probe = next_scan_params();
  probe->type = GAPM_SCAN_TYPE_OBSERVER;
  probe->prop = GAPM_SCAN_PROP_PHY_1M_BIT;
  probe->dup_filt_pol = GAPM_DUP_FILT_DIS;
  probe->scan_intv = 160;
  probe->scan_wd = 16;
  ln_ble_scan_start(probe);
  delay(10);
  ln_ble_scan_stop();

  // Register the scan-report event exactly once, after the event manager is up.
  // Repeated registration corrupts the SDK's event registry (verified on
  // hardware), which is why this lives here and not in scan_start().
  ln_ble_evt_mgr_reg_evt(BLE_EVT_ID_SCAN_REPORT, ble_scan_callback);

  this->state_ = BLEComponentState::ACTIVE;
  ESP_LOGD(TAG, "BLE stack initialised");
}

void LN882HBLE::loop() {
  // Log dropped reports before the empty-queue return: a drop can also mean
  // EventPool::allocate() failed on heap exhaustion, and that can happen with
  // the queue empty — from the very first report on. Checking here keeps that
  // failure visible instead of producing a scanner that is silently dead.
  uint16_t dropped = this->report_queue_.get_and_reset_dropped_count();
  if (dropped > 0)
    ESP_LOGW(TAG, "Dropped %u scan reports (queue full or out of memory for a report slot)", dropped);
  // Drain the lock-free ring filled by the rw task; all per-report work runs
  // here on the main task, then the report returns to the pool.
  BLEScanReport *report = this->report_queue_.pop();
  if (report != nullptr) {
    this->reject_diagnosis_done_ = true;
    do {
#ifdef LN882H_BLE_SCAN_LISTENER_COUNT
      for (auto *listener : this->scan_listeners_)
        listener->on_scan_report(*report);
#endif
      this->report_pool_.release(report);
    } while ((report = this->report_queue_.pop()) != nullptr);
  }

  // Rejected-report accounting AFTER the drain: a stray non-legacy frame
  // arriving ahead of the first good one must not latch the dead-scanner
  // warning; the threshold keeps one-off boot noise below it while a truly
  // dead scanner (~200 reports/s all rejected) crosses it within a second.
  // Avoid the sub-word CAS in the common case (LockFreeQueue's dropped-count
  // pattern): rejects are rare, the load is cheap.
  uint16_t rejected = this->rejected_reports_.load(std::memory_order_relaxed);
  if (rejected > 0) {
    rejected = this->rejected_reports_.exchange(0, std::memory_order_relaxed);
    if (!this->reject_diagnosis_done_) {
      this->rejected_before_delivery_ += rejected;
      if (this->rejected_before_delivery_ >= REJECTED_DEAD_SCANNER_THRESHOLD) {
        this->reject_diagnosis_done_ = true;
        ESP_LOGW(TAG, "Rejected %u scan reports before any was delivered - unexpected report encoding?",
                 static_cast<unsigned>(this->rejected_before_delivery_));
      }
    }
    ESP_LOGV(TAG, "Rejected %u non-legacy scan reports", rejected);
  }
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
    uint8_t wifi_mac[MAC_ADDRESS_SIZE] = {0};
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
  memcpy(this->ble_mac_, bt_addr.addr, MAC_ADDRESS_SIZE);
}

// ---------------------------------------------------------------------------
// Controller scan primitives
// ---------------------------------------------------------------------------

void LN882HBLE::scan_start(uint16_t interval, uint16_t window, bool active) {
  if (!this->is_active())
    this->enable();

  if (this->scanning_) {
    // Already scanning - stop first so this call cleanly restarts with the new
    // parameters (re-entry guard). Give the GAPM stop the same settle time
    // enable() grants between consecutive GAPM operations before restarting.
    this->scan_stop();
    delay(10);  // NOLINT — restart-only, mirrors enable()'s inter-operation settle
  }

  // Double-buffered static block — see s_scan_params for the lifetime rationale.
  le_scan_parameters_t *p = next_scan_params();
  p->dup_filt_pol = GAPM_DUP_FILT_DIS;
  p->type = GAPM_SCAN_TYPE_OBSERVER;
  p->scan_intv = interval;
  p->scan_wd = window;
  // Legacy 1M PHY only: consumers size their buffers for legacy advertisements
  // (62 B); coded/extended PHY (up to 255 B) would be silently truncated.
  p->prop = GAPM_SCAN_PROP_PHY_1M_BIT;
  if (active)
    p->prop |= GAPM_SCAN_PROP_ACTIVE_1M_BIT;

  ln_ble_scan_start(p);
  // ln_ble_scan_start() returns void, so this tracks the requested state, not a
  // confirmed one — a controller-side failure surfaces as an idle scanner (no
  // reports), which the consumer's start retry/backoff owns.
  this->scanning_ = true;
}

void LN882HBLE::scan_stop() {
  // No-op when idle, as documented: the guard keeps a redundant SDK stop off
  // the GAPM path (scan_start()'s re-entry guard calls this while scanning).
  if (!this->scanning_)
    return;
  ln_ble_scan_stop();
  this->scanning_ = false;
}

}  // namespace esphome::ln882h_ble

#endif  // USE_LN882H_BLE
