#pragma once

#include <cstdint>

class IRSender {
 protected:
  explicit IRSender(uint8_t pin) : pin_(pin) {}

 public:
  virtual ~IRSender() = default;
  virtual void setFrequency(int frequency) = 0;
  virtual void space(int space_length) = 0;
  virtual void mark(int mark_length) = 0;

 protected:
  uint8_t pin_;
};
