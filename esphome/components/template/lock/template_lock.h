#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/template_lambda.h"
#include "esphome/components/lock/lock.h"

namespace esphome::template_ {

class TemplateLock final : public lock::Lock, public Component {
 public:
  TemplateLock();

  void setup() override;
  void dump_config() override;

  template<typename F> void set_state_lambda(F &&f) { this->f_.set(std::forward<F>(f)); }
  Trigger<> *get_lock_trigger() { return &this->lock_trigger_; }
  Trigger<> *get_unlock_trigger() { return &this->unlock_trigger_; }
  Trigger<> *get_open_trigger() { return &this->open_trigger_; }
  void set_optimistic(bool optimistic);
  void loop() override;

  float get_setup_priority() const override;

 protected:
  void control(const lock::LockCall &call) override;
  void open_latch() override;

  TemplateLambda<lock::LockState> f_;
  bool optimistic_{false};
  Trigger<> lock_trigger_;
  Trigger<> unlock_trigger_;
  Trigger<> open_trigger_;
  Trigger<> *prev_trigger_{nullptr};
};

}  // namespace esphome::template_
