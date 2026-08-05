#ifdef USE_LIBRETINY

#include "ln882h_ble_tracker.h"

#include <cinttypes>
#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::ln882h_ble_tracker {

static const char *const TAG = "ln882h_ble_tracker";

static constexpr float BLE_SCAN_UNIT_MS = 0.625f;

// ---------------------------------------------------------------------------
// Component lifecycle
// ---------------------------------------------------------------------------

void LN882HBLETracker::setup() {
  // Receive the controller's scan reports; the controller queues them from the
  // rw task and delivers here on the main task.
  this->parent_->register_scan_listener(this);
  // scan_running_ check: an on_boot start_scan action (priority 600) runs
  // before this setup() (200) and enable_loop() is a no-op pre-setup — parking
  // the loop here would strand that already-running scan.
  if (!this->scan_continuous_ && !this->scan_running_ && !this->pending_start_) {
    // Say so once: with continuous: false nothing scans until an explicit
    // start_scan() — silence here reads as a broken scanner.
    ESP_LOGD(TAG, "Scanning not started (continuous: false) - waiting for an explicit start_scan()");
    // Nothing to time until then; start_scan_() re-enables the loop.
    this->disable_loop();
  }
#ifdef USE_OTA_STATE_LISTENER
  // Pause scanning while an OTA update is in flight — on the single-core LN882H the
  // BLE scan competes with the OTA flash writes. Mirrors esp32_ble_tracker.
  ota::get_global_ota_callback()->add_global_state_listener(this);
#endif
}

#ifdef USE_OTA_STATE_LISTENER
void LN882HBLETracker::on_ota_global_state(ota::OTAState state, float progress, uint8_t error,
                                           ota::OTAComponent *comp) {
  if (state == ota::OTA_STARTED) {
    this->scan_continuous_before_ota_ = this->scan_continuous_;
    this->scan_running_before_ota_ = this->scan_running_;
    this->stop_scan();
  } else if (state == ota::OTA_ERROR || state == ota::OTA_ABORT) {
    // On success the device reboots, so restore only on a failed/aborted
    // update. Continuous mode resumes via loop()'s idle branch; a one-shot
    // scan that was running is restarted explicitly (bk72xx sibling parity —
    // stop_scan() cleared it and nothing else would bring it back).
    if (this->scan_continuous_before_ota_) {
      this->scan_continuous_ = true;
      this->enable_loop();  // stop_scan() disabled it; loop()'s idle branch restarts the scan
    } else if (this->scan_running_before_ota_) {
      this->start_scan();
    }
    this->scan_continuous_before_ota_ = false;
    this->scan_running_before_ota_ = false;
  }
}
#endif  // USE_OTA_STATE_LISTENER

void LN882HBLETracker::loop() {
  if (this->pending_start_) {
    // A start_scan latched before the controller's setup(); safe now — loop()
    // only runs after every component set up.
    this->pending_start_ = false;
    if (!this->scan_running_) {
      this->start_scan_();
    }
  }
  // Flush pending scannable advertisements whose scan response never arrived
  // (device didn't answer / frame lost) — delivered unmerged after the timeout.
  // Main-task only, like every consumer of pending_adv_.
  const uint32_t now = millis();
  if (this->pending_count_ != 0) {
    for (auto &p : this->pending_adv_) {
      if (p.used && now - p.stored_ms > PENDING_ADV_TIMEOUT_MS) {
        p.used = false;
        this->pending_count_--;
        this->process_adv_(p.mac, p.rssi, p.addr_type, p.data, p.data_len, /*raw_only=*/false);
      }
    }
  }

  if (this->scan_continuous_) {
    if (!this->scan_running_) {
      this->start_scan_();
      // start_scan_() re-anchors scan_period_start_ from a later millis() than
      // the cached `now`; resume the period timer next iteration.
      return;
    }
    // Period timer: once per scan_duration_ window, restart the controller scan
    // and fire on_scan_end(), mirroring esp32_ble_tracker::cleanup_scan_state_().
    // The restart is the recovery path for the coexistence failure documented in
    // the header. scan_start() re-enters cleanly on its own: it stops an
    // in-flight scan and grants the controller's 10 ms GAPM settle before
    // restarting — an explicit scan_stop() first would clear the controller's
    // re-entry guard and skip that settle.
    if (now - this->scan_period_start_ >= this->scan_duration_) {
      ESP_LOGD(TAG, "Scan period elapsed - restarting scan");
      this->parent_->scan_start(this->scan_interval_, this->scan_window_, this->scan_active_);
      // Keep both clocks anchored to the restart: a runtime switch to
      // non-continuous then times out the current period, not the whole run.
      this->scan_start_time_ = now;
      this->end_scan_period_(now);
    }
    return;
  }

  // Non-continuous mode: run for scan_duration_ ms, then stop and fire on_scan_end.
  // Restart is driven externally (e.g. wifi: on_connect:).
  if (this->scan_running_ && now - this->scan_start_time_ >= this->scan_duration_) {
    this->stop_scan_();
  }
}

