#include "watchdog.h"

#include "esphome/core/application.h"
#include "esphome/core/defines.h"
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
#ifdef USE_ESP8266
#include "Esp.h"
#endif

namespace esphome {
namespace http_request {
namespace watchdog {

uint32_t Watchdog::timeout_ms = 0;                             // NOLINT
uint32_t Watchdog::init_timeout_ms = Watchdog::get_timeout();  // NOLINT

void Watchdog::set_timeout(uint32_t timeout_ms) {
  ESP_LOGV(TAG, "set_timeout: %" PRIu32 "ms", timeout_ms);
#ifdef USE_ESP8266
  EspClass::wdtEnable(timeout_ms);
#endif  // USE_ESP8266

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
  Watchdog::timeout_ms = timeout_ms;
}

uint32_t Watchdog::get_timeout() {
  uint32_t timeout_ms = 0;

#ifdef USE_ESP32
  timeout_ms = std::max((uint32_t) CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000, Watchdog::timeout_ms);
#endif  // USE_ESP32

#ifdef USE_RP2040
  timeout_ms = watchdog_get_count() / 1000;
#endif

  if (timeout_ms == 0) {
    // fallback to stored timeout
    timeout_ms = Watchdog::timeout_ms;
  }
  ESP_LOGVV(TAG, "get_timeout: %" PRIu32 "ms", timeout_ms);

  return timeout_ms;
}

void Watchdog::reset() {
  ESP_LOGV(TAG, "reset");
  Watchdog::set_timeout(Watchdog::init_timeout_ms);
}

WatchdogSupervisor::WatchdogSupervisor() {
#ifdef USE_HTTP_REQUEST_OTA_WATCHDOG_TIMEOUT
  Watchdog::set_timeout(USE_HTTP_REQUEST_OTA_WATCHDOG_TIMEOUT);
#endif
}

WatchdogSupervisor::~WatchdogSupervisor() {
#ifdef USE_HTTP_REQUEST_OTA_WATCHDOG_TIMEOUT
  Watchdog::reset();
#endif
}

}  // namespace watchdog
}  // namespace http_request
}  // namespace esphome
