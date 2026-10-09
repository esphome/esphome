#include "uart_button.h"
#include "esphome/core/log.h"

namespace esphome::uart {

ESPHOME_LOG_TAG(TAG, "uart.button");

void UARTButton::press_action() {
  ESP_LOGD(TAG, "'%s': Sending data", LOG_STR_ARG(this->get_log_name()));
  this->write_array(this->data_.data(), this->data_.size());
}

void UARTButton::dump_config() { LOG_BUTTON("", "UART Button", this); }

}  // namespace esphome::uart
