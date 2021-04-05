#include "rs485.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rs485 {

static const char *TAG = "rs485";

void RS485::loop() {
  const uint32_t now = millis();
  if (now - this->last_rs485_byte_ > 50) {
    if (this->rx_buffer_.size() > 0) {
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
      std::vector<uint8_t> frame = this->tx_buffer_.front();
      this->write_array(frame, frame.size());
      this->tx_buffer_.pop();
      this->ready_to_tx_ = false;
    }
  }
}

void RS485::parse_rs485_frame_() {
  ESP_LOGV(TAG, "Received frame");
  bool next_device = false;
  for (auto *device : this->devices_) {
    next_device = false;
    for (size_t i = 0; i < device->header_.size(); i++) {
      if (device->header_[i] == -1)
        continue;
      if (device->header_[i] != this->rx_buffer_[i]) {
        next_device = true;
        break;
      }
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
  ESP_LOGCONFIG(TAG, "%u configured devices", this->devices_.size());
}
float RS485::get_setup_priority() const {
  // After UART bus
  return setup_priority::BUS - 1.0f;
}
void RS485::send(std::vector<uint8_t> &data) {
  this->tx_buffer_.push(data)
}

}  // namespace rs485
}  // namespace esphome
