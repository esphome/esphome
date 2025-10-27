#pragma once

#include "esphome/core/defines.h"

#ifdef USE_DATETIME_TIME

#include "esphome/components/datetime/time_entity.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/time.h"

namespace esphome {
namespace template_ {

class TemplateTime : public datetime::TimeEntity, public PollingComponent {
 public:
  void set_template(std::function<optional<ESPTime>()> &&f) { this->f_ = f; }

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  Trigger<ESPTime> *get_set_trigger() const { return this->set_trigger_; }
  void set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }

  void set_initial_value(ESPTime initial_value) { this->initial_value_ = initial_value; }
  void set_restore_value(bool restore_value) { this->restore_value_ = restore_value; }

 protected:
  void control(const datetime::TimeCall &call) override;

  bool optimistic_{false};
  ESPTime initial_value_{};
  bool restore_value_{false};
  Trigger<ESPTime> *set_trigger_ = new Trigger<ESPTime>();
  optional<std::function<optional<ESPTime>()>> f_;

  ESPPreferenceObject pref_;
};

}  // namespace template_
}  // namespace esphome

#endif  // USE_DATETIME_TIME
