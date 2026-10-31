#pragma once

#include <list>
#include <memory>
#include <tuple>
#include "esphome/core/application.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::script {

class ScriptLogger {
 protected:
  void esp_logw_(int line, ProgmemStr format, const char *param) {
    esp_log_(ESPHOME_LOG_LEVEL_WARN, line, format, param);
  }
  void esp_logd_(int line, ProgmemStr format, const char *param) {
    esp_log_(ESPHOME_LOG_LEVEL_DEBUG, line, format, param);
  }
  void esp_log_(int level, int line, ProgmemStr format, const char *param);
};

/// The abstract base class for all script types.
template<typename... Ts> class Script : public ScriptLogger {
 public:
  /** Execute a new instance of this script.
   *
   * The behavior of this function when a script is already running is defined by the subtypes
   */
  virtual void execute(Ts...) = 0;
  /// Check if any instance of this script is currently running.
  virtual bool is_running() = 0;
  /// Stop all instances of this script. Fires the on_stop trigger once if any instance was running.
  virtual void stop() = 0;

#ifdef USE_SCRIPT_ON_STOP
  Trigger<> *get_stop_trigger() { return &this->stop_trigger_; }
#endif

  // Internal function to give scripts readable names.
  void set_name(const LogString *name) { name_ = name; }

 protected:
  const LogString *name_{nullptr};
#ifdef USE_SCRIPT_ON_STOP
  Trigger<> stop_trigger_;
#endif
};

/// Runs the script's actions through an Automation, for actions that can wait (delay, wait_until, ...).
template<typename... Ts> class AutomationScript : public Script<Ts...>, public Trigger<Ts...> {
 protected:
  bool body_running_() { return this->is_action_running(); }
  int body_num_running_() { return this->automation_parent_->num_running(); }
  inline void run_body_(const Ts &...x) ESPHOME_ALWAYS_INLINE { this->trigger(x...); }
  void stop_body_() { this->stop_action(); }
};

/** Runs the script's actions as one generated function, when they are all inline.
 *
 * Like ApplyRunAction, Fn gets the run count by reference and returns early once a stop clears it.
 */
template<auto Fn, typename... Ts> class FunctionScript : public Script<Ts...> {
 protected:
  bool body_running_() { return this->num_running_ > 0; }
  int body_num_running_() { return this->num_running_; }
  void run_body_(const Ts &...x) {
    this->num_running_++;
    Fn(this->num_running_, x...);
    if (this->num_running_ > 0)
      this->num_running_--;
  }
  void stop_body_() { this->num_running_ = 0; }

  int num_running_{0};
};

/// What all script modes share, on top of the class that runs the actions.
template<class Body, typename... Ts> class ScriptBase : public Body {
 public:
  bool is_running() override { return this->body_running_(); }
#ifdef USE_SCRIPT_ON_STOP
  void stop() override {
    if (this->body_running_())
      this->stop_running_();
  }
#else
  void stop() override { this->stop_body_(); }
#endif

 protected:
  // Run the actions with this script's name published as the current source (RAII save/restore,
  // so nesting composes), so deferred work inside the script is attributed to it in blocking
  // warnings. Force-inlined to fold into the always-inlined trigger chain (no extra stack frame).
  inline void run_actions_(const Ts &...x) ESPHOME_ALWAYS_INLINE {
    ScopedSourceGuard source_guard{this->name_};
    this->run_body_(x...);
  }

  // Stop the running actions early and fire the on_stop trigger.
  void stop_running_() {
    this->stop_body_();
#ifdef USE_SCRIPT_ON_STOP
    this->stop_trigger_.trigger();
#endif
  }
};

/** A script type for which only a single instance at a time is allowed.
 *
 * If a new instance is executed while the previous one hasn't finished yet,
 * a warning is printed and the new instance is discarded.
 */
template<class Body, typename... Ts> class SingleMode : public ScriptBase<Body, Ts...> {
 public:
  void execute(Ts... x) override {
    if (this->body_running_()) {
      this->esp_logw_(__LINE__, ESPHOME_LOG_FORMAT("Script '%s' is already running! (mode: single)"),
                      LOG_STR_ARG(this->name_));
      return;
    }

    this->run_actions_(x...);
  }
};

/** A script type that restarts scripts from the beginning when a new instance is started.
 *
 * If a new instance is started but another one is already running, the existing
 * script is stopped and the new instance starts from the beginning.
 */
