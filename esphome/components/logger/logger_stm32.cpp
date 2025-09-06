#ifdef USE_STM32
#include "logger.h"
#include "esphome/core/log.h"

namespace esphome {
namespace logger {

static const char *const TAG = "logger";

void Logger::pre_setup() {
  global_logger = this;
  this->hw_uart_ = nullptr;
}

void Logger::set_uart_parent(uart::UARTComponent *uart) { this->hw_uart_ = uart; }

void HOT Logger::write_msg_(const char *msg) {
  if (this->hw_uart_) {
    this->hw_uart_->write_array((uint8_t *) msg, strlen(msg));
    this->hw_uart_->write_array((uint8_t *) "\n", 1);
  }
}

const LogString *Logger::get_uart_selection_() {
  return LOG_STR(((uart::STM32UARTComponent *) this->hw_uart_)->get_name());
}

}  // namespace logger
}  // namespace esphome
#endif  // USE_STM32
