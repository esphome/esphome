#pragma once

#include "../emc2303.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::emc2303 {

// This class exposes the EMC2303 sensors.
class Emc2303Sensor : public PollingComponent {
 public:
  Emc2303Sensor(Emc2303Component *parent) : parent_(parent) {}

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
  Emc2303Component *parent_;
  sensor::Sensor *speed_sensor_{nullptr};
};

}  // namespace esphome::emc2303
