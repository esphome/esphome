#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace template_ {

class TemplateBinarySensor : public Component, public binary_sensor::BinarySensor {
 public:
  void set_template(std::function<optional<bool>()> &&f) { this->f_ = f; }

  void setup() override;
  void loop() override;
  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  std::function<optional<bool>()> f_{nullptr};
};

}  // namespace template_
}  // namespace esphome
