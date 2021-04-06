#include "rs485.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rs485 {

static const char *TAG = "rs485";

void RS485::loop() {
  const uint32_t now = millis();
  if (now - this->last_rs485_byte_ > 50) {
    if (!this->rx_buffer_.empty()) {
      this->parse_rs485_frame_();
      this->rx_buffer_.clear();
    }
    this->ready_to_tx_ = true;
    this->last_rs485_byte_ = now;
  }

  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);
    this->rx_buffer_.push_back(byte);
    this->last_rs485_byte_ = now;
  }

  if (this->ready_to_tx_) {
    if (!this->tx_buffer_.empty()) {
      this->write_array(this->tx_buffer_.front());
      this->tx_buffer_.pop();
      this->ready_to_tx_ = false;
    }
  }
}

void RS485::parse_rs485_frame_() {
  ESP_LOGV(TAG, "Received frame");
  bool next_device = false;
  size_t header_idx = 0;
  for (auto *device : this->devices_) {
    next_device = false;
    for (auto *i : device->header_) {
      if (i == nullptr) {
        header_idx++;
        continue;
      }
      if (*i != this->rx_buffer_[header_idx]) {
        next_device = true;
        header_idx = 0;
        break;
      }
      header_idx++;
    }
    if (next_device)
      continue;
    ESP_LOGV(TAG, "Found device, forwarding data");
    device->on_rs485_data(this->rx_buffer_);
    return;
  }
  ESP_LOGW(TAG, "No device found, discarding frame");
}

void RS485::dump_config() {
  ESP_LOGCONFIG(TAG, "RS485:");
  ESP_LOGCONFIG(TAG, "%zu configured devices", this->devices_.size());
}
float RS485::get_setup_priority() const {
  // After UART bus
  return setup_priority::BUS - 1.0f;
}
void RS485::send(std::vector<uint8_t> &data) { this->tx_buffer_.push(data); }

}  // namespace rs485
}  // namespace esphome
