#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace rx8130 {

class RX8130Component : public time::RealTimeClock, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void read_time();
  void write_time();
  /// Ensure RTC is initialized at the correct time in the setup sequence
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void stop_(bool stop);
};

template<typename... Ts> class WriteAction : public Action<Ts...>, public Parented<RX8130Component> {
 public:
  void play(const Ts... x) override { this->parent_->write_time(); }
};

template<typename... Ts> class ReadAction : public Action<Ts...>, public Parented<RX8130Component> {
 public:
  void play(const Ts... x) override { this->parent_->read_time(); }
};

}  // namespace rx8130
}  // namespace esphome
