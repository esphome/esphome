#pragma once
// esp_pm_configure() by hand: 80 MHz max, 10 MHz min, light sleep optional
#include "esp_pm.h"
#include "esphome/core/log.h"
inline void pms_pm_configure(bool light_sleep) {
  esp_pm_config_t cfg = {};
  cfg.max_freq_mhz = 80;
  cfg.min_freq_mhz = 10;
  cfg.light_sleep_enable = light_sleep;
  ESP_LOGW("pmh", "esp_pm_configure(ls=%d) = %d", light_sleep, (int) esp_pm_configure(&cfg));
}
