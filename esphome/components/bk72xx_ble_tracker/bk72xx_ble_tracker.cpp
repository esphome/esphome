// bk72xx_ble_tracker.cpp
//
// BLE scan policy for the BK72xx BLE-5.x chips: parameters, duration/period
// timers and the rate-limited start retry. All controller access (stack
// bring-up, scan primitives, the BLE-task → main-task report queue) goes
// through the bk72xx_ble component — no SDK calls and no cross-task state here.

#ifdef USE_LIBRETINY

#include "bk72xx_ble_tracker.h"

#include <cinttypes>

#include "esphome/core/application.h"
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
// Stable-run time before the failure streak clears; reset-on-start would keep
// a flapping controller at the 1 s gate.
static constexpr uint32_t SCAN_STABLE_RESET_MS = 30000;

// Radio-idle deadline for the bounded stop drain at OTA start.
static constexpr uint32_t OTA_STOP_FLUSH_MS = 100;

// 0.625 ms BLE units; integer math avoids soft-float on this FPU-less part.
constexpr uint32_t ble_units_to_ms(uint32_t units) { return units * 5 / 8; }

// ---------------------------------------------------------------------------
// Component lifecycle
// ---------------------------------------------------------------------------

void BK72xxBLETracker::setup() {
  // Receive the controller's scan reports; the controller queues them from the
  // BLE task and delivers here on the main task.
  this->parent_->register_scan_listener(this);
  // Merged (and unmerged) frames go to the shared dispatcher; unclaimed
  // devices are logged only on one-shot scans (continuous would spam).
  this->merger_.bind(&this->dispatcher_, &this->scan_continuous_, TAG);
#ifdef USE_OTA_STATE_LISTENER
  // Pause scanning while an OTA update is in flight — on the single-core BK72xx the
  // BLE scan competes with the OTA flash writes. Mirrors esp32_ble_tracker.
  ota::get_global_ota_callback()->add_global_state_listener(this);
#endif
  // scan_requested_ check: an on_boot start_scan latched before this setup()
  // must keep the retry loop running (rp2/ln882h parity).
  if (!this->scan_continuous_ && !this->scan_requested_) {
    // Nothing to time until an explicit start_scan(); it re-enables the loop.
    this->disable_loop();
  }
}

#ifdef USE_OTA_STATE_LISTENER
void BK72xxBLETracker::on_ota_global_state(ota::OTAState state, float progress, uint8_t error,
                                           ota::OTAComponent *comp) {
  if (state == ota::OTA_STARTED) {
    this->scan_continuous_before_ota_ = this->scan_continuous_;
    this->scan_requested_before_ota_ = this->scan_requested_;
    this->stop_scan();
    // The transfer starves the loop; a deferred stop would leave the radio
    // scanning for the whole update, so drain it here, bounded.
    if (!this->parent_->flush_pending_stop(OTA_STOP_FLUSH_MS))
      ESP_LOGE(TAG, "Scan still stopping at OTA start; the radio may contend with the update");
  } else if (state == ota::OTA_ERROR || state == ota::OTA_ABORT) {
    // On success the device reboots, so restore only on a failed/aborted update;
    // loop() restarts the scan on its next iteration (continuous idle branch).
    if (this->scan_continuous_before_ota_) {
      this->scan_continuous_before_ota_ = false;
      this->scan_continuous_ = true;
      this->enable_loop();  // stop_scan() parked it
    }
    // A one-shot request that was still pending (latched, retrying) when the
    // OTA paused scanning is re-latched, not dropped — loop() resumes the retry.
    if (this->scan_requested_before_ota_) {
      this->scan_requested_before_ota_ = false;
      this->scan_requested_ = true;
      this->enable_loop();
    }
  }
}
#endif  // USE_OTA_STATE_LISTENER

