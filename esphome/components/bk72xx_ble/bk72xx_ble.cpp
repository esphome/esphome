// bk72xx_ble.cpp
//
// BLE controller support for the BK72xx BLE-5.x chips (LibreTiny beken-72xx
// family) — the platform analog of esp32_ble / rp2040_ble. Owns everything that
// talks to the Beken BDK BLE stack:
//   - one-time stack bring-up (ble_set_notice_cb() + ble_entry()),
//   - the controller BLE address,
//   - the scan reconciler (request, pacing, bring-up budget) over the
//     bdk_scan surface,
//   - the scan-report ring: the BDK notice callback (BLE task) takes a report
//     from a fixed pool and pushes it on a lock-free SPSC queue; loop() drains,
//     dispatches on the main task and returns reports to the pool — the same
//     EventPool + LockFreeQueue handoff esp32_ble uses, zero allocation at
//     steady state.
// Consumers contain no SDK calls of their own.
//
// NOTE: the Beken BDK BLE 5.x stack is compiled and linked by the LibreTiny
// beken-72xx builder itself (prebuilt libble_<chip>.a + ble_5_x sources, gated
// on CFG_SUPPORT_BLE / CFG_BLE_VERSION in sys_config.h). This component only
// calls into it via the public ble_api.h — no framework patch is required.

#include "bk72xx_ble.h"  // pulls esphome/core/defines.h for USE_BK72XX_BLE

#include "bdk_scan.h"  // the raw BDK scan surface (state reads, starts, release)

#ifdef USE_BK72XX_BLE

#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"  // get_mac_address_raw()
#include "esphome/core/log.h"

// ---------------------------------------------------------------------------
// SDK-capability gate (not a chip allowlist).
// This component drives the Beken BLE *5.x* controller. `ble_api.h` cannot be
// the probe: it ships for every SoC (driver/include) and merely switches on
// CFG_BLE_VERSION internally. `app_ble.h` is on the include path only when the
// LibreTiny beken-72xx builder selects a 5.x stack, so gating on it supports
// any BLE-5.x chip — present or future — without a hard-coded list, and a
// non-5.x build fails here with a clear message instead of a cryptic
// "app_ble.h: No such file or directory".
// ---------------------------------------------------------------------------
#if defined(CLANG_TIDY)
// The clang-tidy environment does not carry the full Beken BDK BLE 5.x API
// (its ble_api.h variant lacks parts of the 5.x surface), so there is nothing
// accurate to analyze the SDK calls against — skip the file under analysis.
#define BK72XX_BLE_NO_SDK
#elif !__has_include("ble_api.h") || !__has_include("app_ble.h")
// Also skip the SDK body: #error does not stop the preprocessor, and on a 4.2
// SoC ble_api.h exists, so without the guard the 5.x symbols would fail one by
// one and bury this message.
#define BK72XX_BLE_NO_SDK
#error \
    "bk72xx_ble requires a BLE 5.x Beken SDK (app_ble.h). Supported SoCs: BK7231N/BK7236 (BLE 5.1) and BK7238/BK7252N/BK7253 (BLE 5.2). BK7231T/BK7251/BK7271 (BLE 4.2) and BK7231Q (no BLE) are not supported."
#endif

#ifndef BK72XX_BLE_NO_SDK

// ---------------------------------------------------------------------------
// Beken BDK BLE 5.x SDK — public API.
// Exposed on the include path by the LibreTiny beken-72xx builder
// (cores/.../ble_5_x_rw + driver/include). Wrapped in extern "C" because these
// are C headers consumed from C++ (a standard C-header-from-C++ pattern).
// ---------------------------------------------------------------------------
extern "C" {
#include "ble_api.h"  // ble_set_notice_cb, recv_adv_t, ble_notice_t,
                      // BLE_5_REPORT_ADV (scan primitives live in bdk_scan.cpp)
#ifdef BK72XX_BLE_HAS_COMMON_BDADDR
#include "common_bt_defines.h"  // struct bd_addr
// The controller's public BLE address, populated by the BDK during ble_entry().
// Present on BK7231N; the other BLE-5.x chips' stacks have no such symbol — there the
// address is derived from the WiFi MAC instead (matching the BDK's own fallback).
extern struct bd_addr common_default_bdaddr;
#endif
// ble_entry() brings up the BDK BLE stack; it is not declared in ble_api.h, so
// declare it here.
void ble_entry(void);
}

