#pragma once

#include "../emc2101.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::emc2101 {

/// This class exposes the EMC2101 sensors.
class EMC2101Sensor final : public PollingComponent {
 public:
  EMC2101Sensor(Emc2101Component *parent) : parent_(parent) {}
  /** Used by ESPHome framework. */
  void dump_config() override;
  /** Used by ESPHome framework. */
  void update() override;

  /** Used by ESPHome framework. */
  SUB_SENSOR(internal_temperature)
  /** Used by ESPHome framework. */
  SUB_SENSOR(external_temperature)
  /** Used by ESPHome framework. */
  SUB_SENSOR(speed)
  /** Used by ESPHome framework. */
  SUB_SENSOR(duty_cycle)

 protected:
  Emc2101Component *parent_;
};

}  // namespace esphome::emc2101
