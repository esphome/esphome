#pragma once
#if defined(CONFIG_CDC_ACM_DTE_RATE_CALLBACK_SUPPORT)

#include "esphome/core/helpers.h"
#include <zephyr/device.h>

namespace esphome::zephyr {

/// Forwards the bit rate a USB host sets on a CDC ACM port to the main loop.
class CdcAcm final {
 public:
  /// Call from setup(). Rates reach the callbacks on a scheduler tick, which normally comes after every
  /// subscriber's setup(); a component that holds setup back with can_proceed() runs the scheduler earlier.
  template<typename F> void add_on_rate_callback(F &&callback) {
    if (this->rate_callbacks_.size() == 0)
      this->set_driver_callbacks_();
    this->rate_callbacks_.add(std::forward<F>(callback));
  }

 protected:
  void set_driver_callbacks_();
  static void cdc_dte_rate_callback_(const device *device, uint32_t rate);
  CallbackManager<void(const device *, uint32_t)> rate_callbacks_;
};

extern CdcAcm *const global_cdc_acm;

}  // namespace esphome::zephyr

#endif