namespace esphome::bk72xx_ble {

static const char *const TAG = "bk72xx_ble";

static constexpr uint32_t RECONCILE_RETRY_MS = 10;              // pump floor for fast loops
static constexpr uint32_t RECONCILE_REJECTED_RETRY_MS = 500;    // retry gate after a rejected release
static constexpr uint32_t RECONCILE_PENDING_TIMEOUT_MS = 2000;  // bring-up budget before FAILED
static constexpr uint32_t SCAN_LIVENESS_CHECK_MS = 1000;        // settled-scan re-check cadence
static constexpr uint32_t TEARDOWN_STUCK_ERROR_MS = 30000;      // stuck-teardown ERROR (stop also goes FAILED)

// The BDK notice callback is a plain C function pointer with no user argument,
// so it reaches the (single) component instance through a file-static pointer.
static BK72xxBLE *s_ble = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// ---------------------------------------------------------------------------
// BLE notice callback — runs in the BDK BLE task context.
// The BK controller reports every advertisement as a BLE_5_REPORT_ADV notice
// carrying a recv_adv_t. Copy it into the queue and return; all dispatch
// happens in loop() on the main task.
// ---------------------------------------------------------------------------
static void ble_notice_callback(ble_notice_t notice, void *param) {
  if (s_ble == nullptr || param == nullptr)
    return;
  if (notice != BLE_5_REPORT_ADV)
    return;

  const recv_adv_t *info = reinterpret_cast<const recv_adv_t *>(param);
  // rssi is a signed dBm carried in a uint8_t; cast through int8_t (standard for
  // a signed dBm value packed in a uint8_t).
  s_ble->enqueue_scan_report(info->adv_addr, static_cast<int8_t>(info->rssi), info->adv_addr_type,
                             static_cast<uint8_t>(info->evt_type), info->data, info->data_len);
}

void BK72xxBLE::enqueue_scan_report(const uint8_t *mac, int8_t rssi, uint8_t addr_type, uint8_t evt_type,
                                    const uint8_t *data, uint16_t data_len) {
  BLEScanReport *report = this->report_pool_.allocate();
  if (report == nullptr) {
    // Pool exhausted — the queue is full; count and drop.
    this->report_queue_.increment_dropped_count();
    return;
  }
  memcpy(report->mac, mac, MAC_ADDRESS_SIZE);
  report->rssi = rssi;
  report->addr_type = addr_type;
  report->evt_type = evt_type;
  report->data_len =
      (data_len <= sizeof(report->data)) ? static_cast<uint8_t>(data_len) : static_cast<uint8_t>(sizeof(report->data));
  memcpy(report->data, data, report->data_len);
  // Cannot fail: the pool is sized to the queue capacity.
  this->report_queue_.push(report);
}

// ---------------------------------------------------------------------------
// Component lifecycle
// ---------------------------------------------------------------------------

void BK72xxBLE::setup() {
  s_ble = this;
  // The report pool grows lazily on purpose: the BDK notice callback runs in
  // task context (malloc-safe, unlike rp2040's IRQ path), and typical traffic
  // stays far below the pool cap, so not warming contains RAM.
  // Resolve the MAC early so get_mac_lsb_first() is valid for consumers before
  // the stack is up (it is re-read once ble_entry() has run).
  this->resolve_mac_();
  if (this->enable_on_boot_) {
    this->enable();
  }
}

// AFTER_WIFI, not BLUETOOTH: replicates the proven pre-split timing — the BDK
// is first touched only once WiFi is up (single-core WiFi/BLE bring-up order).
float BK72xxBLE::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

void BK72xxBLE::enable() {
  if (this->state_ != BLEComponentState::STATE_OFF)
    return;
  this->state_ = BLEComponentState::ENABLING;

  // One-time BLE stack init: register the notice callback, then bring up the
  // BDK BLE stack. The BDK has no teardown path — init happens at most once.
  ble_set_notice_cb(ble_notice_callback);
  ble_entry();

  delay(100);  // NOLINT — one-time BLE stack init; the SDK needs this settle time

  // Re-read the BLE MAC now that the controller is up (common_default_bdaddr is
  // populated by ble_entry()); resolve_mac_() may have fallen back earlier.
  this->resolve_mac_();

#ifdef BK72XX_BLE_HAS_COMMON_BDADDR
  // Liveness heuristic (BK7231N): a healthy ble_entry() populates
  // common_default_bdaddr during init, so all-zero after the settle delay
  // suggests the stack did not come up. The BDK entry point returns void — no
  // return code exists — so warn rather than fail: scan starts against a dead
  // stack already fail cleanly downstream (no idle activity handle).
  bool bdaddr_live = false;
  for (uint8_t b : common_default_bdaddr.addr) {
    if (b != 0) {
      bdaddr_live = true;
      break;
    }
  }
  if (!bdaddr_live)
    ESP_LOGW(TAG, "Controller address still unset after init; BLE stack may not have started");
#endif

  this->state_ = BLEComponentState::ACTIVE;
  ESP_LOGD(TAG, "BLE stack initialised");
}

void BK72xxBLE::loop() {
  // Keep reconciling toward the requested scan state (e.g. complete a stop
  // that arrived while a controller operation was in flight), and re-check a
  // settled scan at low frequency: a controller-side drop re-enters the
  // bring-up, and the budget's FAILED feeds the tracker's recovery.
  // Keep driving until settled: any PENDING, plus a terminal stop whose slot
  // must still be freed. A FAILED scan request is the one combination not
  // re-driven here — that belongs to the tracker's backoff.
  const uint32_t pump_now = App.get_loop_component_start_time();
  if (this->last_result_ == ScanOpResult::PENDING ||
      (!this->scan_wanted_ && this->last_result_ == ScanOpResult::FAILED)) {
    const uint32_t gate = (this->release_warned_ || this->last_result_ == ScanOpResult::FAILED)
                              ? RECONCILE_REJECTED_RETRY_MS
                              : RECONCILE_RETRY_MS;
    if (pump_now - this->last_advance_ms_ >= gate)
      this->advance_();
  } else if (this->scan_wanted_ && this->last_result_ == ScanOpResult::SETTLED &&
             pump_now - this->last_advance_ms_ >= SCAN_LIVENESS_CHECK_MS) {
    // Re-check a settled scan; scan_start() refills the bring-up budget.
    // WARN: the only report of a drop that recovers inside its budget.
    if (this->scan_start(this->requested_.interval, this->requested_.window, this->requested_.active) !=
        ScanOpResult::SETTLED)
      ESP_LOGW(TAG, "Controller dropped the scan; restarting");
  }

  // Drain the lock-free ring filled by the BLE task; all per-report work runs
  // here on the main task, then the report returns to the pool.
  BLEScanReport *report = this->report_queue_.pop();
  if (report == nullptr)
    return;
  do {
#ifdef BK72XX_BLE_SCAN_LISTENER_COUNT
    for (auto *listener : this->scan_listeners_)
      listener->on_scan_report(*report);
#endif
    this->report_pool_.release(report);
  } while ((report = this->report_queue_.pop()) != nullptr);

  // Log dropped reports — only reachable when reports were processed; drops can
  // only occur while the queue is full, and only this loop drains it.
  uint16_t dropped = this->report_queue_.get_and_reset_dropped_count();
  if (dropped > 0)
    ESP_LOGW(TAG, "Dropped %u scan reports due to queue overflow", dropped);
}

void BK72xxBLE::get_mac_lsb_first(uint8_t out[MAC_ADDRESS_SIZE]) const {
  for (int i = 0; i < 6; i++)
    out[i] = this->ble_mac_[i];
}

void BK72xxBLE::dump_config() {
  // ble_mac_ is stored LSB-first (BLE convention); print [5..0] for the
  // MSB-first order Home Assistant shows.
  ESP_LOGCONFIG(TAG,
                "BK72xx BLE:\n"
                "  MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n"
                "  Active: %s",
                this->ble_mac_[5], this->ble_mac_[4], this->ble_mac_[3], this->ble_mac_[2], this->ble_mac_[1],
                this->ble_mac_[0], YESNO(this->is_active()));
}

// ---------------------------------------------------------------------------
// MAC resolution
// ---------------------------------------------------------------------------

void BK72xxBLE::resolve_mac_() {
#ifdef BK72XX_BLE_HAS_COMMON_BDADDR
  // BK7231N: the BDK populates common_default_bdaddr (LSB-first, BLE convention)
  // during ble_entry(). It may still be zero before the stack is up; if so, fall
  // through to the WiFi-derived MAC below.
  bool nonzero = false;
  for (uint8_t b : common_default_bdaddr.addr) {
    if (b != 0) {
      nonzero = true;
      break;
    }
  }
  if (nonzero) {
    memcpy(this->ble_mac_, common_default_bdaddr.addr, MAC_ADDRESS_SIZE);
    return;
  }
#endif
  // Chips whose BLE stack does not export common_default_bdaddr (BK7238 and the other
  // BLE-5.x SoCs), or BK7231N before the stack is up: derive the BLE MAC exactly as the
  // Beken BDK does in bdaddr_env_init() — the WiFi STA MAC with only its last byte
  // incremented (sta_mac[5] += 1, a plain byte increment with no carry into the next
  // byte), OUI unchanged. This reproduces the address the controller advertises with
  // (verified against the BK7231N BLE-5.1 and BK7252N/BK7238 BLE-5.2 SDK sources), so it
  // matches on every device, including the last-byte == 0xFF edge that a 24-bit increment
  // would carry differently.
  uint8_t wifi_mac[MAC_ADDRESS_SIZE];
  get_mac_address_raw(wifi_mac);  // MSB-first
  const uint8_t ble[MAC_ADDRESS_SIZE] = {wifi_mac[0], wifi_mac[1], wifi_mac[2],
                                         wifi_mac[3], wifi_mac[4], static_cast<uint8_t>(wifi_mac[5] + 1)};
  // Store LSB-first to match recv_adv_t adv_addr ordering.
  for (int i = 0; i < 6; i++)
    this->ble_mac_[i] = ble[5 - i];
}

// ---------------------------------------------------------------------------
// Scan reconciler
// ---------------------------------------------------------------------------

// Episode boundary: fresh teardown deadline and error bookkeeping.
void BK72xxBLE::reset_teardown_episode_() {
  this->teardown_since_ms_ = 0;
  this->restarting_ = false;
  this->last_release_err_ = 0;
}

ScanOpResult BK72xxBLE::scan_start(uint16_t interval, uint16_t window, bool active) {
  if (!this->is_active())
    this->enable();

  const ScanParams params{active, interval, window};
  // A new episode refills the budget and gets a fresh teardown deadline; a
  // re-call observing an in-flight bring-up (last result PENDING) must not.
  if (this->last_result_ != ScanOpResult::PENDING || !this->scan_wanted_ || params != this->requested_) {
    this->pending_since_ms_ = App.get_loop_component_start_time();
    this->reset_teardown_episode_();
  }
  this->scan_wanted_ = true;
  this->requested_ = params;
  return this->advance_();
}

void BK72xxBLE::scan_stop() {
  if (this->scan_wanted_) {
    // A stamp inherited from a stuck restart would fail the stop on its
    // first advance.
    this->reset_teardown_episode_();
  }
  this->scan_wanted_ = false;
  this->advance_();
}

bool BK72xxBLE::flush_pending_stop(uint32_t timeout_ms) {
  // millis() on both sides: the loop clock is frozen while this blocks.
  const uint32_t start = millis();
  while (!this->scan_wanted_ && this->last_result_ == ScanOpResult::PENDING) {
    if (millis() - start >= timeout_ms)
      return false;
    delay(RECONCILE_RETRY_MS);
    this->advance_();
  }
  return this->last_result_ == ScanOpResult::SETTLED;
}

// Teardown is asynchronous: the handle is kept until an IDLE observation
// confirms the radio is idle. A rejection WARNs once per failure streak and
// widens the pump gate; the epilogue owns the stuck-teardown deadline.
void BK72xxBLE::release_activity_(BdkActivityState state) {
  const BdkOpResult result =
      bdk_scan_release(this->scan_activity_idx_, state == BdkActivityState::CREATED, &this->last_release_err_);
  if (result == BdkOpResult::OK) {
    this->release_warned_ = false;
    return;
  }
  if (!this->release_warned_) {
    // A hard error carries its code immediately; the 30 s stuck ERROR follows
    // if it persists.
    if (result == BdkOpResult::FAILED) {
      ESP_LOGW(TAG, "Scan activity release failed (err %d); retrying", this->last_release_err_);
    } else {
      ESP_LOGW(TAG, "Scan activity release rejected; retrying");
    }
    this->release_warned_ = true;
  }
}

// Stamp/track the teardown episode; once past the deadline, ERROR (re-logged
// each interval) and report stuck.
bool BK72xxBLE::teardown_stuck_(uint32_t now) {
  if (this->teardown_since_ms_ == 0) {
    this->teardown_since_ms_ = now;
    this->teardown_stuck_log_ms_ = now;  // first ERROR fires at the deadline
    return false;
  }
  if (now - this->teardown_since_ms_ < TEARDOWN_STUCK_ERROR_MS)
    return false;
  if (now - this->teardown_stuck_log_ms_ >= TEARDOWN_STUCK_ERROR_MS) {
    if (this->last_release_err_ != 0) {
      ESP_LOGE(TAG, "Scan teardown cannot proceed; scanner is stuck (release err %d)", this->last_release_err_);
    } else {
      // No rejected release this episode: stuck waiting on the controller.
      ESP_LOGE(TAG, "Scan teardown cannot proceed; scanner is stuck (controller busy)");
    }
    this->teardown_stuck_log_ms_ = now;
  }
  return true;
}

// One SDK operation per call toward the latched request; controller state is
// read live each time (it changes on the BLE task, so nothing is mirrored).
// The epilogue owns all deadlines and episode bookkeeping.
ScanOpResult BK72xxBLE::advance_() {
  if (!this->scan_wanted_ && this->scan_activity_idx_ == INVALID_ACTIVITY_IDX) {
    // Nothing to do; also keeps SDK reads off the pre-enable() path.
    this->last_result_ = ScanOpResult::SETTLED;
    return ScanOpResult::SETTLED;
  }
  const BdkActivityState state = bdk_scan_state(this->scan_activity_idx_);
  const bool ready = bdk_scan_ready();
  ScanOpResult result = this->scan_wanted_ ? this->advance_start_(state, ready) : this->advance_stop_(state, ready);

  const uint32_t now = App.get_loop_component_start_time();
  this->last_advance_ms_ = now;
  if (result == ScanOpResult::SETTLED || (state == BdkActivityState::IDLE && ready)) {
    // Any teardown episode is over (IDLE observed with the controller
    // settled, or e.g. a mode flip that settled back without ever reaching
    // IDLE). An IDLE read while an operation is in flight proves nothing —
    // a stop deferred there must keep its episode running.
    this->reset_teardown_episode_();
    this->release_warned_ = false;
  }
  if (this->restarting_ && (state == BdkActivityState::IDLE || state == BdkActivityState::CREATED)) {
    // The mode-change release is observed complete; the rest is a normal
    // bring-up on a fresh budget.
    this->restarting_ = false;
    this->pending_since_ms_ = now;
  }
  // Not chained to the clear above: a bring-up waiting at IDLE (create still
  // in flight) must keep spending its budget.
  if (result == ScanOpResult::PENDING) {
    if (this->scan_wanted_ && state != BdkActivityState::STARTED && !this->restarting_) {
      // A downed radio spends the bring-up budget; exhausting it hands
      // recovery to the tracker's backoff.
      if (now - this->pending_since_ms_ >= RECONCILE_PENDING_TIMEOUT_MS) {
        ESP_LOGE(TAG, "Scan bring-up did not settle; giving up until the next start");
        result = ScanOpResult::FAILED;
      }
    } else {
      // A teardown is pending: a stop, or a mode-change release still in
      // flight (restarting_); either way the bring-up budget waits.
      if (this->scan_wanted_)
        this->pending_since_ms_ = now;
      if (this->teardown_stuck_(now)) {
        // Terminal for stop AND restart: the tracker's backoff owns recovery
        // (a stop's release keeps re-driving from loop(); a restart is
        // re-requested through scan_start() with a fresh deadline).
        result = ScanOpResult::FAILED;
      }
    }
  }
  this->last_result_ = result;
  return result;
}

ScanOpResult BK72xxBLE::advance_stop_(BdkActivityState state, bool ready) {
  if (state == BdkActivityState::IDLE && ready) {
    // Fully torn down (or never created): the radio is idle. IDLE is trusted
    // only when the controller is settled — mid-create the slot still reads
    // IDLE, and dropping the handle then would leak the activity once the
    // create lands.
    this->scan_activity_idx_ = INVALID_ACTIVITY_IDX;
    return ScanOpResult::SETTLED;
  }
  if (!ready) {
    // Acting mid-operation could delete an activity whose start lands
    // afterwards, leaking the slot with the radio on; wait.
    if (this->last_result_ == ScanOpResult::SETTLED)
      ESP_LOGD(TAG, "Scan stop deferred (controller busy)");
    return ScanOpResult::PENDING;
  }
  // Settled, so CREATED unambiguously means "never started".
  this->release_activity_(state);
  return ScanOpResult::PENDING;  // confirmed once IDLE is observed
}

ScanOpResult BK72xxBLE::advance_start_(BdkActivityState state, bool ready) {
  if (state == BdkActivityState::STARTED) {
    if (this->applied_ == this->requested_)
      return ScanOpResult::SETTLED;
    // Running with different mode or parameters: tear down (the SDK stop
    // chain also deletes the activity) and recreate on a later advance.
    if (ready) {
      this->release_activity_(state);
      // Invalidate so a flip back to the old params cannot SETTLE against the
      // activity being deleted (interval 0 never matches a real request).
      this->applied_.interval = 0;
      this->restarting_ = true;
    }
    return ScanOpResult::PENDING;
  }
  if (!ready) {
    if (this->last_result_ == ScanOpResult::SETTLED)
      ESP_LOGD(TAG, "Scan start deferred (controller busy)");
    return ScanOpResult::PENDING;
  }
  if (state == BdkActivityState::CREATED) {
    // Fire-and-forget: SETTLED only once a later advance observes the scan
    // running, so a rejected start is retried rather than silently dead. On
    // failure the created activity is intact; keep the handle.
    if (bdk_scan_start(this->scan_activity_idx_, this->requested_.interval, this->requested_.window,
                       this->requested_.active) != BdkOpResult::OK)
      return ScanOpResult::FAILED;
    this->applied_ = this->requested_;
    return ScanOpResult::PENDING;
  }
  if (state == BdkActivityState::OTHER)
    return ScanOpResult::PENDING;  // transitional; settles on a later read

  // IDLE and ready: acquire a slot and create. A kept index is deliberately
  // reused: SDK delete returns the slot to idle and create requires an idle
  // slot, so it equals a fresh acquire — while clearing here would orphan a
  // create still in flight (the BUSY race below).
  if (this->scan_activity_idx_ == INVALID_ACTIVITY_IDX) {
    this->scan_activity_idx_ = bdk_scan_acquire_activity();
    if (this->scan_activity_idx_ == INVALID_ACTIVITY_IDX)
      return ScanOpResult::FAILED;
  }
  switch (bdk_scan_create(this->scan_activity_idx_)) {
    case BdkOpResult::BUSY:  // raced the BLE task; keep the index, the retry resumes this slot
    case BdkOpResult::OK:
      return ScanOpResult::PENDING;
    case BdkOpResult::FAILED:
      break;
  }
  // Safe to clear (unlike BUSY): acquire is a pure search, so a rejected
  // create leaves the slot IDLE for re-acquire.
  this->scan_activity_idx_ = INVALID_ACTIVITY_IDX;
  return ScanOpResult::FAILED;
}

}  // namespace esphome::bk72xx_ble

#endif  // BK72XX_BLE_NO_SDK
#endif  // USE_BK72XX_BLE
