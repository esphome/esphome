#pragma once
#include "esphome/components/i2c/i2c_bus.h"

// Wraps the real bus and fails chosen VL53L0X transfers without touching the wire.
//   A: the next read of register 0x00 (measurement in progress) fails
//   B: the next measurement start (the write 0x00=0x01 that follows 0x80=0x00) fails
class FaultBus : public esphome::i2c::I2CBus {
 public:
  explicit FaultBus(esphome::i2c::I2CBus *real) : real_(real) {}
  bool arm_a{false}, arm_b{false};
  uint32_t fired_a{0}, fired_b{0};
  esphome::i2c::ErrorCode write_readv(uint8_t address, const uint8_t *w, size_t wn, uint8_t *r, size_t rn) override {
    if (address == 0x29) {
      if (this->arm_a && wn == 1 && w[0] == 0x00 && rn > 0) {
        this->arm_a = false;
        this->fired_a++;
        return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
      }
      if (wn == 2 && w[0] == 0x80 && w[1] == 0x00)
        this->after_80_ = true;
      else if (this->after_80_ && wn == 2 && w[0] == 0x00 && w[1] == 0x01) {
        this->after_80_ = false;
        if (this->arm_b) {
          this->arm_b = false;
          this->fired_b++;
          return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
        }
      } else if (wn > 0) {
        this->after_80_ = false;
      }
    }
    return this->real_->write_readv(address, w, wn, r, rn);
  }

 protected:
  esphome::i2c::I2CBus *real_;
  bool after_80_{false};
};
inline FaultBus *g_fb = nullptr;
