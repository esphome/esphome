#pragma once

#include "ash_protocol.h"

#include <cstddef>
#include <cstdint>

namespace esphome::ezsp_proxy_tap {

// Reassembles one direction of the byte stream into unstuffed, CRC-checked frames.
// FLAG ends a frame, CANCEL discards what precedes it, SUBSTITUTE poisons everything up
// to the next FLAG, and XON/XOFF are transport flow control removed without disturbing
// the frame around them.
class AshFrameScanner {
 public:
  // Returns true when the byte completes a frame with a valid CRC
  bool feed(uint8_t byte);
  void reset();

  // Valid only until the next feed() call, which begins overwriting the buffer.
  const uint8_t *frame() const { return this->buffer_; }
  size_t length() const { return this->frame_length_; }

 private:
  void begin_frame_();

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

  // Feed a byte from the NCP. Returns true when it completes a frame that is owed ack_num().
  bool feed(uint8_t byte);
  uint8_t ack_num() const { return this->rx_sequence_; }

 protected:
  bool handle_frame_();

  AshFrameScanner scanner_;
  uint8_t rx_sequence_{0};
};

}  // namespace esphome::ezsp_proxy_tap
