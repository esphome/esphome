#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::template_ {

/// Action-driven template binary sensor (no lambda). State set via binary_sensor.template.publish.
class TemplateBinarySensor : public Component, public binary_sensor::BinarySensor {
 public:
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
};

/// Template binary sensor with compile-time lambda for zero-overhead inlined evaluation.
/// @tparam Func Constexpr function pointer returning optional<bool>.
template<optional<bool> (*Func)()> class TemplateBinarySensorLambda final : public TemplateBinarySensor {
 public:
  void setup() override { this->loop(); }
  void loop() override {
    auto s = Func();
    if (s.has_value()) {
      this->publish_state(*s);
    }
  }
};

}  // namespace esphome::template_
