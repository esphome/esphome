#include "systa_bus.h"
#include "esphome/core/log.h"

namespace esphome::systa_bus {

static const char *const TAG = "systa_bus";

void SystaBus::dump_config() { ESP_LOGCONFIG(TAG, "SystaBus:"); }

static bool checksum(std::span<const uint8_t> data) {
  uint8_t csum = 0;
  for (uint8_t i : data)
    csum += i;
  return csum == 0;
}

void SystaBus::loop() {
  uint8_t c;
  while (this->available() && this->read_byte(&c)) {
    if (this->buffer_.empty()) {
      if (c == START_BYTE)
        this->buffer_.push_back(c);
      continue;
    }
    this->buffer_.push_back(c);
    if (this->buffer_.size() == 2) {
      // The length byte is only trusted for known message types; anything else restarts the search
      uint16_t message_type = get_message_type(this->buffer_);
      if (message_type != MESSAGE_TYPE_AQUA_SENSOR_DATA) {
        ESP_LOGV(TAG, "Unknown message type 0x%04x", message_type);
        this->buffer_.clear();
      }
      continue;
    }
    if (this->buffer_.size() < this->buffer_[1] + FRAME_OVERHEAD)
      continue;
    if (checksum(this->buffer_)) {
#ifdef SYSTA_BUS_LISTENER_COUNT
      for (auto *listener : this->listeners_)
        listener->handle_message(this->buffer_);
#endif
    } else {
      ESP_LOGW(TAG, "Checksum failed");
    }
    this->buffer_.clear();
  }
}

}  // namespace esphome::systa_bus
