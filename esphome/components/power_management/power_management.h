#pragma once

#include "esphome/core/component.h"

#ifdef USE_ESP_IDF
#include "esp_private/esp_clk.h"
#include "esp_pm.h"
#include "soc/rtc.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <mutex>
#endif

namespace esphome {
namespace power_management {

enum PowerManagementLockType {
  TMR = 0,  // CPU lock used with timer
  CPU = 1,
  APB = 2,
  SLP = 3,
};
const char *power_manager_type_to_string(PowerManagementLockType type);

// add to this enum if additional users are needed, used for logging
enum PowerManagementLockUser : uint8_t {
  UNKNOWN = 0,
  PM = 1,
  ACTION = 2,
  API = 3,
  OT = 4,
};
const char *power_manager_user_to_string(PowerManagementLockUser user);

class PowerManagement : public Component {
 public:
  float get_setup_priority() const override { return setup_priority::BUS; }
  void setup() override;
  void dump_config() override;
  void set_timer_lock_duration(uint32_t timer_lock_duration) { this->timer_lock_duration_ = timer_lock_duration; }
  void set_max_freq_mhz(uint32_t max_freq_mhz) { this->max_freq_mhz_ = max_freq_mhz; }
  void set_min_freq_mhz(uint32_t min_freq_mhz) { this->min_freq_mhz_ = min_freq_mhz; }
  void acquire_lock(PowerManagementLockUser user, PowerManagementLockType lt);
  void release_lock(PowerManagementLockUser user, PowerManagementLockType lt);
#ifdef USE_ESP_IDF
  static void timer_callback(TimerHandle_t xTimer);
#endif
 protected:
#ifdef USE_ESP_IDF
  mutable std::mutex pm_lock_mutex_;
  static const uint8_t pm_lock_array_size_ = 4;
  esp_pm_lock_handle_t pm_lock_handles_[pm_lock_array_size_];
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
  // match with PowerManagementLockType
  esp_pm_lock_type_t pm_lock_types_[pm_lock_array_size_] = {ESP_PM_CPU_FREQ_MAX, ESP_PM_CPU_FREQ_MAX,
                                                            ESP_PM_APB_FREQ_MAX, ESP_PM_NO_LIGHT_SLEEP};
#else
  // substitute apb for slp
  esp_pm_lock_type_t pm_lock_types_[pm_lock_array_size_] = {ESP_PM_CPU_FREQ_MAX, ESP_PM_CPU_FREQ_MAX,
                                                            ESP_PM_APB_FREQ_MAX, ESP_PM_APB_FREQ_MAX};
#endif

#endif
  uint32_t timer_lock_duration_{0};
  uint32_t max_freq_mhz_{0};
  uint32_t min_freq_mhz_{0};
};

extern PowerManagement *global_pm;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace power_management
}  // namespace esphome
