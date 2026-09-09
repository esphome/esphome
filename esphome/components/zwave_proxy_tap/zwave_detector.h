#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ZWAVE_PROXY_TAP

#include "zwave_protocol.h"

#include <cstdint>

namespace esphome::zwave_proxy_tap {

// Decides when it is safe to acknowledge controller frames on a client's behalf.
//
// The client suppresses its own ACKs, so nobody else will send them, and injecting a
// stray 0x06 into a stream that is not the Serial API would corrupt it. Detection is
// therefore one-sided: arm only on evidence that cannot arise by accident, and never on
// frame validity alone, which other traffic can satisfy by luck.
//
// The Serial API has no fixed opening handshake to key off, but every session is a
// sequence of request/response exchanges, and one of those is evidence enough:
//
//   request   (host -> ctrl)   01 <len> 00 <cmd> <payload> <chk>
//   response  (ctrl -> host)   01 <len> 01 <cmd> <payload> <chk>
//
// Requiring a well-formed request and then a well-formed response carrying the same
// command, in opposite directions, cannot be satisfied by a unidirectional byte stream
// whatever it contains -- which is exactly the situation during a firmware upload. It
// also rules out the bootloader, which only ever emits single bytes and menu text and
// never a 0x01-framed multi-byte reply. Keying on the exchange rather than on one
// particular command means it does not matter which command the client opens with.
//
// Getting it wrong in the other direction is cheap: a frame we decline to acknowledge is
// retransmitted by the controller once its ack timeout expires, so we see a clean copy
// and lose only that delay. That asymmetry is why this errs towards silence everywhere,
// including on a bad checksum -- where the zwave_proxy component answers with a NAK, this
// says nothing and lets the timeout do the work.

enum class ZWaveDetectState : uint8_t {
  IDLE,         // Not the Serial API, or not yet proven to be
  SAW_REQUEST,  // Exchange half-complete; watching for the matching response
  ARMED,        // Session confirmed; acknowledging on the client's behalf
};

enum class ScanResult : uint8_t {
  NONE,     // Mid-frame, or a byte that carried nothing
  FRAME,    // type()/command() describe a complete frame with a verified checksum
  INVALID,  // A frame started but was not well formed
};

// Reassembles one direction of the byte stream into checksum-verified frames.
//
// Because the framing is length-prefixed, the length is known before the payload
// arrives, so the checksum can be folded in byte by byte and nothing needs to be
// buffered. Only the two header fields the detector actually reads are kept, which is
// what makes a scanner per direction cost a handful of bytes rather than 257 each.
class ZWaveFrameScanner {
 public:
  ScanResult feed(uint8_t byte);
  void reset() { this->state_ = ScanState::WAIT_SOF; }

  /// True while a frame is part-received, so the caller can time it out.
  bool in_frame() const { return this->state_ != ScanState::WAIT_SOF; }

  // Valid only for the frame the last feed() reported.
  uint8_t type() const { return this->type_; }
  uint8_t command() const { return this->command_; }

 private:
  enum class ScanState : uint8_t {
    WAIT_SOF,
    WAIT_LENGTH,
    WAIT_TYPE,
    WAIT_COMMAND,
    WAIT_BODY,  // Payload bytes, then the checksum that ends the frame
  };

  ScanState state_{ScanState::WAIT_SOF};
  uint8_t remaining_{0};  // Bytes of the current frame still to come, checksum included
  uint8_t checksum_{ZWAVE_CHECKSUM_INIT};
  uint8_t type_{0};
  uint8_t command_{0};
};

class ZWaveDetector {
 public:
  void reset();

  /// Time-stamp a batch of observed bytes, before feeding them, so a frame left
  /// part-received by an earlier batch is abandoned rather than swallowing this one.
  void begin_batch(uint32_t now);

  // Feed observed traffic. Neither call gates forwarding: the detector only watches.
  void from_device(uint8_t byte);
  void from_host(uint8_t byte);

  bool armed() const { return this->state_ == ZWaveDetectState::ARMED; }

  /// True only while the host direction can still affect the state machine. Lets the
  /// caller skip scanning that direction once armed -- it is the busier of the two.
  bool needs_host_scan() const { return this->state_ != ZWaveDetectState::ARMED; }

  /// An acknowledgement became owed during the last from_device() call. Clears the flag.
  bool take_pending_ack();

 protected:
  void handle_device_frame_();
  void reject_();

  ZWaveFrameScanner device_scanner_;
  ZWaveFrameScanner host_scanner_;
  uint32_t device_frame_start_{0};
  uint32_t host_frame_start_{0};
  ZWaveDetectState state_{ZWaveDetectState::IDLE};
  uint8_t pending_command_{0};  // Command of the request awaiting its response
  uint8_t unconfirmed_rejects_{0};
  bool ack_owed_{false};
};

}  // namespace esphome::zwave_proxy_tap

#endif  // USE_ZWAVE_PROXY_TAP
