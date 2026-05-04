#pragma once

#include <cstdint>

class IRSender {
 protected:
  explicit IRSender(uint8_t pin) : pin_(pin) {}

 public:
  virtual ~IRSender() = default;
  virtual void setFrequency(int frequency) {}
  virtual void space(int space_length) = 0;
  virtual void mark(int mark_length) = 0;

  void sendIRbyte(uint8_t send_byte, int bit_mark_length, int zero_space_length, int one_space_length,
                  uint8_t bit_count = 8) {
    for (uint8_t bit = 0; bit < bit_count; bit++) {
      const uint8_t mask = uint8_t(1U << bit);
      this->mark(bit_mark_length);
      this->space((send_byte & mask) ? one_space_length : zero_space_length);
    }
  }

 protected:
  uint8_t pin_;
};
