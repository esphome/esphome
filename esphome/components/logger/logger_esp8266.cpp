#ifdef USE_ESP8266
#include "logger.h"
#include "esphome/core/log.h"

namespace esphome {
namespace logger {

static const char *const TAG = "logger";

void Logger::pre_setup() {
  if (this->baud_rate_ > 0) {
    switch (this->uart_) {
      case UART_SELECTION_UART0:
      case UART_SELECTION_UART0_SWAP:
        this->hw_serial_ = &Serial;
        Serial.begin(this->baud_rate_);
        if (this->uart_ == UART_SELECTION_UART0_SWAP) {
          Serial.swap();
        }
        Serial.setDebugOutput(ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE);
        break;
      case UART_SELECTION_UART1:
        this->hw_serial_ = &Serial1;
        Serial1.begin(this->baud_rate_);
        Serial1.setDebugOutput(ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE);
        break;
    }
  } else {
    uart_set_debug(UART_NO);
  }

  global_logger = this;

  ESP_LOGI(TAG, "Log initialized");
}

void HOT Logger::write_msg_(const char *msg) { this->hw_serial_->println(msg); }

const char *const UART_SELECTIONS[] = {"UART0", "UART1", "UART0_SWAP"};

const char *Logger::get_uart_selection_() { return UART_SELECTIONS[this->uart_]; }

}  // namespace logger
}  // namespace esphome
#endif
