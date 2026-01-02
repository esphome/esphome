#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/defines.h"
#include "esphome/core/preferences.h"
#include "esphome/core/scheduler.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

#include <list>
#include <vector>

namespace esphome {

template<typename... Ts> class AndCondition : public Condition<Ts...> {
 public:
  explicit AndCondition(std::initializer_list<Condition<Ts...> *> conditions) : conditions_(conditions) {}
  bool check(const Ts &...x) override {
    for (auto *condition : this->conditions_) {
      if (!condition->check(x...))
        return false;
    }

    return true;
  }

 protected:
  FixedVector<Condition<Ts...> *> conditions_;
};

template<typename... Ts> class OrCondition : public Condition<Ts...> {
 public:
  explicit OrCondition(std::initializer_list<Condition<Ts...> *> conditions) : conditions_(conditions) {}
  bool check(const Ts &...x) override {
    for (auto *condition : this->conditions_) {
      if (condition->check(x...))
        return true;
    }

    return false;
  }

 protected:
  FixedVector<Condition<Ts...> *> conditions_;
};

template<typename... Ts> class NotCondition : public Condition<Ts...> {
 public:
  explicit NotCondition(Condition<Ts...> *condition) : condition_(condition) {}
  bool check(const Ts &...x) override { return !this->condition_->check(x...); }

 protected:
  Condition<Ts...> *condition_;
};

template<typename... Ts> class XorCondition : public Condition<Ts...> {
 public:
  explicit XorCondition(std::initializer_list<Condition<Ts...> *> conditions) : conditions_(conditions) {}
  bool check(const Ts &...x) override {
    size_t result = 0;
    for (auto *condition : this->conditions_) {
      result += condition->check(x...);
    }

    return result == 1;
  }

 protected:
  FixedVector<Condition<Ts...> *> conditions_;
};

template<typename... Ts> class LambdaCondition : public Condition<Ts...> {
 public:
  explicit LambdaCondition(std::function<bool(Ts...)> &&f) : f_(std::move(f)) {}
  bool check(const Ts &...x) override { return this->f_(x...); }

 protected:
  std::function<bool(Ts...)> f_;
};

/// Optimized lambda condition for stateless lambdas (no capture).
/// Uses function pointer instead of std::function to reduce memory overhead.
/// Memory: 4 bytes (function pointer on 32-bit) vs 32 bytes (std::function).
template<typename... Ts> class StatelessLambdaCondition : public Condition<Ts...> {
 public:
  explicit StatelessLambdaCondition(bool (*f)(Ts...)) : f_(f) {}
  bool check(const Ts &...x) override { return this->f_(x...); }

 protected:
  bool (*f_)(Ts...);
};

template<typename... Ts> class ForCondition : public Condition<Ts...>, public Component {
 public:
  explicit ForCondition(Condition<> *condition) : condition_(condition) {}

  TEMPLATABLE_VALUE(uint32_t, time);

  void loop() override {
    // Safe to use cached time - only called from Application::loop()
    this->check_internal_(App.get_loop_component_start_time());
  }

  float get_setup_priority() const override { return setup_priority::DATA; }

  bool check(const Ts &...x) override {
    auto now = millis();
    if (!this->check_internal_(now))
      return false;
    return now - this->last_inactive_ >= this->time_.value(x...);
  }

 protected:
  bool check_internal_(uint32_t now) {
    bool cond = this->condition_->check();
    if (!cond)
      this->last_inactive_ = now;
    return cond;
  }

  Condition<> *condition_;
  uint32_t last_inactive_{0};
};

class StartupTrigger : public Trigger<>, public Component {
 public:
  explicit StartupTrigger(float setup_priority) : setup_priority_(setup_priority) {}
  void setup() override { this->trigger(); }
  float get_setup_priority() const override { return this->setup_priority_; }

 protected:
  float setup_priority_;
};

class ShutdownTrigger : public Trigger<>, public Component {
 public:
  explicit ShutdownTrigger(float setup_priority) : setup_priority_(setup_priority) {}
  void on_shutdown() override { this->trigger(); }
  float get_setup_priority() const override { return this->setup_priority_; }

