#ifdef USE_ESP8266
#include "logger.h"
#include "esphome/core/defines.h"
#ifdef USE_ESP8266_CRASH_HANDLER
#include "esphome/components/esp8266/crash_handler.h"
#endif
#include "esphome/core/log.h"

#include <esp8266_peri.h>
#include <uart.h>

namespace esphome::logger {

ESPHOME_LOG_TAG(TAG, "logger");

void Logger::pre_setup() {
#if defined(USE_ESP8266_LOGGER_SERIAL)
  this->hw_serial_ = &Serial;
  Serial.begin(this->baud_rate_);
#ifdef USE_LOGGER_UART_SELECTION_UART0_SWAP
  Serial.swap();
#endif
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

void HOT Logger::write_msg_(const char *msg, uint16_t len) {
#if defined(USE_ESP8266_LOGGER_SERIAL) || defined(USE_ESP8266_LOGGER_SERIAL1)
#ifdef USE_ESP8266_LOGGER_SERIAL
  static constexpr int LOGGER_UART = UART0;
#else
  static constexpr int LOGGER_UART = UART1;
#endif
  // The core writes one byte per call with a FIFO poll and a yield check, ~100 cycles a byte. A line that fits the
  // free FIFO space goes in directly; otherwise the core's write waits for the wire as before.
  const uint32_t used = (USS(LOGGER_UART) >> USTXC) & 0xff;
  if (used + len < UART_TX_FIFO_SIZE) {  // the core's writer also stops one short of a full FIFO
    for (uint16_t i = 0; i < len; i++)
      USF(LOGGER_UART) = static_cast<uint8_t>(msg[i]);
    return;
  }
#endif
  this->hw_serial_->write(msg, len);
}

#ifdef USE_CRASH_HANDLER
bool crash_log_pending() { return esp8266::crash_handler_has_data(); }
void log_crash_to_client() { esp8266::crash_handler_log(); }
#endif

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
