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

  StatusTrigger *get_trigger(const std::string &key);
  void set_trigger(const std::string &key, StatusTrigger *trigger);
  void push_trigger(StatusTrigger *trigger);
  void pop_trigger(StatusTrigger *trigger, bool incl_group = false);
  void pop_trigger(const std::string &group);

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
      : parent_(parent), group_(std::move(group)), priority_(priority) {}
  std::string get_group() { return this->group_; }
  uint32_t get_priority() { return this->priority_; }

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

template<typename... Ts> class StatusAction : public Action<Ts...>, public Parented<StatusIndicator> {
 public:
  void set_state(bool state) { this->state_ = state; }
  void set_trigger(StatusTrigger *trigger) { this->trigger_ = trigger; }
  void set_group(const std::string &group) { this->group_ = std::move(group); }

  void play(Ts... x) override {
    if (this->state_) {
      if (this->trigger_ != nullptr) {
        this->parent_->push_trigger(this->trigger_);
      }
    } else if (this->group_ != "") {
      this->parent_->pop_trigger(this->group_);
    } else if (this->trigger_ != nullptr) {
      this->parent_->pop_trigger(this->trigger_, false);
    }
  }

 protected:
  StatusIndicator *indicator_;
  StatusTrigger *trigger_{nullptr};
  std::string group_{""};
  bool state_{false};
};

}  // namespace status_indicator
}  // namespace esphome
