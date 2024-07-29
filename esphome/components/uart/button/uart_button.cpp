#include "uart_button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uart {

static const char *const TAG = "uart.button";

void UARTButton::press_action() {
  ESP_LOGD(TAG, "'%s': Sending data...", this->get_name().c_str());
  this->write_array(this->data_.data(), this->data_.size());
}

void UARTButton::dump_config() { LOG_BUTTON("", "UART Button", this); }

}  // namespace uart
}  // namespace esphome
