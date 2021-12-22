#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome {
namespace script {

/// The abstract base class for all script types.
class Script : public Trigger<> {
 public:
  /** Execute a new instance of this script.
   *
   * The behavior of this function when a script is already running is defined by the subtypes
   */
  virtual void execute() = 0;
  /// Check if any instance of this script is currently running.
  virtual bool is_running() { return this->is_action_running(); }
  /// Stop all instances of this script.
  virtual void stop() { this->stop_action(); }

  // Internal function to give scripts readable names.
  void set_name(const std::string &name) { name_ = name; }

 protected:
  std::string name_;
};

/** A script type for which only a single instance at a time is allowed.
 *
 * If a new instance is executed while the previous one hasn't finished yet,
 * a warning is printed and the new instance is discarded.
 */
class SingleScript : public Script {
 public:
  void execute() override;
};

/** A script type that restarts scripts from the beginning when a new instance is started.
 *
 * If a new instance is started but another one is already running, the existing
 * script is stopped and the new instance starts from the beginning.
 */
class RestartScript : public Script {
 public:
  void execute() override;
};

/** A script type that queues new instances that are created.
 *
 * Only one instance of the script can be active at a time.
 */
class QueueingScript : public Script, public Component {
 public:
  void execute() override;
  void stop() override;
  void loop() override;
  void set_max_runs(int max_runs) { max_runs_ = max_runs; }

 protected:
  int num_runs_ = 0;
  int max_runs_ = 0;
};

/** A script type that executes new instances in parallel.
 *
 * If a new instance is started while previous ones haven't finished yet,
 * the new one is executed in parallel to the other instances.
 */
class ParallelScript : public Script {
 public:
  void execute() override;
  void set_max_runs(int max_runs) { max_runs_ = max_runs; }

 protected:
  int max_runs_ = 0;
};

template<typename... Ts> class ScriptExecuteAction : public Action<Ts...> {
 public:
  ScriptExecuteAction(Script *script) : script_(script) {}

  void play(Ts... x) override { this->script_->execute(); }

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

  bool check(Ts... x) override { return this->parent_->is_running(); }

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
