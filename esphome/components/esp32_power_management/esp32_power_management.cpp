#include "esp32_power_management.h"
#ifdef USE_ESP32
#include "esphome/core/log.h"

#include "esp_private/esp_clk.h"
#include "soc/rtc.h"

namespace esphome::esp32_power_management {

ESPHOME_LOG_TAG(TAG, "esp32_power_management");

void ESP32PowerManagement::setup() {
  if (this->max_freq_mhz_ == 0)
    this->max_freq_mhz_ = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
  if (this->min_freq_mhz_ == 0)
    this->min_freq_mhz_ = esp_clk_xtal_freq() / MHZ;
  // A low boot frequency (ESP32-H2 at 16 MHz) can sit below the crystal frequency.
  if (this->min_freq_mhz_ > this->max_freq_mhz_)
    this->min_freq_mhz_ = this->max_freq_mhz_;

  esp_pm_config_t pm_config = {
      .max_freq_mhz = this->max_freq_mhz_,
      .min_freq_mhz = this->min_freq_mhz_,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
      .light_sleep_enable = true,
#endif
  };
  esp_err_t rc = esp_pm_configure(&pm_config);
  if (rc != ESP_OK) {
    // The usual cause is a minimum frequency that does not divide this board's crystal
    ESP_LOGE(TAG, "esp_pm_configure failed: %s (crystal %u MHz)", esp_err_to_name(rc),
             static_cast<unsigned>(esp_clk_xtal_freq() / MHZ));
    this->mark_failed();
  }
}

void ESP32PowerManagement::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Power Management:\n"
                "  Max Frequency: %uMHZ\n"
                "  Min Frequency: %uMHZ",
                this->max_freq_mhz_, this->min_freq_mhz_);
#if SOC_CLK_XTAL32K_SUPPORTED
  ESP_LOGCONFIG(TAG, "  32k XTAL: %s",
                rtc_clk_slow_src_get() == SOC_RTC_SLOW_CLK_SRC_XTAL32K ? LOG_STR_LITERAL("in use")
                                                                       : LOG_STR_LITERAL("not in use"));
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
#if defined(CONFIG_LWIP_IPV6) && !defined(CONFIG_LWIP_ND6)
  ESP_LOGCONFIG(TAG, "  IPv6 Neighbor Discovery Disabled (CONFIG_LWIP_ND6)");
#endif
#if CONFIG_USJ_NO_AUTO_LS_ON_CONNECTION
  ESP_LOGCONFIG(TAG, "  USB Serial/JTAG Light Sleep Prevention Enabled");
#endif
#endif
}

}  // namespace esphome::esp32_power_management
#endif
