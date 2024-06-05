#include "watchdog.h"

#ifdef USE_HTTP_REQUEST_OTA_WATCHDOG_TIMEOUT

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cstdint>
#ifdef USE_ESP32
#include "esp_idf_version.h"
#include "esp_task_wdt.h"
#endif
#ifdef USE_RP2040
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#endif

namespace esphome {
namespace http_request {
namespace watchdog {

static const char *const TAG = "watchdog.http_request.ota";

WatchdogManager::WatchdogManager() {
  this->saved_timeout_ms_ = this->get_timeout_();
  this->set_timeout_(USE_HTTP_REQUEST_OTA_WATCHDOG_TIMEOUT);
}

WatchdogManager::~WatchdogManager() { this->set_timeout_(this->saved_timeout_ms_); }

void WatchdogManager::set_timeout_(uint32_t timeout_ms) {
  ESP_LOGV(TAG, "Adjusting WDT to %" PRIu32 "ms", timeout_ms);
#ifdef USE_ESP32
#if ESP_IDF_VERSION_MAJOR >= 5
  esp_task_wdt_config_t wdt_config = {
      .timeout_ms = timeout_ms,
      .idle_core_mask = 0x03,
      .trigger_panic = true,
  };
  esp_task_wdt_reconfigure(&wdt_config);
#else
  esp_task_wdt_init(timeout_ms, true);
#endif  // ESP_IDF_VERSION_MAJOR
#endif  // USE_ESP32

#ifdef USE_RP2040
  watchdog_enable(timeout_ms, true);
#endif
}

uint32_t WatchdogManager::get_timeout_() {
  uint32_t timeout_ms = 0;

#ifdef USE_ESP32
  timeout_ms = (uint32_t) CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000;
#endif  // USE_ESP32

#ifdef USE_RP2040
  timeout_ms = watchdog_get_count() / 1000;
#endif

  ESP_LOGVV(TAG, "get_timeout: %" PRIu32 "ms", timeout_ms);

  return timeout_ms;
}

}  // namespace watchdog
}  // namespace http_request
}  // namespace esphome
#endif
