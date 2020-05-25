#pragma once

#include "esphome/core/automation.h"

namespace esphome {
namespace script {

class Script : public Trigger<> {
 public:
  void execute() {
    bool prev = this->in_stack_;
    this->in_stack_ = true;
    this->trigger();
    this->in_stack_ = prev;
  }
  bool script_is_running() { return this->in_stack_ || this->is_running(); }

 protected:
  bool in_stack_{false};
};

template<typename... Ts> class ScriptExecuteAction : public Action<Ts...> {
 public:
  ScriptExecuteAction(Script *script) : script_(script) {}

  void play(Ts... x) override { this->script_->trigger(); }

 protected:
  Script *script_;
};

template<typename... Ts> class ScriptStopAction : public Action<Ts...> {
 public:
  ScriptStopAction(Script *script) : script_(script) {}

  void play(Ts... x) override { this->script_->stop(); }

 protected:
  Script *script_;
};

template<typename... Ts> class IsRunningCondition : public Condition<Ts...> {
 public:
  explicit IsRunningCondition(Script *parent) : parent_(parent) {}

  bool check(Ts... x) override { return this->parent_->script_is_running(); }

 protected:
  Script *parent_;
};

template<typename... Ts> class ScriptWaitAction : public Action<Ts...>, public Component {
 public:
  ScriptWaitAction(Script *script) : script_(script) {}

  void play_complex(Ts... x) override {
    this->num_running_++;
    // Check if we can continue immediately.
    if (!this->script_->is_running()) {
      this->play_next_(x...);
      return;
    }
    this->var_ = std::make_tuple(x...);
    this->loop();
  }

  void loop() override {
    if (this->num_running_ == 0)
      return;

    if (this->script_->is_running())
      return;

    this->play_next_tuple_(this->var_);
  }

  float get_setup_priority() const override { return setup_priority::DATA; }

  void play(Ts... x) override { /* ignore - see play_complex */
  }

 protected:
  Script *script_;
  std::tuple<Ts...> var_{};
};

}  // namespace script
}  // namespace esphome
