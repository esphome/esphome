#ifdef USE_LIBRETINY
#include "logger.h"

namespace esphome::logger {

constexpr uint8_t esphome_uart_number_2_lt(UARTSelection uart) {
  switch (uart) {
    case UART_SELECTION_DEFAULT:
      return LT_UART_DEFAULT_SERIAL;
    case UART_SELECTION_UART0:
      return 0;
    case UART_SELECTION_UART1:
      return 1;
    case UART_SELECTION_UART2:
      return 2;
    default:
      return LT_UART_DEFAULT_SERIAL;
  }
}

constexpr UARTSelection lt_uart_number_2_esphome(uint8_t lt_uart) {
  switch (lt_uart) {
    case 0:
      return UART_SELECTION_UART0;
    case 1:
      return UART_SELECTION_UART1;
    case 2:
      return UART_SELECTION_UART2;
    default:
      return UART_SELECTION_DEFAULT;
  }
}

static const char *const TAG = "logger";

void Logger::pre_setup() {
  if (this->baud_rate_ > 0) {
    auto &uart_manager = this->lt_component_->get_uart_manager();
    auto lt_uart = esphome_uart_number_2_lt(this->uart_);
    if (!uart_manager.init_uart_for_logger(lt_uart, this->baud_rate_)) {
      lt_uart = LT_UART_DEFAULT_SERIAL;
      if (!uart_manager.init_uart_for_logger(lt_uart, this->baud_rate_)) {
        LT_E("  Failed to initialize default logger uart UART%d. Logging disabled.", LT_UART_DEFAULT_SERIAL);
        this->baud_rate_ = 0;
        global_logger = this;
        return;
      } else {
        LT_E("  The chosen logger UART port is not available on this board."
             "The default port (%u) will be used instead.",
             LT_UART_DEFAULT_SERIAL);
      }
    }
    this->uart_ = lt_uart_number_2_esphome(lt_uart);
    this->hardware_idx_ = lt_uart;
    this->hw_serial_ = uart_manager.get_hw_serial_by_number(lt_uart);
  }

  ESP_LOGI(TAG, "Log initialized");
  global_logger = this;
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
