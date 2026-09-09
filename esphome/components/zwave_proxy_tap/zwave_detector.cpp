#include "zwave_detector.h"

#ifdef USE_ZWAVE_PROXY_TAP

namespace esphome::zwave_proxy_tap {

// Consecutive malformed frames, with no well-formed one in between, before concluding the
// controller is no longer speaking the Serial API. Repeated checksum failures mean our
// idea of where frames begin is wrong, and acknowledging frames we are misreading is
// worse than acknowledging none, so the safe move is to stop and wait to be convinced
// again. A well-formed frame is the evidence that clears the suspicion; garbage is not,
// since noise proves nothing either way.
static constexpr uint8_t MAX_UNCONFIRMED_REJECTS = 4;

// Abandons a frame that stalled part-received, and time-stamps the batch about to be fed.
static void expire_stalled_frame(ZWaveFrameScanner &scanner, uint32_t &frame_start, uint32_t now) {
  if (scanner.in_frame()) {
    if (now - frame_start <= ZWAVE_FRAME_TIMEOUT_MS) {
      return;  // Still within its window; keep the start time it already has
    }
    scanner.reset();
  }
  frame_start = now;
}

ScanResult ZWaveFrameScanner::feed(uint8_t byte) {
  switch (this->state_) {
    case ScanState::WAIT_SOF:
      // ACK/NAK/CAN and anything else carry no framing, so there is nothing to reassemble
      if (byte == ZWAVE_SOF_BYTE) {
        this->state_ = ScanState::WAIT_LENGTH;
      }
      return ScanResult::NONE;

    case ScanState::WAIT_LENGTH:
      if (byte < ZWAVE_MIN_LENGTH) {
        // Not a length the protocol can produce. A 0x01 in this position is far more
        // likely to be the real start of a frame than a length, so treat it as one.
        this->state_ = byte == ZWAVE_SOF_BYTE ? ScanState::WAIT_LENGTH : ScanState::WAIT_SOF;
        return ScanResult::INVALID;
      }
      this->remaining_ = byte;
      this->checksum_ = ZWAVE_CHECKSUM_INIT ^ byte;
      this->state_ = ScanState::WAIT_TYPE;
      return ScanResult::NONE;

    case ScanState::WAIT_TYPE:
      this->type_ = byte;
      this->checksum_ ^= byte;
      this->remaining_--;
      this->state_ = ScanState::WAIT_COMMAND;
      return ScanResult::NONE;

    case ScanState::WAIT_COMMAND:
      this->command_ = byte;
      this->checksum_ ^= byte;
      this->remaining_--;
      this->state_ = ScanState::WAIT_BODY;
      return ScanResult::NONE;

    case ScanState::WAIT_BODY:
      break;
  }

  if (this->remaining_ > 1) {
    this->checksum_ ^= byte;
    this->remaining_--;
    return ScanResult::NONE;
  }
  // The frame's last byte is its checksum, which the accumulator can be compared against
  // directly -- everything it covers has already been folded in.
  this->state_ = ScanState::WAIT_SOF;
  return byte == this->checksum_ ? ScanResult::FRAME : ScanResult::INVALID;
}

void ZWaveDetector::reset() {
  this->device_scanner_.reset();
  this->host_scanner_.reset();
  this->state_ = ZWaveDetectState::IDLE;
  this->pending_command_ = 0;
  this->ack_owed_ = false;
  this->unconfirmed_rejects_ = 0;
}

void ZWaveDetector::begin_batch(uint32_t now) {
  // Both directions, from either caller: a frame stalled in the quiet direction still has
  // to expire, and the direction being fed is by definition not stalled.
  expire_stalled_frame(this->device_scanner_, this->device_frame_start_, now);
  expire_stalled_frame(this->host_scanner_, this->host_frame_start_, now);
}

void ZWaveDetector::from_device(uint8_t byte) {
  switch (this->device_scanner_.feed(byte)) {
    case ScanResult::FRAME:
      this->handle_device_frame_();
      break;
    case ScanResult::INVALID:
      // While armed this may be a corrupted frame, which the controller will retransmit,
      // or a sign it stopped speaking the Serial API. reject_() distinguishes the two by
      // whether a well-formed frame ever follows.
      this->reject_();
      break;
    case ScanResult::NONE:
      break;
  }
}

void ZWaveDetector::handle_device_frame_() {
  if (this->state_ == ZWaveDetectState::SAW_REQUEST) {
    // An unsolicited request from the controller can arrive before the response we are
    // waiting for; it is not the other half of the exchange, so it proves nothing.
    if (this->device_scanner_.type() != ZWAVE_FRAME_TYPE_RESPONSE ||
        this->device_scanner_.command() != this->pending_command_) {
      return;
    }
    this->state_ = ZWaveDetectState::ARMED;
    // The host direction stops being scanned from here, so leave nothing part-read behind
    this->host_scanner_.reset();
  } else if (this->state_ != ZWaveDetectState::ARMED) {
    return;
  }

  // Every well-formed frame is acknowledged, the one that armed us included: the
  // controller is already waiting on that one, so answering now saves a retransmit.
  this->ack_owed_ = true;
  this->unconfirmed_rejects_ = 0;
}

void ZWaveDetector::reject_() {
  if (this->state_ != ZWaveDetectState::ARMED) {
    return;
  }
  if (++this->unconfirmed_rejects_ >= MAX_UNCONFIRMED_REJECTS) {
    this->state_ = ZWaveDetectState::IDLE;
    this->unconfirmed_rejects_ = 0;
    this->ack_owed_ = false;
  }
}

void ZWaveDetector::from_host(uint8_t byte) {
  if (this->host_scanner_.feed(byte) != ScanResult::FRAME) {
    return;
  }
  if (this->state_ == ZWaveDetectState::ARMED) {
    return;
  }
  // Only a request opens an exchange. A later request replaces the one being waited on:
  // the host does not repeat a command it has given up on.
  if (this->host_scanner_.type() != ZWAVE_FRAME_TYPE_REQUEST) {
    return;
  }
  this->pending_command_ = this->host_scanner_.command();
  this->state_ = ZWaveDetectState::SAW_REQUEST;
}

bool ZWaveDetector::take_pending_ack() {
  if (!this->ack_owed_) {
    return false;
  }
  this->ack_owed_ = false;
  return true;
}

}  // namespace esphome::zwave_proxy_tap

#endif  // USE_ZWAVE_PROXY_TAP
