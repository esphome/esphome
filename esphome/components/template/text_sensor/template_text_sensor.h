#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/template_lambda.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::template_ {

class TemplateTextSensor final : public text_sensor::TextSensor, public PollingComponent {
 public:
  template<typename F> void set_template(F &&f) { this->f_.set(std::forward<F>(f)); }

  void update() override;

  float get_setup_priority() const override;

  void dump_config() override;

 protected:
  TemplateLambda<std::string> f_{};
};

}  // namespace esphome::template_
