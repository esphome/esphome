#include "uart_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uart {

static const char *const TAG = "uart.switch";

void UARTSwitch::loop() {
  if (this->state && this->send_every_) {
    const uint32_t now = millis();
    if (now - this->last_transmission_ > this->send_every_) {
      this->write_command_();
      this->last_transmission_ = now;
    }
  }
}

void UARTSwitch::write_command_() {
  ESP_LOGD(TAG, "'%s': Sending data...", this->get_name().c_str());
  this->write_array(this->data_.data(), this->data_.size());
}

void UARTSwitch::write_state(bool state) {
  if (!state) {
    this->publish_state(false);
    return;
  }

  this->publish_state(true);
  this->write_command_();

  if (this->send_every_ == 0) {
    this->publish_state(false);
  } else {
    this->last_transmission_ = millis();
  }
}
void UARTSwitch::dump_config() {
  LOG_SWITCH("", "UART Switch", this);
  if (this->send_every_) {
    ESP_LOGCONFIG(TAG, "  Send Every: %u", this->send_every_);
  }
}

}  // namespace uart
}  // namespace esphome
