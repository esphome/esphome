#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sun/sun.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace sun {

enum SensorType {
  SUN_SENSOR_ELEVATION,
  SUN_SENSOR_AZIMUTH,
};

class SunSensor : public sensor::Sensor, public PollingComponent {
 public:
  void set_parent(Sun *parent) { parent_ = parent; }
  void set_type(SensorType type) { type_ = type; }
  void dump_config() override;
  void update() override {
    double val;
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
  SensorType type_;
};

}  // namespace sun
}  // namespace esphome