void BK72xxBLETracker::loop() {
  const uint32_t now = App.get_loop_component_start_time();

  // Deliver held scannable advertisements whose scan response never arrived —
  // unmerged after the merger's timeout.
  if (!this->merger_.empty())
    this->merger_.sweep(now);

  // Before the drop branch: a drop after a stable run starts a fresh streak.
  if (this->scan_running_ && this->failed_start_count_ != 0 && now - this->scan_start_time_ >= SCAN_STABLE_RESET_MS)
    this->failed_start_count_ = 0;

  // A terminal failure while we report running recovers via the normal retry
  // path; the drop charges the backoff so a flapping controller escalates.
  if (this->scan_running_ && this->parent_->last_scan_result() == bk72xx_ble::ScanOpResult::FAILED) {
    ESP_LOGW(TAG, "Controller scan lost; retrying");
    this->scan_requested_ = true;
    this->count_failed_start_();
    this->mark_scan_ended_(now);
  }

  if (this->scan_continuous_) {
    if (!this->scan_running_) {
      // One-iteration deferral; all stamps share this iteration's cached
      // timestamp, so the period check below cannot underflow.
      if (this->try_start_with_backoff_(now))
        return;
    }
    // Period timer: fire on_scan_end() once per scan_duration_ window, mirroring
    // esp32_ble_tracker::cleanup_scan_state_(). Gated on scan_started_once_ so a scan
    // that never came up (start kept failing) does not fire spurious on_scan_end events.
    if (this->scan_started_once_ && now - this->scan_period_start_ >= this->scan_duration_) {
      this->fire_scan_end_();
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
    // Same one-iteration deferral as the continuous branch.
    if (this->try_start_with_backoff_(now))
      return;
  }
  if (this->scan_running_ && now - this->scan_start_time_ >= this->scan_duration_) {
    // A full-duration run proves the controller healthy even when duration is
    // shorter than SCAN_STABLE_RESET_MS.
    this->failed_start_count_ = 0;
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
  // Mid bring-up, observe instead of re-issuing (the hub self-advances). A
  // SETTLED outcome completes immediately; only fresh attempts after FAILED
  // are rate-limited.
  const auto hub = this->parent_->last_scan_result();
  if (hub == bk72xx_ble::ScanOpResult::PENDING)
    return false;
  if (hub == bk72xx_ble::ScanOpResult::FAILED) {
    if (this->start_attempt_open_) {
      // Our bring-up gave up asynchronously; charge it to the backoff.
      this->start_attempt_open_ = false;
      this->count_failed_start_();
    }
    if ((!force || this->failed_start_count_ != 0) &&
        now - this->last_scan_start_attempt_ < (SCAN_START_RETRY_MS << this->failed_start_count_))
      return false;
  }
  this->start_scan_();
  if (!this->scan_running_) {
    if (this->parent_->last_scan_result() == bk72xx_ble::ScanOpResult::PENDING) {
      this->start_attempt_open_ = true;
      return false;  // the controller is still bringing the scan up; not a failure
    }
    this->count_failed_start_();
  }
  return this->scan_running_;
}

void BK72xxBLETracker::count_failed_start_() {
  if (this->failed_start_count_ < SCAN_START_RETRY_MAX_DOUBLINGS) {
    ++this->failed_start_count_;
    if (this->failed_start_count_ == SCAN_START_RETRY_MAX_DOUBLINGS) {
      ESP_LOGW(TAG, "Scan start keeps failing; retrying every %" PRIu32 " s",
               (SCAN_START_RETRY_MS << SCAN_START_RETRY_MAX_DOUBLINGS) / 1000);
    }
  }
}

void BK72xxBLETracker::dump_config() {
  ESP_LOGCONFIG(TAG,
                "BK72xx BLE Tracker:\n"
                "  Scan Duration: %" PRIu32 " s\n"
                "  Scan Interval: %" PRIu32 " ms (%" PRIu32 " BLE units)\n"
                "  Scan Window: %" PRIu32 " ms (%" PRIu32 " BLE units)\n"
                "  Scan Type: %s (configured %s)\n"
                "  Continuous Scanning: %s",
                this->scan_duration_ / 1000, ble_units_to_ms(this->scan_interval_), this->scan_interval_,
                ble_units_to_ms(this->scan_window_), this->scan_window_, this->scan_active_ ? "ACTIVE" : "PASSIVE",
                this->scan_active_configured_ ? "ACTIVE" : "PASSIVE", YESNO(this->scan_continuous_));
}

// ---------------------------------------------------------------------------
// Scan report — delivered by the controller's loop() on the ESPHome main task
// (the controller queues reports from the BLE task), so publish_state() and
// listener dispatch run in main-loop context with no cross-task handling here.
// ---------------------------------------------------------------------------

// GAPM report info byte (BLEScanReport::evt_type): bits 0-2 report type,
// bit 5 scannable advertisement. Verified against both BDK stacks (5.1 and
// 5.2 fill it from gapm_ext_adv_report_ind.info).
static constexpr uint8_t GAPM_REPORT_TYPE_MASK = 0x07;
static constexpr uint8_t GAPM_REPORT_TYPE_SCAN_RSP_EXT = 2;
static constexpr uint8_t GAPM_REPORT_TYPE_SCAN_RSP_LEG = 3;
static constexpr uint8_t GAPM_REPORT_INFO_SCAN_ADV_BIT = 1 << 5;

// Demux advertisements vs scan responses into the shared merger: the BDK
// delivers the pair as separate reports; a scannable advertisement is held
// until its scan response arrives and delivered as one merged frame.
void BK72xxBLETracker::on_scan_report(const bk72xx_ble::BLEScanReport &report) {
  const uint8_t rtype = report.evt_type & GAPM_REPORT_TYPE_MASK;
  if (rtype == GAPM_REPORT_TYPE_SCAN_RSP_LEG || rtype == GAPM_REPORT_TYPE_SCAN_RSP_EXT) {
    this->merger_.submit_scan_rsp(report.mac, report.rssi, report.addr_type, report.data, report.data_len);
    return;
  }
  // Stash only while an active scan runs: a passive scan never gets a
  // response, and after a stop nothing would sweep the merger, so a late
  // report would surface minutes later as a fresh advertisement.
  if (this->scan_running_ && this->scan_active_ && (report.evt_type & GAPM_REPORT_INFO_SCAN_ADV_BIT)) {
    this->merger_.stash_adv(report.mac, report.rssi, report.addr_type, report.data, report.data_len,
                            App.get_loop_component_start_time());
    return;
  }
  this->dispatcher_.dispatch(report.mac, report.rssi, report.addr_type, report.data, report.data_len,
                             /*raw_only=*/false, this->scan_continuous_ ? nullptr : TAG);
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
  this->enable_loop();  // an idle one-shot tracker parked it in stop_scan_()
  this->try_start_with_backoff_(App.get_loop_component_start_time(), /* force= */ true);
}

void BK72xxBLETracker::restart_scan_duration() {
  if (!this->scan_running_)
    return;
  // Re-anchor only the one-shot duration clock. scan_period_start_ (the
  // continuous-mode on_scan_end period) is deliberately left alone: a
  // start_scan action fired more often than scan_duration_ would otherwise
  // suppress on_scan_end indefinitely — and absence detection (ble_rssi's NAN
  // publish) rides on that period.
  this->scan_start_time_ = App.get_loop_component_start_time();
}

void BK72xxBLETracker::stop_scan() {
  this->scan_continuous_ = false;
  this->scan_requested_ = false;  // also cancels a pending (not yet successful) start
  this->stop_scan_();
}

// ---------------------------------------------------------------------------
// Internal scan start / stop
// ---------------------------------------------------------------------------

bk72xx_ble::ScanOpResult BK72xxBLETracker::controller_scan_start_() {
  this->last_scan_start_attempt_ = App.get_loop_component_start_time();
  return this->parent_->scan_start(static_cast<uint16_t>(this->scan_interval_),
                                   static_cast<uint16_t>(this->scan_window_), this->scan_active_);
}

void BK72xxBLETracker::start_scan_() {
  if (this->scan_running_)
    return;

  if (this->controller_scan_start_() != bk72xx_ble::ScanOpResult::SETTLED)
    return;

  const uint32_t now = App.get_loop_component_start_time();
  this->scan_running_ = true;
  this->scan_requested_ = false;  // the latched one-shot request is satisfied
  this->start_attempt_open_ = false;
  // failed_start_count_ deliberately not reset here; only a stable run clears it (loop()).
  this->scan_start_time_ = now;
  // Log every explicit start at DEBUG — stop_scan_() logs every stop at DEBUG, and
  // in non-continuous mode each period is an explicit start, so asymmetric logging
  // would read as the scanner failing to come back up.
  ESP_LOGD(TAG, "Scan started (%s, window=%" PRIu32 "ms, interval=%" PRIu32 "ms)",
           this->scan_active_ ? "active" : "passive", ble_units_to_ms(this->scan_window_),
           ble_units_to_ms(this->scan_interval_));
  // Re-anchor the on_scan_end period to every successful start — first start (so the
  // period counts from the scan, not from boot) and every restart after a stop (so
  // resuming after longer than scan_duration, e.g. a failed OTA restoring continuous
  // mode 10 minutes later, does not fire on_scan_end before an advertisement can
  // arrive). scan_started_once_ purely gates the period timer.
  this->scan_period_start_ = now;
  this->scan_started_once_ = true;
}

// Deliberate logical/physical split: on_scan_end() reports the tracker's
// intent while the hub winds the radio down asynchronously; OTA is the one
// path that must wait, and it flushes explicitly.
void BK72xxBLETracker::stop_scan_() {
  this->start_attempt_open_ = false;  // an abandoned bring-up is not charged
  this->parent_->scan_stop();         // idempotent: releases whatever the hub holds
  if (this->scan_running_) {
    ESP_LOGD(TAG, "Scan stopped");
    this->mark_scan_ended_(App.get_loop_component_start_time());
  }
  // Park when idle (the hub drives its own teardown); re-check because an
  // on_scan_end automation may have restarted the scan.
  if (!this->scan_continuous_ && !this->scan_running_ && !this->scan_requested_)
    this->disable_loop();
}

// The period re-anchor keeps on_scan_end from double-firing in one iteration.
void BK72xxBLETracker::mark_scan_ended_(uint32_t now) {
  this->scan_running_ = false;
  this->fire_scan_end_();
  this->scan_period_start_ = now;
}

void BK72xxBLETracker::fire_scan_end_() {
  // Deliver held advertisements whose scan response never came (unmerged)
  // BEFORE on_scan_end fires.
  this->merger_.flush();
  this->dispatcher_.on_scan_end();
}

// true = request latched, not applied: the reconciler applies it
// asynchronously and loop() recovers a failed re-arm (ln882h parity).
bool BK72xxBLETracker::request_scan_mode(bool active) {
  if (this->scan_active_ == active)
    return true;
  this->scan_active_ = active;
  // V: the proxy's "Setting scanner mode" line already narrates this at D.
  ESP_LOGV(TAG, "Scan mode %s", active ? "active" : "passive");
  // The controller reconciler restarts a running scan itself; the scan stays
  // logically running. An idle scanner picks the mode up on its next start.
  if (this->scan_running_)
    this->controller_scan_start_();
  return true;
}

}  // namespace esphome::bk72xx_ble_tracker

#endif  // USE_LIBRETINY
