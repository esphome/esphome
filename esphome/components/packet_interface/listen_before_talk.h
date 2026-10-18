#pragma once

#include <cstdint>

namespace esphome::packet_interface {

/**
 * Decides whether a half-duplex radio link may send while its radio reports that it is receiving a packet.
 *
 * Sending is refused while the radio is busy, so the packet being received is not lost. A busy period starts when a
 * send first finds the radio busy, and ends when one finds it free. Once a busy period has lasted max_busy_ms, sends
 * go ahead, so a busy flag that never clears cannot stop sending for good, however often sends are tried. Code
 * generation sets max_busy_ms from the time the radio takes to receive the largest packet.
 */
class ListenBeforeTalk {
 public:
  static constexpr uint32_t DEFAULT_MAX_BUSY_MS = 2000;

  void set_max_busy(uint32_t max_busy_ms) { this->max_busy_ms_ = max_busy_ms; }
  uint32_t get_max_busy() const { return this->max_busy_ms_; }

  /// True if a packet may be sent at \p now; \p busy says whether the radio is receiving.
  bool may_send(bool busy, uint32_t now) {
    if (!busy) {
      this->busy_ = false;
      return true;
    }
    if (!this->busy_) {
      this->busy_ = true;
      this->busy_since_ms_ = now;
    }
    return now - this->busy_since_ms_ >= this->max_busy_ms_;
  }
  /// How long the current busy period has lasted at \p now.
  uint32_t busy_for(uint32_t now) const { return now - this->busy_since_ms_; }

 protected:
  uint32_t busy_since_ms_{0};
  uint32_t max_busy_ms_{DEFAULT_MAX_BUSY_MS};
  bool busy_{false};
};

}  // namespace esphome::packet_interface
