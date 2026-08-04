// bk72xx_ble_tracker.cpp
//
// BLE scan policy for the BK72xx BLE-5.x chips: parameters, duration/period
// timers and the rate-limited start retry. All controller access (stack
// bring-up, scan primitives, the BLE-task → main-task report queue) goes
// through the bk72xx_ble component — no SDK calls and no cross-task state here.

#ifdef USE_LIBRETINY

#include "bk72xx_ble_tracker.h"

#include <algorithm>
#include <cinttypes>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::bk72xx_ble_tracker {

static const char *const TAG = "bk72xx_ble_tracker";

// Minimum interval between scan (re)start attempts, so a failing controller start
// cannot be retried every main-loop iteration (single-core CPU starvation). The
// interval doubles with consecutive failed starts (1 s up to 64 s) so a controller
// that never comes up — the controller logs each failure at ERROR — settles into a
// slow, quiet poll instead of an error line every second for the rest of uptime;
// a single WARN is emitted when the retry interval first saturates.
static constexpr uint32_t SCAN_START_RETRY_MS = 1000;
static constexpr uint8_t SCAN_START_RETRY_MAX_DOUBLINGS = 6;  // 1 s << 6 = 64 s

// ---------------------------------------------------------------------------
// Component lifecycle
// ---------------------------------------------------------------------------

void BK72xxBLETracker::setup() {
  // Receive the controller's scan reports; the controller queues them from the
  // BLE task and delivers here on the main task.
  this->parent_->register_scan_listener(this);
#ifdef USE_OTA_STATE_LISTENER
  // Pause scanning while an OTA update is in flight — on the single-core BK72xx the
  // BLE scan competes with the OTA flash writes. Mirrors esp32_ble_tracker.
  ota::get_global_ota_callback()->add_global_state_listener(this);
#endif
}

#ifdef USE_OTA_STATE_LISTENER
void BK72xxBLETracker::on_ota_global_state(ota::OTAState state, float progress, uint8_t error,
                                           ota::OTAComponent *comp) {
  if (state == ota::OTA_STARTED) {
    this->scan_continuous_before_ota_ = this->scan_continuous_;
    this->scan_requested_before_ota_ = this->scan_requested_;
    this->stop_scan();
  } else if (state == ota::OTA_ERROR || state == ota::OTA_ABORT) {
    // On success the device reboots, so restore only on a failed/aborted update;
    // loop() restarts the scan on its next iteration (continuous idle branch).
    if (this->scan_continuous_before_ota_) {
      this->scan_continuous_before_ota_ = false;
      this->scan_continuous_ = true;
    }
    // A one-shot request that was still pending (latched, retrying) when the
    // OTA paused scanning is re-latched, not dropped — loop() resumes the retry.
    if (this->scan_requested_before_ota_) {
      this->scan_requested_before_ota_ = false;
      this->scan_requested_ = true;
    }
  }
}
#endif  // USE_OTA_STATE_LISTENER

void BK72xxBLETracker::loop() {
  const uint32_t now = millis();
  if (this->scan_continuous_) {
    if (!this->scan_running_) {
      // A start that succeeded re-anchored the period timer from a later millis(),
      // so the stale `now` below would underflow the comparison and fire
      // on_scan_end() for a scan that just began. Resume next iteration.
      if (this->try_start_with_backoff_(now))
        return;
    }
    // Period timer: fire on_scan_end() once per scan_duration_ window, mirroring
    // esp32_ble_tracker::cleanup_scan_state_(). Gated on scan_started_once_ so a scan
    // that never came up (start kept failing) does not fire spurious on_scan_end events.
    if (this->scan_started_once_ && now - this->scan_period_start_ >= this->scan_duration_) {
#ifdef ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
      for (auto *listener : this->listeners_)
        listener->on_scan_end();
      this->discovered_log_.clear();  // reset per-scan "Found device" dedup (esp32_ble_tracker parity)
#endif
      this->scan_period_start_ = now;
    }
    return;
  }

  // Non-continuous mode: run for scan_duration_ ms, then stop and fire on_scan_end.
  // Restart is driven externally (e.g. api: on_client_connected:).
  //
  // A requested start that failed (same controller failures the continuous branch
  // absorbs) is retried with the same backoff — otherwise a failed one-shot start
  // would be silent: the scan never runs, stop_scan_() is never reached and
  // on_scan_end() never fires, leaving period-keyed consumers waiting forever.
  if (this->scan_requested_ && !this->scan_running_) {
    // Same stale-`now` hazard as the continuous branch: start_scan_() stamps
    // scan_start_time_ from a later millis(), so the duration check below would
    // underflow and stop the scan in the iteration that started it.
    if (this->try_start_with_backoff_(now))
      return;
  }
  if (this->scan_running_ && now - this->scan_start_time_ >= this->scan_duration_) {
    this->stop_scan_();
  }
}

