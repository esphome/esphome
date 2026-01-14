#include "power_management.h"
#ifdef USE_ESP32
#include "esphome/core/log.h"

namespace esphome::power_management {

static const char *const TAG = "power_management";

void PowerManagement::setup() {
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
    ESP_LOGE(TAG, "Failed esp_pm_configure %d", rc);
    this->mark_failed();
    return;
  }

#ifdef USE_POWER_MANAGEMENT
  // Create Locks
  for (uint8_t i = 0; i < PowerManagement::PM_LOCK_ARRAY_SIZE; i++) {
    rc = esp_pm_lock_create(this->pm_lock_types_[i], 0, power_manager_type_to_string((PowerManagementLockType) i),
                            &(this->pm_lock_handles_[i]));
    if (rc != ESP_OK) {
      esp_pm_lock_delete(this->pm_lock_handles_[i]);
      this->pm_lock_handles_[i] = NULL;
      ESP_LOGE(TAG, "Failed esp_pm_lock_create %s %d", power_manager_type_to_string((PowerManagementLockType) i), rc);
      return;
    }
  }
#endif
}

#ifdef USE_POWER_MANAGEMENT
// Thread Safe
void PowerManagement::acquire_lock(PowerManagementLockType lt) {
  if (this->is_ready()) {
    std::lock_guard<std::mutex> lock(this->pm_lock_mutex_);
    esp_err_t rc = esp_pm_lock_acquire(this->pm_lock_handles_[lt]);
    if (rc != ESP_OK) {
      ESP_LOGE(TAG, "Failed esp_pm_lock_acquire %s %d", power_manager_type_to_string(lt), rc);
      return;
    }
    ESP_LOGD(TAG, "Acquired pm lock: %s", power_manager_type_to_string(lt));
  }
}

// Thread Safe
void PowerManagement::release_lock(PowerManagementLockType lt) {
  if (this->is_ready()) {
    std::lock_guard<std::mutex> lock(this->pm_lock_mutex_);
    esp_err_t rc = esp_pm_lock_release(this->pm_lock_handles_[lt]);
    if (rc != ESP_OK) {
      ESP_LOGE(TAG, "Failed esp_pm_lock_release %s %d", power_manager_type_to_string(lt), rc);
      return;
    }
    ESP_LOGD(TAG, "Released pm lock: %s", power_manager_type_to_string(lt));
  }
}
#endif

void PowerManagement::dump_config() {
  ESP_LOGCONFIG(TAG, "Power Management:");
#ifdef USE_POWER_MANAGEMENT
  ESP_LOGCONFIG(TAG, "  EspHome Locks available");
#endif
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
#if CONFIG_PM_PROFILING
  ESP_LOGCONFIG(TAG, "  PM Profiling Enabled");
#endif
#if CONFIG_PM_TRACE
  ESP_LOGCONFIG(TAG, "  PM Trace Enabled");
#endif
#endif
}

}  // namespace esphome::power_management
#endif
