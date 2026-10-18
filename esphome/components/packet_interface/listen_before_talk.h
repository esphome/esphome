#pragma once

#include <cstdint>

namespace esphome::packet_interface {

/**
 * Decides whether a half-duplex radio link may send while its radio reports that it is receiving a packet.
 *
 * Sending is refused while the radio is busy, so the packet being received is not lost, but for at most MAX_BUSY_MS,
 * so a busy flag that never clears cannot stop sending for good. A refusal more than BUSY_GAP_MS after the previous
 * one starts a new busy period.
 */
class ListenBeforeTalk {
 public:
  /// Longer than any packet takes to receive.
  static constexpr uint32_t MAX_BUSY_MS = 2000;
  static constexpr uint32_t BUSY_GAP_MS = 500;

  /// True if a packet may be sent at \p now; \p busy says whether the radio is receiving.
  bool may_send(bool busy, uint32_t now) {
    if (!busy)
      return true;
    if (now - this->last_refused_ms_ > BUSY_GAP_MS)
      this->busy_since_ms_ = now;
    this->last_refused_ms_ = now;
    return now - this->busy_since_ms_ >= MAX_BUSY_MS;
  }
  /// How long the current busy period has lasted at \p now.
  uint32_t busy_for(uint32_t now) const { return now - this->busy_since_ms_; }

 protected:
  uint32_t busy_since_ms_{0};
  uint32_t last_refused_ms_{0};
};

}  // namespace esphome::packet_interface
