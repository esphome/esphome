#pragma once

// Protocol-controlled Tuya MCU autobaud, exercised independently of ESPHome.
// Each candidate sends CMD01 and accepts only a checksummed product-info reply.
#include <cstddef>
#include <cstdint>

namespace esphome::tuya {

class TuyaLowPowerAutoBaud {
 public:
  static constexpr uint32_t PROBE_TIMEOUT_MS = 500;
  static constexpr uint32_t BAUD_SLOW = 9600;
  static constexpr uint32_t BAUD_FAST = 115200;

  void start(uint32_t configured_baud) {
    first_baud_ = configured_baud == BAUD_FAST ? BAUD_FAST : BAUD_SLOW;
    candidate_ = first_baud_;
    attempts_ = 0;
    searching_ = true;
    waiting_ = false;
    locked_ = false;
    sent_at_ms_ = 0;
  }

  bool searching() const { return searching_; }
  bool locked() const { return locked_; }
  bool waiting() const { return waiting_; }
  uint32_t baud() const { return candidate_; }
  uint32_t attempts() const { return attempts_; }

  // Poll from the main loop. A timeout advances to the other supported speed.
  // No sleep, busy-wait or task blocking is necessary.
  bool probe_due(uint32_t now) {
    if (!searching_)
      return false;
    if (!waiting_)
      return true;
    if (static_cast<uint32_t>(now - sent_at_ms_) < PROBE_TIMEOUT_MS)
      return false;
    waiting_ = false;
    candidate_ = candidate_ == BAUD_SLOW ? BAUD_FAST : BAUD_SLOW;
    return true;
  }

  void probe_sent(uint32_t now) {
    waiting_ = true;
    sent_at_ms_ = now;
    ++attempts_;
  }

  // Call only for fully received frames; this independent check also makes
  // it possible to test rejection of bad checksums and wrong commands.
  bool accept_frame(const uint8_t *frame, size_t frame_len, uint32_t now) {
    if (!searching_ || !waiting_ || frame == nullptr || static_cast<uint32_t>(now - sent_at_ms_) >= PROBE_TIMEOUT_MS)
      return false;
    // Version 0 (HC32 firmware 1.3.1) or 3 (standard Tuya MCU replies).
    if (frame_len < 9 || frame[0] != 0x55 || frame[1] != 0xAA || (frame[2] != 0x00 && frame[2] != 0x03) ||
        frame[3] != 0x01)
      return false;
    const size_t payload_len = (static_cast<size_t>(frame[4]) << 8) | frame[5];
    if (payload_len > 512 || frame_len != payload_len + 7)
      return false;
    // Product information is JSON. Do not lock to an empty/stray ACK.
    if (payload_len < 2 || frame[6] != '{' || frame[6 + payload_len - 1] != '}')
      return false;
    uint8_t checksum = 0;
    for (size_t i = 0; i + 1 < frame_len; ++i)
      checksum += frame[i];
    if (checksum != frame[frame_len - 1])
      return false;
    searching_ = false;
    waiting_ = false;
    locked_ = true;
    return true;
  }

  void stop() {
    searching_ = false;
    waiting_ = false;
  }

 private:
  uint32_t first_baud_{BAUD_SLOW};
  uint32_t candidate_{BAUD_SLOW};
  uint32_t sent_at_ms_{0};
  uint32_t attempts_{0};
  bool searching_{false};
  bool waiting_{false};
  bool locked_{false};
};

}  // namespace esphome::tuya
