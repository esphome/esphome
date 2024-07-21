#pragma once

#ifdef USE_ESP_IDF

#include "modem_component.h"

#include <cxx_include/esp_modem_api.hpp>

#include <esp_idf_version.h>
#include <esp_task_wdt.h>

#define ESPHL_ERROR_CHECK(err, message) \
  if ((err) != ESP_OK) { \
    ESP_LOGE(TAG, message ": (%d) %s", err, esp_err_to_name(err)); \
    this->mark_failed(); \
    return; \
  }

namespace esphome {
namespace modem {

std::string command_result_to_string(command_result err);

std::string state_to_string(ModemComponentState state);

// While instanciated, will set the WDT to timeout_s
// When deleted, will restore default WDT
class Watchdog {
 public:
  Watchdog(int timeout_s) { this->set_wdt_(timeout_s); }

  ~Watchdog() { this->set_wdt_(CONFIG_TASK_WDT_TIMEOUT_S); }

 private:
  void set_wdt_(uint32_t timeout_s) {
#if ESP_IDF_VERSION_MAJOR >= 5
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = timeout_s * 1000,
        .idle_core_mask = 0x03,
        .trigger_panic = true,
    };
    esp_task_wdt_reconfigure(&wdt_config);
#else
    esp_task_wdt_init(timeout_s, true);
#endif  // ESP_IDF_VERSION_MAJOR
  }
};

}  // namespace modem
}  // namespace esphome
#endif  // USE_ESP_IDF