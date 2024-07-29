#pragma once

#include "../emc2101.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace emc2101 {

/// This class exposes the EMC2101 sensors.
class EMC2101Sensor : public PollingComponent {
 public:
  EMC2101Sensor(Emc2101Component *parent) : parent_(parent) {}
  /** Used by ESPHome framework. */
  void dump_config() override;
  /** Used by ESPHome framework. */
  void update() override;
  /** Used by ESPHome framework. */
  float get_setup_priority() const override;

  /** Used by ESPHome framework. */
  void set_internal_temperature_sensor(sensor::Sensor *sensor) { this->internal_temperature_sensor_ = sensor; }
  /** Used by ESPHome framework. */
  void set_external_temperature_sensor(sensor::Sensor *sensor) { this->external_temperature_sensor_ = sensor; }
  /** Used by ESPHome framework. */
  void set_speed_sensor(sensor::Sensor *sensor) { this->speed_sensor_ = sensor; }
  /** Used by ESPHome framework. */
  void set_duty_cycle_sensor(sensor::Sensor *sensor) { this->duty_cycle_sensor_ = sensor; }

 protected:
  Emc2101Component *parent_;
  sensor::Sensor *internal_temperature_sensor_{nullptr};
  sensor::Sensor *external_temperature_sensor_{nullptr};
  sensor::Sensor *speed_sensor_{nullptr};
  sensor::Sensor *duty_cycle_sensor_{nullptr};
};

}  // namespace emc2101
}  // namespace esphome
