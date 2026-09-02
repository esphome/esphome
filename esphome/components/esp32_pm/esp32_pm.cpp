#include "esp32_pm.h"
#ifdef USE_ESP32
#include "esphome/core/log.h"

#include "esp_private/esp_clk.h"
#include "soc/rtc.h"

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
  this->applied_max_freq_mhz_ = max_freq_mhz;
  this->applied_min_freq_mhz_ = min_freq_mhz;

  rc = esp_pm_configure(&pm_config);
  if (rc != ESP_OK) {
    ESP_LOGE(TAG, "Failed esp_pm_configure %s", esp_err_to_name(rc));
    this->mark_failed();
    return;
  }
}

void ESP32PowerManagement::dump_config() {
  ESP_LOGCONFIG(TAG, "Power Management:");
  ESP_LOGCONFIG(TAG, "  Max Frequency: %dMHZ", this->applied_max_freq_mhz_);
  ESP_LOGCONFIG(TAG, "  Min Frequency: %dMHZ", this->applied_min_freq_mhz_);
#if SOC_CLK_XTAL32K_SUPPORTED
  ESP_LOGCONFIG(TAG, "  32k XTAL: %s",
                rtc_clk_slow_src_get() == SOC_RTC_SLOW_CLK_SRC_XTAL32K ? "in use" : "not in use");
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
#if !CONFIG_LWIP_ND6
  ESP_LOGCONFIG(TAG, "  IPv6 Neighbor Discovery Disabled (CONFIG_LWIP_ND6)");
#endif
#endif
}

}  // namespace esphome::esp32_pm
#endif
