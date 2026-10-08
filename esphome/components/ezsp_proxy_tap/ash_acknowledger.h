#pragma once

#include "ash_protocol.h"

#include <cstddef>
#include <cstdint>

namespace esphome::ezsp_proxy_tap {

// Reassembles one direction of the byte stream into unstuffed, CRC-checked frames. Only
// the first ASH_HEADER_SIZE bytes of a frame are kept, as the CRC is checked as bytes
// arrive and acknowledging needs nothing past the header.
class AshFrameScanner {
 public:
  // Returns true when the byte completes a frame with a valid CRC
  bool feed(uint8_t byte);
  void reset();

  // The first min(length(), ASH_HEADER_SIZE) bytes of the last completed frame's body.
  // Valid only until the next feed() call, which begins overwriting them.
  const uint8_t *header() const { return this->header_; }
  size_t length() const { return this->frame_length_; }

 private:
  void begin_frame_();

  uint8_t header_[ASH_HEADER_SIZE];
  size_t index_{0};         // body bytes read so far in the frame being read
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
  // Forgets the NCP's frame numbering. The next RSTACK or DATA frame sets it again.
  void reset();

  // Feed a byte from the NCP. Returns true when it completes a frame that is owed ack_num().
  bool feed(uint8_t byte);
  uint8_t ack_num() const { return this->rx_sequence_; }

 protected:
  bool handle_frame_();

  AshFrameScanner scanner_;
  uint8_t rx_sequence_{0};
  bool synced_{false};
};

}  // namespace esphome::ezsp_proxy_tap
