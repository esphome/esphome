#include "systa_bus.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <array>

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
  while (this->available() && this->read_byte(&c))
    this->handle_byte_(c);
}

void SystaBus::handle_byte_(uint8_t c) {
  if (this->buffer_.empty()) {
    if (c == START_BYTE)
      this->buffer_.push_back(c);
    return;
  }
  this->buffer_.push_back(c);
  if (this->buffer_.size() == 2) {
    // The length byte is only trusted for known message types; anything else restarts the search
    uint16_t message_type = get_message_type(this->buffer_);
    if (message_type != MESSAGE_TYPE_AQUA_SENSOR_DATA) {
      ESP_LOGV(TAG, "Unknown message type 0x%04x", message_type);
      this->buffer_.clear();
      // A stray start byte followed by a real frame: keep this byte as the new start
      if (c == START_BYTE)
        this->buffer_.push_back(c);
    }
    return;
  }
  if (this->buffer_.size() < this->buffer_[1] + FRAME_OVERHEAD)
    return;
  if (!checksum(this->buffer_)) {
    ESP_LOGW(TAG, "Checksum failed");
    this->resync_();
    return;
  }
#ifdef SYSTA_BUS_LISTENER_COUNT
  for (auto *listener : this->listeners_)
    listener->handle_message(this->buffer_);
#endif
  this->buffer_.clear();
}

// A frame that lost a byte swallows the start of the next one, so the failed bytes are fed back through the
// parser from the second byte on. They are fewer than a full frame, so this cannot fail the checksum again.
void SystaBus::resync_() {
  std::array<uint8_t, MAX_MESSAGE_SIZE> failed;
  const size_t count = this->buffer_.size();
  std::copy(this->buffer_.begin(), this->buffer_.end(), failed.begin());
  this->buffer_.clear();
  for (size_t i = 1; i < count; i++)
    this->handle_byte_(failed[i]);
}

}  // namespace esphome::systa_bus
