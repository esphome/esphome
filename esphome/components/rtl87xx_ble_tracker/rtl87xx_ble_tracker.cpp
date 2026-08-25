#include "rtl87xx_ble_tracker.h"

#ifdef USE_LIBRETINY

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::rtl87xx_ble_tracker {

static const char *const TAG = "rtl87xx_ble_tracker";

// The GAP callback is a plain C function pointer with no user argument, so it
// reaches the (single) tracker instance through a file-static pointer.
static RTL87xxBLETracker *s_tracker = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void rtl_ble_adv_cb(const uint8_t bd_addr[MAC_ADDRESS_SIZE], uint8_t addr_type, uint8_t adv_type, int8_t rssi,
                           const uint8_t *data, uint8_t len) {
  if (s_tracker != nullptr)
    s_tracker->on_raw_report(bd_addr, addr_type, adv_type, rssi, data, len);
}

float RTL87xxBLETracker::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

void RTL87xxBLETracker::setup() {
  s_tracker = this;
  this->merger_.bind(&this->dispatcher_, &this->scan_continuous_, TAG);
  this->parent_->set_adv_callback(rtl_ble_adv_cb);
  this->setup_time_ = App.get_loop_component_start_time();
  this->boot_time_ = this->setup_time_;
#ifdef USE_OTA_STATE_LISTENER
  // Pause scanning while an OTA update is in flight — WiFi and BLE share the
  // radio, so a live scan contends with the transfer. Mirrors esp32/bk72xx.
  ota::get_global_ota_callback()->add_global_state_listener(this);
#endif
  // Nothing to time until an explicit start_scan() (bk72xx parity);
  // scan_requested_ covers an on_boot start latched before setup().
  if (!this->scan_continuous_ && !this->scan_requested_) {
    this->disable_loop();
  }
}

#ifdef USE_OTA_STATE_LISTENER
void RTL87xxBLETracker::on_ota_global_state(ota::OTAState state, float progress, uint8_t error,
                                            ota::OTAComponent *comp) {
  if (state == ota::OTA_STARTED) {
    this->scan_continuous_before_ota_ = this->scan_continuous_;
    // A one-shot scan counts as pending when it is running or still latched.
    this->scan_pending_before_ota_ = !this->scan_continuous_ && (this->scan_running_ || this->scan_requested_);
    this->stop_scan();  // synchronous le_scan_stop; also clears scan_continuous_
    // The main loop is starved during the transfer, so loop()'s stop_pending_
    // retry cannot run: drain a refused stop inline, bounded, before the
    // update owns the shared radio.
    for (uint8_t i = 0; this->stop_pending_ && i < OTA_STOP_RETRIES; i++) {
      delay(OTA_STOP_RETRY_DELAY_MS);
      this->parent_->scan_stop();
      if (!this->parent_->scan_running()) {
        this->finish_stop_(App.get_loop_component_start_time());
      }
    }
    if (this->stop_pending_) {
      // The radio will contend with the OTA; nothing more can be done here.
      ESP_LOGE(TAG, "Scan still stopping at OTA start");
    }
  } else if (state == ota::OTA_ERROR || state == ota::OTA_ABORT) {
    // On success the device reboots, so restore only on a failed/aborted
    // update; loop()'s reconciler re-establishes the scan on its next pass.
    if (this->scan_continuous_before_ota_) {
      this->scan_continuous_before_ota_ = false;
      this->scan_continuous_ = true;
      this->enable_loop();  // stop_scan() parked it at OTA start
    }
    // A one-shot scan interrupted by the OTA resumes for a fresh duration —
    // an OTA failure does not reboot, so nothing external would restart it.
    if (this->scan_pending_before_ota_) {
      this->scan_pending_before_ota_ = false;
      this->scan_requested_ = true;
      this->enable_loop();
    }
  }
}
#endif  // USE_OTA_STATE_LISTENER

void RTL87xxBLETracker::on_raw_report(const uint8_t mac[MAC_ADDRESS_SIZE], uint8_t addr_type, uint8_t adv_type,
                                      int8_t rssi, const uint8_t *data, uint8_t len) {
  const uint16_t head = this->ring_head_.load(std::memory_order_relaxed);
  const uint16_t next = (head + 1) & (REPORT_RING_SIZE - 1);
  if (next == this->ring_tail_.load(std::memory_order_acquire)) {
    this->dropped_.fetch_add(1, std::memory_order_relaxed);  // consumer behind; drop
    return;
  }
  RawReport &slot = this->ring_[head];
  memcpy(slot.mac, mac, MAC_ADDRESS_SIZE);
  slot.addr_type = addr_type;
  slot.adv_type = adv_type;
  slot.rssi = rssi;
  slot.len = len > sizeof(slot.data) ? static_cast<uint8_t>(sizeof(slot.data)) : len;
  if (data != nullptr && slot.len != 0)
    memcpy(slot.data, data, slot.len);
  this->ring_head_.store(next, std::memory_order_release);
}

void RTL87xxBLETracker::apply_scan_params_() {
  this->parent_->set_scan_params(static_cast<uint16_t>(this->scan_interval_), static_cast<uint16_t>(this->scan_window_),
                                 this->scan_active_);
}

void RTL87xxBLETracker::loop() {
  const uint32_t now_ms = App.get_loop_component_start_time();

  // Reconcile toward the requested state every loop: le_scan_start() can fail
  // transiently after a stop, and the controller can drop a scan under
  // coexistence. Retries are rate-limited.
  if (this->parent_->stack_ready()) {
    if (!this->ble_started_) {
      this->ble_started_ = true;
      this->apply_scan_params_();
      if (this->ready_timeout_logged_)  // late bring-up after the timeout fired
        this->status_clear_warning();
    }
    // A stop the controller refused earlier: re-issue until it takes, then
    // finish what stop_scan() could not (mark ended, park when idle). Rate
    // limited like every other retry here, so a wedged controller does not
    // flood the log with per-pass WARNs.
    if (this->stop_pending_ && now_ms - this->last_stop_attempt_ >= SCAN_RETRY_MS) {
      this->last_stop_attempt_ = now_ms;
      this->parent_->scan_stop();
      if (!this->parent_->scan_running()) {
        this->finish_stop_(now_ms);
      }
    }
    // scan_running() reads the GAP stack's own state, so a controller-side drop
    // is visible even though the local flag still says running. The grace window
    // covers the asynchronous IDLE -> START -> SCANNING transition, and polling
    // at the same cadence loses nothing.
    if (this->scan_running_ && now_ms - this->scan_start_time_ >= SCAN_RETRY_MS &&
        now_ms - this->last_drop_poll_ >= SCAN_RETRY_MS) {
      this->last_drop_poll_ = now_ms;
      if (!this->parent_->scan_running()) {
        ESP_LOGW(TAG, "Controller dropped the scan; re-establishing");
        if (!this->scan_continuous_)
          this->scan_requested_ = true;  // a one-shot must resume too
        this->count_failed_start_();     // drops share the backoff with failed starts
        this->mark_scan_ended_(now_ms);
      }
    }
    // A stable run proves the controller healthy; clear the failure streak.
    if (this->scan_running_ && this->failed_start_count_ != 0 &&
        now_ms - this->scan_start_time_ >= SCAN_STABLE_RESET_MS) {
      this->failed_start_count_ = 0;
    }
    if ((this->scan_continuous_ || this->scan_requested_) && !this->scan_running_ &&
        now_ms - this->last_start_attempt_ > (SCAN_RETRY_MS << this->failed_start_count_)) {
      this->start_scan();
    }
  } else if (this->parent_->waiting_for_network()) {
    // Bring-up waits for the STA, so that wait must not count against the
    // ready timeout. It has no timeout of its own (AP-only never ends it),
    // hence the one-shot notice.
    if (!this->network_wait_logged_ && now_ms - this->boot_time_ >= STACK_READY_TIMEOUT_MS) {
      this->network_wait_logged_ = true;
      ESP_LOGW(TAG, "BLE bring-up waiting for the WiFi station (coexistence requirement); scanning starts on connect");
    }
    this->setup_time_ = now_ms;
  } else if (!this->ready_timeout_logged_ && now_ms - this->setup_time_ >= STACK_READY_TIMEOUT_MS) {
    // bte_init cannot be re-entered, so a wedged init is terminal for this
    // boot; surface it in the device status rather than just missing data.
    this->ready_timeout_logged_ = true;
    this->status_set_warning("BLE stack failed to initialize; scanning unavailable until reboot");
    ESP_LOGE(TAG, "BLE stack not ready after %" PRIu32 " s; scanning unavailable until reboot",
             STACK_READY_TIMEOUT_MS / 1000);
  }

  // Drain the cross-task ring on the main loop and dispatch.
  uint16_t tail = this->ring_tail_.load(std::memory_order_relaxed);
  while (tail != this->ring_head_.load(std::memory_order_acquire)) {
    const RawReport &r = this->ring_[tail];
    // adv_type demuxes advertisements from scan responses (rp2 parity); the
    // merger holds a scannable advertisement until its response arrives. Only
    // while an active scan runs: nothing sweeps the merger after a stop.
    if (r.adv_type == rtl87xx_ble::ADV_EVENT_TYPE_SCAN_RSP) {
      this->merger_.submit_scan_rsp(r.mac, r.rssi, r.addr_type, r.data, r.len);
    } else if (this->scan_running_ && this->scan_active_ &&
               (r.adv_type == rtl87xx_ble::ADV_EVENT_TYPE_ADV_IND ||
                r.adv_type == rtl87xx_ble::ADV_EVENT_TYPE_ADV_SCAN_IND)) {
      this->merger_.stash_adv(r.mac, r.rssi, r.addr_type, r.data, r.len, now_ms);
    } else {
      this->dispatcher_.dispatch(r.mac, r.rssi, r.addr_type, r.data, r.len, /*raw_only=*/false,
                                 this->scan_continuous_ ? nullptr : TAG);
    }
    tail = (tail + 1) & (REPORT_RING_SIZE - 1);
    this->ring_tail_.store(tail, std::memory_order_release);
  }

  // Duration timers (bk72xx/esp32 parity): continuous fires on_scan_end once
  // per window without stopping; one-shot stops after it. Gated on
  // scan_started_once_ so a scan that never came up stays silent.
  if (this->scan_started_once_) {
    if (this->scan_continuous_) {
      if (now_ms - this->scan_period_start_ >= this->scan_duration_) {
        // The radio keeps scanning across a period boundary, so the running
        // flag stays set; a stop_scan from on_scan_end must still reach the
        // controller.
        this->fire_scan_end_();
        this->scan_period_start_ = now_ms;
      }
    } else if (this->scan_running_ && now_ms - this->scan_start_time_ >= this->scan_duration_) {
      // A full-duration run proves the controller healthy even when duration is
      // shorter than SCAN_STABLE_RESET_MS (bk72xx parity).
      this->failed_start_count_ = 0;
      this->stop_scan();
    }
  }

  if (!this->merger_.empty())
    this->merger_.sweep(now_ms);

  // Ring-overflow accounting, both scan modes, rate-limited to the period.
  if (now_ms - this->last_drop_report_ >= this->scan_duration_) {
    this->last_drop_report_ = now_ms;
    const uint32_t dropped = this->dropped_.exchange(0, std::memory_order_relaxed);
    if (dropped != 0)
      ESP_LOGW(TAG, "%" PRIu32 " reports dropped (ring full)", dropped);
  }
}

void RTL87xxBLETracker::start_scan() {
  this->enable_loop();
  if (this->stop_pending_) {
    this->scan_requested_ = true;  // restart once the refused stop takes
    return;
  }
  if (this->scan_running_)
    return;
  // bk72xx force semantics: a user start jumps the floor only while the
  // controller is healthy; the reconciler picks it up once the backoff ends.
  if (this->failed_start_count_ != 0 &&
      App.get_loop_component_start_time() - this->last_start_attempt_ < (SCAN_RETRY_MS << this->failed_start_count_)) {
    this->scan_requested_ = true;
    return;
  }
  if (!this->parent_->stack_ready()) {
    // Latch the request (an on_boot start_scan runs before the async stack is
    // up); loop()'s reconciler drives the retry for one-shot scans too.
    this->scan_requested_ = true;
    return;
  }
  // Stamped here, at the real attempt: the reconciler must not stamp before
  // calling in, or the floor above would see zero elapsed and never attempt.
  this->last_start_attempt_ = App.get_loop_component_start_time();
  this->apply_scan_params_();
  this->scan_running_ = this->parent_->scan_start();
  if (this->scan_running_) {
    // Both edges are logged (bk72xx/rp2 parity): the reconciler can restart
    // scans on its own, so a silent start reads as the scanner never running.
    ESP_LOGD(TAG, "Scan started (%s, interval %" PRIu32 " ms, window %" PRIu32 " ms)",
             this->scan_active_ ? "active" : "passive", this->scan_interval_, this->scan_window_);
  }
  if (!this->scan_running_) {
    ESP_LOGW(TAG, "BLE scan start failed");
    this->scan_requested_ = true;  // reconciler retries with backoff
    this->count_failed_start_();
    return;
  }
  this->scan_requested_ = false;
  const uint32_t now = App.get_loop_component_start_time();
  this->scan_start_time_ = now;
  // Anchor the period to every successful start, so resuming after a long gap
  // does not fire on_scan_end before an advertisement can arrive.
  this->scan_period_start_ = now;
  this->scan_started_once_ = true;
}

void RTL87xxBLETracker::count_failed_start_() {
  if (this->failed_start_count_ < SCAN_START_RETRY_MAX_DOUBLINGS) {
    ++this->failed_start_count_;
    if (this->failed_start_count_ == SCAN_START_RETRY_MAX_DOUBLINGS) {
      ESP_LOGW(TAG, "Scan start keeps failing; retrying every %" PRIu32 " s",
               (SCAN_RETRY_MS << SCAN_START_RETRY_MAX_DOUBLINGS) / 1000);
    }
  }
}

// Deliver held frames before the marker; no state change, so the continuous
// period boundary can use it while the radio keeps scanning.
void RTL87xxBLETracker::fire_scan_end_() {
  this->merger_.flush();
  this->dispatcher_.on_scan_end();
}

// The scan actually stopped: re-anchoring here stops the duration timer firing
// a second on_scan_end in the same iteration (bk72xx parity).
void RTL87xxBLETracker::mark_scan_ended_(uint32_t now) {
  ESP_LOGD(TAG, "Scan ended");
  this->scan_running_ = false;
  this->fire_scan_end_();
  this->scan_period_start_ = now;
}

void RTL87xxBLETracker::restart_scan_duration() { this->scan_start_time_ = App.get_loop_component_start_time(); }

void RTL87xxBLETracker::stop_scan() {
  this->scan_continuous_ = false;
  this->scan_requested_ = false;  // also cancels a start latched before ready
  if (this->stop_pending_)
    return;                    // loop() is already re-issuing the stop, rate-limited
  this->parent_->scan_stop();  // idempotent; also clears a controller-side scan
  if (this->scan_running_ && this->parent_->scan_running()) {
    // The controller refused the stop (its scan_stop() keeps the flag set for
    // exactly this): the radio is still scanning. Stay running with the loop
    // alive and draining, and let loop() re-issue the stop until it takes,
    // instead of both layers silently disagreeing.
    ESP_LOGW(TAG, "Stop refused by controller; retrying");
    this->stop_pending_ = true;
    this->last_stop_attempt_ = App.get_loop_component_start_time();
    return;
  }
  this->finish_stop_(App.get_loop_component_start_time());
}

// The common tail of every stop path (direct, loop()-retried, OTA drain), so
// they all converge on the same end state.
void RTL87xxBLETracker::finish_stop_(uint32_t now) {
  this->stop_pending_ = false;
  if (this->scan_running_) {
    this->mark_scan_ended_(now);
  }
  // Discard anything still in the ring: with the loop parked nothing drains
  // it, and the next start would dispatch stale pre-stop reports.
  this->ring_tail_.store(this->ring_head_.load(std::memory_order_acquire), std::memory_order_release);
  // Park once nothing is left to time; re-checked because the synchronous
  // on_scan_end above may have restarted the scan.
  if (this->is_ready() && !this->scan_running_ && !this->scan_requested_ && !this->scan_continuous_) {
    this->disable_loop();
  }
}

bool RTL87xxBLETracker::request_scan_mode(bool active) {
  if (this->scan_active_ == active)
    return true;
  this->scan_active_ = active;
  this->apply_scan_params_();
  // Restart inline so the scan stays logically running across the switch
  // (sibling parity: bluetooth_proxy polls scan_running() and would publish a
  // spurious IDLE frame otherwise). A transient le_scan_start() failure right
  // after the stop falls back to the reconciler.
  if (this->scan_running_) {
    this->parent_->scan_stop();
    if (this->parent_->scan_running()) {
      // The controller refused the stop: the radio kept the old parameters, so
      // report the real, unchanged mode (false = cannot honor, state untouched)
      // instead of letting the proxy publish a mode the radio is not in.
      this->scan_active_ = !active;
      this->apply_scan_params_();  // restore the stored parameters to match
      return false;
    }
    const uint32_t now = App.get_loop_component_start_time();
    this->last_start_attempt_ = now;
    if (this->parent_->scan_start()) {
      this->scan_start_time_ = now;  // fresh grace window; period clock untouched
    } else {
      // Latch before mark_scan_ended_(): its synchronous on_scan_end may call
      // stop_scan(), and re-arming afterwards would resurrect the scan that
      // handler just cancelled (the drop detector latches in this order too).
      if (!this->scan_continuous_)
        this->scan_requested_ = true;  // a mode switch must not kill a one-shot scan
      this->mark_scan_ended_(now);
      this->last_start_attempt_ = 0;  // let the reconciler retry immediately
    }
  }
  return true;
}

void RTL87xxBLETracker::get_adapter_mac(uint8_t out[MAC_ADDRESS_SIZE]) { this->parent_->get_mac_msb_first(out); }

void RTL87xxBLETracker::dump_config() {
  ESP_LOGCONFIG(TAG,
                "RTL87xx BLE Tracker:\n"
                "  Scan duration: %" PRIu32 " s\n"
                "  Scan interval: %" PRIu32 " ms\n"
                "  Scan window: %" PRIu32 " ms\n"
                "  Active: %s (configured %s), continuous: %s",
                this->scan_duration_ / 1000, this->scan_interval_, this->scan_window_, YESNO(this->scan_active_),
                YESNO(this->scan_active_configured_), YESNO(this->scan_continuous_));
}

}  // namespace esphome::rtl87xx_ble_tracker

#endif  // USE_LIBRETINY
