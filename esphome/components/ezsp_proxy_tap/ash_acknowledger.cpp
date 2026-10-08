#include "ash_acknowledger.h"

#include "esphome/core/helpers.h"

namespace esphome::ezsp_proxy_tap {

// Control byte of an RSTACK, and the only ASH version byte that can follow it
static constexpr uint8_t ASH_RSTACK_CONTROL = 0xC1;
static constexpr uint8_t ASH_PROTOCOL_VERSION = 0x02;
static constexpr size_t ASH_RSTACK_BODY_SIZE = 3;  // control, version, reset code
// A DATA frame carries an EZSP frame of at least 3 bytes after its control byte
static constexpr size_t ASH_MIN_DATA_BODY_SIZE = 1 + 3;
static constexpr size_t ASH_CRC_SIZE = 2;
// Smallest legal frame on the wire: a bare control byte plus its CRC
static constexpr size_t ASH_MIN_FRAME_SIZE = 1 + ASH_CRC_SIZE;

void AshFrameScanner::begin_frame_() {
  this->index_ = 0;
  this->crc_ = ASH_CRC_INIT;
  this->escaped_ = false;
  this->poisoned_ = false;
}

void AshFrameScanner::reset() {
  this->begin_frame_();
  this->discarding_ = false;
}

bool AshFrameScanner::feed(uint8_t byte) {
  if (byte == ASH_FLAG_BYTE) {
    // Running the CRC over the body and its trailing CRC bytes leaves zero when correct
    const bool valid = !this->discarding_ && !this->poisoned_ && !this->escaped_ &&
                       this->index_ >= ASH_MIN_FRAME_SIZE && this->crc_ == 0;
    if (valid) {
      this->frame_length_ = this->index_ - ASH_CRC_SIZE;
    }
    this->begin_frame_();
    this->discarding_ = false;
    return valid;
  }

  if (this->discarding_) {
    return false;
  }

  switch (byte) {
    case ASH_CANCEL_BYTE:
      this->begin_frame_();
      return false;

    case ASH_SUBSTITUTE_BYTE:
      this->discarding_ = true;
      return false;

    case ASH_XON_BYTE:
    case ASH_XOFF_BYTE:
      return false;

    case ASH_ESCAPE_BYTE:
      this->escaped_ = true;
      return false;

    default:
      break;
  }

  uint8_t value = byte;
  if (this->escaped_) {
    this->escaped_ = false;
    value = byte ^ ASH_XOR_BYTE;
  }

  if (this->index_ >= MAX_ASH_FRAME_SIZE) {
    this->poisoned_ = true;
    return false;
  }

  if (this->index_ < ASH_HEADER_SIZE) {
    this->header_[this->index_] = value;
  }
  this->index_++;
  this->crc_ = crc16be(&value, 1, this->crc_);
  return false;
}

void AshAcknowledger::reset() {
  this->scanner_.reset();
  this->rx_sequence_ = 0;
  this->synced_ = false;
}

bool AshAcknowledger::feed(uint8_t byte) { return this->scanner_.feed(byte) && this->handle_frame_(); }

bool AshAcknowledger::handle_frame_() {
  const uint8_t *body = this->scanner_.header();
  const uint8_t control = body[0];

  // An RSTACK restarts the NCP's frame numbering
  if (control == ASH_RSTACK_CONTROL) {
    if (this->scanner_.length() == ASH_RSTACK_BODY_SIZE && body[1] == ASH_PROTOCOL_VERSION) {
      this->rx_sequence_ = 0;
      this->synced_ = true;
    }
    return false;
  }

  if ((control & 0x80) != 0) {
    return false;  // ACK/NAK/RST/ERROR: nothing is owed for these
  }

  if (this->scanner_.length() < ASH_MIN_DATA_BODY_SIZE) {
    return false;
  }

  const uint8_t frame_num = (control >> 4) & ASH_MAX_SEQUENCE;
  const bool re_tx = (control & 0x08) != 0;
  // Without an RSTACK, the session began before we were watching: follow the NCP's numbering
  if (!this->synced_) {
    this->rx_sequence_ = frame_num;
    this->synced_ = true;
  }
  if (frame_num == this->rx_sequence_) {
    this->rx_sequence_ = (this->rx_sequence_ + 1) & ASH_MAX_SEQUENCE;
    return true;
  }
  return re_tx;
}

}  // namespace esphome::ezsp_proxy_tap
