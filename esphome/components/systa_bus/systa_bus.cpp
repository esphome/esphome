#include "systa_bus.h"
#include "esphome/core/log.h"

namespace esphome::systa_bus {

static const char *const TAG = "systa_bus";

void SystaBus::dump_config() {
  ESP_LOGCONFIG(TAG, "SystaBus:");
  check_uart_settings(9600);
}

static bool checksum(const StaticVector<uint8_t, BUFFER_SIZE> &data) {
  uint8_t csum = 0;
  for (uint8_t i : data)
    csum += i;
  return csum == 0;
}

void SystaBus::loop() {
  if (!this->available())
    return;

  do {
    uint8_t c;
    if (!this->read_byte(&c))
      break;

    if (this->state_ == ParseState::IDLE && c == 0xfc) {
      this->state_ = ParseState::HEADER;
      this->buffer_.clear();
      this->buffer_.push_back(c);
    } else if (this->state_ == ParseState::HEADER) {
      this->state_ = ParseState::BODY;
      this->buffer_.push_back(c);
      uint16_t message_type = get_message_type(this->buffer_);
      this->length_ = this->buffer_[1] + 3;
      if (message_type != MESSAGE_TYPE_AQUA_SENSOR_DATA) {
        ESP_LOGV(TAG, "Unknown message type 0x%04x", message_type);
        this->state_ = ParseState::IDLE;
      }
    } else if (this->state_ == ParseState::BODY) {
      this->buffer_.push_back(c);
      if (this->buffer_.size() == this->length_) {
        this->state_ = ParseState::IDLE;
        if (!checksum(this->buffer_)) {
          ESP_LOGW(TAG, "Checksum failed");
          continue;
        }
        for (auto &listener : this->listeners_)
          listener->handle_message(this->buffer_);
      }
    }
  } while (this->available());
}

}  // namespace esphome::systa_bus
