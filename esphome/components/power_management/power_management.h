#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#ifdef USE_ESP32
#include "esp_private/esp_clk.h"
#include "esp_pm.h"
#include "soc/rtc.h"
#include "esp_sleep.h"
#endif

namespace esphome::power_management {

class PowerManagement : public Component {
 public:
  float get_setup_priority() const override { return setup_priority::BUS; }
  void setup() override;
  void dump_config() override;
  void set_max_freq_mhz(uint32_t max_freq_mhz) { this->max_freq_mhz_ = max_freq_mhz; }
  void set_min_freq_mhz(uint32_t min_freq_mhz) { this->min_freq_mhz_ = min_freq_mhz; }

 protected:
  uint32_t max_freq_mhz_{0};
  uint32_t min_freq_mhz_{0};
};

}  // namespace esphome::power_management
