#pragma once
#include "esphome/core/defines.h"
#if defined(USE_ZEPHYR_CDC_ACM_RATE_CALLBACK)

#include "esphome/core/helpers.h"
#include <zephyr/device.h>

namespace esphome::zephyr {

/// Passes the bit rate a USB host sets on a CDC ACM port from the USB driver's thread to the main loop.
class CdcAcm final {
 public:
  /// Call from setup(). Callbacks get each port's current rate, then every rate the host sets, on the main loop.
  /// Rates reach the callbacks on a scheduler tick, which normally comes after every subscriber's setup(); a
  /// component that holds setup back with can_proceed() runs the scheduler earlier, and a subscriber registered
  /// after that tick misses the boot-time rate.
  template<typename F> void add_on_rate_callback(F &&callback) {
    if (this->rate_callbacks_.size() == 0)
      this->start_();
    this->rate_callbacks_.add(std::forward<F>(callback));
  }

  /// Called by the USB driver, from its own thread, after the host set the bit rate of `dev`.
  static void rate_changed(const device *dev);

 protected:
  void start_();
  static void post_rate(const device *dev);
  CallbackManager<void(const device *, uint32_t)> rate_callbacks_;
};

extern CdcAcm *const global_cdc_acm;

}  // namespace esphome::zephyr

#endif
