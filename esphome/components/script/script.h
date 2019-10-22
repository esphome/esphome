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

  void play(Ts... x) { /* ignore - see play_complex */
  }

  void play_complex(Ts... x) override {
    // Check if we can continue immediately.
    if (!this->script_->is_running()) {
      this->triggered_ = false;
      this->play_next(x...);
      return;
    }
    this->var_ = std::make_tuple(x...);
    this->triggered_ = true;
    this->loop();
  }

  void stop() override { this->triggered_ = false; }

  void loop() override {
    if (!this->triggered_)
      return;

    if (this->script_->is_running())
      return;

    this->triggered_ = false;
    this->play_next_tuple(this->var_);
  }

  float get_setup_priority() const override { return setup_priority::DATA; }

  bool is_running() override { return this->triggered_ || this->is_running_next(); }

 protected:
  Script *script_;
  bool triggered_{false};
  std::tuple<Ts...> var_{};
};

}  // namespace script
}  // namespace esphome
