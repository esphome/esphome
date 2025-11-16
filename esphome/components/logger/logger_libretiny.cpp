#ifdef USE_LIBRETINY
#include "logger.h"

namespace esphome::logger {

static const char *const TAG = "logger";

void Logger::pre_setup() {
  if (this->baud_rate_ > 0) {
    switch (this->uart_) {
#if LT_HW_UART0
      case UART_SELECTION_UART0:
        this->hw_serial_ = &Serial0;
        Serial0.begin(this->baud_rate_);
        break;
#endif
#if LT_HW_UART1
      case UART_SELECTION_UART1:
        this->hw_serial_ = &Serial1;
        Serial1.begin(this->baud_rate_);
        break;
#endif
#if LT_HW_UART2
      case UART_SELECTION_UART2:
        this->hw_serial_ = &Serial2;
        Serial2.begin(this->baud_rate_);
        break;
#endif
      default:
        this->hw_serial_ = &Serial;
        Serial.begin(this->baud_rate_);
        if (this->uart_ != UART_SELECTION_DEFAULT) {
          ESP_LOGW(TAG, "  The chosen logger UART port is not available on this board."
                        "The default port was used instead.");
        }
        break;
    }

    // change lt_log() port to match default Serial
    if (this->uart_ == UART_SELECTION_DEFAULT) {
      this->uart_ = (UARTSelection) (LT_UART_DEFAULT_SERIAL + 1);
      lt_log_set_port(LT_UART_DEFAULT_SERIAL);
    } else {
      lt_log_set_port(this->uart_ - 1);
    }
  }

  global_logger = this;
  ESP_LOGI(TAG, "Log initialized");
}

void HOT Logger::write_msg_(const char *msg, size_t len) { this->hw_serial_->write(msg, len); }

const LogString *Logger::get_uart_selection_() {
  switch (this->uart_) {
    case UART_SELECTION_DEFAULT:
      return LOG_STR("DEFAULT");
    case UART_SELECTION_UART0:
      return LOG_STR("UART0");
    case UART_SELECTION_UART1:
      return LOG_STR("UART1");
    case UART_SELECTION_UART2:
    default:
      return LOG_STR("UART2");
  }
}

}  // namespace esphome::logger

#endif  // USE_LIBRETINY