 protected:
  float setup_priority_;
};

class LoopTrigger : public Trigger<>, public Component {
 public:
  void loop() override { this->trigger(); }
  float get_setup_priority() const override { return setup_priority::DATA; }
};

#ifdef ESPHOME_PROJECT_NAME
class ProjectUpdateTrigger : public Trigger<std::string>, public Component {
 public:
  void setup() override {
    uint32_t hash = fnv1_hash(ESPHOME_PROJECT_NAME);
    ESPPreferenceObject pref = global_preferences->make_preference<char[30]>(hash, true);
    char previous_version[30];
    char current_version[30] = ESPHOME_PROJECT_VERSION_30;
    if (pref.load(&previous_version)) {
      int cmp = strcmp(previous_version, current_version);
      if (cmp < 0) {
        this->trigger(previous_version);
      }
    }
    pref.save(&current_version);
    global_preferences->sync();
  }
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
};
#endif

template<typename... Ts> class DelayAction : public Action<Ts...>, public Component {
 public:
  explicit DelayAction() = default;

  TEMPLATABLE_VALUE(uint32_t, delay)

  void play_complex(const Ts &...x) override {
    this->num_running_++;

    // If num_running_ > 1, we have multiple instances running in parallel
    // In single/restart/queued modes, only one instance runs at a time
    // Parallel mode uses skip_cancel=true to allow multiple delays to coexist
    // WARNING: This can accumulate delays if scripts are triggered faster than they complete!
    // Users should set max_runs on parallel scripts to limit concurrent executions.
    // Issue #10264: This is a workaround for parallel script delays interfering with each other.

    // Optimization: For no-argument delays (most common case), use direct lambda
    // instead of std::bind to avoid bind overhead (~16 bytes heap + faster execution)
    if constexpr (sizeof...(Ts) == 0) {
      App.scheduler.set_timer_common_(
          this, Scheduler::SchedulerItem::TIMEOUT,
          /* is_static_string= */ true, "delay", this->delay_.value(), [this]() { this->play_next_(); },
          /* is_retry= */ false, /* skip_cancel= */ this->num_running_ > 1);
    } else {
      // For delays with arguments, use std::bind to preserve argument values
      // Arguments must be copied because original references may be invalid after delay
      auto f = std::bind(&DelayAction<Ts...>::play_next_, this, x...);
      App.scheduler.set_timer_common_(this, Scheduler::SchedulerItem::TIMEOUT,
                                      /* is_static_string= */ true, "delay", this->delay_.value(x...), std::move(f),
                                      /* is_retry= */ false, /* skip_cancel= */ this->num_running_ > 1);
    }
  }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void play(const Ts &...x) override { /* ignore - see play_complex */
  }

  void stop() override { this->cancel_timeout("delay"); }
};

template<typename... Ts> class LambdaAction : public Action<Ts...> {
 public:
  explicit LambdaAction(std::function<void(Ts...)> &&f) : f_(std::move(f)) {}

  void play(const Ts &...x) override { this->f_(x...); }

 protected:
  std::function<void(Ts...)> f_;
};

/// Optimized lambda action for stateless lambdas (no capture).
/// Uses function pointer instead of std::function to reduce memory overhead.
/// Memory: 4 bytes (function pointer on 32-bit) vs 32 bytes (std::function).
template<typename... Ts> class StatelessLambdaAction : public Action<Ts...> {
 public:
  explicit StatelessLambdaAction(void (*f)(Ts...)) : f_(f) {}

  void play(const Ts &...x) override { this->f_(x...); }

 protected:
  void (*f_)(Ts...);
};

/// Simple continuation action that calls play_next_ on a parent action.
/// Used internally by IfAction, WhileAction, RepeatAction, etc. to chain actions.
/// Memory: 4-8 bytes (parent pointer) vs 40 bytes (LambdaAction with std::function).
template<typename... Ts> class ContinuationAction : public Action<Ts...> {
 public:
  explicit ContinuationAction(Action<Ts...> *parent) : parent_(parent) {}

  void play(const Ts &...x) override { this->parent_->play_next_(x...); }

 protected:
  Action<Ts...> *parent_;
};

// Forward declaration for WhileLoopContinuation
template<typename... Ts> class WhileAction;

/// Loop continuation for WhileAction that checks condition and repeats or continues.
/// Memory: 4-8 bytes (parent pointer) vs 40 bytes (LambdaAction with std::function).
template<typename... Ts> class WhileLoopContinuation : public Action<Ts...> {
 public:
  explicit WhileLoopContinuation(WhileAction<Ts...> *parent) : parent_(parent) {}

  void play(const Ts &...x) override;

 protected:
  WhileAction<Ts...> *parent_;
};

template<typename... Ts> class IfAction : public Action<Ts...> {
 public:
  explicit IfAction(Condition<Ts...> *condition) : condition_(condition) {}

  void add_then(const std::initializer_list<Action<Ts...> *> &actions) {
    this->then_.add_actions(actions);
    this->then_.add_action(new ContinuationAction<Ts...>(this));
  }

  void add_else(const std::initializer_list<Action<Ts...> *> &actions) {
    this->else_.add_actions(actions);
    this->else_.add_action(new ContinuationAction<Ts...>(this));
  }

  void play_complex(const Ts &...x) override {
    this->num_running_++;
    bool res = this->condition_->check(x...);
    if (res) {
      if (this->then_.empty()) {
        this->play_next_(x...);
      } else if (this->num_running_ > 0) {
        this->then_.play(x...);
      }
    } else {
      if (this->else_.empty()) {
        this->play_next_(x...);
      } else if (this->num_running_ > 0) {
        this->else_.play(x...);
      }
    }
  }

  void play(const Ts &...x) override { /* ignore - see play_complex */
  }

  void stop() override {
    this->then_.stop();
    this->else_.stop();
  }

 protected:
  Condition<Ts...> *condition_;
  ActionList<Ts...> then_;
  ActionList<Ts...> else_;
};

template<typename... Ts> class WhileAction : public Action<Ts...> {
 public:
  WhileAction(Condition<Ts...> *condition) : condition_(condition) {}

  void add_then(const std::initializer_list<Action<Ts...> *> &actions) {
    this->then_.add_actions(actions);
    this->then_.add_action(new WhileLoopContinuation<Ts...>(this));
  }

  friend class WhileLoopContinuation<Ts...>;

  void play_complex(const Ts &...x) override {
    this->num_running_++;
    // Initial condition check
    if (!this->condition_->check(x...)) {
      // If new condition check failed, stop loop if running
      this->then_.stop();
      this->play_next_(x...);
      return;
    }

    if (this->num_running_ > 0) {
      this->then_.play(x...);
    }
  }

  void play(const Ts &...x) override { /* ignore - see play_complex */
  }

  void stop() override { this->then_.stop(); }

 protected:
  Condition<Ts...> *condition_;
  ActionList<Ts...> then_;
};

// Implementation of WhileLoopContinuation::play
template<typename... Ts> void WhileLoopContinuation<Ts...>::play(const Ts &...x) {
  if (this->parent_->num_running_ > 0 && this->parent_->condition_->check(x...)) {
    // play again
    this->parent_->then_.play(x...);
  } else {
    // condition false, play next
    this->parent_->play_next_(x...);
  }
}

// Forward declaration for RepeatLoopContinuation
template<typename... Ts> class RepeatAction;

/// Loop continuation for RepeatAction that increments iteration and repeats or continues.
/// Memory: 4-8 bytes (parent pointer) vs 40 bytes (LambdaAction with std::function).
template<typename... Ts> class RepeatLoopContinuation : public Action<uint32_t, Ts...> {
 public:
  explicit RepeatLoopContinuation(RepeatAction<Ts...> *parent) : parent_(parent) {}

  void play(const uint32_t &iteration, const Ts &...x) override;

 protected:
  RepeatAction<Ts...> *parent_;
};

template<typename... Ts> class RepeatAction : public Action<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint32_t, count)

  void add_then(const std::initializer_list<Action<uint32_t, Ts...> *> &actions) {
    this->then_.add_actions(actions);
    this->then_.add_action(new RepeatLoopContinuation<Ts...>(this));
  }

  friend class RepeatLoopContinuation<Ts...>;

  void play_complex(const Ts &...x) override {
    this->num_running_++;
    if (this->count_.value(x...) > 0) {
      this->then_.play(0, x...);
    } else {
      this->play_next_(x...);
    }
  }

  void play(const Ts &...x) override { /* ignore - see play_complex */
  }

  void stop() override { this->then_.stop(); }

 protected:
  ActionList<uint32_t, Ts...> then_;
};

