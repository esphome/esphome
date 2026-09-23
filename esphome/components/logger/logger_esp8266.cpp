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

#if !defined(USE_ESP8266_LOGGER_SERIAL) && !defined(USE_ESP8266_LOGGER_SERIAL1)
// With serial logging disabled, ROM ets_putc still writes to the physical UART0
// at whatever baud rate a uart bus configured there; uart_set_debug(UART_NO)
// only silences the installable putc1 hook, not ets_putc itself. Blocking
// writes at a low baud rate (for example 4800 for a power monitoring chip) can
// starve the soft watchdog. All linked callers (newlib stdout, lwIP
// diagnostics, postmortem dumps) are redirected here by -Wl,--wrap=ets_putc.
// IRAM_ATTR because the ROM original is callable with the flash cache
// disabled (for example from newlib's _write_r, which is placed in IRAM).
extern "C" void IRAM_ATTR __wrap_ets_putc(char) {}
#endif

#endif
