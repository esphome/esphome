#ifdef USE_RP2

#include "rp2_ble_tracker.h"

#include <cinttypes>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::rp2_ble_tracker {

static const char *const TAG = "rp2_ble_tracker";

// Floor between controller start attempts; insurance against a failing
// scan_start() being retried every loop.
static constexpr uint32_t SCAN_START_RETRY_MS = 1000;

// One BLE scan unit in milliseconds; the controller programs interval/window in these units.
static constexpr float BLE_SCAN_UNIT_MS = 0.625f;

void RP2BLETracker::setup() {
  // Receive the controller's scan reports; the controller queues them from the
  // BTstack packet handler (IRQ) and delivers here on the main loop.
  this->parent_->register_scan_listener(this);
  // Merged (and unmerged) frames go to the shared dispatcher; scan_continuous_
  // is read at each delivery to decide unclaimed-device logging.
  this->merger_.bind(&this->dispatcher_, &this->scan_continuous_, TAG);
#ifdef USE_OTA_STATE_LISTENER
  // Pause scanning while an OTA update is in flight — the BLE scan competes with
  // the OTA download on the shared CYW43 radio. Mirrors esp32_ble_tracker.
  ota::get_global_ota_callback()->add_global_state_listener(this);
#endif
  // An on_boot start_scan runs before setup(); parking here would strand it.
  if (!this->scan_continuous_ && !this->scan_running_ && !this->pending_start_) {
    // Nothing to do until an external start_scan(); the loop is re-enabled there.
    this->disable_loop();
  }
}

#ifdef USE_OTA_STATE_LISTENER
void RP2BLETracker::on_ota_global_state(ota::OTAState state, float progress, uint8_t error, ota::OTAComponent *comp) {
  if (state == ota::OTA_STARTED) {
    // Set before stop_scan(): its on_scan_end automations run synchronously and
    // may call start_scan(), which must defer instead of resuming the radio.
    this->ota_in_progress_ = true;
    this->scan_continuous_before_ota_ = this->scan_continuous_;
    // A one-shot scan counts as pending when it is running, latched, or still
    // retrying its start (loop enabled); captured before stop_scan() parks it.
    this->scan_pending_before_ota_ =
        !this->scan_continuous_ && (this->scan_running_ || this->pending_start_ || this->is_in_loop_state());
    // The pause's own stop is not a user stop, so it must not clear the latches
    // captured just above.
    this->ota_pausing_ = true;
    this->stop_scan();
    this->ota_pausing_ = false;
  } else if (state == ota::OTA_ERROR || state == ota::OTA_ABORT) {
    this->ota_in_progress_ = false;
    // On success the device reboots, so restore only on a failed/aborted update;
    // loop()'s retry branch restarts the scan on its next iteration.
    if (this->scan_continuous_before_ota_) {
      this->scan_continuous_before_ota_ = false;
      this->scan_continuous_ = true;
      this->enable_loop();
    }
    // A failed OTA does not reboot, so nothing else would restart a one-shot.
    if (this->scan_pending_before_ota_) {
      this->scan_pending_before_ota_ = false;
      this->enable_loop();
    }
  }
}
#endif  // USE_OTA_STATE_LISTENER

void RP2BLETracker::loop() {
#ifdef USE_OTA_STATE_LISTENER
  // Keeps "no radio during an OTA" local instead of emergent from the
  // parking sites.
  if (this->ota_in_progress_)
    return;
#endif
  const uint32_t now = App.get_loop_component_start_time();
  if (this->pending_start_ && this->parent_->is_active()) {
    // Latched start, applied once the stack is ACTIVE; earlier attempts would
    // fail and arm the retry floor for nothing.
    this->pending_start_ = false;
    if (!this->scan_running_)
      this->start_scan_();
  }
  // Deliver held scannable advertisements whose scan response never arrived —
  // unmerged after the merger's timeout.
  if (!this->merger_.empty())
    this->merger_.sweep(now);
  if (this->scan_running_ && !this->parent_->is_active()) {
    // Stack disabled underneath us; reconcile so the retry branch takes over.
    this->scan_running_ = false;
    this->fire_scan_end_();
  }
  if (!this->scan_running_) {
    // Should be scanning but is not: continuous until the start succeeds,
    // one-shot only between start_scan() and a successful controller start.
    if (!this->parent_->is_active()) {
      // Stack not up: scan_start() cannot succeed yet.
      return;
    }
    if (now - this->last_scan_start_attempt_ >= SCAN_START_RETRY_MS) {
      this->start_scan_();
    }
    return;
  }

  if (this->scan_continuous_) {
    // Period timer: fire on_scan_end() once per scan_duration_ window, mirroring
    // esp32_ble_tracker::cleanup_scan_state_().
    if (now - this->scan_period_start_ >= this->scan_duration_) {
      this->fire_scan_end_();
      this->scan_period_start_ = now;
    }
    return;
  }

  // Non-continuous mode: run for scan_duration_ ms, then stop and fire on_scan_end.
  // Restart is driven externally (e.g. api: on_client_connected:).
  if (now - this->scan_start_time_ >= this->scan_duration_) {
    this->stop_scan_();
  }
}

