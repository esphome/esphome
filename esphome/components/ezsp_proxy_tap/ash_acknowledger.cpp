#include "ash_acknowledger.h"

#include "esphome/core/helpers.h"

namespace esphome::ezsp_proxy_tap {

// Control byte of an RSTACK, and the only ASH version byte that can follow it
static constexpr uint8_t ASH_RSTACK_CONTROL = 0xC1;
static constexpr uint8_t ASH_PROTOCOL_VERSION = 0x02;
static constexpr size_t ASH_RSTACK_BODY_SIZE = 3;  // control, version, reset code
static constexpr size_t ASH_CRC_SIZE = 2;
// Smallest legal frame on the wire: a bare control byte plus its CRC
static constexpr size_t ASH_MIN_FRAME_SIZE = 1 + ASH_CRC_SIZE;

static bool ash_reset_code_is_known(uint8_t code) {
  switch (code) {
    case 0x00:  // RESET_UNKNOWN
    case 0x01:  // RESET_EXTERNAL
    case 0x02:  // RESET_POWER_ON
    case 0x03:  // RESET_WATCHDOG
    case 0x06:  // RESET_ASSERT
    case 0x09:  // RESET_BOOTLOADER
    case 0x0B:  // RESET_SOFTWARE
    case 0x51:  // ERROR_EXCEEDED_MAXIMUM_ACK_TIMEOUT_COUNT
    case 0x80:  // ERROR_CHIP_SPECIFIC
    case 0x81:  // RESET_CHIP_SPECIFIC
      return true;
    default:
      return false;
  }
}

void AshFrameScanner::begin_frame_() {
  this->index_ = 0;
  this->crc_ = ASH_CRC_INIT;
  this->escaped_ = false;
  this->poisoned_ = false;
}

void AshFrameScanner::reset() {
  this->begin_frame_();
  this->frame_length_ = 0;
  this->discarding_ = false;
}

ScanResult AshFrameScanner::feed(uint8_t byte) {
  if (byte == ASH_FLAG_BYTE) {
    // Snapshot everything the verdict depends on: begin_frame_() clears all of it.
    const bool discarding = this->discarding_;
    const bool poisoned = this->poisoned_;
    const bool escaped = this->escaped_;
    const size_t index = this->index_;
    const uint16_t crc = this->crc_;
    // A FLAG always starts the next frame afresh, whatever preceded it
    this->begin_frame_();
    this->discarding_ = false;

    if (discarding || index == 0) {
      // Consecutive delimiters carry no frame at all, so there is nothing to judge
      this->frame_length_ = 0;
      return ScanResult::NONE;
    }
    // Running the CRC over the body *and* its trailing CRC bytes leaves zero when
    // correct, so validity needs no second pass over the frame.
    if (poisoned || escaped || index < ASH_MIN_FRAME_SIZE || crc != 0) {
      this->frame_length_ = 0;
      return ScanResult::INVALID;
    }
    this->frame_length_ = index - ASH_CRC_SIZE;
    return ScanResult::FRAME;
  }

  if (this->discarding_) {
    return ScanResult::NONE;
  }

  switch (byte) {
    case ASH_CANCEL_BYTE:
      // Everything received since the last FLAG is to be ignored
      this->begin_frame_();
      return ScanResult::NONE;

    case ASH_SUBSTITUTE_BYTE:
      // A low-level error was flagged; ignore everything up to the next FLAG
      this->discarding_ = true;
      return ScanResult::NONE;

    case ASH_XON_BYTE:
    case ASH_XOFF_BYTE:
      // Transport flow control, not frame content: skip it without disturbing the frame
      return ScanResult::NONE;

    case ASH_ESCAPE_BYTE:
      this->escaped_ = true;
      return ScanResult::NONE;

    default:
      break;
  }

  uint8_t value = byte;
  if (this->escaped_) {
    this->escaped_ = false;
    value = byte ^ ASH_XOR_BYTE;
    // An escape must decode to a reserved byte; anything else is not ASH framing at all
    if (!ash_is_reserved(value)) {
      this->poisoned_ = true;
      return ScanResult::NONE;
    }
  }

  if (this->index_ >= sizeof(this->buffer_)) {
    this->poisoned_ = true;
    return ScanResult::NONE;
  }

  this->buffer_[this->index_++] = value;
  this->crc_ = crc16be(&value, 1, this->crc_);
  return ScanResult::NONE;
}

void AshAcknowledger::reset() {
  this->scanner_.reset();
  this->rx_sequence_ = 0;
  this->ack_owed_ = false;
}

void AshAcknowledger::feed(uint8_t byte) {
  if (this->scanner_.feed(byte) == ScanResult::FRAME) {
    this->handle_frame_();
  }
}

void AshAcknowledger::handle_frame_() {
  const uint8_t *body = this->scanner_.frame();
  const size_t length = this->scanner_.length();
  const uint8_t control = body[0];

  // An RSTACK restarts the NCP's frame numbering
  if (control == ASH_RSTACK_CONTROL) {
    if (length == ASH_RSTACK_BODY_SIZE && body[1] == ASH_PROTOCOL_VERSION && ash_reset_code_is_known(body[2])) {
      this->rx_sequence_ = 0;
      this->ack_owed_ = false;
    }
    return;
  }

  if ((control & 0x80) != 0) {
    return;  // ACK/NAK/RST/ERROR: nothing is owed for these
  }

  const uint8_t frame_num = (control >> 4) & ASH_MAX_SEQUENCE;
  if (frame_num != this->rx_sequence_) {
    return;
  }

  this->rx_sequence_ = (this->rx_sequence_ + 1) & ASH_MAX_SEQUENCE;
  this->pending_ack_ = this->rx_sequence_;
  this->ack_owed_ = true;
}

bool AshAcknowledger::take_pending_ack(uint8_t &ack_num) {
  if (!this->ack_owed_) {
    return false;
  }
  this->ack_owed_ = false;
  ack_num = this->pending_ack_;
  return true;
}

}  // namespace esphome::ezsp_proxy_tap
