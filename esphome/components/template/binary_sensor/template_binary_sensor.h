#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::template_ {

/// Out-of-line dump_config shared by all template binary sensor variants.
void log_template_binary_sensor(binary_sensor::BinarySensor *obj);

/// Action-driven template binary sensor (no lambda). State set via binary_sensor.template.publish.
class TemplateBinarySensor : public Component, public binary_sensor::BinarySensor {
 public:
  void dump_config() override { log_template_binary_sensor(this); }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
};

/// Template binary sensor with compile-time lambda for zero-overhead inlined evaluation.
/// @tparam F Constexpr function pointer returning optional<bool>.
template<optional<bool> (*F)()> class TemplateBinarySensorLambda final : public TemplateBinarySensor {
 public:
  void setup() override { this->loop(); }

  void loop() override {
    auto s = F();
    if (s.has_value()) {
      this->publish_state(*s);
    }
  }
};

}  // namespace esphome::template_