void RP2BLETracker::dump_config() {
  ESP_LOGCONFIG(TAG,
                "RP2 BLE Tracker:\n"
                "  Scan Duration: %" PRIu32 " s\n"
                "  Scan Interval: %.0f ms (%" PRIu32 " BLE units)\n"
                "  Scan Window: %.0f ms (%" PRIu32 " BLE units)\n"
                "  Scan Type: %s\n"
                "  Continuous Scanning: %s",
                this->scan_duration_ / 1000, this->scan_interval_ * BLE_SCAN_UNIT_MS, this->scan_interval_,
                this->scan_window_ * BLE_SCAN_UNIT_MS, this->scan_window_,
                this->scan_active_ ? LOG_STR_LITERAL("ACTIVE") : LOG_STR_LITERAL("PASSIVE"),
                YESNO(this->scan_continuous_));
}

// Core spec advertising report event types (BTstack headers stay out of this
// TU). ADV_IND and ADV_SCAN_IND are the scannable ones.
static constexpr uint8_t ADV_EVENT_TYPE_ADV_IND = 0;
static constexpr uint8_t ADV_EVENT_TYPE_ADV_SCAN_IND = 2;
static constexpr uint8_t ADV_EVENT_TYPE_SCAN_RSP = 4;

// BTstack delivers the pair as separate reports; the merger holds a scannable
// advertisement until its response arrives.
void RP2BLETracker::on_scan_report(const rp2040_ble::BLEScanReport &report) {
  if (report.adv_event_type == ADV_EVENT_TYPE_SCAN_RSP) {
    this->merger_.submit_scan_rsp(report.mac, report.rssi, report.addr_type, report.data, report.data_len);
    return;
  }
  // Only while an active scan runs: nothing sweeps the merger after a stop.
  if (this->scan_running_ && this->scan_active_ &&
      (report.adv_event_type == ADV_EVENT_TYPE_ADV_IND || report.adv_event_type == ADV_EVENT_TYPE_ADV_SCAN_IND)) {
    this->merger_.stash_adv(report.mac, report.rssi, report.addr_type, report.data, report.data_len,
                            App.get_loop_component_start_time());
    return;
  }
  this->dispatcher_.dispatch(report.mac, report.rssi, report.addr_type, report.data, report.data_len,
                             /*raw_only=*/false, this->scan_continuous_ ? nullptr : TAG);
}

void RP2BLETracker::start_scan() {
  // Mirrors esp32_ble_tracker::start_scan(): caller sets scan_continuous_ via
  // set_scan_continuous() first, then calls start_scan() to begin scanning.
#ifdef USE_OTA_STATE_LISTENER
  if (this->ota_in_progress_) {
    // Defer to the post-OTA resume path, carrying the requested mode. Not
    // while ota_pausing_: scan_continuous_ is an artefact of the pause's own
    // stop there, not intent.
    if (!this->ota_pausing_) {
      this->scan_continuous_before_ota_ = this->scan_continuous_;
      this->scan_pending_before_ota_ = !this->scan_continuous_;
    }
    return;
  }
#endif
  this->enable_loop();
  if (!this->is_ready() || !this->parent_->is_active()) {
    // Pre-setup or stack not ACTIVE: latch, loop() applies it.
    this->pending_start_ = true;
    return;
  }
  // bk72xx force semantics: a user start jumps the floor only while the
  // controller is healthy. loop()'s retry branch picks the request up.
  if (this->last_start_failed_ &&
      App.get_loop_component_start_time() - this->last_scan_start_attempt_ < SCAN_START_RETRY_MS) {
    return;
  }
  this->start_scan_();
}

