// Scan-control actions for rtl87xx_ble_tracker. The tracker delivers parsed
// devices through the neutral ble_device_base dispatcher; only start/stop
// scan is platform-specific, mirroring esp32_ble_tracker / bk72xx_ble_tracker.

#pragma once

#ifdef USE_LIBRETINY

#include "rtl87xx_ble_tracker.h"

#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

namespace esphome::rtl87xx_ble_tracker {

template<typename... Ts> class StartScanAction final : public Action<Ts...>, public Parented<RTL87xxBLETracker> {
 public:
  TEMPLATABLE_VALUE(bool, continuous)
  void play(const Ts &...x) override {
    // With continuous: set, the action wins. Without it, the configured value
    // is used - stop_scan() clears the runtime flag permanently, so a bare
    // stop_scan/start_scan pair would otherwise never resume continuous mode.
    const bool want =
        this->continuous_.has_value() ? this->continuous_.value(x...) : this->parent_->configured_continuous();
    // While a refused stop is being retried, the running flag is still set but
    // the scan is ending: fall through so start_scan() latches the request and
    // the reconciler restarts after the stop lands.
    if (this->parent_->scan_running() && !this->parent_->stop_pending()) {
      // Same mode on a running scan is a no-op (esp32 parity); re-anchoring on
      // every call would keep a one-shot scan alive forever.
      if (want != this->parent_->scan_continuous()) {
        this->parent_->set_scan_continuous(want);
        this->parent_->restart_scan_duration();
      }
      return;
    }
    this->parent_->set_scan_continuous(want);
    this->parent_->start_scan();  // latched if the stack is not ready yet
  }
};

template<typename... Ts> class StopScanAction final : public Action<Ts...>, public Parented<RTL87xxBLETracker> {
 public:
  void play(const Ts &...x) override { this->parent_->stop_scan(); }
};

}  // namespace esphome::rtl87xx_ble_tracker

#endif  // USE_LIBRETINY
