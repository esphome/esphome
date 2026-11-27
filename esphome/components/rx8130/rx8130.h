#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome::rx8130 {

class RX8130Component final : public time::RealTimeClock, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void read_time();
  void write_time();

 protected:
  void stop_(bool stop);

  // The voltage-low flag is set: the time registers are not used until written
  bool time_lost_{false};
};

}  // namespace esphome::rx8130
