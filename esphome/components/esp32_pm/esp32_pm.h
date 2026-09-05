#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/components/power_management/power_management.h"
// need this header to be able to run esp_pm_dump_locks from yaml lambda
#include "esp_pm.h"

namespace esphome::esp32_pm {

class ESP32PowerManagement : public power_management::PowerManagementComponent {
 public:
  float get_setup_priority() const override { return setup_priority::POWER; }
  void setup() override;
  void dump_config() override;
  void set_max_freq_mhz(uint32_t max_freq_mhz) { this->max_freq_mhz_ = max_freq_mhz; }
  void set_min_freq_mhz(uint32_t min_freq_mhz) { this->min_freq_mhz_ = min_freq_mhz; }

 protected:
  uint32_t max_freq_mhz_{0};
  uint32_t min_freq_mhz_{0};
  int applied_max_freq_mhz_{0};
  int applied_min_freq_mhz_{0};
};

}  // namespace esphome::esp32_pm

#endif
