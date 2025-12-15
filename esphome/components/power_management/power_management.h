#pragma once

#include "esphome/core/component.h"

#ifdef USE_ESP_IDF
#include "esp_private/esp_clk.h"
#include "esp_pm.h"
#include "soc/rtc.h"
#include "esp_sleep.h"
#include <mutex>
#endif

namespace esphome {
namespace power_management {

enum PowerManagementLockType {
  CPU = 0,
  APB = 1,
  SLP = 2,
};
const char *power_manager_type_to_string(PowerManagementLockType type);

class PowerManagement : public Component {
 public:
  float get_setup_priority() const override { return setup_priority::BUS; }
  void setup() override;
  void dump_config() override;
  void set_max_freq_mhz(uint32_t max_freq_mhz) { this->max_freq_mhz_ = max_freq_mhz; }
  void set_min_freq_mhz(uint32_t min_freq_mhz) { this->min_freq_mhz_ = min_freq_mhz; }
  void acquire_lock(PowerManagementLockType lt);
  void release_lock(PowerManagementLockType lt);
#ifdef USE_ESP_IDF
  static const uint8_t PM_LOCK_ARRAY_SIZE = 3;
#endif
 protected:
#ifdef USE_ESP_IDF
  mutable std::mutex pm_lock_mutex_;
  esp_pm_lock_handle_t pm_lock_handles_[PM_LOCK_ARRAY_SIZE];
  // match with PowerManagementLockType
  esp_pm_lock_type_t pm_lock_types_[PM_LOCK_ARRAY_SIZE] = {ESP_PM_CPU_FREQ_MAX, ESP_PM_APB_FREQ_MAX,
                                                           ESP_PM_NO_LIGHT_SLEEP};
#endif
  uint32_t max_freq_mhz_{0};
  uint32_t min_freq_mhz_{0};
};

}  // namespace power_management
}  // namespace esphome
