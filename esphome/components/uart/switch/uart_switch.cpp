#include "uart_switch.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace uart {

static const char *const TAG = "uart.switch";

void UARTSwitch::loop() {
  if (this->send_every_) {
    const uint32_t now = App.get_loop_component_start_time();
    if (now - this->last_transmission_ > this->send_every_) {
      this->write_command_(this->state);
      this->last_transmission_ = now;
    }
  }
}

void UARTSwitch::write_command_(bool state) {
  if (state && !this->data_on_.empty()) {
    ESP_LOGD(TAG, "'%s': Sending on data...", this->get_name().c_str());
    this->write_array(this->data_on_.data(), this->data_on_.size());
  }
  if (!state && !this->data_off_.empty()) {
    ESP_LOGD(TAG, "'%s': Sending off data...", this->get_name().c_str());
    this->write_array(this->data_off_.data(), this->data_off_.size());
  }
}

void UARTSwitch::write_state(bool state) {
  if (!this->single_state_) {
    this->publish_state(state);
    this->write_command_(state);
    this->last_transmission_ = millis();
    return;
  }

  if (!state) {
    this->publish_state(false);
    return;
  }

  this->publish_state(true);
  this->write_command_(true);

  if (this->send_every_ == 0) {
    this->publish_state(false);
  } else {
    this->last_transmission_ = millis();
  }
}

void UARTSwitch::dump_config() {
  LOG_SWITCH("", "UART Switch", this);
  if (this->send_every_) {
    ESP_LOGCONFIG(TAG, "  Send Every: %" PRIu32, this->send_every_);
  }
}

}  // namespace uart
}  // namespace esphome
