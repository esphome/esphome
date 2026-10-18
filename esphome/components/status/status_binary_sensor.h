#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::status {

class StatusBinarySensor final : public binary_sensor::BinarySensor, public PollingComponent {
 public:
  // User provided, not "= default": `new(p) StatusBinarySensor()` would zero-fill .bss that is already zero.
  StatusBinarySensor() {}
  void update() override;

  void setup() override;
  void dump_config() override;

  bool is_status_binary_sensor() const override { return true; }
};

}  // namespace esphome::status
