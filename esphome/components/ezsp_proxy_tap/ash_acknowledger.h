#pragma once

#include "ash_protocol.h"

#include <cstddef>
#include <cstdint>

namespace esphome::ezsp_proxy_tap {

enum class ScanResult : uint8_t {
  NONE,     // Mid-frame, or a delimiter that carried nothing
  FRAME,    // frame()/length() hold a complete body with a verified CRC
  INVALID,  // A delimited chunk arrived but was not a well-formed ASH frame
};

// Reassembles one direction of the byte stream into unstuffed, CRC-checked frames.
// FLAG ends a frame, CANCEL discards what precedes it, SUBSTITUTE poisons everything up
// to the next FLAG, and XON/XOFF are transport flow control removed without disturbing
// the frame around them.
class AshFrameScanner {
 public:
  ScanResult feed(uint8_t byte);
  void reset();

  // Valid only until the next feed() call, which begins overwriting the buffer.
  const uint8_t *frame() const { return this->buffer_; }
  size_t length() const { return this->frame_length_; }

 private:
  void begin_frame_();

  // Frames are bounded by the ASH maximum, so a stream carrying no delimiters cannot
  // grow the buffer without limit; it just keeps failing.
  uint8_t buffer_[MAX_ASH_FRAME_SIZE];
  size_t index_{0};         // accumulation position for the frame being read
  size_t frame_length_{0};  // body length of the last completed frame
  uint16_t crc_{ASH_CRC_INIT};
  bool escaped_{false};
  bool discarding_{false};
  bool poisoned_{false};
};

// Works out which acknowledgements an NCP is owed while a client has handed them to this
// device. Whether to acknowledge at all is the client's decision, made by putting the port
// in PROTOCOL mode, so this only follows the NCP's frame numbering.
class AshAcknowledger {
 public:
  void reset();

  // Feed bytes from the NCP. Never gates forwarding: this only watches.
  void feed(uint8_t byte);

  // An acknowledgement became owed after the last feed() call. Clears the flag.
  bool take_pending_ack(uint8_t &ack_num);

 protected:
  void handle_frame_();

  AshFrameScanner scanner_;
  uint8_t rx_sequence_{0};
  uint8_t pending_ack_{0};
  bool ack_owed_{false};
};

}  // namespace esphome::ezsp_proxy_tap
