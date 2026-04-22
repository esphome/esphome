#ifdef USE_ESP8266
#include "logger.h"
#include "esphome/core/defines.h"
#ifdef USE_ESP8266_CRASH_HANDLER
#include "esphome/components/esp8266/crash_handler.h"
#endif
#include "esphome/core/log.h"

namespace esphome::logger {

static const char *const TAG = "logger";

void Logger::pre_setup() {
#if defined(USE_ESP8266_LOGGER_SERIAL)
  this->hw_serial_ = &Serial;
  Serial.begin(this->baud_rate_);
  if (this->uart_ == UART_SELECTION_UART0_SWAP) {
    Serial.swap();
  }
  Serial.setDebugOutput(ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE);
#elif defined(USE_ESP8266_LOGGER_SERIAL1)
  this->hw_serial_ = &Serial1;
  Serial1.begin(this->baud_rate_);
  Serial1.setDebugOutput(ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE);
#else
  // No serial logging - disable debug output
  uart_set_debug(UART_NO);
#endif

  global_logger = this;

  ESP_LOGI(TAG, "Log initialized");
#ifdef USE_ESP8266_CRASH_HANDLER
  esp8266::crash_handler_log();
#endif
}

const LogString *Logger::get_uart_selection_() {
#if defined(USE_ESP8266_LOGGER_SERIAL)
  if (this->uart_ == UART_SELECTION_UART0_SWAP) {
    return LOG_STR("UART0_SWAP");
  }
  return LOG_STR("UART0");
#elif defined(USE_ESP8266_LOGGER_SERIAL1)
  return LOG_STR("UART1");
#else
  return LOG_STR("NONE");
#endif
}

}  // namespace esphome::logger
#endif
