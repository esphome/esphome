#pragma once
#include "esphome/core/defines.h"
#if defined(USE_ZEPHYR_CDC_ACM_RATE_CALLBACK)

#include "esphome/core/helpers.h"
#include <zephyr/device.h>

namespace esphome::zephyr {

/// Passes the bit rate a USB host sets on a CDC ACM port from the USB driver's thread to the main loop.
class CdcAcm final {
 public:
  /// Call from setup(). Callbacks get each port's current rate, then every rate the host sets, in order, on the
  /// main loop.
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

extern CdcAcm *global_cdc_acm;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::zephyr

#endif
