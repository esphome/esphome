#pragma once
#include "esphome/core/defines.h"
#if defined(USE_ZEPHYR_CDC_ACM_RATE_CALLBACK)

#include "esphome/core/helpers.h"
#include <array>
#include <zephyr/device.h>

namespace esphome::zephyr {

/// Passes the bit rate a USB host sets on a CDC ACM port from the USB driver's thread to the main loop.
class CdcAcm final {
 public:
  /// Call from setup(). Rates reach the callbacks on the main loop, starting with each port's current rate, so a
  /// rate the host set during boot is not missed.
  template<typename F> void add_on_rate_callback(F &&callback) {
    if (this->rate_callbacks_.size() == 0) {
      this->start_();
    } else {
      // The first report may already have run (the scheduler runs while a setup() waits)
      for (size_t i = 0; i < this->rates_.size(); i++) {
        if (this->rates_[i] != 0)
          callback(device_at(i), this->rates_[i]);
      }
    }
    this->rate_callbacks_.add(std::forward<F>(callback));
  }

  /// Called by the USB driver, from its own thread, when the host sets a bit rate.
  static void rate_changed();

 protected:
  void start_();
  static const device *device_at(size_t index);
  void report_rates_();
  CallbackManager<void(const device *, uint32_t)> rate_callbacks_;
  // Last bit rate reported for each zephyr,cdc-acm-uart port, in device tree order
  std::array<uint32_t, DT_NUM_INST_STATUS_OKAY(zephyr_cdc_acm_uart)> rates_{};
};

extern CdcAcm *global_cdc_acm;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::zephyr

#endif
