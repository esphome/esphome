#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome {
namespace template_ {

class TemplateNumber : public number::Number, public PollingComponent {
 public:
  void set_template(std::function<optional<float>()> &&f) { this->f_ = f; }

  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  Trigger<float> *get_set_trigger() const { return set_trigger_; }
  void set_optimistic(bool optimistic) { optimistic_ = optimistic; }

 protected:
  void control(float value) override;
  bool optimistic_{false};
  Trigger<float> *set_trigger_ = new Trigger<float>();
  optional<std::function<optional<float>()>> f_;
};

}  // namespace template_
}  // namespace esphome
