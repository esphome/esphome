#pragma once

#include <list>
#include <memory>
#include <tuple>
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
namespace esphome {
namespace script {

class ScriptLogger {
 protected:
#ifdef USE_STORE_LOG_STR_IN_FLASH
  void esp_logw_(int line, const __FlashStringHelper *format, const char *param) {
    esp_log_(ESPHOME_LOG_LEVEL_WARN, line, format, param);
  }
  void esp_logd_(int line, const __FlashStringHelper *format, const char *param) {
    esp_log_(ESPHOME_LOG_LEVEL_DEBUG, line, format, param);
  }
  void esp_log_(int level, int line, const __FlashStringHelper *format, const char *param);
#else
  void esp_logw_(int line, const char *format, const char *param) {
    esp_log_(ESPHOME_LOG_LEVEL_WARN, line, format, param);
  }
  void esp_logd_(int line, const char *format, const char *param) {
    esp_log_(ESPHOME_LOG_LEVEL_DEBUG, line, format, param);
  }
  void esp_log_(int level, int line, const char *format, const char *param);
#endif
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
    this->execute_tuple_(tuple, std::make_index_sequence<sizeof...(Ts)>{});
  }

  // Internal function to give scripts readable names.
  void set_name(const LogString *name) { name_ = name; }

 protected:
  template<size_t... S> void execute_tuple_(const std::tuple<Ts...> &tuple, std::index_sequence<S...> /*unused*/) {
    this->execute(std::get<S>(tuple)...);
  }

  const LogString *name_{nullptr};
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
      this->esp_logw_(__LINE__, ESPHOME_LOG_FORMAT("Script '%s' is already running! (mode: single)"),
                      LOG_STR_ARG(this->name_));
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
      this->esp_logd_(__LINE__, ESPHOME_LOG_FORMAT("Script '%s' restarting (mode: restart)"), LOG_STR_ARG(this->name_));
      this->stop_action();
    }

    this->trigger(x...);
  }
};

/** A script type that queues new instances that are created.
 *
 * Only one instance of the script can be active at a time.
 *
 * Ring buffer implementation:
 * - num_queued_ tracks the number of queued (waiting) instances, NOT including the currently running one
 * - queue_front_ points to the next item to execute (read position)
 * - Buffer size is max_runs_ - 1 (max total instances minus the running one)
 * - Write position is calculated as: (queue_front_ + num_queued_) % (max_runs_ - 1)
 * - When an item finishes, queue_front_ advances: (queue_front_ + 1) % (max_runs_ - 1)
 * - First execute() runs immediately without queuing (num_queued_ stays 0)
 * - Subsequent executes while running are queued starting at position 0
 * - Maximum total instances = max_runs_ (includes 1 running + (max_runs_ - 1) queued)
 */
