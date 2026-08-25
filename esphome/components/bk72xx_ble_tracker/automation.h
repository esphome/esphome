// Automation triggers and actions for bk72xx_ble_tracker: triggers are the
// neutral ble_device_base classes; only the scan-control actions are
// platform-specific.

#pragma once

#ifdef USE_LIBRETINY

#include "bk72xx_ble_tracker.h"

#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

namespace esphome::bk72xx_ble_tracker {

template<typename... Ts> class StartScanAction final : public Action<Ts...>, public Parented<BK72xxBLETracker> {
 public:
  TEMPLATABLE_VALUE(bool, continuous)
  void play(const Ts &...x) override {
    // With continuous: set, the action wins. Without it, the configured value
    // is used - stop_scan() clears the runtime flag permanently, so a bare
    // stop_scan/start_scan pair would otherwise never resume continuous mode.
    const bool want =
        this->continuous_.has_value() ? this->continuous_.value(x...) : this->parent_->configured_continuous();
    if (this->parent_->scan_running()) {
      // Same mode on a running scan is a no-op (esp32 parity): re-anchoring
      // the duration window here would let a repeated action keep a one-shot
      // scan alive forever. A real mode switch re-anchors so a change to
      // one-shot runs a full duration from now.
      if (want != this->parent_->scan_continuous()) {
        this->parent_->set_scan_continuous(want);
        this->parent_->restart_scan_duration();
      }
      return;
    }
    this->parent_->set_scan_continuous(want);
    this->parent_->start_scan();
  }
};

template<typename... Ts> class StopScanAction final : public Action<Ts...>, public Parented<BK72xxBLETracker> {
 public:
  void play(const Ts &...x) override { this->parent_->stop_scan(); }
};

}  // namespace esphome::bk72xx_ble_tracker

#endif  // USE_LIBRETINY
