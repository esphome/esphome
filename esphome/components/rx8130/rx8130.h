#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace rx8130 {

static const uint8_t RX8130_REG_SEC = 0x10;
static const uint8_t RX8130_REG_MIN = 0x11;
static const uint8_t RX8130_REG_HOUR = 0x12;
static const uint8_t RX8130_REG_WDAY = 0x13;
static const uint8_t RX8130_REG_MDAY = 0x14;
static const uint8_t RX8130_REG_MONTH = 0x15;
static const uint8_t RX8130_REG_YEAR = 0x16;
static const uint8_t RX8130_REG_EXTEN = 0x1C;
static const uint8_t RX8130_REG_FLAG = 0x1D;
static const uint8_t RX8130_REG_CTRL0 = 0x1E;
static const uint8_t RX8130_REG_CTRL1 = 0x1F;
static const uint8_t RX8130_REG_DIG_OFFSET = 0x30;
static const uint8_t RX8130_BIT_CTRL_STOP = 0x40;

class RX8130Component : public time::RealTimeClock, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void read_time();
  void write_time();

 protected:
  void stop_(bool stop);
};

template<typename... Ts> class WriteAction : public Action<Ts...>, public Parented<RX8130Component> {
 public:
  void play(Ts... x) override { this->parent_->write_time(); }
};

template<typename... Ts> class ReadAction : public Action<Ts...>, public Parented<RX8130Component> {
 public:
  void play(Ts... x) override { this->parent_->read_time(); }
};

}  // namespace rx8130
}  // namespace esphome
