#pragma once
#if defined(CONFIG_CDC_ACM_DTE_RATE_CALLBACK_SUPPORT)

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include <zephyr/device.h>

namespace esphome::zephyr {

class CdcAcm : public Component {
 public:
  CdcAcm();
  void setup() override;
  void add_on_rate_callback(std::function<void(const device *, uint32_t)> &&callback) {
    this->rate_callbacks_.add(std::move(callback));
  }

 protected:
  static void cdc_dte_rate_callback_(const device *device, uint32_t rate);
  CallbackManager<void(const device *, uint32_t)> rate_callbacks_;
};

extern CdcAcm *global_cdc_acm;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::zephyr

#endif
