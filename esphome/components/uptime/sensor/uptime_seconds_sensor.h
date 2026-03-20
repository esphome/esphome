#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::uptime {

class UptimeSecondsSensor : public sensor::Sensor, public PollingComponent {
 public:
  void update() override;
  void dump_config() override;

  float get_setup_priority() const override;
};

}  // namespace esphome::uptime
