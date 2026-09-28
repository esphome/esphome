#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::uptime {

class UptimeSecondsSensor final : public sensor::Sensor, public PollingComponent {
 public:
  // User provided, not "= default": `new(p) UptimeSecondsSensor()` would zero-fill .bss that is already zero.
  UptimeSecondsSensor() {}
  void update() override;
  void dump_config() override;

  float get_setup_priority() const override;
};

}  // namespace esphome::uptime
