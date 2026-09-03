#pragma once

#include "esphome/core/component.h"
#include "esphome/core/template_lambda.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::template_ {

class TemplateSensor final : public sensor::Sensor, public PollingComponent {
 public:
  template<typename F> void set_template(F &&f) { this->f_.set(std::forward<F>(f)); }

  void update() override;

  void dump_config() override;

  float get_setup_priority() const override;

 protected:
  TemplateLambda<float> f_;
};

}  // namespace esphome::template_