bool LN882HBLETracker::request_scan_mode(bool active) {
  if (this->scan_active_ == active)
    return true;
  this->scan_active_ = active;
  ESP_LOGD(TAG, "Scan mode %s", active ? "active" : "passive");
  // scan_start() re-enters cleanly (stops + GAPM settle). No on_scan_end and
  // no period reset: the scan logically continues, only the mode changes.
  if (this->scan_running_) {
    this->parent_->scan_start(this->scan_interval_, this->scan_window_, this->scan_active_);
  }
  return true;
}

void LN882HBLETracker::dump_config() {
  ESP_LOGCONFIG(TAG,
                "LN882H BLE Tracker:\n"
                "  Scan Duration: %" PRIu32 " s\n"
                "  Scan Interval: %.0f ms (%" PRIu16 " BLE units)\n"
                "  Scan Window: %.0f ms (%" PRIu16 " BLE units)\n"
                "  Scan Type: %s\n"
                "  Continuous Scanning: %s",
                this->scan_duration_ / 1000, this->scan_interval_ * BLE_SCAN_UNIT_MS, this->scan_interval_,
                this->scan_window_ * BLE_SCAN_UNIT_MS, this->scan_window_, this->scan_active_ ? "ACTIVE" : "PASSIVE",
                YESNO(this->scan_continuous_));
}

// ---------------------------------------------------------------------------
// Adv/scan-response demux with Bluedroid-style merge: the LN controller
// delivers the pair as separate reports; a scannable advertisement is held
// until its scan response arrives and delivered as one merged frame.
// ---------------------------------------------------------------------------

void LN882HBLETracker::on_scan_report(const ln882h_ble::BLEScanReport &report) {
  if (report.is_scan_response) {
    this->deliver_scan_rsp_(report);
    return;
  }
  // Stash only while the scan runs: after a one-shot stop the loop is
  // disabled and nothing would sweep the table, so a late report would
  // surface minutes later as a fresh advertisement.
  if (this->scan_running_ && this->scan_active_ && report.scannable) {
    this->stash_adv_(report);
    return;
  }
  this->process_adv_(report.mac, report.rssi, report.addr_type, report.data, report.data_len, /*raw_only=*/false);
}

// Hold a scannable advertisement, waiting (≤ PENDING_ADV_TIMEOUT_MS) for its
// scan response.
void LN882HBLETracker::stash_adv_(const ln882h_ble::BLEScanReport &report) {
  // One pass: find a same-device entry (deliver + reuse) while remembering the
  // first free slot as the fallback.
  PendingAdv *slot = nullptr;
  PendingAdv *free_slot = nullptr;
  for (auto &p : this->pending_adv_) {
    if (!p.used) {
      if (free_slot == nullptr)
        free_slot = &p;
      continue;
    }
    if (p.addr_type == report.addr_type && memcmp(p.mac, report.mac, 6) == 0) {
      // Same device advertised again before its scan response arrived — deliver
      // the previous advertisement (its scan response is not coming) and reuse
      // the slot, so no frame is ever lost.
      p.used = false;
      this->pending_count_--;
      this->process_adv_(p.mac, p.rssi, p.addr_type, p.data, p.data_len, /*raw_only=*/false);
      slot = &p;
      break;
    }
  }
  if (slot == nullptr)
    slot = free_slot;
  if (slot == nullptr) {
    // Table full — degrade gracefully: deliver the advertisement unmerged.
    this->process_adv_(report.mac, report.rssi, report.addr_type, report.data, report.data_len, /*raw_only=*/false);
    return;
  }
  slot->used = true;
  this->pending_count_++;
  memcpy(slot->mac, report.mac, 6);
  slot->addr_type = report.addr_type;
  slot->rssi = report.rssi;
  slot->data_len = (report.data_len <= sizeof(slot->data)) ? report.data_len : sizeof(slot->data);
  memcpy(slot->data, report.data, slot->data_len);
  slot->stored_ms = millis();
}