void RP2BLETracker::restart_scan_duration() {
  if (!this->scan_running_)
    return;  // start_scan_() anchors the clock itself on the next real start
  // One-shot clock only (bk72xx parity); re-anchoring the period would let
  // repeated actions starve on_scan_end. Same clock as loop()'s now.
  this->scan_start_time_ = App.get_loop_component_start_time();
}

bool RP2BLETracker::request_scan_mode(bool active) {
  if (this->scan_active_ == active)
    return true;
  this->scan_active_ = active;
  // V: the proxy's "Setting scanner mode" line already narrates this at D.
  ESP_LOGV(TAG, "Scan mode %s", active ? "active" : "passive");
  // Restart the controller scan only: the scan logically continues, so no
  // on_scan_end and no period reset. An idle scanner applies it on next start.
  if (this->scan_running_) {
    this->parent_->scan_stop();
    if (!this->controller_scan_start_()) {
      // The controller really stopped: behave exactly like loop()'s
      // reconciliation branch - notify listeners and let its retry recover.
      this->scan_running_ = false;
      this->fire_scan_end_();
    }
  }
  return true;
}

void RP2BLETracker::stop_scan() {
  // Cancel a start latched before setup(); without this an on_boot
  // start_scan/stop_scan pair would still start at the first loop().
  this->pending_start_ = false;
  this->scan_continuous_ = false;
#ifdef USE_OTA_STATE_LISTENER
  // A user stop during the OTA is the latest intent; the pause's own stop
  // (ota_pausing_) is exempt - it armed that state.
  if (this->ota_in_progress_ && !this->ota_pausing_) {
    this->scan_pending_before_ota_ = false;
    this->scan_continuous_before_ota_ = false;
  }
#endif
  this->stop_scan_();
  // stop_scan_() early-returns when idle, so park here too - once set up, and
  // re-checked: its synchronous on_scan_end may have restarted the scan.
  if (this->is_ready() && !this->scan_running_ && !this->pending_start_) {
    this->disable_loop();
  }
}

// Stamp-and-start for every controller scan attempt: the stamp keeps the
// SCAN_START_RETRY_MS floor covering all callers, not only loop()'s retry.
bool RP2BLETracker::controller_scan_start_() {
  this->last_scan_start_attempt_ = App.get_loop_component_start_time();
  const bool ok = this->parent_->scan_start(static_cast<uint16_t>(this->scan_interval_),
                                            static_cast<uint16_t>(this->scan_window_), this->scan_active_);
  this->last_start_failed_ = !ok;
  return ok;
}

void RP2BLETracker::start_scan_() {
  if (this->scan_running_)
    return;

  if (!this->controller_scan_start_())
    return;

  this->scan_running_ = true;
  // Symmetric with stop_scan_()'s stop log; asymmetry would read as the
  // scanner failing to come back.
  ESP_LOGD(TAG, "Scan started (%s, window=%.0fms, interval=%.0fms)",
           this->scan_active_ ? LOG_STR_LITERAL("active") : LOG_STR_LITERAL("passive"),
           this->scan_window_ * BLE_SCAN_UNIT_MS, this->scan_interval_ * BLE_SCAN_UNIT_MS);
  // Anchor the period to the scan, not to boot, so a restart after a long gap
  // does not fire on_scan_end immediately. Same clock as loop()'s now.
  this->scan_period_start_ = App.get_loop_component_start_time();
  this->scan_start_time_ = this->scan_period_start_;
}

void RP2BLETracker::stop_scan_() {
  if (!this->scan_running_)
    return;
  this->parent_->scan_stop();
  this->scan_running_ = false;
  ESP_LOGD(TAG, "Scan stopped");
  this->fire_scan_end_();
  // Reset the period clock so on_scan_end does not double-fire; same clock as loop().
  this->scan_period_start_ = App.get_loop_component_start_time();
  // on_scan_end runs synchronously and may restart the scan; re-check before
  // parking or that scan runs untimed.
  if (!this->scan_continuous_ && !this->scan_running_ && !this->pending_start_) {
    // Nothing left to time; start_scan() re-enables the loop.
    this->disable_loop();
  }
}

void RP2BLETracker::fire_scan_end_() {
  // Deliver held advertisements whose scan response never came (unmerged)
  // BEFORE on_scan_end fires.
  this->merger_.flush();
  this->dispatcher_.on_scan_end();
}

}  // namespace esphome::rp2_ble_tracker

#endif  // USE_RP2
