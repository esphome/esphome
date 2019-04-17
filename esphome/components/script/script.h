#pragma once

#include "esphome/core/automation.h"

namespace esphome {
namespace script {

class Script : public Trigger<> {
 public:
  void execute() { this->trigger(); }
};

template<typename... Ts> class ScriptExecuteAction : public Action<Ts...> {
 public:
  ScriptExecuteAction(Script *script) : script_(script) {}

  void play(Ts... x) override {
    this->script_->trigger();
    this->play_next(x...);
  }

 protected:
  Script *script_;
};

template<typename... Ts> class ScriptStopAction : public Action<Ts...> {
 public:
  ScriptStopAction(Script *script) : script_(script) {}

  void play(Ts... x) override {
    this->script_->stop();
    this->play_next(x...);
  }

 protected:
  Script *script_;
};

}  // namespace script
}  // namespace esphome
