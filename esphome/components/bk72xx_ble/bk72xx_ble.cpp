// bk72xx_ble.cpp
//
// BLE controller support for the BK72xx BLE-5.x chips (LibreTiny beken-72xx
// family) — the platform analog of esp32_ble / rp2040_ble. Owns everything that
// talks to the Beken BDK BLE stack:
//   - one-time stack bring-up (ble_set_notice_cb() + ble_entry()),
//   - the controller BLE address,
//   - the raw controller scan primitives (bk_ble_scan_start/stop),
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

#ifdef USE_BK72XX_BLE

#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"  // get_mac_address_raw()
#include "esphome/core/log.h"

// ---------------------------------------------------------------------------
// SDK-capability gate (not a chip allowlist).
// This component drives the Beken BLE *5.x* controller via its public API,
// `ble_api.h`, which the LibreTiny beken-72xx builder ships only for the
// BLE-5.x SoCs (it selects the `ble_pub` 5.x stack from CFG_BLE_VERSION; the
// 4.2 SoCs build a different, older API with no ble_api.h). Gate on the header
// itself so any BLE-5.x Beken chip — present or future — is supported without a
// hard-coded list, and a non-5.x build fails here with a clear message instead
// of a cryptic "ble_api.h: No such file or directory".
// ---------------------------------------------------------------------------
#if defined(CLANG_TIDY)
// The clang-tidy environment does not carry the full Beken BDK BLE 5.x API
// (its ble_api.h variant lacks parts of the 5.x surface), so there is nothing
// accurate to analyze the SDK calls against — skip the file under analysis.
#define BK72XX_BLE_NO_SDK
#elif !__has_include("ble_api.h")
#error \
    "bk72xx_ble requires a BLE 5.x Beken SDK (ble_api.h). Supported SoCs: BK7231N/BK7236 (BLE 5.1) and BK7238/BK7252N/BK7253 (BLE 5.2). BK7231T/BK7251/BK7271 (BLE 4.2) and BK7231Q (no BLE) are not supported."
#endif

#ifndef BK72XX_BLE_NO_SDK

// ---------------------------------------------------------------------------
// Beken BDK BLE 5.x SDK — public API.
// Exposed on the include path by the LibreTiny beken-72xx builder
// (cores/.../ble_5_x_rw + driver/include). Wrapped in extern "C" because these
// are C headers consumed from C++ (a standard C-header-from-C++ pattern).
// ---------------------------------------------------------------------------
extern "C" {
#include "ble_api.h"  // bk_ble_scan_start/stop, ble_entry, ble_set_notice_cb,
                      // app_ble_get_idle_actv_idx_handle, struct scan_param,
                      // recv_adv_t, ble_notice_t, BLE_5_REPORT_ADV, SCAN_ACTV
// Real SDK headers for the packed active-scan start (see
// send_active_start_()); the component pins beken-bdk 3.0.78.
#include "app_ble.h"     // app_ble_env, app_ble_run, app_ble_actv_state_get,
                         // actv_state_t, BLE_OP_START_SCAN_POS, BLE_START_SCAN
#include "kernel_msg.h"  // KERNEL_MSG_ALLOC, kernel_msg_send
#if __has_include("gapm_msg.h")
#include "gapm_msg.h"  // BLE 5.2 (BK7238/BK7252N): gapm_activity_start_cmd, GAPM_SCAN_*
#else
#include "gapm_task.h"  // BLE 5.1 (BK7231N/BK7236): same declarations, older header name
#endif
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

// Reconciliation pacing. The main loop period (typically 16-24 ms) is the
// usual spacing between loop()-driven retries; RECONCILE_RETRY_MS is only a
// floor for faster loops. A scan bring-up still PENDING after
// RECONCILE_PENDING_LIMIT attempts (~2 s at the typical loop rate) reports
// FAILED so the tracker's exponential backoff owns recovery; a stop keeps
// retrying, the slot must be freed.
static constexpr uint32_t RECONCILE_RETRY_MS = 10;
static constexpr uint8_t RECONCILE_PENDING_LIMIT = 100;

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
  s_ble->enqueue_scan_report(info->adv_addr, static_cast<int8_t>(info->rssi), info->adv_addr_type, info->data,
                             info->data_len);
}

