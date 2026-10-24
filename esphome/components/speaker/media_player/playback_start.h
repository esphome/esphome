#pragma once

#include <cstddef>

namespace esphome::speaker {

static constexpr size_t ANNOUNCEMENT_STARTUP_DIVISOR = 4;

inline bool playback_start_ready(bool announcement, size_t raw_bytes, size_t media_initial_bytes) {
  return raw_bytes >= (announcement ? media_initial_bytes / ANNOUNCEMENT_STARTUP_DIVISOR : media_initial_bytes);
}

}  // namespace esphome::speaker
