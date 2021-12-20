#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sun/sun.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace sun {

enum SensorType {
  SUN_SENSOR_ELEVATION,
  SUN_SENSOR_AZIMUTH,
};

class SunSensor : public sensor::Sensor, public PollingComponent {
 public:
  void set_parent(Sun *parent) { parent_ = parent; }
  void set_clock(time::RealTimeClock *clock) { clock_ = clock; }
  void set_type(SensorType type) { type_ = type; }
  void dump_config() override;
  void update() override {
    double val;
    this->parent_->set_time(clock_->utcnow());
    switch (this->type_) {
      case SUN_SENSOR_ELEVATION:
        val = this->parent_->elevation();
        break;
      case SUN_SENSOR_AZIMUTH:
        val = this->parent_->azimuth();
        break;
      default:
        return;
    }
    this->publish_state(val);
  }

 protected:
  sun::Sun *parent_;
  time::RealTimeClock *clock_;
  SensorType type_;
};

}  // namespace sun
}  // namespace esphome
