#pragma once

#include "../emc230x.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::emc230x {

// This class exposes the EMC230X sensors.
class Emc230xSensor : public PollingComponent {
 public:
  Emc230xSensor(Emc230xComponent *parent) : parent_(parent) {}

  /** Set the fan number for this sensor
   *
   * @param fan The fan number this sensor is associated with
   */
  void set_fan(uint8_t fan) { this->fan_ = fan; }

  /** Used by ESPHome framework. */
  void update() override;

  /** Used by ESPHome framework. */
  void set_speed_sensor(sensor::Sensor *sensor) { this->speed_sensor_ = sensor; }

  uint8_t fan_;

 protected:
  Emc230xComponent *parent_;
  sensor::Sensor *speed_sensor_{nullptr};
};

}  // namespace esphome::emc230x
