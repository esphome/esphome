#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"

namespace esphome {
namespace template_ {

class TemplateNumber : public number::Number, public PollingComponent {
 public:
  void set_template(std::function<optional<float>()> &&f);

  void update() override;

  void dump_config() override;

  float get_setup_priority() const override;

 protected:
  void set(float value) override;
  optional<std::function<optional<float>()>> f_;
};

}  // namespace template_
}  // namespace esphome