void BK72xxBLE::enqueue_scan_report(const uint8_t *mac, int8_t rssi, uint8_t addr_type, const uint8_t *data,
                                    uint16_t data_len) {
  BLEScanReport *report = this->report_pool_.allocate();
  if (report == nullptr) {
    // Pool exhausted — the queue is full; count and drop.
    this->report_queue_.increment_dropped_count();
    return;
  }
  memcpy(report->mac, mac, 6);
  report->rssi = rssi;
  report->addr_type = addr_type;
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
  // that arrived while a controller operation was in flight). Rate-limited so
  // a controller that keeps rejecting cannot be hammered at loop rate.
  if (this->last_result_ == ScanStartResult::PENDING &&
      App.get_loop_component_start_time() - this->last_advance_ms_ >= RECONCILE_RETRY_MS)
    this->advance_();

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

void BK72xxBLE::get_mac_lsb_first(uint8_t out[6]) const {
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
    memcpy(this->ble_mac_, common_default_bdaddr.addr, 6);
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
  uint8_t wifi_mac[6];
  get_mac_address_raw(wifi_mac);  // MSB-first
  const uint8_t ble[6] = {wifi_mac[0], wifi_mac[1], wifi_mac[2],
                          wifi_mac[3], wifi_mac[4], static_cast<uint8_t>(wifi_mac[5] + 1)};
  // Store LSB-first to match recv_adv_t adv_addr ordering.
  for (int i = 0; i < 6; i++)
    this->ble_mac_[i] = ble[5 - i];
}

// ---------------------------------------------------------------------------
// Controller scan primitives
// ---------------------------------------------------------------------------

// Pin the SDK surface the packed start command depends on: a beken-bdk bump
// that moves these must fail the build, not corrupt the kernel message.
static_assert(GAPM_SCAN_PROP_PHY_1M_BIT == (1 << 0) && GAPM_SCAN_PROP_ACTIVE_1M_BIT == (1 << 2),
              "beken-bdk GAPM scan property bits changed; revalidate send_active_start_() "
              "against the SDK's app_ble_start_scaning()");
static_assert(sizeof(struct gapm_scan_param) == 16 && sizeof(struct gapm_scan_wd_op_param) == 4,
              "beken-bdk gapm_scan_param layout changed; revalidate send_active_start_() "
              "against the SDK's app_ble_start_scaning()");

ScanStartResult BK72xxBLE::scan_start(uint16_t interval, uint16_t window, bool active) {
  // The BDK's own convention for "no activity", asserted against its symbol
  // like the GAPM pins above.
  static_assert(INVALID_ACTIVITY_IDX == UNKNOW_ACT_IDX,
                "beken-bdk activity sentinel changed; revalidate the reconciler");

  if (!this->is_active())
    this->enable();

  const ScanIntent intent = active ? ScanIntent::ACTIVE : ScanIntent::PASSIVE;
  const ScanParams params{active, interval, window};
  if (intent != this->scan_intent_ || params != this->requested_) {
    // A genuinely new request gets a fresh retry budget; a re-call observing
    // an in-flight bring-up must not refill the ceiling.
    this->pending_attempts_ = 0;
  }
  this->scan_intent_ = intent;
  this->requested_ = params;
  return this->advance_();
}

void BK72xxBLE::scan_stop() {
  this->scan_intent_ = ScanIntent::STOPPED;
  this->advance_();
}

bool BK72xxBLE::flush_pending_stop(uint32_t timeout_ms) {
  // Controller operations settle within a few ms; the bound is a safety net.
  const uint32_t start = millis();
  while (this->scan_intent_ == ScanIntent::STOPPED && this->last_result_ == ScanStartResult::PENDING) {
    if (millis() - start >= timeout_ms)
      return false;
    delay(RECONCILE_RETRY_MS);
    this->advance_();
  }
  return true;
}

// Release the current scan activity: delete when it was never started (a
// stop would be rejected), stop otherwise. Clears the handle on success; on
// rejection keeps it and logs once per episode (the paced retry stays quiet).
bool BK72xxBLE::release_activity_(bool created) {
  ble_err_t ret = created ? bk_ble_delete_scaning(this->scan_activity_idx_, nullptr)
                          : bk_ble_scan_stop(this->scan_activity_idx_, nullptr);
  if (ret == ERR_SUCCESS) {
    this->scan_activity_idx_ = INVALID_ACTIVITY_IDX;
    return true;
  }
  if (this->last_result_ != ScanStartResult::PENDING)
    ESP_LOGW(TAG, "Scan activity release rejected (err %d); retrying", static_cast<int>(ret));
  return false;
}

// The single reconciler: moves the controller toward scan_intent_, at most
// one SDK operation per call, reading the live controller state each time
// (its transitions happen on the BLE task, so nothing is mirrored).
ScanStartResult BK72xxBLE::advance_() {
  if (this->scan_intent_ == ScanIntent::STOPPED && this->scan_activity_idx_ == INVALID_ACTIVITY_IDX)
    return this->finish_advance_(ScanStartResult::STARTED);  // nothing to release

  const actv_state_t state =
      (this->scan_activity_idx_ == INVALID_ACTIVITY_IDX) ? ACTV_IDLE : app_ble_actv_state_get(this->scan_activity_idx_);
  const bool ready = app_ble_env_state_get() == APP_BLE_READY;
  const ScanStartResult result = (this->scan_intent_ == ScanIntent::STOPPED) ? this->advance_stop_(state, ready)
                                                                             : this->advance_start_(state, ready);
  return this->finish_advance_(result);
}

ScanStartResult BK72xxBLE::advance_stop_(uint8_t state, bool ready) {
  if (!ready) {
    // Acting while an operation is in flight could delete an activity whose
    // start lands afterwards (leaking the slot with the radio on); wait.
    if (this->last_result_ != ScanStartResult::PENDING)
      ESP_LOGD(TAG, "Scan stop deferred (controller busy)");
    return ScanStartResult::PENDING;
  }
  if (state == ACTV_IDLE) {
    // A create that failed at the GAPM level: nothing to stop.
    this->scan_activity_idx_ = INVALID_ACTIVITY_IDX;
    return ScanStartResult::STARTED;
  }
  // Settled, so ACTV_SCAN_CREATED unambiguously means "never started".
  return this->release_activity_(state == ACTV_SCAN_CREATED) ? ScanStartResult::STARTED : ScanStartResult::PENDING;
}

ScanStartResult BK72xxBLE::advance_start_(uint8_t state, bool ready) {
  if (state == ACTV_SCAN_STARTED) {
    if (this->applied_ == this->requested_)
      return ScanStartResult::STARTED;
    if (ready) {
      // Running with different mode or parameters: tear down (the SDK stop
      // chain also deletes the activity) and recreate on a later advance.
      this->release_activity_(false);
    }
    return ScanStartResult::PENDING;
  }
  if (!ready) {
    // A create/start/stop is in flight; observe again next call.
    if (this->last_result_ != ScanStartResult::PENDING)
      ESP_LOGD(TAG, "Scan start deferred (controller busy)");
    return ScanStartResult::PENDING;
  }
  if (state == ACTV_SCAN_CREATED) {
    if (this->requested_.active)
      return this->send_active_start_();
    // Leftover from an active-scan attempt; the passive chain creates its
    // own activity, so release this one first.
    this->release_activity_(true);
    return ScanStartResult::PENDING;
  }

  // ACTV_IDLE with the controller ready: acquire and start.
  if (this->scan_activity_idx_ == INVALID_ACTIVITY_IDX) {
    this->scan_activity_idx_ = app_ble_get_idle_actv_idx_handle(SCAN_ACTV);
    if (this->scan_activity_idx_ == INVALID_ACTIVITY_IDX) {
      ESP_LOGE(TAG, "Scan start failed: no idle activity handle");
      return ScanStartResult::FAILED;
    }
  }
  if (this->requested_.active) {
    // Step 1 of the active sequence: create the activity through the public
    // API; a later advance starts it once the creation is observed.
    ble_err_t ret = bk_ble_create_scaning(this->scan_activity_idx_, nullptr);
    if (ret == ERR_BLE_STATUS) {
      // Raced the BLE task; keep the index so the retry resumes on the same
      // slot instead of orphaning it.
      return ScanStartResult::PENDING;
    }
    if (ret != ERR_SUCCESS) {
      ESP_LOGE(TAG, "Scan activity create failed (err %d)", static_cast<int>(ret));
      this->scan_activity_idx_ = INVALID_ACTIVITY_IDX;
      return ScanStartResult::FAILED;
    }
    return ScanStartResult::PENDING;
  }
  // Passive: the SDK's own create+start chain, one call.
  struct scan_param sp;
  memset(&sp, 0, sizeof(sp));
  sp.channel_map = 7;  // advertising channels 37/38/39
  sp.interval = this->requested_.interval;
  sp.window = this->requested_.window;
  ble_err_t ret = bk_ble_scan_start(this->scan_activity_idx_, &sp, nullptr);
  if (ret != ERR_SUCCESS) {
    ESP_LOGE(TAG, "Scan start failed (err %d)", static_cast<int>(ret));
    this->scan_activity_idx_ = INVALID_ACTIVITY_IDX;
    return ScanStartResult::FAILED;
  }
  this->applied_ = this->requested_;
  return ScanStartResult::STARTED;
}

// Shared advance_() epilogue: bounded pending for scan intents, pacing stamp,
// result bookkeeping.
ScanStartResult BK72xxBLE::finish_advance_(ScanStartResult result) {
  if (result == ScanStartResult::PENDING) {
    this->last_advance_ms_ = App.get_loop_component_start_time();
    if (this->scan_intent_ != ScanIntent::STOPPED && ++this->pending_attempts_ >= RECONCILE_PENDING_LIMIT) {
      ESP_LOGE(TAG, "Scan bring-up did not settle; giving up until the next start");
      result = ScanStartResult::FAILED;
      this->pending_attempts_ = 0;
    }
  } else {
    this->pending_attempts_ = 0;
  }
  this->last_result_ = result;
  return result;
}

// The active-scan start the BDK does not offer: its start path hardcodes the
// observer type with the active property bit commented out (identically in
// the 5.1 and 5.2 stacks), so this packs the GAPM_ACTIVITY_START_CMD itself —
// field-for-field the SDK's app_ble_start_scaning() except for that bit —
// armed through the SDK's own operation bookkeeping so its state machine runs
// unchanged. Fire-and-forget: STARTED is reported only once a later advance
// observes the scan running, so a GAPM-rejected start is retried rather than
// silently dead.
ScanStartResult BK72xxBLE::send_active_start_() {
  app_ble_run(this->scan_activity_idx_, BLE_START_SCAN, 1 << BLE_OP_START_SCAN_POS, nullptr);
  struct gapm_activity_start_cmd *cmd =
      KERNEL_MSG_ALLOC(GAPM_ACTIVITY_START_CMD, TASK_BLE_GAPM, TASK_BLE_APP, gapm_activity_start_cmd);
  if (cmd == nullptr) {
    app_ble_reset();  // the SDK's own failure path for an unsent operation
    ESP_LOGE(TAG, "Scan start failed: kernel message allocation");
    // The created activity is intact; keep the handle so the next attempt
    // reuses it instead of orphaning the slot.
    return ScanStartResult::FAILED;
  }
  cmd->operation = GAPM_START_ACTIVITY;
  cmd->actv_idx = app_ble_env.actvs[this->scan_activity_idx_].gap_advt_idx;
  cmd->u_param.scan_param.type = GAPM_SCAN_TYPE_OBSERVER;
  cmd->u_param.scan_param.prop = GAPM_SCAN_PROP_PHY_1M_BIT | GAPM_SCAN_PROP_ACTIVE_1M_BIT;
  cmd->u_param.scan_param.scan_param_1m.scan_intv = this->requested_.interval;
  cmd->u_param.scan_param.scan_param_1m.scan_wd = this->requested_.window;
  cmd->u_param.scan_param.scan_param_coded.scan_intv = 0;
  cmd->u_param.scan_param.scan_param_coded.scan_wd = 0;
  cmd->u_param.scan_param.dup_filt_pol = 0;
  cmd->u_param.scan_param.rsvd = 0;
  cmd->u_param.scan_param.duration = 0;  // scan until stopped
  cmd->u_param.scan_param.period = 10;   // matches the SDK's passive start
  kernel_msg_send(cmd);
  this->applied_ = this->requested_;
  return ScanStartResult::PENDING;
}

}  // namespace esphome::bk72xx_ble

#endif  // BK72XX_BLE_NO_SDK
#endif  // USE_BK72XX_BLE