bool BK72xxBLETracker::try_start_with_backoff_(uint32_t now, bool force) {
  // Rate-limit (re)start attempts. The controller start can fail (no idle activity
  // handle, WiFi/BLE coexistence) and leave scan_running_ false; retrying every
  // main-loop iteration would spin the single-core CPU and starve WiFi (device
  // becomes unresponsive). The interval backs off with consecutive failures so a
  // controller that never comes up polls slowly and quietly.
  //
  // force bypasses the gate for an explicit user start (start_scan()) — but
  // only while the failure streak is clean. Once the controller is failing,
  // even user-initiated attempts respect the backoff, so a start_scan() action
  // on a short cadence cannot hammer a failing controller; the attempt stays
  // inside the failure accounting below either way.
  const uint8_t doublings = std::min<uint8_t>(this->failed_start_count_, SCAN_START_RETRY_MAX_DOUBLINGS);
  if ((!force || this->failed_start_count_ != 0) &&
      now - this->last_scan_start_attempt_ < (SCAN_START_RETRY_MS << doublings))
    return false;
  this->last_scan_start_attempt_ = now;
  this->start_scan_();
  if (!this->scan_running_ && this->failed_start_count_ < SCAN_START_RETRY_MAX_DOUBLINGS) {
    ++this->failed_start_count_;
    if (this->failed_start_count_ == SCAN_START_RETRY_MAX_DOUBLINGS) {
      ESP_LOGW(TAG, "Scan start keeps failing; retrying every %" PRIu32 " s",
               (SCAN_START_RETRY_MS << SCAN_START_RETRY_MAX_DOUBLINGS) / 1000);
    }
  }
  return this->scan_running_;
}

void BK72xxBLETracker::dump_config() {
  ESP_LOGCONFIG(TAG,
                "BK72xx BLE Tracker:\n"
                "  Scan Duration: %" PRIu32 " s\n"
                "  Scan Interval: %.0f ms (%" PRIu32 " BLE units)\n"
                "  Scan Window: %.0f ms (%" PRIu32 " BLE units)\n"
                "  Scan Type: PASSIVE\n"
                "  Continuous Scanning: %s",
                this->scan_duration_ / 1000, this->scan_interval_ * 0.625f, this->scan_interval_,
                this->scan_window_ * 0.625f, this->scan_window_, YESNO(this->scan_continuous_));
}

// ---------------------------------------------------------------------------
// Scan report — delivered by the controller's loop() on the ESPHome main task
// (the controller queues reports from the BLE task), so publish_state() and
// listener dispatch run in main-loop context with no cross-task handling here.
// ---------------------------------------------------------------------------

void BK72xxBLETracker::on_scan_report(const bk72xx_ble::BLEScanReport &report) {
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
    if (listener->parse_device(device)) {
      found = true;
    }
  }
  // Mirror esp32_ble_tracker: log a newly-seen device only when nothing claimed
  // it and the scan is one-shot (continuous scans would spam).
  if (!found && !this->scan_continuous_)
    this->discovered_log_.log_device(TAG, device);
#endif  // ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
}

// ---------------------------------------------------------------------------
// Public scan control
// ---------------------------------------------------------------------------

void BK72xxBLETracker::start_scan() {
  // Mirrors esp32_ble_tracker::start_scan(): caller sets scan_continuous_ via
  // set_scan_continuous() first, then calls start_scan() to begin scanning.
  //
  // Nothing to do while a scan is already running: latching here would leave
  // scan_requested_ set after that scan ends and silently restart a one-shot
  // scan nobody asked for.
  if (this->scan_running_)
    return;

  // The request is latched: if this immediate attempt fails (controller busy,
  // WiFi/BLE coexistence), loop() keeps retrying it with backoff even in
  // non-continuous mode, so a one-shot start cannot fail silently.
  //
  // Routed through the backoff helper (forced: the user asked for an immediate
  // attempt) so a failure here still counts toward the backoff escalation and
  // its WARN. The force bypass only applies while the failure streak is clean —
  // against a failing controller, repeated start_scan() calls are rate-limited
  // like any other attempt.
  this->scan_requested_ = true;
  this->try_start_with_backoff_(millis(), /* force= */ true);
}

void BK72xxBLETracker::stop_scan() {
  this->scan_continuous_ = false;
  this->scan_requested_ = false;  // also cancels a pending (not yet successful) start
  this->stop_scan_();
}

// ---------------------------------------------------------------------------
// Internal scan start / stop
// ---------------------------------------------------------------------------

void BK72xxBLETracker::start_scan_() {
  if (this->scan_running_)
    return;

  if (!this->parent_->scan_start(static_cast<uint16_t>(this->scan_interval_),
                                 static_cast<uint16_t>(this->scan_window_)))
    return;

  const uint32_t now = millis();
  this->scan_running_ = true;
  this->scan_requested_ = false;  // the latched one-shot request is satisfied
  this->failed_start_count_ = 0;  // reset here so direct starts clear the backoff too
  this->scan_start_time_ = now;
  // Log every explicit start at DEBUG — stop_scan_() logs every stop at DEBUG, and
  // in non-continuous mode each period is an explicit start, so asymmetric logging
  // would read as the scanner failing to come back up.
  ESP_LOGD(TAG, "Scan started (passive, window=%.0fms, interval=%.0fms)", this->scan_window_ * 0.625f,
           this->scan_interval_ * 0.625f);
  // Re-anchor the on_scan_end period to every successful start — first start (so the
  // period counts from the scan, not from boot) and every restart after a stop (so
  // resuming after longer than scan_duration, e.g. a failed OTA restoring continuous
  // mode 10 minutes later, does not fire on_scan_end before an advertisement can
  // arrive). scan_started_once_ purely gates the period timer.
  this->scan_period_start_ = now;
  this->scan_started_once_ = true;
}

void BK72xxBLETracker::stop_scan_() {
  if (!this->scan_running_)
    return;
  this->parent_->scan_stop();
  this->scan_running_ = false;
  ESP_LOGD(TAG, "Scan stopped");
#ifdef ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
  for (auto *listener : this->listeners_)
    listener->on_scan_end();
  this->discovered_log_.clear();  // reset per-scan "Found device" dedup (esp32_ble_tracker parity)
#endif
  this->scan_period_start_ = millis();  // reset period clock so on_scan_end does not double-fire
}

}  // namespace esphome::bk72xx_ble_tracker

#endif  // USE_LIBRETINY
