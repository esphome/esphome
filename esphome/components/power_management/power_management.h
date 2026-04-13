#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#ifdef USE_ESP32
#include "esp_private/esp_clk.h"
#include "esp_pm.h"
#include "soc/rtc.h"
#include "esp_sleep.h"
#include <mutex>
#endif

namespace esphome::power_management {

#ifdef USE_POWER_MANAGEMENT
enum PowerManagementLockType {
  CPU = 0,
  APB = 1,
  SLP = 2,
};
const char *power_manager_type_to_string(PowerManagementLockType type);
#endif

class PowerManagement : public Component {
 public:
  float get_setup_priority() const override { return setup_priority::BUS; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_max_freq_mhz(uint32_t max_freq_mhz) { this->max_freq_mhz_ = max_freq_mhz; }
  void set_min_freq_mhz(uint32_t min_freq_mhz) { this->min_freq_mhz_ = min_freq_mhz; }
#ifdef USE_POWER_MANAGEMENT
  void acquire_lock(PowerManagementLockType lt);
  void release_lock(PowerManagementLockType lt);
#ifdef USE_ESP32
  static const uint8_t PM_LOCK_ARRAY_SIZE = 3;
#endif
#endif
#ifdef USE_ESP32
  TaskHandle_t task_handle{nullptr};
  bool is_delay_aborted{false};
#endif
 protected:
#ifdef USE_ESP32
  bool ready_to_sleep_();
#ifdef USE_POWER_MANAGEMENT
  mutable std::mutex pm_lock_mutex_;
  esp_pm_lock_handle_t pm_lock_handles_[PM_LOCK_ARRAY_SIZE];
  // match with PowerManagementLockType
  esp_pm_lock_type_t pm_lock_types_[PM_LOCK_ARRAY_SIZE] = {ESP_PM_CPU_FREQ_MAX, ESP_PM_APB_FREQ_MAX,
                                                           ESP_PM_NO_LIGHT_SLEEP};
#endif
#if CONFIG_PM_LIGHT_SLEEP_CALLBACKS
  uint32_t task_notify_take_timeout_ms_{(uint32_t) (CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000) - 100};
#endif
#endif
  uint32_t max_freq_mhz_{0};
  uint32_t min_freq_mhz_{0};
};

}  // namespace esphome::power_management
