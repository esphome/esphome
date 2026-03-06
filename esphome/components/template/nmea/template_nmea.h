#pragma once

#include "esphome/components/nmea/nmea.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace template_ {

class TemplateNMEA : public nmea::NMEAComponent {
 public:
  void set_latitude_sensor(sensor::Sensor *sensor) { this->latitude_sensor_ = sensor; }
  void set_longitude_sensor(sensor::Sensor *sensor) { this->longitude_sensor_ = sensor; }
  void set_altitude_sensor(sensor::Sensor *sensor) { this->altitude_sensor_ = sensor; }
  void set_speed_sensor(sensor::Sensor *sensor) { this->speed_sensor_ = sensor; }
  void set_course_sensor(sensor::Sensor *sensor) { this->course_sensor_ = sensor; }
  void set_hdop_sensor(sensor::Sensor *sensor) { this->hdop_sensor_ = sensor; }
  void set_satellites_sensor(sensor::Sensor *sensor) { this->satellites_sensor_ = sensor; }

  void update() override;

 protected:
  sensor::Sensor *latitude_sensor_{nullptr};
  sensor::Sensor *longitude_sensor_{nullptr};
  sensor::Sensor *altitude_sensor_{nullptr};
  sensor::Sensor *speed_sensor_{nullptr};
  sensor::Sensor *course_sensor_{nullptr};
  sensor::Sensor *hdop_sensor_{nullptr};
  sensor::Sensor *satellites_sensor_{nullptr};
};

}  // namespace template_
}  // namespace esphome
