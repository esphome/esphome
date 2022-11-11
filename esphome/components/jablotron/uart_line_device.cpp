#include "uart_line_device.h"

namespace esphome {
namespace jablotron {

static const char *const TAG = "uart_line";

bool UARTLineDevice::line_buffer_empty() const { return this->line_.empty(); }

void UARTLineDevice::setup() {
  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->setup();
  }
}

void UARTLineDevice::set_flow_control_pin(GPIOPin *flow_control_pin) { this->flow_control_pin_ = flow_control_pin; }

std::vector<std::string> UARTLineDevice::read_lines() {
  std::vector<std::string> lines;

  while (this->available()) {
    uint8_t byte;
    if (!this->read_byte(&byte)) {
      break;
    }
    if (byte == '\n') {
      continue;
    } else if (byte == '\r') {
      if (!this->line_.empty()) {
        ESP_LOGD(TAG, "read line '%s'", this->line_.c_str());
        lines.push_back(this->line_);
        this->line_.clear();
      }
    } else {
      this->line_.push_back(byte);
    }
  }

  return lines;
}

void UARTLineDevice::write_line(std::string str) {
  str += "\r\n";

  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->digital_write(true);
  }

  this->write_array(reinterpret_cast<const uint8_t *>(str.data()), str.size());
  this->flush();

  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->digital_write(false);
  }
}

}  // namespace jablotron
}  // namespace esphome
