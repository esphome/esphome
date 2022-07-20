#pragma once

#include "esphome/components/input_text/input_text.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace template_ {

class TemplateInputText : public input_text::InputText, public PollingComponent {
 public:
  void set_template(std::function<optional<std::string>()> &&f) { this->f_ = f; }

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  Trigger<std::string> *get_set_trigger() const { return this->set_trigger_; }
  void set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
  void set_initial_value(const std::string &initial_value) { this->initial_value_ = initial_value; }
  void set_restore_value(bool restore_value) { this->restore_value_ = restore_value; }

 protected:
  void control(const std::string &value) override;
  bool optimistic_ = false;
  std::string initial_value_;
  bool restore_value_ = false;
  Trigger<std::string> *set_trigger_ = new Trigger<std::string>();
  optional<std::function<optional<std::string>()>> f_;

  ESPPreferenceObject pref_;
};

}  // namespace template_
}  // namespace esphome
