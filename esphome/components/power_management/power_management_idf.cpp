#include "power_management.h"
#ifdef USE_ESP32
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <cstdio>

namespace esphome::power_management {

static const char *const TAG = "power_management";

#if CONFIG_PM_LIGHT_SLEEP_CALLBACKS
esp_err_t pm_after_wake_up_callback(int64_t sleep_time_us, void *arg) {
  PowerManagement *obj = (PowerManagement *) arg;
  obj->is_delay_aborted = true;
  xTaskNotifyGive(obj->task_handle);
  return ESP_OK;
}
#endif

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

#if CONFIG_PM_LIGHT_SLEEP_CALLBACKS
  task_handle = xTaskGetCurrentTaskHandle();
  esp_pm_sleep_cbs_register_config_t pm_callbacks = {
      .enter_cb = NULL,
      .exit_cb = pm_after_wake_up_callback,
      .enter_cb_user_arg = NULL,
      .exit_cb_user_arg = this,
      .enter_cb_prior = 1,
      .exit_cb_prior = 1,
  };

  rc = esp_pm_light_sleep_register_cbs(&pm_callbacks);
  if (rc != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register power management callbacks: %s", esp_err_to_name(rc));
  }
#endif
}

void PowerManagement::loop() {
#if CONFIG_PM_LIGHT_SLEEP_CALLBACKS
  if (this->ready_to_sleep_()) {
    this->is_delay_aborted = false;
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(60000));
    if (this->is_delay_aborted) {
      esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
      switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
          ESP_LOGV(TAG, "light sleep wakeup: RTC_IO (EXT0 - single pad)");
          break;
        case ESP_SLEEP_WAKEUP_EXT1:
          ESP_LOGV(TAG, "light sleep wakeup: RTC_IO (EXT1 - multiple pads)");
          break;
        case ESP_SLEEP_WAKEUP_TIMER:
          ESP_LOGV(TAG, "light sleep wakeup: Timer");
          break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
          ESP_LOGV(TAG, "light sleep wakeup: Touchpad");
          break;
        case ESP_SLEEP_WAKEUP_ULP:
          ESP_LOGV(TAG, "light sleep wakeup: ULP program");
          break;
        case ESP_SLEEP_WAKEUP_GPIO:
          ESP_LOGV(TAG, "light sleep wakeup: GPIO (Light Sleep only)");
          break;
        case ESP_SLEEP_WAKEUP_UART:
          ESP_LOGV(TAG, "light sleep wakeup: UART");
          break;
        default:
          ESP_LOGV(TAG, "light sleep wakeup unknown: %d", wakeup_reason);
          break;
      }
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

static const size_t PM_BUF_SIZE = 1024;
// Todo: use pm function esp_pm_get_lock_stats_all when it is available.
bool PowerManagement::ready_to_sleep_() {
  char pm_buffer[PM_BUF_SIZE];
  int32_t acquired = 0;

  FILE *f = fmemopen(pm_buffer, PM_BUF_SIZE, "w");
  if (f == NULL) {
    ESP_LOGE(TAG, "count_pm_locks, fmemopen failed %d", errno);
    return false;
  }

  esp_pm_dump_locks(f);
  fclose(f);

  if (pm_buffer[0] == '\0') {
    ESP_LOGE(TAG, "esp_pm_dump_locks produced no output");
    return false;
  }

  char *line_saveptr;
  char *word_saveptr;
  char *line = strtok_r(pm_buffer, "\n", &line_saveptr);  // NOLINT(clang-analyzer-deadcode.DeadStores)
  while ((line = strtok_r(NULL, "\n", &line_saveptr)) != NULL) {
    if (strncmp(line, "Mode", 4) == 0) {
      break;
    }
    if (strncmp(line, "Name", 4) != 0 && strncmp(line, "Lock", 4) != 0) {
      char *word = strtok_r(line, " ", &word_saveptr);
      for (int i = 0; i < 3; i++) {
        word = strtok_r(NULL, " ", &word_saveptr);
      }
      // at 4th word
      if (word != NULL) {
        acquired += strtol(word, NULL, 10);
      }
    }
  }
  return (acquired < 2);
}

}  // namespace esphome::power_management
#endif
