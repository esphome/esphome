#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome::rx8111 {

class RX8111Component final : public time::RealTimeClock, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void read_time();
  void write_time();

 protected:
  bool set_stop_bit_(bool stop);
};

}  // namespace esphome::rx8111
