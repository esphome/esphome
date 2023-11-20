#pragma once

#include <map>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/light/automation.h"

namespace esphome {
namespace status_indicator {
class StatusTrigger;
union StatusFlags {
  struct {
    int on_error : 1;
    int on_warning : 1;
    int on_network : 1;
    int on_api : 1;
    int on_mqtt : 1;
    int on_wifi_ap : 1;
  };
  int setter = 0;
};

class StatusIndicator : public Component {
 public:
  void dump_config() override;
  void loop() override;

  float get_setup_priority() const override;
  float get_loop_priority() const override;

  StatusTrigger *get_trigger(std::string key);
  void set_trigger(std::string key, StatusTrigger *trigger);
  void push_trigger(StatusTrigger *trigger);
  void pop_trigger(StatusTrigger *trigger, bool incl_group = false);
  void pop_trigger(std::string group);

 protected:
  std::string current_status_{""};
  StatusTrigger *current_trigger_{nullptr};
  StatusFlags status_;
  std::map<std::string, StatusTrigger *> triggers_{};
  std::vector<StatusTrigger *> custom_triggers_{};
};

class StatusTrigger : public Trigger<> {
 public:
  explicit StatusTrigger(StatusIndicator *parent, std::string group, uint32_t priority)
      : parent_(parent), group_(group), priority_(priority) {}
  std::string get_group() { return this->group_; }
  uint32 get_priority() { return this->priority_; }
  void push_me() { parent_->push_trigger(this); }
  void pop_me() { parent_->pop_trigger(this, false); }

 protected:
  StatusIndicator *parent_;
  std::string group_;  /// Minimum length of click. 0 means no minimum.
  uint32_t priority_;  /// Maximum length of click. 0 means no maximum.
};

template<typename... Ts> class StatusCondition : public Condition<Ts...> {
 public:
  StatusCondition(StatusIndicator *parent, bool state) : parent_(parent), state_(state) {}
  bool check(Ts... x) override { return (this->parent_->status_.setter == 0) == this->state_; }

 protected:
  StatusIndicator *parent_;
  bool state_;
};

template<typename... Ts> class StatusAction : public Action<Ts...> {
 public:
  explicit StatusAction(StatusTrigger *trigger) : trigger_(trigger) {}
  TEMPLATABLE_VALUE(bool, state)

  void play(Ts... x) override {
    if (this->state_) {

    } else {

    }
  }

 protected:
    StatusTrigger *trigger_;
};

}  // namespace status_indicator
}  // namespace esphome
