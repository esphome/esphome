#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace template_ {

class TemplateSelect : public select::Select, public PollingComponent {
 public:
  void set_template(optional<std::string> (*f)()) { this->f_ = f; }

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  Trigger<std::string> *get_set_trigger() const { return this->set_trigger_; }
  void set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
  void set_initial_option_index(size_t initial_option_index) { this->initial_option_index_ = initial_option_index; }
  void set_restore_value(bool restore_value) { this->restore_value_ = restore_value; }

 protected:
  void control(const std::string &value) override;
  bool optimistic_ = false;
  size_t initial_option_index_{0};
  bool restore_value_ = false;
  Trigger<std::string> *set_trigger_ = new Trigger<std::string>();
  optional<optional<std::string> (*)()> f_;

  ESPPreferenceObject pref_;
};

}  // namespace template_
}  // namespace esphome
