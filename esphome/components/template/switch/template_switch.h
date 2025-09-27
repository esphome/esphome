#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/switch/switch.h"
#include <string>

#ifdef USE_TEMPLATE_SWITCH_DYNAMIC_LAMBDA
#include "esphome/components/text_sensor/text_sensor.h"
#include "wrench.h"
#endif

namespace esphome {
namespace template_ {

class TemplateSwitch : public switch_::Switch, public Component {
 public:
  TemplateSwitch();

  void setup() override;
  void dump_config() override;

  void set_state_lambda(std::function<optional<bool>()> &&f);
  Trigger<> *get_turn_on_trigger() const;
  Trigger<> *get_turn_off_trigger() const;
  void set_optimistic(bool optimistic);
  void set_assumed_state(bool assumed_state);
  void loop() override;

  float get_setup_priority() const override;

#ifdef USE_TEMPLATE_SWITCH_DYNAMIC_LAMBDA
  ~TemplateSwitch();
  void set_dynamic(bool dynamic) { this->dynamic_ = dynamic; }
  void set_lambda_source(text_sensor::TextSensor *source);
#endif

 protected:
  bool assumed_state() override;

  void write_state(bool state) override;

  optional<std::function<optional<bool>()>> f_;
  bool optimistic_{false};
  bool assumed_state_{false};
  Trigger<> *turn_on_trigger_;
  Trigger<> *turn_off_trigger_;
  Trigger<> *prev_trigger_{nullptr};

#ifdef USE_TEMPLATE_SWITCH_DYNAMIC_LAMBDA
  bool ensure_runtime_();
  void destroy_runtime_();
  void execute_wrench_(const std::string &source);
  void handle_source_update_(const std::string &source);

  bool dynamic_{false};
  text_sensor::TextSensor *lambda_source_{nullptr};
  WRState *wrench_state_{nullptr};
  std::string last_source_;
#endif
};

}  // namespace template_
}  // namespace esphome
