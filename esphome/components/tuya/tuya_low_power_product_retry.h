#pragma once

#include <cstdint>

namespace esphome::tuya {

// Bounded, nonblocking substitute for the WXU product handler's 500-ms
// retry on malformed/missing descriptors. The original firmware has wider
// retry state outside the emulated function; do not assume infinite retries.
class TuyaLowPowerProductRetry {
 public:
  static constexpr uint32_t RETRY_MS = 500;
  static constexpr uint8_t MAX_RETRIES = 5;

  void begin(uint32_t now) {
    if (active_) {
      return;
    }
    active_ = true;
    attempts_ = 0;
    next_ms_ = now + RETRY_MS;
  }

  bool active() const { return active_; }
  uint8_t attempts() const { return attempts_; }
  bool due(uint32_t now) const { return active_ && static_cast<int32_t>(now - next_ms_) >= 0; }
  bool exhausted() const { return active_ && attempts_ >= MAX_RETRIES; }

  void query_sent(uint32_t now) {
    if (!active_ || this->exhausted()) {
      return;
    }
    ++attempts_;
    next_ms_ = now + RETRY_MS;
  }

  void success() {
    active_ = false;
    attempts_ = 0;
  }

 private:
  uint32_t next_ms_{0};
  uint8_t attempts_{0};
  bool active_{false};
};

}  // namespace esphome::tuya
