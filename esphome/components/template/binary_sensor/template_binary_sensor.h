#pragma once

#include "esphome/core/component.h"
#include "esphome/core/template_lambda.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace template_ {

class TemplateBinarySensor final : public Component, public binary_sensor::BinarySensor {
 public:
  template<typename F> void set_template(F &&f) { this->f_.set(std::forward<F>(f)); }

  void setup() override;
  void loop() override;
  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  TemplateLambda<bool> f_;
};

}  // namespace template_
}  // namespace esphome