template<class Body, typename... Ts> class RestartMode : public ScriptBase<Body, Ts...> {
 public:
  void execute(Ts... x) override {
    if (this->body_running_()) {
      this->esp_logd_(__LINE__, ESPHOME_LOG_FORMAT("Script '%s' restarting (mode: restart)"), LOG_STR_ARG(this->name_));
      this->stop_running_();
#ifdef USE_SCRIPT_ON_STOP
      // on_stop may have executed this script again; only the newest run may continue
      if (this->body_running_())
        this->stop_body_();
#endif
    }

    this->run_actions_(x...);
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
template<class Body, typename... Ts> class QueueingMode : public ScriptBase<Body, Ts...>, public Component {
 public:
  void execute(Ts... x) override {
    if (this->body_running_() || this->num_queued_ > 0) {
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
      // Enable loop now that there is something to dequeue - don't call loop()
      // synchronously! Let the event loop call it to avoid reentrancy issues
      this->enable_loop();
      return;
    }

    this->run_actions_(x...);
    // Check if the trigger was immediate and we can continue right away.
    this->loop();
  }

  void stop() override {
#ifdef USE_SCRIPT_ON_STOP
    // Queued runs are unfinished work too, even between two runs when nothing is running
    const bool stopping = this->body_running_() || this->num_queued_ != 0;
#endif
    // Clear all queued items to free memory immediately
    // Resetting the array automatically destroys all unique_ptrs and their contents
    this->var_queue_.reset();
    this->num_queued_ = 0;
    this->queue_front_ = 0;
#ifdef USE_SCRIPT_ON_STOP
    if (stopping)
      this->stop_running_();
#else
    this->stop_body_();
#endif
  }

  void loop() override {
    if (this->num_queued_ != 0 && !this->body_running_()) {
      // Dequeue: decrement count, move tuple out (frees slot), advance read position
      this->num_queued_--;
      const size_t queue_capacity = static_cast<size_t>(this->max_runs_ - 1);
      auto tuple_ptr = std::move(this->var_queue_[this->queue_front_]);
      this->queue_front_ = (this->queue_front_ + 1) % queue_capacity;
      this->trigger_tuple_(*tuple_ptr, std::make_index_sequence<sizeof...(Ts)>{});
    }
    if (this->num_queued_ == 0 && !this->is_idle()) {
      // Queue is now empty - disable loop until the next execute() queues an
      // instance. The inline is_idle() check skips the out-of-line call when
      // the loop is already disabled (execute() calls loop() synchronously).
      // This can run before this component's setup() (execute() from on_boot),
      // which leaves the state machine in LOOP_DONE and skips call_setup();
      // this class therefore must not rely on a setup() override.
      this->disable_loop();
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
    this->run_actions_(std::get<S>(tuple)...);
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
template<class Body, typename... Ts> class ParallelMode : public ScriptBase<Body, Ts...> {
 public:
  void execute(Ts... x) override {
    if (this->max_runs_ != 0 && this->body_num_running_() >= this->max_runs_) {
      this->esp_logw_(__LINE__, ESPHOME_LOG_FORMAT("Script '%s' maximum number of parallel runs exceeded!"),
                      LOG_STR_ARG(this->name_));
      return;
    }
    this->run_actions_(x...);
  }
  void set_max_runs(int max_runs) { max_runs_ = max_runs; }

 protected:
  int max_runs_ = 0;
};

template<typename... Ts> using SingleScript = SingleMode<AutomationScript<Ts...>, Ts...>;
template<typename... Ts> using RestartScript = RestartMode<AutomationScript<Ts...>, Ts...>;
template<typename... Ts> using QueueingScript = QueueingMode<AutomationScript<Ts...>, Ts...>;
template<typename... Ts> using ParallelScript = ParallelMode<AutomationScript<Ts...>, Ts...>;

/** Wait for a script to finish before continuing.
 *
 * Uses queue-based storage to safely handle concurrent executions.
 * While concurrent execution from the same trigger is uncommon, it's possible
 * (e.g., rapid button presses, high-frequency sensor updates), so we use
 * queue-based storage for correctness.
 */
template<class C, typename... Ts> class ScriptWaitAction final : public Action<Ts...>, public Component {
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
    if (this->num_running_ == 0) {
      // Nothing left to wait for, e.g. once the last queued item has run
      this->disable_loop();
      return;
    }

    if (this->script_->is_running())
      return;

    // Only process ONE queued item per loop iteration
    // Processing all items in a while loop causes infinite loops because
    // play_next_() can trigger more items to be queued
    if (!this->param_queue_.empty()) {
      // Take the item out first: the chain may call stop(), which clears the queue
      auto params = std::move(this->param_queue_.front());
      this->param_queue_.pop_front();
      std::apply([this](auto &...args) { this->play_next_(args...); }, params);
    } else {
      // Queue is now empty - disable loop until next play_complex
      this->disable_loop();
    }
  }

  void play(const Ts &...x) override { /* ignore - see play_complex */ }

  void stop() override {
    this->param_queue_.clear();
    this->disable_loop();
  }

 protected:
  C *script_;
  std::list<std::tuple<stored_trigger_arg_t<Ts>...>> param_queue_;
};

}  // namespace esphome::script
