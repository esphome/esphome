#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/component.h"

namespace esphome {
namespace ds3231 {

class DS3231TimeComponent : public time::RealTimeClock, public i2c::I2CDevice {
 public:
  void dump_config() override;

  time::ESPTime now() { return time::ESPTime::from_epoch_local(this->timestamp_now()); }
  time::ESPTime utcnow() { return time::ESPTime::from_epoch_utc(this->timestamp_now()); }
  time_t timestamp_now();

  void set_epoch_time(uint32_t epoch);

  void update() override;

 protected:
  void write_time_(time::ESPTime tm);
  time::ESPTime read_time_();
};

}  // namespace ds3231
}  // namespace esphome
