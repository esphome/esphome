#ifdef USE_ESP8266
#include "logger.h"
#include "esphome/core/log.h"

// Direct UART register access for optimized write path
// Arduino's Serial.write() (uart.cpp lines 545-548) has significant overhead:
// - PROGMEM read for every character (unnecessary for RAM buffers)
// - optimistic_yield(10000) after every character (scheduler overhead)
// - Per-character FIFO checks via uart_do_write_char() (line 513)
// Direct register writes with batching eliminate ~300 function calls per 100-byte message.
#include <esp8266_peri.h>  // USF, USS, USTXC register macros

namespace esphome::logger {

static const char *const TAG = "logger";

// UART TX FIFO size is 128 bytes, use 0x7F as threshold
static constexpr uint8_t UART_TX_FIFO_THRESHOLD = 0x7F;

// Yield timeout in microseconds - matches Arduino's uart_write behavior
static constexpr uint32_t YIELD_TIMEOUT_US = 10000UL;

// Determine UART number at compile time
#if defined(USE_ESP8266_LOGGER_SERIAL)
static const uint8_t LOGGER_UART_NUM = 0;
#elif defined(USE_ESP8266_LOGGER_SERIAL1)
static const uint8_t LOGGER_UART_NUM = 1;
#endif

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
}

void HOT Logger::write_msg_(const char *msg, size_t len) {
#if defined(USE_ESP8266_LOGGER_SERIAL) || defined(USE_ESP8266_LOGGER_SERIAL1)
  // Direct FIFO writes with batching - much faster than Arduino's per-character approach
  // Arduino's uart_write() calls optimistic_yield() after EVERY character,
  // but we can burst up to 127 bytes at once and only yield when FIFO is actually full.
  while (len > 0) {
    // Check current FIFO level (USTXC field at bits 16-23)
    uint8_t fifo_cnt = (USS(LOGGER_UART_NUM) >> USTXC) & 0xFF;

    if (fifo_cnt >= UART_TX_FIFO_THRESHOLD) {
      // FIFO full - yield once and retry
      optimistic_yield(YIELD_TIMEOUT_US);
      continue;
    }

    // Burst write to FIFO - no function calls, no PROGMEM overhead
    size_t fifo_free = UART_TX_FIFO_THRESHOLD - fifo_cnt;
    size_t to_write = len < fifo_free ? len : fifo_free;
    len -= to_write;

    while (to_write--) {
      USF(LOGGER_UART_NUM) = *msg++;
    }
  }
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
