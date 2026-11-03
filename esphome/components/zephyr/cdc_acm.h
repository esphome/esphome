#pragma once
#include "esphome/core/defines.h"
#if defined(USE_ZEPHYR_CDC_ACM_RATE_CALLBACK)

#include "esphome/core/helpers.h"
#include <zephyr/device.h>
#ifdef CONFIG_USB_DEVICE_STACK_NEXT
#include <array>
#endif

namespace esphome::zephyr {

/// Forwards the bit rate a USB host sets on a CDC ACM port to the main loop.
class CdcAcm final {
 public:
  /// Call from setup(). Rates reach the callbacks on a scheduler tick, which normally comes after every
  /// subscriber's setup(); a component that holds setup back with can_proceed() runs the scheduler earlier.
  template<typename F> void add_on_rate_callback(F &&callback) {
    if (this->rate_callbacks_.size() == 0)
      this->start_();
    this->rate_callbacks_.add(std::forward<F>(callback));
  }

 protected:
  void start_();
  CallbackManager<void(const device *, uint32_t)> rate_callbacks_;
#ifdef CONFIG_USB_DEVICE_STACK_NEXT
  void poll_rates_();
  // Last bit rate seen on each zephyr,cdc-acm-uart port, in device tree order
  std::array<uint32_t, DT_NUM_INST_STATUS_OKAY(zephyr_cdc_acm_uart)> rates_{};
#else
  static void cdc_dte_rate_callback_(const device *device, uint32_t rate);
#endif
};

extern CdcAcm *const global_cdc_acm;

}  // namespace esphome::zephyr

#endif
