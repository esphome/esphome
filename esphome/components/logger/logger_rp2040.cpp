#ifdef USE_RP2040
#include "logger.h"
#include "esphome/core/log.h"

namespace esphome::logger {

static const char *const TAG = "logger";

void Logger::pre_setup() {
  if (this->baud_rate_ > 0) {
    switch (this->uart_) {
      case UART_SELECTION_UART0:
        this->hw_serial_ = &Serial1;
        Serial1.begin(this->baud_rate_);
        break;
      case UART_SELECTION_UART1:
        this->hw_serial_ = &Serial2;
        Serial2.begin(this->baud_rate_);
        break;
      case UART_SELECTION_USB_CDC:
        this->hw_serial_ = &Serial;
        Serial.begin(this->baud_rate_);
        break;
    }
  }
  global_logger = this;
  ESP_LOGI(TAG, "Log initialized");
}

void HOT Logger::write_msg_(const char *msg, size_t) { this->hw_serial_->println(msg); }

const LogString *Logger::get_uart_selection_() {
  switch (this->uart_) {
    case UART_SELECTION_UART0:
      return LOG_STR("UART0");
    case UART_SELECTION_UART1:
      return LOG_STR("UART1");
#ifdef USE_LOGGER_USB_CDC
    case UART_SELECTION_USB_CDC:
      return LOG_STR("USB_CDC");
#endif
    default:
      return LOG_STR("UNKNOWN");
  }
}

}  // namespace esphome::logger
#endif  // USE_RP2040
