#include "power_management.h"
#ifdef USE_ESP_IDF
#include "esphome/core/log.h"

namespace esphome {
namespace power_management {

static const char *TAG = "power_management";

// static only use is in setup
void PowerManagement::timer_callback(TimerHandle_t xTimer) {
  void *context = pvTimerGetTimerID(xTimer);
  PowerManagement *obj = (PowerManagement *) context;
  obj->release_lock(PowerManagementLockUser::PM, PowerManagementLockType::CPU);
}

void PowerManagement::setup() {
  power_management::global_pm = this;
  esp_err_t rc = ESP_OK;
  // Configure PM
  int max_freq_mhz = this->max_freq_mhz_ > 0 ? this->max_freq_mhz_ : CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
  int min_freq_mhz = this->min_freq_mhz_ > 0 ? this->min_freq_mhz_ : esp_clk_xtal_freq() / MHZ;

  esp_pm_config_t pm_config = {
    .max_freq_mhz = max_freq_mhz,
    .min_freq_mhz = min_freq_mhz,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
    .light_sleep_enable = true
#endif
  };
  ESP_LOGI(TAG, "PM Max Freq: %dMHZ", pm_config.max_freq_mhz);
  ESP_LOGI(TAG, "PM Min Freq: %dMHZ", pm_config.min_freq_mhz);
  ESP_LOGI(TAG, "PM Light Sleep Enable: %s", pm_config.light_sleep_enable ? "true" : "false");

  rc = esp_pm_configure(&pm_config);
  if (rc != 0) {
    this->mark_failed();
    ESP_LOGE(TAG, "Failed esp_pm_configure %d", rc);
    return;
  }

  // Create Locks
  for (uint8_t i = 0; i < this->pm_lock_array_size_; i++) {
    rc = esp_pm_lock_create(this->pm_lock_types_[i], 0, power_manager_type_to_string((PowerManagementLockType) i),
                            &(this->pm_lock_handles_[i]));
    if (rc != ESP_OK) {
      esp_pm_lock_delete(this->pm_lock_handles_[i]);
      this->pm_lock_handles_[i] = NULL;
      ESP_LOGE(TAG, "Failed esp_pm_lock_create %s %d", power_manager_type_to_string((PowerManagementLockType) i), rc);
      return;
    }
  }

  if (this->timer_lock_duration_ > 0) {
    // Acquire Initial Lock
    this->acquire_lock(PowerManagementLockUser::PM, PowerManagementLockType::CPU);
    TimerHandle_t xTimer = xTimerCreate("PM Sleep Timer", pdMS_TO_TICKS(this->timer_lock_duration_), pdFALSE, this,
                                        PowerManagement::timer_callback);
    xTimerStart(xTimer, 0);
  }
}

// Thread Safe
void PowerManagement::acquire_lock(PowerManagementLockUser user, PowerManagementLockType lt) {
  if (this->is_ready()) {
    std::lock_guard<std::mutex> lock(this->pm_lock_mutex_);
    esp_err_t rc = esp_pm_lock_acquire(this->pm_lock_handles_[lt]);
    if (rc != ESP_OK) {
      ESP_LOGE(TAG, "Failed esp_pm_lock_acquire %s %d", power_manager_type_to_string(lt), rc);
    }
    ESP_LOGD(TAG, "Acquired pm lock: %s, user: %s", power_manager_type_to_string(lt),
             power_manager_user_to_string(user));
    if (lt == PowerManagementLockType::TMR && this->timer_lock_duration_ > 0) {
      TimerHandle_t xTimer = xTimerCreate("PM Sleep Timer", pdMS_TO_TICKS(this->timer_lock_duration_), pdFALSE, this,
                                          PowerManagement::timer_callback);
      xTimerStart(xTimer, 0);
    }
  }
}

// Thread Safe
void PowerManagement::release_lock(PowerManagementLockUser user, PowerManagementLockType lt) {
  if (this->is_ready()) {
    std::lock_guard<std::mutex> lock(this->pm_lock_mutex_);
    esp_err_t rc = esp_pm_lock_release(this->pm_lock_handles_[lt]);
    if (rc != ESP_OK) {
      ESP_LOGE(TAG, "Failed esp_pm_lock_release %s %d", power_manager_type_to_string(lt), rc);
    }
    ESP_LOGD(TAG, "Released pm lock: %s, user: %s", power_manager_type_to_string(lt),
             power_manager_user_to_string(user));
  }
}

void PowerManagement::dump_config() {
  ESP_LOGCONFIG(TAG, "Power Management:");
  uint32_t duration = this->timer_lock_duration_ / 1000;
  ESP_LOGCONFIG(TAG, "  Timer Lock Duration: %" PRIu32 "s", duration);
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
  ESP_LOGCONFIG(TAG, "  Light Sleep Enabled");
#if CONFIG_ESP_SLEEP_POWER_DOWN_FLASH
  ESP_LOGCONFIG(TAG, "  PM Flash Power Down in Light Sleep Enabled");
#endif
#if CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
  ESP_LOGCONFIG(TAG, "  PM Peripheral Power Down in Light Sleep Enabled");
#endif
#if CONFIG_IEEE802154_SLEEP_ENABLE
  ESP_LOGCONFIG(TAG, "  ieee802154 Sleep Enabled");
#endif
#if CONF_PM_PROFILING
  ESP_LOGCONFIG(TAG, "  PM Profiling Enabled");
#endif
#if CONF_PM_TRACE
  ESP_LOGCONFIG(TAG, "  PM Trace Enabled");
#endif
#endif
}

}  // namespace power_management
}  // namespace esphome
#endif
