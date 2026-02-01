#ifdef USE_LIBRETINY
#include "logger.h"

namespace esphome::logger {

#if LT_HW_UART0
static const FixedVector<pin_size_t> pins_serial0_tx(PINS_SERIAL0_TX);
static const FixedVector<pin_size_t> pins_serial0_rx(PINS_SERIAL0_RX);
#endif

#if LT_HW_UART1
static const FixedVector<pin_size_t> pins_serial1_tx(PINS_SERIAL1_TX);
static const FixedVector<pin_size_t> pins_serial1_rx(PINS_SERIAL1_RX);
#endif

#if LT_HW_UART2
static const FixedVector<pin_size_t> pins_serial2_tx(PINS_SERIAL2_TX);
static const FixedVector<pin_size_t> pins_serial2_rx(PINS_SERIAL2_RX);
#endif

static const char *const TAG = "logger";

void Logger::pre_setup() {
  if (this->baud_rate_ > 0) {
    switch (this->uart_) {
#if LT_HW_UART0
      case UART_SELECTION_UART0:
        this->hw_serial_ = &Serial0;
        this->hardware_idx_ = 0;
        Serial0.begin(this->baud_rate_, SERIAL_8N1, pins_serial0_rx[0], pins_serial0_tx[0]);
        break;
#endif
#if LT_HW_UART1
      case UART_SELECTION_UART1:
        this->hw_serial_ = &Serial1;
        this->hardware_idx_ = 1;
        Serial1.begin(this->baud_rate_, SERIAL_8N1, pins_serial1_rx[0], pins_serial1_tx[0]);
        break;
#endif
#if LT_HW_UART2
      case UART_SELECTION_UART2:
        this->hw_serial_ = &Serial2;
        this->hardware_idx_ = 2;
        Serial2.begin(this->baud_rate_, SERIAL_8N1, pins_serial2_rx[0], pins_serial2_tx[0]);
        break;
#endif
      default:
        this->hw_serial_ = &Serial;
        this->hardware_idx_ = LT_UART_DEFAULT_SERIAL;
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
