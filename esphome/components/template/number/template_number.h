#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome {
namespace template_ {

class TemplateNumber : public number::Number, public PollingComponent {
 public:
  TemplateNumber();
  void set_template(std::function<optional<float>()> &&f);

  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;

  Trigger<float> *get_set_trigger() const;
  void set_optimistic(bool optimistic);

 protected:
  void set(float value) override;
  bool optimistic_{false};
  Trigger<float> *set_trigger_;
  optional<std::function<optional<float>()>> f_;
};

}  // namespace template_
}  // namespace esphome
