#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace template_ {

class TemplateNumber : public number::Number, public PollingComponent {
 public:
  void set_template(std::function<optional<float>()> &&f) { this->f_ = f; }

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  Trigger<float> *get_set_trigger() const { return set_trigger_; }
  void set_optimistic(bool optimistic) { optimistic_ = optimistic; }
  void set_initial_value(float initial_value) { initial_value_ = initial_value; }
  void set_restore_value(bool restore_value) { this->restore_value_ = restore_value; }

 protected:
  void control(float value) override;
  bool optimistic_{false};
  float initial_value_{NAN};
  bool restore_value_{false};
  Trigger<float> *set_trigger_ = new Trigger<float>();
  optional<std::function<optional<float>()>> f_;

  ESPPreferenceObject pref_;
};

}  // namespace template_
}  // namespace esphome
