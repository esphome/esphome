#include "ash_detector.h"

#ifdef USE_ZIGBEE_PROXY

namespace esphome::zigbee_proxy {

// Control byte of an RSTACK, and the only ASH version byte that can follow it
static constexpr uint8_t ASH_RSTACK_CONTROL = 0xC1;
static constexpr uint8_t ASH_PROTOCOL_VERSION = 0x02;
static constexpr size_t ASH_RSTACK_BODY_SIZE = 3;  // control, version, reset code
static constexpr size_t ASH_CRC_SIZE = 2;
// Smallest legal frame on the wire: a bare control byte plus its CRC
static constexpr size_t ASH_MIN_FRAME_SIZE = 1 + ASH_CRC_SIZE;

// The opening EZSP version command is a constant: control 0x00 (frmNum 0, ackNum 0)
// followed by [seq=0][frameControl=0][frameId=0] randomized by 0x42 0x21 0xA8. Only the
// requested version varies, as version ^ 0x54, so it can be recovered for free.
static constexpr uint8_t EZSP_VERSION_CMD_PREFIX[] = {0x00, 0x42, 0x21, 0xA8};
static constexpr size_t EZSP_VERSION_CMD_SIZE = 5;
static constexpr uint8_t EZSP_VERSION_RANDOM_MASK = 0x54;

// Consecutive frames we could not accept, with neither a good frame nor a retransmission
// in between, before concluding the peer is no longer speaking ASH. A real ASH peer must
// retransmit an unacknowledged frame, so the absence of one is the positive evidence
// here -- garbage on the line is not, since noise proves nothing either way.
static constexpr uint8_t MAX_UNCONFIRMED_REJECTS = 4;

bool ash_reset_code_is_known(uint8_t code) {
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
  this->crc_ = ash_crc16(&value, 1, this->crc_);
  return ScanResult::NONE;
}

void AshDetector::reset() {
  this->ncp_scanner_.reset();
  this->host_scanner_.reset();
  this->state_ = AshDetectState::IDLE;
  this->rx_sequence_ = 0;
  this->ack_owed_ = false;
  this->data_frame_ready_ = false;
  this->unconfirmed_rejects_ = 0;
  this->negotiated_version_ = 0;
}

void AshDetector::from_ncp(uint8_t byte) {
  this->data_frame_ready_ = false;
  switch (this->ncp_scanner_.feed(byte)) {
    case ScanResult::FRAME:
      this->handle_ncp_frame_();
      break;
    case ScanResult::INVALID:
      // A delimited chunk that is not a frame. While armed this may be a corrupted ASH
      // frame, which the peer will retransmit, or a sign the peer stopped speaking ASH.
      // reject_() distinguishes the two by whether a retransmission ever arrives.
      this->reject_();
      break;
    case ScanResult::NONE:
      break;
  }
}

void AshDetector::handle_ncp_frame_() {
  const uint8_t *body = this->ncp_scanner_.frame();
  const size_t length = this->ncp_scanner_.length();
  const uint8_t control = body[0];

  // RSTACK is the only way into the handshake, and the only way back after a firmware
  // swap: a Spinel or bootloader NCP never emits one, so those stay unarmed forever.
  if (control == ASH_RSTACK_CONTROL) {
    if (length == ASH_RSTACK_BODY_SIZE && body[1] == ASH_PROTOCOL_VERSION && ash_reset_code_is_known(body[2])) {
      this->state_ = AshDetectState::SAW_RSTACK;
      this->rx_sequence_ = 0;
      this->ack_owed_ = false;
      this->unconfirmed_rejects_ = 0;
    }
    return;
  }

  if (this->state_ != AshDetectState::ARMED) {
    return;
  }

  if ((control & 0x80) != 0) {
    return;  // ACK/NAK/RST/ERROR: nothing is owed for these
  }

  const uint8_t frame_num = (control >> 4) & ASH_MAX_SEQUENCE;
  const bool re_tx = (control & 0x08) != 0;

  if (frame_num != this->rx_sequence_) {
    // A retransmission still proves the peer is speaking ASH even though we cannot use
    // this copy, so it clears the suspicion without being acknowledged.
    if (re_tx) {
      this->unconfirmed_rejects_ = 0;
    } else {
      this->reject_();
    }
    return;
  }

  this->rx_sequence_ = (this->rx_sequence_ + 1) & ASH_MAX_SEQUENCE;
  this->pending_ack_ = this->rx_sequence_;
  this->ack_owed_ = true;
  this->data_frame_ready_ = true;
  this->unconfirmed_rejects_ = 0;
}

void AshDetector::reject_() {
  if (this->state_ != AshDetectState::ARMED) {
    return;
  }
  if (++this->unconfirmed_rejects_ >= MAX_UNCONFIRMED_REJECTS) {
    this->state_ = AshDetectState::IDLE;
    this->unconfirmed_rejects_ = 0;
  }
}

void AshDetector::from_host(uint8_t byte) {
  if (this->host_scanner_.feed(byte) != ScanResult::FRAME) {
    return;
  }

  if (this->state_ != AshDetectState::SAW_RSTACK) {
    return;
  }

  const uint8_t *body = this->host_scanner_.frame();
  if (this->host_scanner_.length() != EZSP_VERSION_CMD_SIZE) {
    return;
  }
  for (size_t i = 0; i < sizeof(EZSP_VERSION_CMD_PREFIX); i++) {
    if (body[i] != EZSP_VERSION_CMD_PREFIX[i]) {
      return;
    }
  }

  this->negotiated_version_ = body[4] ^ EZSP_VERSION_RANDOM_MASK;
  this->state_ = AshDetectState::ARMED;
  this->rx_sequence_ = 0;
  this->ack_owed_ = false;
  this->unconfirmed_rejects_ = 0;
}

bool AshDetector::take_pending_ack(uint8_t &ack_num) {
  if (!this->ack_owed_) {
    return false;
  }
  this->ack_owed_ = false;
  ack_num = this->pending_ack_;
  return true;
}

}  // namespace esphome::zigbee_proxy

#endif  // USE_ZIGBEE_PROXY
