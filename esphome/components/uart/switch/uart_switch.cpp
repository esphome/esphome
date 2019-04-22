#include "uart_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uart {

static const char *TAG = "uart.switch";

void UARTSwitch::write_state(bool state) {
  if (!state) {
    this->publish_state(false);
    return;
  }

  this->publish_state(true);
  ESP_LOGD(TAG, "'%s': Sending data...", this->get_name().c_str());
  this->write_array(this->data_.data(), this->data_.size());
  this->publish_state(false);
}
void UARTSwitch::dump_config() { LOG_SWITCH("", "UART Switch", this); }

}  // namespace uart
}  // namespace esphome
