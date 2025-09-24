#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

#include <queue>
namespace esphome {
namespace script {

class ScriptLogger {
 protected:
  void esp_logw_(int line, const char *format, const char *param) {
    esp_log_(ESPHOME_LOG_LEVEL_WARN, line, format, param);
  }
  void esp_logd_(int line, const char *format, const char *param) {
    esp_log_(ESPHOME_LOG_LEVEL_DEBUG, line, format, param);
  }
  void esp_log_(int level, int line, const char *format, const char *param);
};

/// The abstract base class for all script types.
template<typename... Ts> class Script : public ScriptLogger, public Trigger<Ts...> {
 public:
  /** Execute a new instance of this script.
   *
   * The behavior of this function when a script is already running is defined by the subtypes
   */
  virtual void execute(Ts...) = 0;
  /// Check if any instance of this script is currently running.
  virtual bool is_running() { return this->is_action_running(); }
  /// Stop all instances of this script.
  virtual void stop() { this->stop_action(); }

  // execute this script using a tuple that contains the arguments
  void execute_tuple(const std::tuple<Ts...> &tuple) {
    this->execute_tuple_(tuple, typename gens<sizeof...(Ts)>::type());
  }

  // Internal function to give scripts readable names.
  void set_name(const std::string &name) { name_ = name; }

 protected:
  template<int... S> void execute_tuple_(const std::tuple<Ts...> &tuple, seq<S...> /*unused*/) {
    this->execute(std::get<S>(tuple)...);
  }

  std::string name_;
};

/** A script type for which only a single instance at a time is allowed.
 *
 * If a new instance is executed while the previous one hasn't finished yet,
 * a warning is printed and the new instance is discarded.
 */
template<typename... Ts> class SingleScript : public Script<Ts...> {
 public:
  void execute(Ts... x) override {
    if (this->is_action_running()) {
      this->esp_logw_(__LINE__, "Script '%s' is already running! (mode: single)", this->name_.c_str());
      return;
    }

    this->trigger(x...);
  }
};

/** A script type that restarts scripts from the beginning when a new instance is started.
 *
 * If a new instance is started but another one is already running, the existing
 * script is stopped and the new instance starts from the beginning.
 */
template<typename... Ts> class RestartScript : public Script<Ts...> {
 public:
  void execute(Ts... x) override {
    if (this->is_action_running()) {
      this->esp_logd_(__LINE__, "Script '%s' restarting (mode: restart)", this->name_.c_str());
      this->stop_action();
    }

    this->trigger(x...);
  }
};

/** A script type that queues new instances that are created.
 *
 * Only one instance of the script can be active at a time.
 */
template<typename... Ts> class QueueingScript : public Script<Ts...>, public Component {
 public:
  void execute(Ts... x) override {
    if (this->is_action_running() || this->num_runs_ > 0) {
      // num_runs_ is the number of *queued* instances, so total number of instances is
      // num_runs_ + 1
      if (this->max_runs_ != 0 && this->num_runs_ + 1 >= this->max_runs_) {
        this->esp_logw_(__LINE__, "Script '%s' maximum number of queued runs exceeded!", this->name_.c_str());
        return;
      }

      this->esp_logd_(__LINE__, "Script '%s' queueing new instance (mode: queued)", this->name_.c_str());
      this->num_runs_++;
      this->var_queue_.push(std::make_tuple(x...));
      return;
    }

    this->trigger(x...);
    // Check if the trigger was immediate and we can continue right away.
    this->loop();
  }

  void stop() override {
    this->num_runs_ = 0;
    Script<Ts...>::stop();
  }

  void loop() override {
    if (this->num_runs_ != 0 && !this->is_action_running()) {
      this->num_runs_--;
      auto &vars = this->var_queue_.front();
      this->var_queue_.pop();
      this->trigger_tuple_(vars, typename gens<sizeof...(Ts)>::type());
    }
  }

  void set_max_runs(int max_runs) { max_runs_ = max_runs; }

 protected:
  template<int... S> void trigger_tuple_(const std::tuple<Ts...> &tuple, seq<S...> /*unused*/) {
    this->trigger(std::get<S>(tuple)...);
  }

  int num_runs_ = 0;
  int max_runs_ = 0;
  std::queue<std::tuple<Ts...>> var_queue_;
};

/** A script type that executes new instances in parallel.
 *
 * If a new instance is started while previous ones haven't finished yet,
 * the new one is executed in parallel to the other instances.
 */
template<typename... Ts> class ParallelScript : public Script<Ts...> {
 public:
  void execute(Ts... x) override {
    if (this->max_runs_ != 0 && this->automation_parent_->num_running() >= this->max_runs_) {
      this->esp_logw_(__LINE__, "Script '%s' maximum number of parallel runs exceeded!", this->name_.c_str());
      return;
    }
    this->trigger(x...);
  }
  void set_max_runs(int max_runs) { max_runs_ = max_runs; }

 protected:
  int max_runs_ = 0;
};

template<class S, typename... Ts> class ScriptExecuteAction;

template<class... As, typename... Ts> class ScriptExecuteAction<Script<As...>, Ts...> : public Action<Ts...> {
 public:
  ScriptExecuteAction(Script<As...> *script) : script_(script) {}

  using Args = std::tuple<TemplatableValue<As, Ts...>...>;

  template<typename... F> void set_args(F... x) { args_ = Args{x...}; }

  void play(Ts... x) override { this->script_->execute_tuple(this->eval_args_(x...)); }

 protected:
  // NOTE:
  //  `eval_args_impl` functions evaluates `I`th the functions in `args` member.
  //  and then recursively calls `eval_args_impl` for the `I+1`th arg.
  //  if `I` = `N` all args have been stored, and nothing is done.

  template<std::size_t N>
  void eval_args_impl_(std::tuple<As...> & /*unused*/, std::integral_constant<std::size_t, N> /*unused*/,
                       std::integral_constant<std::size_t, N> /*unused*/, Ts... /*unused*/) {}

  template<std::size_t I, std::size_t N>
  void eval_args_impl_(std::tuple<As...> &evaled_args, std::integral_constant<std::size_t, I> /*unused*/,
                       std::integral_constant<std::size_t, N> n, Ts... x) {
    std::get<I>(evaled_args) = std::get<I>(args_).value(x...);  // NOTE: evaluate `i`th arg, and store in tuple.
    eval_args_impl_(evaled_args, std::integral_constant<std::size_t, I + 1>{}, n,
                    x...);  // NOTE: recurse to next index.
  }

  std::tuple<As...> eval_args_(Ts... x) {
    std::tuple<As...> evaled_args;
    eval_args_impl_(evaled_args, std::integral_constant<std::size_t, 0>{}, std::tuple_size<Args>{}, x...);
    return evaled_args;
  }

  Script<As...> *script_;
  Args args_;
};

template<class C, typename... Ts> class ScriptStopAction : public Action<Ts...> {
 public:
  ScriptStopAction(C *script) : script_(script) {}

  void play(Ts... x) override { this->script_->stop(); }

 protected:
  C *script_;
};

template<class C, typename... Ts> class IsRunningCondition : public Condition<Ts...> {
 public:
  explicit IsRunningCondition(C *parent) : parent_(parent) {}

  bool check(Ts... x) override { return this->parent_->is_running(); }

 protected:
  C *parent_;
};

template<class C, typename... Ts> class ScriptWaitAction : public Action<Ts...>, public Component {
 public:
  ScriptWaitAction(C *script) : script_(script) {}

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
  C *script_;
  std::tuple<Ts...> var_{};
};

}  // namespace script
}  // namespace esphome
