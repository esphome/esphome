#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TIME

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/component.h"

namespace esphome {
namespace uptime {

class UptimeTimestampSensor : public sensor::Sensor, public Component {
 public:
  void setup() override;
  void dump_config() override;

  float get_setup_priority() const override;

  void set_time(time::RealTimeClock *time) { this->time_ = time; }

 protected:
  time::RealTimeClock *time_;
};

}  // namespace uptime
}  // namespace esphome

#endif  // USE_TIME