// Implementation of RepeatLoopContinuation::play
template<typename... Ts> void RepeatLoopContinuation<Ts...>::play(const uint32_t &iteration, const Ts &...x) {
  uint32_t next_iteration = iteration + 1;
  if (next_iteration >= this->parent_->count_.value(x...)) {
    this->parent_->play_next_(x...);
  } else {
    this->parent_->then_.play(next_iteration, x...);
  }
}

/** Wait until a condition is true to continue execution.
 *
 * Uses queue-based storage to safely handle concurrent executions.
 * While concurrent execution from the same trigger is uncommon, it's possible
 * (e.g., rapid button presses, high-frequency sensor updates), so we use
 * queue-based storage for correctness.
 */
template<typename... Ts> class WaitUntilAction : public Action<Ts...>, public Component {
 public:
  WaitUntilAction(Condition<Ts...> *condition) : condition_(condition) {}

  TEMPLATABLE_VALUE(uint32_t, timeout_value)

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
    if (this->condition_->check(x...)) {
      if (this->num_running_ > 0) {
        this->play_next_(x...);
      }
      return;
    }

    // Store for later processing
    auto now = millis();
    auto timeout = this->timeout_value_.optional_value(x...);
    this->var_queue_.emplace_back(now, timeout, std::make_tuple(x...));

    // Do immediate check with fresh timestamp - don't call loop() synchronously!
    // Let the event loop call it to avoid reentrancy issues
    if (this->process_queue_(now)) {
      // Only enable loop if we still have pending items
      this->enable_loop();
    }
  }

  void loop() override {
    // Safe to use cached time - only called from Application::loop()
    if (this->num_running_ > 0 && !this->process_queue_(App.get_loop_component_start_time())) {
      // If queue is now empty, disable loop until next play_complex
      this->disable_loop();
    }
  }

  void stop() override {
    this->var_queue_.clear();
    this->disable_loop();
  }

  float get_setup_priority() const override { return setup_priority::DATA; }

  void play(const Ts &...x) override { /* ignore - see play_complex */
  }

 protected:
  // Helper: Process queue, triggering completed items and removing them
  // Returns true if queue still has pending items
  bool process_queue_(uint32_t now) {
    // Process each queued wait_until and remove completed ones
    this->var_queue_.remove_if([&](auto &queued) {
      auto start = std::get<uint32_t>(queued);
      auto timeout = std::get<optional<uint32_t>>(queued);
      auto &var = std::get<std::tuple<Ts...>>(queued);

      // Check if timeout has expired
      auto expired = timeout && (now - start) >= *timeout;

      // Keep waiting if not expired and condition not met
      if (!expired && !this->condition_->check_tuple(var)) {
        return false;
      }

      // Condition met or timed out - trigger next action
      this->play_next_tuple_(var);
      return true;
    });

    return !this->var_queue_.empty();
  }

  Condition<Ts...> *condition_;
  std::list<std::tuple<uint32_t, optional<uint32_t>, std::tuple<Ts...>>> var_queue_{};
};

template<typename... Ts> class UpdateComponentAction : public Action<Ts...> {
 public:
  UpdateComponentAction(PollingComponent *component) : component_(component) {}

  void play(const Ts &...x) override {
    if (!this->component_->is_ready())
      return;
    this->component_->update();
  }

 protected:
  PollingComponent *component_;
};

template<typename... Ts> class SuspendComponentAction : public Action<Ts...> {
 public:
  SuspendComponentAction(PollingComponent *component) : component_(component) {}

  void play(const Ts &...x) override {
    if (!this->component_->is_ready())
      return;
    this->component_->stop_poller();
  }

 protected:
  PollingComponent *component_;
};

template<typename... Ts> class ResumeComponentAction : public Action<Ts...> {
 public:
  ResumeComponentAction(PollingComponent *component) : component_(component) {}
  TEMPLATABLE_VALUE(uint32_t, update_interval)

  void play(const Ts &...x) override {
    if (!this->component_->is_ready()) {
      return;
    }
    optional<uint32_t> update_interval = this->update_interval_.optional_value(x...);
    if (update_interval.has_value()) {
      this->component_->set_update_interval(update_interval.value());
    }
    this->component_->start_poller();
  }

 protected:
  PollingComponent *component_;
};

}  // namespace esphome
