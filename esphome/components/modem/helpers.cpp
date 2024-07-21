#ifdef USE_ESP_IDF

#include "modem_component.h"
#include "helpers.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include <esp_idf_version.h>
#include <esp_task_wdt.h>

namespace esphome {
namespace modem {

Watchdog::Watchdog(u_int32_t timeout_s) {
  this->timeout_s_ = timeout_s;
  this->start_time_ms_ = millis();
  this->set_wdt_(timeout_s);
  ESP_LOGV(TAG, "Watchog timeout init: %ds", timeout_s);
}

Watchdog::~Watchdog() {
  this->set_wdt_(CONFIG_TASK_WDT_TIMEOUT_S);
  ESP_LOGV(TAG, "Watchog timeout reset to default after %.1fs", float(millis() - this->start_time_ms_) / 1000);
}

void Watchdog::set_wdt_(uint32_t timeout_s) {
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

std::string command_result_to_string(command_result err) {
  std::string res = "UNKNOWN";
  switch (err) {
    case command_result::FAIL:
      res = "FAIL";
      break;
    case command_result::OK:
      res = "OK";
      break;
    case command_result::TIMEOUT:
      res = "TIMEOUT";
  }
  return res;
}

std::string state_to_string(ModemComponentState state) {
  std::string str;
  switch (state) {
    case ModemComponentState::NOT_RESPONDING:
      str = "NOT_RESPONDING";
      break;
    case ModemComponentState::DISCONNECTED:
      str = "DISCONNECTED";
      break;
    case ModemComponentState::CONNECTING:
      str = "CONNECTING";
      break;
    case ModemComponentState::CONNECTED:
      str = "CONNECTED";
      break;
    case ModemComponentState::DISCONNECTING:
      str = "DISCONNECTING";
      break;
    case ModemComponentState::DISABLED:
      str = "DISABLED";
      break;
  }
  return str;
}

}  // namespace modem
}  // namespace esphome
#endif  // USE_ESP_IDF