// Scan response arrived: merge it with the pending advertisement from the same
// device into ONE frame (ESP-IDF/Bluedroid semantics).
void LN882HBLETracker::deliver_scan_rsp_(const ln882h_ble::BLEScanReport &report) {
  // Fast-out on the empty table (loop()/flush use the same guard); this is
  // the hottest caller.
  if (this->pending_count_ != 0) {
    for (auto &p : this->pending_adv_) {
      if (p.used && p.addr_type == report.addr_type && memcmp(p.mac, report.mac, 6) == 0) {
        // Append in place: the slot is released on delivery, so its 62-byte
        // buffer (legacy adv + scan response) holds the merged frame directly.
        const uint8_t room = sizeof(p.data) - p.data_len;
        const uint8_t add = (report.data_len <= room) ? report.data_len : room;
        memcpy(p.data + p.data_len, report.data, add);
        p.used = false;
        this->pending_count_--;
        // The advertisement's RSSI, not the scan response's: every unmerged path
        // reports the advertisement's measurement, so a device's RSSI must not
        // jump between two measurements depending on merge timing.
        this->process_adv_(report.mac, p.rssi, report.addr_type, p.data, p.data_len + add, /*raw_only=*/false);
        return;
      }
    }
  }
  // Unmatched scan-response: goes out on the raw callback only (HA merges per
  // address); local listeners/triggers receive each advertisement exactly once
  // via the merged/plain path above.
  this->process_adv_(report.mac, report.rssi, report.addr_type, report.data, report.data_len, /*raw_only=*/true);
}

