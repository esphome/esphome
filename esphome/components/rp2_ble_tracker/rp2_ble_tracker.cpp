#ifdef USE_RP2

#include "rp2_ble_tracker.h"

#include <cinttypes>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::rp2_ble_tracker {

static const char *const TAG = "rp2_ble_tracker";

// Minimum interval between scan start attempts on an active stack. The
// controller start has no failure mode once HCI is WORKING, so this fires at
// most once per enable cycle today; the floor is insurance against a future
// scan_start() failure being retried every main-loop iteration.
static constexpr uint32_t SCAN_START_RETRY_MS = 1000;

// One BLE scan unit in milliseconds; the controller programs interval/window in these units.
static constexpr float BLE_SCAN_UNIT_MS = 0.625f;

void RP2BLETracker::setup() {
  // Receive the controller's scan reports; the controller queues them from the
  // BTstack packet handler (IRQ) and delivers here on the main loop.
  this->parent_->register_scan_listener(this);
#ifdef USE_OTA_STATE_LISTENER
  // Pause scanning while an OTA update is in flight — the BLE scan competes with
  // the OTA download on the shared CYW43 radio. Mirrors esp32_ble_tracker.
  ota::get_global_ota_callback()->add_global_state_listener(this);
#endif
  if (!this->scan_continuous_) {
    // Nothing to do until an external start_scan(); the loop is re-enabled there.
    this->disable_loop();
  }
}

#ifdef USE_OTA_STATE_LISTENER
void RP2BLETracker::on_ota_global_state(ota::OTAState state, float progress, uint8_t error, ota::OTAComponent *comp) {
  if (state == ota::OTA_STARTED) {
    this->scan_continuous_before_ota_ = this->scan_continuous_;
    // A one-shot scan counts as pending when it is running or still retrying
    // its start (loop enabled); captured before stop_scan() disables the loop.
    this->scan_pending_before_ota_ = !this->scan_continuous_ && (this->scan_running_ || this->is_in_loop_state());
    this->stop_scan();
  } else if (state == ota::OTA_ERROR || state == ota::OTA_ABORT) {
    // On success the device reboots, so restore only on a failed/aborted update;
    // loop()'s retry branch restarts the scan on its next iteration.
    if (this->scan_continuous_before_ota_) {
      this->scan_continuous_before_ota_ = false;
      this->scan_continuous_ = true;
      this->enable_loop();
    }
    // A one-shot scan interrupted by the OTA resumes for a fresh duration
    // rather than silently staying idle — an OTA failure does not reboot, so
    // nothing external would restart it.
    if (this->scan_pending_before_ota_) {
      this->scan_pending_before_ota_ = false;
      this->enable_loop();
    }
  }
}
#endif  // USE_OTA_STATE_LISTENER

void RP2BLETracker::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  if (this->scan_running_ && !this->parent_->is_active()) {
    // The controller was disabled underneath us (e.g. a lambda calling
    // rp2040_ble's disable()); the scan died with the stack. Reconcile so the
    // retry branch below takes over once the user re-enables the stack.
    this->scan_running_ = false;
    this->fire_scan_end_();
  }
  if (!this->scan_running_) {
    // A scan should be running but is not: continuous mode is always in this
    // state until the start succeeds, and non-continuous mode only reaches
    // here between start_scan() and a successful controller start, because
    // stop_scan_() disables the loop otherwise.
    if (!this->parent_->is_active()) {
      // Stack not up (still booting, or the user called disable()) —
      // scan_start() cannot succeed, so there is nothing to attempt; scanning
      // starts on the first iteration after HCI reaches WORKING.
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
  if (now - this->scan_period_start_ >= this->scan_duration_) {
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

void RP2BLETracker::on_scan_report(const rp2040_ble::BLEScanReport &report) {
  // Raw callback (the raw-advertisement path).
  if (this->raw_advertisement_callback_.is_set()) {
    const ble_device_base::RawAdvertisement adv{.mac = report.mac,
                                                .data = report.data,
                                                .data_len = report.data_len,
                                                .rssi = report.rssi,
                                                .addr_type = report.addr_type};
    this->raw_advertisement_callback_.invoke(adv);
  }

#ifdef ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
  ble_device_base::ESPBTDevice device;
  device.from_scan_result(report.mac, report.rssi, report.addr_type, report.data, report.data_len);
  bool found = false;
  for (auto *listener : this->listeners_) {
    if (listener->parse_device(device))
      found = true;
  }
  // Mirror esp32_ble_tracker: log a newly-seen device only when nothing claimed
  // it and the scan is one-shot (continuous scans would spam).
  if (!found && !this->scan_continuous_)
    this->discovered_log_.log_device(TAG, device);
#endif  // ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
}

void RP2BLETracker::start_scan() {
  // Mirrors esp32_ble_tracker::start_scan(): caller sets scan_continuous_ via
  // set_scan_continuous() first, then calls start_scan() to begin scanning.
  this->enable_loop();
  this->start_scan_();
}

void RP2BLETracker::stop_scan() {
  this->scan_continuous_ = false;
  this->stop_scan_();
  // stop_scan_() early-returns when no scan is running, so disable the loop
  // here too: a scan that never came up (stack still powering on at OTA start)
  // must not keep attempting scan_start() from the loop's retry branch.
  this->disable_loop();
}

void RP2BLETracker::start_scan_() {
  if (this->scan_running_)
    return;

  // Stamp every attempt regardless of caller so the loop's rate limit also
  // covers a failed start that came through the public start_scan().
  this->last_scan_start_attempt_ = App.get_loop_component_start_time();

  if (!this->parent_->scan_start(static_cast<uint16_t>(this->scan_interval_), static_cast<uint16_t>(this->scan_window_),
                                 this->scan_active_))
    return;

  this->scan_running_ = true;
  // Log every explicit start at DEBUG — stop_scan_() logs every stop at DEBUG, and
  // in non-continuous mode each period is an explicit start, so asymmetric logging
  // would read as the scanner failing to come back up.
  ESP_LOGD(TAG, "Scan started (%s, window=%.0fms, interval=%.0fms)",
           this->scan_active_ ? LOG_STR_LITERAL("active") : LOG_STR_LITERAL("passive"),
           this->scan_window_ * BLE_SCAN_UNIT_MS, this->scan_interval_ * BLE_SCAN_UNIT_MS);
  // Re-anchor the scan period to every successful start — first start (so the
  // period counts from the scan, not from boot) and every restart after a stop (so
  // resuming after longer than scan_duration, e.g. a failed OTA restoring continuous
  // mode 10 minutes later, does not fire on_scan_end before an advertisement can
  // arrive). Same clock as loop()'s `now`: a fresh millis() here would be ahead of
  // the cached loop time and make the same-iteration period check underflow.
  this->scan_period_start_ = App.get_loop_component_start_time();
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
  if (!this->scan_continuous_) {
    // Nothing left to time; start_scan() re-enables the loop.
    this->disable_loop();
  }
}

void RP2BLETracker::fire_scan_end_() {
#ifdef ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
  for (auto *listener : this->listeners_)
    listener->on_scan_end();
  this->discovered_log_.clear();  // reset per-scan "Found device" dedup (esp32_ble_tracker parity)
#endif
}

}  // namespace esphome::rp2_ble_tracker

#endif  // USE_RP2
