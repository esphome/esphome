#pragma once

#include <cstdint>

namespace esphome::packet_interface {

/**
 * Decides whether a half-duplex radio link may send while its radio reports that it is receiving a packet.
 *
 * Sending is refused while the radio is busy, so the packet being received is not lost. A busy period starts when a
 * send first finds the radio busy, and ends when a send finds it free or end_busy() is called. Once a busy period has
 * lasted max_busy_ms, sends go ahead, so a busy flag that never clears cannot stop sending for good, however often
 * sends are tried. Code generation sets max_busy_ms from the time the radio takes to receive the largest packet.
 *
 * On a link whose peer replies, sending also takes turns: after a send, a send in a later loop pass waits up to
 * turnaround_ms, the time the peer takes to start a reply and the radio to notice it, unless a packet arrives first.
 * Without this, the next frame of a burst goes out on top of the peer's acknowledgement. Sends in the same loop pass,
 * such as the fragments of one packet, are not held back.
 */
class ListenBeforeTalk {
 public:
  static constexpr uint32_t DEFAULT_MAX_BUSY_MS = 2000;

  void set_max_busy(uint32_t max_busy_ms) { this->max_busy_ms_ = max_busy_ms; }
  uint32_t get_max_busy() const { return this->max_busy_ms_; }
  void set_turnaround(uint32_t turnaround_ms) { this->turnaround_ms_ = turnaround_ms; }

  /// True if a packet may be sent at \p now, in the loop pass that started at \p pass; \p busy says whether the
  /// radio is receiving.
  bool may_send(bool busy, uint32_t now, uint32_t pass) {
    if (!busy) {
      this->busy_ = false;
      return !this->turn_held_ || pass == this->sent_pass_ || now - this->sent_ms_ >= this->turnaround_ms_;
    }
    if (!this->busy_) {
      this->busy_ = true;
      this->busy_since_ms_ = now;
    }
    return now - this->busy_since_ms_ >= this->max_busy_ms_;
  }
  /// Ends the busy period, when the radio is seen to be free other than by a send.
  void end_busy() { this->busy_ = false; }
  /// A packet was sent, ending at \p now, in the loop pass that started at \p pass.
  void sent(uint32_t now, uint32_t pass) {
    this->turn_held_ = this->turnaround_ms_ != 0;
    this->sent_ms_ = now;
    this->sent_pass_ = pass;
  }
  /// A packet arrived, so the peer has taken its turn.
  void received() { this->turn_held_ = false; }
  /// How long the current busy period has lasted at \p now.
  uint32_t busy_for(uint32_t now) const { return now - this->busy_since_ms_; }

 protected:
  uint32_t busy_since_ms_{0};
  uint32_t max_busy_ms_{DEFAULT_MAX_BUSY_MS};
  uint32_t sent_ms_{0};
  uint32_t sent_pass_{0};
  uint32_t turnaround_ms_{0};
  bool busy_{false};
  bool turn_held_{false};
};

}  // namespace esphome::packet_interface
