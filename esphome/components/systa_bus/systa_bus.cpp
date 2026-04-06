#include "systa_bus.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cinttypes>

namespace esphome::systa_bus {

static const char *const TAG = "systa_bus";

void SystaBus::dump_config() {
  ESP_LOGCONFIG(TAG, "SystaBus:");
  check_uart_settings(9600);
}

static bool checksum(std::vector<uint8_t> data) {
  uint8_t csum = 0;
  for (uint8_t i : data)
    csum += i;
  return csum == 0;
}

void SystaBus::loop() {
  if (!available())
    return;

  while (available()) {
    uint8_t c;
    read_byte(&c);

    if (this->state_ == 0 && c == 0xfc) {
      this->state_ = 1;
      this->buffer_.clear();
      this->buffer_.push_back(c);
    } else if (this->state_ == 1) {
      this->state_ = 2;
      this->buffer_.push_back(c);
      this->message_type_ = (this->buffer_[0] << 8) + this->buffer_[1];
      this->length_ = this->buffer_[1] + 3;
      if (!(this->message_type_ == 0xfc16)) {
        ESP_LOGW(TAG, "Unknown message type 0x%04x", this->message_type_);
        this->state_ = 0;
      }
    } else if (this->state_ == 2) {
      this->buffer_.push_back(c);
      if (this->buffer_.size() == this->length_) {
        this->state_ = 0;
        if (!checksum(this->buffer_)) {
          ESP_LOGW(TAG, "Checksum failed");
          continue;
        }
        for (auto &listener : this->listeners_)
          listener->handle_message(this->buffer_);
      }
    }
  }
}

}  // namespace esphome::systa_bus
