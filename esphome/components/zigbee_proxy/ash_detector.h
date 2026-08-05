#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ZIGBEE_PROXY

#include "ash_protocol.h"

#include <cstddef>
#include <cstdint>

namespace esphome::zigbee_proxy {

// Decides when it is safe to acknowledge NCP frames on a client's behalf.
//
// The client suppresses its own ACKs, so nobody else will send them, and injecting ASH
// bytes into a stream that is not ASH would corrupt it. Detection is therefore one-sided:
// arm only on the session handshake, which is a fixed byte string, and never on frame
// validity, which non-ASH traffic can satisfy by luck.
//
//   RSTACK  (NCP -> host)   c1 02 <reset_code> <crc> 7e
//   version (host -> NCP)   00 42 21 a8 <version^0x54> <crc> 7e
//
// Requiring both, in that order, in opposite directions cannot be satisfied by a
// unidirectional byte stream whatever it contains -- which is exactly the situation
// during a firmware upload. Verified against real .gbl images and real Spinel traffic:
// zero false arms, and neither pattern occurs even as a substring.
//
// Getting it wrong in the other direction is cheap: a frame we decline to acknowledge is
// retransmitted by the NCP, so we see a clean copy and lose only the ack timeout. That
// asymmetry is why this errs towards silence everywhere.

enum class AshDetectState : uint8_t {
  IDLE,        // Not ASH, or not yet proven to be
  SAW_RSTACK,  // Handshake half-complete; watching for the version command
  ARMED,       // Session confirmed; acknowledging on the client's behalf
};

enum class ScanResult : uint8_t {
  NONE,     // Mid-frame, or a delimiter that carried nothing
  FRAME,    // frame()/length() hold a complete body with a verified CRC
  INVALID,  // A delimited chunk arrived but was not a well-formed ASH frame
};

// Reassembles one direction of the byte stream into unstuffed, CRC-checked frames.
// Mirrors bellows' AshProtocol.data_received: FLAG ends a frame, CANCEL discards what
// precedes it, SUBSTITUTE poisons everything up to the next FLAG, and XON/XOFF are
// transport flow control removed without disturbing the frame around them.
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

class AshDetector {
 public:
  void reset();

  // Feed observed traffic. Neither call gates forwarding: the detector only watches.
  void from_ncp(uint8_t byte);
  void from_host(uint8_t byte);

  bool armed() const { return this->state_ == AshDetectState::ARMED; }

  // True only while the host direction can affect the state machine, i.e. while waiting
  // for the version command. Lets the caller skip scanning that direction entirely the
  // rest of the time -- it is the one carrying firmware uploads.
  bool needs_host_scan() const { return this->state_ == AshDetectState::SAW_RSTACK; }

  // An acknowledgement became owed after the last from_ncp() call. Clears the flag.
  bool take_pending_ack(uint8_t &ack_num);

  // The EZSP frame carried by the DATA frame just accepted, for metadata sniffing. The
  // ASH control byte is skipped, so offset 0 is the EZSP sequence number. Still
  // randomized, and valid only until the next from_ncp() call.
  const uint8_t *last_ezsp_frame() const { return this->ncp_scanner_.frame() + 1; }
  size_t last_ezsp_frame_length() const {
    const size_t length = this->ncp_scanner_.length();
    return length > 0 ? length - 1 : 0;
  }

  AshDetectState state() const { return this->state_; }
  uint8_t negotiated_version() const { return this->negotiated_version_; }

 protected:
  void handle_ncp_frame_();
  void reject_();

  AshFrameScanner ncp_scanner_;
  AshFrameScanner host_scanner_;
  AshDetectState state_{AshDetectState::IDLE};
  uint8_t rx_sequence_{0};
  uint8_t pending_ack_{0};
  bool ack_owed_{false};
  bool data_frame_ready_{false};
  uint8_t unconfirmed_rejects_{0};
  uint8_t negotiated_version_{0};
};

}  // namespace esphome::zigbee_proxy

#endif  // USE_ZIGBEE_PROXY
