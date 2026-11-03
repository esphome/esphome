#pragma once
#include "esphome/core/defines.h"
#if defined(USE_ZEPHYR_CDC_ACM_RATE_CALLBACK)

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include <zephyr/device.h>

namespace esphome::zephyr {

/// Passes the bit rate a USB host sets on a CDC ACM port from the USB driver's thread to the main loop.
class CdcAcm final : public Component {
 public:
  CdcAcm();
  void setup() override;
  void loop() override;

  /// Call from setup(). Rates set before that, even before boot finished, are delivered too.
  template<typename F> void add_on_rate_callback(F &&callback) { this->rate_callbacks_.add(std::forward<F>(callback)); }

  /// Called by the USB driver, from its own thread, when the host sets a bit rate. Only queues it.
  static void rate_changed(const device *device, uint32_t rate);

 protected:
  CallbackManager<void(const device *, uint32_t)> rate_callbacks_;
};

extern CdcAcm *global_cdc_acm;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::zephyr

#endif
