#pragma once

#include "esphome/components/datetime/datetime.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace template_ {

class TemplateDatetime : public datetime::Datetime, public PollingComponent {
 public:
  void set_template(std::function<optional<std::string>()> &&f) { this->f_ = f; }

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  Trigger<std::string> *get_set_trigger() const { return set_trigger_; }
  void set_optimistic(bool optimistic) { optimistic_ = optimistic; }

  void set_initial_value(std::string initial_value) { initial_value_ = std::move(initial_value); }
  void set_restore_value(bool restore_value) { this->restore_value_ = restore_value; }

 protected:
  void control(std::string value) override;

  bool optimistic_{false};
  std::string initial_value_{"00:00:00"};
  bool restore_value_{false};
  Trigger<std::string> *set_trigger_ = new Trigger<std::string>();
  optional<std::function<optional<std::string>()>> f_;

  ESPPreferenceObject pref_;
};

}  // namespace template_
}  // namespace esphome
