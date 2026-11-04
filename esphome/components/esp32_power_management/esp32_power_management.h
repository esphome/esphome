#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/components/power_management/power_management.h"
// Lets a YAML lambda call esp_pm_dump_locks().
#include "esp_pm.h"

namespace esphome::esp32_power_management {

class ESP32PowerManagement : public power_management::PowerManagementComponent {
 public:
  float get_setup_priority() const override { return setup_priority::POWER; }
  void setup() override;
  void dump_config() override;
  void set_max_freq_mhz(uint16_t max_freq_mhz) { this->max_freq_mhz_ = max_freq_mhz; }
  void set_min_freq_mhz(uint16_t min_freq_mhz) { this->min_freq_mhz_ = min_freq_mhz; }

 protected:
  // Zero means the build default; setup() replaces it with the value applied.
  uint16_t max_freq_mhz_{0};
  uint16_t min_freq_mhz_{0};
};

}  // namespace esphome::esp32_power_management

#endif
