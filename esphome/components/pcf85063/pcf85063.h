#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome::pcf85063 {

/// Hour 0-23 from the hours register digits. In 12-hour mode bit 1 of the tens is PM and the hour runs 1-12.
/// Returns 24 or more for a value out of range.
inline uint8_t decode_hour(uint8_t units, uint8_t tens, bool twelve_hour) {
  if (!twelve_hour)
    return units + 10u * tens;
  const uint8_t hour = units + 10u * (tens & 0x01);
  if (hour == 0 || hour > 12)
    return UINT8_MAX;
  return hour % 12 + ((tens & 0x02) ? 12 : 0);
}

class PCF85063Component final : public time::RealTimeClock, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void read_time();
  void write_time();
};

}  // namespace esphome::pcf85063
