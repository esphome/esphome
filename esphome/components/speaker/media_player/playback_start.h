#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::speaker {

/// How long an announcement's raw buffer must stay unchanged below the start threshold before playback starts
static constexpr uint32_t ANNOUNCEMENT_STALL_START_MS = 250;

/// Decides when buffered audio starts playing. Everything waits for the full start threshold while data keeps
/// arriving; an announcement whose stream pauses below it (a spoken intro before a long reply) plays what it has.
class PlaybackStartGate {
 public:
  bool ready(bool announcement, size_t raw_bytes, size_t initial_bytes, uint32_t now_ms) {
    if (raw_bytes >= initial_bytes)
      return true;
    if (raw_bytes != this->last_raw_bytes_) {
      this->last_raw_bytes_ = raw_bytes;
      this->last_change_ms_ = now_ms;
      return false;
    }
    return announcement && raw_bytes > 0 && now_ms - this->last_change_ms_ >= ANNOUNCEMENT_STALL_START_MS;
  }

 protected:
  size_t last_raw_bytes_{0};
  uint32_t last_change_ms_{0};
};

}  // namespace esphome::speaker