void LN882HBLETracker::process_adv_(const uint8_t *mac, int8_t rssi, uint8_t addr_type, const uint8_t *data,
                                    uint8_t data_len, bool raw_only) {
  // Raw callback (the raw-advertisement path). Both full advertisements and
  // unmatched scan responses (raw_only) are forwarded.
  if (this->raw_advertisement_callback_.is_set()) {
    const ble_device_base::RawAdvertisement adv{
        .mac = mac, .data = data, .data_len = data_len, .rssi = rssi, .addr_type = addr_type};
    this->raw_advertisement_callback_.invoke(adv);
  }

#ifdef ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
  // Scan-response-only frames are never parsed for local sensors/triggers.
  if (raw_only)
    return;
  ble_device_base::ESPBTDevice device;
  device.from_scan_result(mac, rssi, addr_type, data, data_len);
  // The listener list holds sensors AND this tracker's automation triggers
  // (the triggers are listeners, exactly like esp32_ble_tracker), so one
  // loop feeds both and ORs into `found`.
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
// Public scan actions
// ---------------------------------------------------------------------------

void LN882HBLETracker::start_scan() {
  // Mirrors esp32_ble_tracker::start_scan(): caller sets scan_continuous_ via
  // set_scan_continuous() first, then calls start_scan() to begin scanning.
  if (!this->parent_->is_ready()) {
    // An on_boot automation (priority 600) runs before the controller's
    // setup() has resolved the BLE MAC; scan_start() now would rw_init() the
    // all-zero address and bring BLE up before WiFi. Latch; loop() applies
    // the start once every setup() has run.
    this->pending_start_ = true;
    return;
  }
  if (!this->scan_running_) {
    this->start_scan_();
  }
}

void LN882HBLETracker::restart_scan_duration() {
  if (!this->scan_running_)
    return;
  // Re-anchor only the one-shot duration clock. scan_period_start_ (the
  // continuous-mode on_scan_end period) is deliberately left alone: a
  // start_scan action fired more often than scan_duration_ would otherwise
  // suppress on_scan_end indefinitely — and absence detection (ble_rssi's NAN
  // publish) rides on that period.
  this->scan_start_time_ = millis();
}

void LN882HBLETracker::stop_scan() {
  // Cancel a start latched before the controller's setup(); without this an
  // on_boot start_scan/stop_scan pair would still start at the first loop().
  this->pending_start_ = false;
  this->scan_continuous_ = false;
  this->stop_scan_();
}

// ---------------------------------------------------------------------------
// Internal scan start / stop
// ---------------------------------------------------------------------------

void LN882HBLETracker::start_scan_() {
  if (this->scan_running_)
    return;

  // The controller enables the stack on first use and owns the report queue;
  // this call is all the SDK interaction the tracker ever needs.
  this->parent_->scan_start(this->scan_interval_, this->scan_window_, this->scan_active_);
  const uint32_t now = millis();
  this->scan_running_ = true;
  this->scan_start_time_ = now;
  this->enable_loop();  // an idle non-continuous tracker disabled it in stop_scan_()
  // Log every explicit start at DEBUG — stop_scan_() logs every stop at DEBUG, and
  // in non-continuous mode each period is an explicit start, so asymmetric logging
  // would read as the scanner failing to come back up.
  ESP_LOGD(TAG, "BLE scan started (%s, window=%.0fms, interval=%.0fms)", this->scan_active_ ? "active" : "passive",
           this->scan_window_ * BLE_SCAN_UNIT_MS, this->scan_interval_ * BLE_SCAN_UNIT_MS);
  // Re-anchor the on_scan_end period to every successful start, so a restart
  // later than scan_duration (e.g. a failed OTA restoring continuous mode)
  // does not fire on_scan_end before an advertisement can arrive.
  this->scan_period_start_ = now;
}

void LN882HBLETracker::stop_scan_() {
  if (!this->scan_running_)
    return;
  this->parent_->scan_stop();
  this->scan_running_ = false;
  // DEBUG like start_scan_() — a per-period stop at INFO would read as the
  // scanner failing to come back up.
  ESP_LOGD(TAG, "BLE scan stopped");
  this->end_scan_period_(millis());  // also resets the period clock so on_scan_end does not double-fire
  // scan_running_ re-check: an on_scan_end automation runs synchronously inside
  // end_scan_period_() and may have called start_scan() — parking the loop then
  // would leave the radio scanning with no period timing or pending-adv sweep.
  if (!this->scan_continuous_ && !this->scan_running_) {
    // Nothing left to time; start_scan_() re-enables the loop.
    this->disable_loop();
  }
}

// Close a scan period: deliver held advertisements whose scan response never
// came (unmerged) BEFORE on_scan_end fires, then re-anchor the period clock.
void LN882HBLETracker::end_scan_period_(uint32_t now) {
  this->flush_pending_adv_();
#ifdef ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
  for (auto *listener : this->listeners_)
    listener->on_scan_end();
  this->discovered_log_.clear();  // reset per-scan "Found device" dedup (esp32_ble_tracker parity)
#endif
  this->scan_period_start_ = now;
}

// Deliver every held advertisement now (scan period/scan is ending): unmerged
// delivery, same as the timeout path in loop(). Main-task only.
void LN882HBLETracker::flush_pending_adv_() {
  if (this->pending_count_ == 0)
    return;
  for (auto &p : this->pending_adv_) {
    if (p.used) {
      p.used = false;
      this->process_adv_(p.mac, p.rssi, p.addr_type, p.data, p.data_len, /*raw_only=*/false);
    }
  }
  this->pending_count_ = 0;
}

}  // namespace esphome::ln882h_ble_tracker

#endif  // USE_LIBRETINY
