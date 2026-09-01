#include "esphome/core/log.h"
#include "esp32_pm.h"
#ifdef CONFIG_OPENTHREAD_MTD
#include "esphome/components/openthread/openthread.h"
#endif

namespace esphome::esp32_pm {

static const char *const TAG = "esp32_pm";

void ESP32PowerManagement::setup() {
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
  if (rc != ESP_OK) {
    ESP_LOGE(TAG, "Failed esp_pm_configure %d", rc);
    this->mark_failed();
    return;
  }

  soc_rtc_slow_clk_src_t slow_clk_src = rtc_clk_slow_src_get();
  if (slow_clk_src != SOC_RTC_SLOW_CLK_SRC_XTAL32K) {
    ESP_LOGI(TAG, "32k XTAL not in use");
  } else {
    ESP_LOGI(TAG, "32k XTAL in use");
  }
}

void ESP32PowerManagement::dump_config() {
  ESP_LOGCONFIG(TAG, "Power Management:");
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

}  // namespace esphome::esp32_pm
