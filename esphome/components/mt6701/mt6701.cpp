#include "mt6701.h"
#include "esphome/core/log.h"

namespace esphome::mt6701 {

uint8_t crc6_mt6701(uint32_t data18) {
  uint8_t crc = 0;
  for (int8_t i = 17; i >= 0; i--) {
    uint8_t bit = ((data18 >> i) & 0x01) ^ ((crc >> 5) & 0x01);
    crc = (crc << 1) & 0x3F;
    if (bit != 0)
      crc ^= 0x03;  // x^6 + x + 1 -> feedback taps at bit 1 and bit 0
  }
  return crc;
}

void MT6701Component::handle_read_error_() {
  if (this->consecutive_errors_ >= MAX_CONSECUTIVE_ERRORS)
    return;
  this->consecutive_errors_++;
  if (this->consecutive_errors_ == MAX_CONSECUTIVE_ERRORS) {
    // The message overload logs once on the transition and is a no-op while
    // the warning is already set.
    this->status_set_warning("repeated read/CRC errors");
  }
}

bool MT6701Component::read_encoder() {
  // Leave the bus alone while a transport has suspended it (EEPROM burn).
  if (this->suspend_sampling_)
    return false;

  uint16_t count;
  if (!this->read_count(count)) {
    this->handle_read_error_();
    return false;
  }
  if (this->consecutive_errors_ != 0) {
    this->consecutive_errors_ = 0;
    this->status_clear_warning();
  }

  this->count_ = count;
  return true;
}

}  // namespace esphome::mt6701
