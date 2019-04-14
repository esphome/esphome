#include "uart_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uart {

static const char *TAG = "uart.switch";

UARTSwitch::UARTSwitch(const std::string &name, const std::vector<uint8_t> &data)
    : switch_::Switch(name), data_(data) {}

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
