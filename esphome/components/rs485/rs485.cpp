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
    this->last_rs485_byte_ = now;
  }

  while (this->available()) {
    this->rx_buffer_.push_back(this->read());
    this->last_rs485_byte_ = now;
  }
}

void RS485::parse_rs485_frame_() {
  ESP_LOGD(TAG, "Received frame");
  for (auto *device : this->devices_) {
    for (size_t i = 0; i < device->header_.size(); i++) {
      if (device->header_[index] == -1)
        continue;
      if (device->header_[index] != this->rx_buffer_[index])
        goto next_device;
    }
    ESP_LOGD(TAG, "Found device, forwarding data");
    std::vector<uint8_t> data(this->rx_buffer_.begin() + device->header_.size(), this->rx_buffer_.end());
    device->on_rs485_data(data);
    next_device:;
  }
  ESP_LOGW(TAG, "No device found");
}

void RS485::dump_config() {
  ESP_LOGCONFIG(TAG, "RS485:");
  ESP_LOGCONFIG(TAG, "%u configured devices", this->devices_.size());
}
float RS485::get_setup_priority() const {
  // After UART bus
  return setup_priority::BUS - 1.0f;
}

}  // namespace rs485
}  // namespace esphome