template<typename... Ts> class QueueingScript : public Script<Ts...>, public Component {
 public:
  void execute(Ts... x) override {
    if (this->is_action_running() || this->num_queued_ > 0) {
      // num_queued_ is the number of *queued* instances (waiting, not including currently running)
      // max_runs_ is the maximum *total* instances (running + queued)
      // So we reject when num_queued_ + 1 >= max_runs_ (queued + running >= max)
      if (this->num_queued_ + 1 >= this->max_runs_) {
        this->esp_logw_(__LINE__, ESPHOME_LOG_FORMAT("Script '%s' max instances (running + queued) reached!"),
                        LOG_STR_ARG(this->name_));
        return;
      }

      // Initialize queue on first queued item (after capacity check)
      this->lazy_init_queue_();

      this->esp_logd_(__LINE__, ESPHOME_LOG_FORMAT("Script '%s' queueing new instance (mode: queued)"),
                      LOG_STR_ARG(this->name_));
      // Ring buffer: write to (queue_front_ + num_queued_) % queue_capacity
      const size_t queue_capacity = static_cast<size_t>(this->max_runs_ - 1);
      size_t write_pos = (this->queue_front_ + this->num_queued_) % queue_capacity;
      // Use std::make_unique to replace the unique_ptr
      this->var_queue_[write_pos] = std::make_unique<std::tuple<Ts...>>(x...);
      this->num_queued_++;
      return;
    }

    this->trigger(x...);
    // Check if the trigger was immediate and we can continue right away.
    this->loop();
  }

  void stop() override {
    // Clear all queued items to free memory immediately
    // Resetting the array automatically destroys all unique_ptrs and their contents
    this->var_queue_.reset();
    this->num_queued_ = 0;
    this->queue_front_ = 0;
    Script<Ts...>::stop();
  }

  void loop() override {
    if (this->num_queued_ != 0 && !this->is_action_running()) {
      // Dequeue: decrement count, move tuple out (frees slot), advance read position
      this->num_queued_--;
      const size_t queue_capacity = static_cast<size_t>(this->max_runs_ - 1);
      auto tuple_ptr = std::move(this->var_queue_[this->queue_front_]);
      this->queue_front_ = (this->queue_front_ + 1) % queue_capacity;
      this->trigger_tuple_(*tuple_ptr, std::make_index_sequence<sizeof...(Ts)>{});
    }
  }

  void set_max_runs(int max_runs) { max_runs_ = max_runs; }

 protected:
  // Lazy init queue on first use - avoids setup() ordering issues and saves memory
  // if script is never executed during this boot cycle
  inline void lazy_init_queue_() {
    if (!this->var_queue_) {
      // Allocate array of max_runs_ - 1 slots for queued items (running item is separate)
      // unique_ptr array is zero-initialized, so all slots start as nullptr
      this->var_queue_ = std::make_unique<std::unique_ptr<std::tuple<Ts...>>[]>(this->max_runs_ - 1);
    }
  }

  template<size_t... S> void trigger_tuple_(const std::tuple<Ts...> &tuple, std::index_sequence<S...> /*unused*/) {
    this->trigger(std::get<S>(tuple)...);
  }

  int num_queued_ = 0;      // Number of queued instances (not including currently running)
  int max_runs_ = 0;        // Maximum total instances (running + queued)
  size_t queue_front_ = 0;  // Ring buffer read position (next item to execute)
  std::unique_ptr<std::unique_ptr<std::tuple<Ts...>>[]> var_queue_;  // Ring buffer of queued parameters
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
      this->esp_logw_(__LINE__, ESPHOME_LOG_FORMAT("Script '%s' maximum number of parallel runs exceeded!"),
                      LOG_STR_ARG(this->name_));
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

  void play(const Ts &...x) override { this->script_->execute_tuple(this->eval_args_(x...)); }

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

  void play(const Ts &...x) override { this->script_->stop(); }

 protected:
  C *script_;
};

template<class C, typename... Ts> class IsRunningCondition : public Condition<Ts...> {
 public:
  explicit IsRunningCondition(C *parent) : parent_(parent) {}

  bool check(const Ts &...x) override { return this->parent_->is_running(); }

 protected:
  C *parent_;
};

/** Wait for a script to finish before continuing.
 *
 * Uses queue-based storage to safely handle concurrent executions.
 * While concurrent execution from the same trigger is uncommon, it's possible
 * (e.g., rapid button presses, high-frequency sensor updates), so we use
 * queue-based storage for correctness.
 */
template<class C, typename... Ts> class ScriptWaitAction : public Action<Ts...>, public Component {
 public:
  ScriptWaitAction(C *script) : script_(script) {}

  void setup() override {
    // Start with loop disabled - only enable when there's work to do
    // IMPORTANT: Only disable if num_running_ is 0, otherwise play_complex() was already
    // called before our setup() (e.g., from on_boot trigger at same priority level)
    // and we must not undo its enable_loop() call
    if (this->num_running_ == 0) {
      this->disable_loop();
    }
  }

  void play_complex(const Ts &...x) override {
    this->num_running_++;
    // Check if we can continue immediately.
    if (!this->script_->is_running()) {
      this->play_next_(x...);
      return;
    }

    // Store parameters for later execution
    this->param_queue_.emplace_back(x...);
    // Enable loop now that we have work to do - don't call loop() synchronously!
    // Let the event loop call it to avoid reentrancy issues
    this->enable_loop();
  }

  void loop() override {
    if (this->num_running_ == 0)
      return;

    if (this->script_->is_running())
      return;

    // Only process ONE queued item per loop iteration
    // Processing all items in a while loop causes infinite loops because
    // play_next_() can trigger more items to be queued
    if (!this->param_queue_.empty()) {
      auto &params = this->param_queue_.front();
      this->play_next_tuple_(params, std::make_index_sequence<sizeof...(Ts)>{});
      this->param_queue_.pop_front();
    } else {
      // Queue is now empty - disable loop until next play_complex
      this->disable_loop();
    }
  }

  void play(const Ts &...x) override { /* ignore - see play_complex */
  }

  void stop() override {
    this->param_queue_.clear();
    this->disable_loop();
  }

 protected:
  template<size_t... S> void play_next_tuple_(const std::tuple<Ts...> &tuple, std::index_sequence<S...> /*unused*/) {
    this->play_next_(std::get<S>(tuple)...);
  }

  C *script_;
  std::list<std::tuple<Ts...>> param_queue_;
};

}  // namespace script
}  // namespace esphome
