#include "zwave_frame_scanner.h"

namespace esphome::zwave_proxy_tap {

bool ZWaveFrameScanner::feed(uint8_t byte, uint32_t now) {
  if (this->state_ != State::WAIT_SOF && now - this->frame_start_ > ZWAVE_FRAME_TIMEOUT_MS) {
    this->state_ = State::WAIT_SOF;
  }

  switch (this->state_) {
    case State::WAIT_SOF:
      if (byte == ZWAVE_SOF_BYTE) {
        this->frame_start_ = now;
        this->state_ = State::WAIT_LENGTH;
      }
      return false;

    case State::WAIT_LENGTH:
      this->length_ = byte;
      this->remaining_ = byte;
      this->checksum_ = 0xFF ^ byte;
      this->state_ = byte == 0 ? State::WAIT_SOF : State::WAIT_BODY;
      return false;

    case State::WAIT_BODY:
      this->checksum_ ^= byte;
      if (--this->remaining_ != 0) {
        return false;
      }
      this->state_ = State::WAIT_SOF;
      return this->length_ >= ZWAVE_MIN_FRAME_LENGTH && this->checksum_ == 0;
  }
  return false;
}

}  // namespace esphome::zwave_proxy_tap
