#include "esphome/components/ld2450/ld2450.h"

#include "esphome/components/ld2450/defines.h"
#include "esphome/components/ld2450/target.h"

#define _USE_MATH_DEFINES

#include <cstring>  // for memcmp
#include <string>

namespace esphome {
namespace ld2450 {

// Data Header & Footer
constexpr const uint8_t DATA_FRAME_HEADER[4] = {0xAA, 0xFF, 0x03, 0x00};
constexpr const uint8_t DATA_FRAME_END[2] = {0x55, 0xCC};
constexpr const char *const TAG = "ld2450";

LD2450Component::LD2450Component(uint8_t num_targets) : num_targets_(num_targets), targets_(new Target[num_targets]) {
  for (size_t i = 0; i < num_targets_; ++i) {
    targets_[i] = Target();
  }
}

void LD2450Component::dump_config() {
  ESP_LOGCONFIG(TAG, "LD2450:");
  LOG_BINARY_SENSOR_("AnyPresence", any_presence)
  LOG_SENSOR_("AllTargetCounts", all_target_counts)
}

Target &LD2450Component::get_target(uint8_t n) const { return targets_[n]; }

void LD2450Component::setup() { ESP_LOGCONFIG(TAG, "LD2450 setup complete."); }

void LD2450Component::parseAndPublishRecord_(const uint8_t *buffer) {
  uint32_t current_millis = millis();
  if (current_millis - last_periodic_millis_ < throttle_)
    return;
  last_periodic_millis_ = current_millis;

  // Extract the targets.
  uint8_t active_targets = 0;
  for (size_t i = 0; i < num_targets_; ++i) {
    targets_[i].update_from_buffer(buffer + 4 + (i * 8));
    active_targets += targets_[i].valid();
    targets_[i].publish();
  }

  PUBLISH_BINARY_SENSOR(any_presence, active_targets > 0)
  PUBLISH_SENSOR(all_target_counts, active_targets)
}

void LD2450Component::loop() {
  while (available() && read_byte(uartBuf_ + uartBufSize_++)) {
    if (uartBufSize_ >= LD2450_RECORD_LENGTH && std::memcmp(uartBuf_ + (uartBufSize_ - 2), DATA_FRAME_END, 2) == 0 &&
        std::memcmp(uartBuf_ + (uartBufSize_ - LD2450_RECORD_LENGTH), DATA_FRAME_HEADER, 4) == 0) {
      parseAndPublishRecord_(uartBuf_);

      uartBufSize_ = 0;
    } else if (uartBufSize_ == BUFFER_CAPACITY) {
      // If this happens, it means we've read almost double the amount of a
      // single record and not encountered a single head/end frame. This is
      // most likely indicating an issue with either the device itself, or
      // the expected values (i.e., did a newer firmware change it).
      ESP_LOGE(TAG, "Read %d bytes without finding a record.", BUFFER_CAPACITY);
      uartBufSize_ = 0;
    }
  }
}

}  // namespace ld2450
}  // namespace esphome
