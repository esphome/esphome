#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome {

template<typename... Ts> class AndCondition : public Condition<Ts...> {
 public:
  explicit AndCondition(const std::vector<Condition<Ts...> *> &conditions) : conditions_(conditions) {}
  bool check(Ts... x) override {
    for (auto *condition : this->conditions_) {
      if (!condition->check(x...))
        return false;
    }

    return true;
  }

 protected:
  std::vector<Condition<Ts...> *> conditions_;
};

template<typename... Ts> class OrCondition : public Condition<Ts...> {
 public:
  explicit OrCondition(const std::vector<Condition<Ts...> *> &conditions) : conditions_(conditions) {}
  bool check(Ts... x) override {
    for (auto *condition : this->conditions_) {
      if (condition->check(x...))
        return true;
    }

    return false;
  }

 protected:
  std::vector<Condition<Ts...> *> conditions_;
};

template<typename... Ts> class NotCondition : public Condition<Ts...> {
 public:
  explicit NotCondition(Condition<Ts...> *condition) : condition_(condition) {}
  bool check(Ts... x) override { return !this->condition_->check(x...); }

 protected:
  Condition<Ts...> *condition_;
};

template<typename... Ts> class LambdaCondition : public Condition<Ts...> {
 public:
  explicit LambdaCondition(std::function<bool(Ts...)> &&f) : f_(std::move(f)) {}
  bool check(Ts... x) override { return this->f_(x...); }

 protected:
  std::function<bool(Ts...)> f_;
};

template<typename... Ts> class ForCondition : public Condition<Ts...>, public Component {
 public:
  explicit ForCondition(Condition<> *condition) : condition_(condition) {}

  TEMPLATABLE_VALUE(uint32_t, time);

  void loop() override { this->check_internal(); }
  float get_setup_priority() const override { return setup_priority::DATA; }
  bool check_internal() {
    bool cond = this->condition_->check();
    if (!cond)
      this->last_inactive_ = millis();
    return cond;
  }

  bool check(Ts... x) override {
    if (!this->check_internal())
      return false;
    return millis() - this->last_inactive_ >= this->time_.value(x...);
  }

 protected:
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
  void on_shutdown() override { this->trigger(); }
};

class LoopTrigger : public Trigger<>, public Component {
 public:
  void loop() override { this->trigger(); }
  float get_setup_priority() const override { return setup_priority::DATA; }
};

template<typename... Ts> class DelayAction : public Action<Ts...>, public Component {
 public:
  explicit DelayAction() = default;

  TEMPLATABLE_VALUE(uint32_t, delay)

  void play_complex(Ts... x) override {
    auto f = std::bind(&DelayAction<Ts...>::play_next_, this, x...);
    this->num_running_++;
    this->set_timeout(this->delay_.value(x...), f);
  }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void play(Ts... x) override { /* ignore - see play_complex */
  }

  void stop() override { this->cancel_timeout(""); }
};

template<typename... Ts> class LambdaAction : public Action<Ts...> {
 public:
  explicit LambdaAction(std::function<void(Ts...)> &&f) : f_(std::move(f)) {}

  void play(Ts... x) override { this->f_(x...); }

 protected:
  std::function<void(Ts...)> f_;
};

template<typename... Ts> class IfAction : public Action<Ts...> {
 public:
  explicit IfAction(Condition<Ts...> *condition) : condition_(condition) {}

  void add_then(const std::vector<Action<Ts...> *> &actions) {
    this->then_.add_actions(actions);
    this->then_.add_action(new LambdaAction<Ts...>([this](Ts... x) { this->play_next_(x...); }));
  }

  void add_else(const std::vector<Action<Ts...> *> &actions) {
    this->else_.add_actions(actions);
    this->else_.add_action(new LambdaAction<Ts...>([this](Ts... x) { this->play_next_(x...); }));
  }

  void play_complex(Ts... x) override {
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

  void play(Ts... x) override { /* ignore - see play_complex */
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

  void add_then(const std::vector<Action<Ts...> *> &actions) {
    this->then_.add_actions(actions);
    this->then_.add_action(new LambdaAction<Ts...>([this](Ts... x) {
      if (this->num_running_ > 0 && this->condition_->check_tuple(this->var_)) {
        // play again
        if (this->num_running_ > 0) {
          this->then_.play_tuple(this->var_);
        }
      } else {
        // condition false, play next
        this->play_next_tuple_(this->var_);
      }
    }));
  }

  void play_complex(Ts... x) override {
    this->num_running_++;
    // Store loop parameters
    this->var_ = std::make_tuple(x...);
    // Initial condition check
    if (!this->condition_->check_tuple(this->var_)) {
      // If new condition check failed, stop loop if running
      this->then_.stop();
      this->play_next_tuple_(this->var_);
      return;
    }

    if (this->num_running_ > 0) {
      this->then_.play_tuple(this->var_);
    }
  }

  void play(Ts... x) override { /* ignore - see play_complex */
  }

  void stop() override { this->then_.stop(); }

 protected:
  Condition<Ts...> *condition_;
  ActionList<Ts...> then_;
  std::tuple<Ts...> var_{};
};

template<typename... Ts> class RepeatAction : public Action<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint32_t, count)

  void add_then(const std::vector<Action<Ts...> *> &actions) {
    this->then_.add_actions(actions);
    this->then_.add_action(new LambdaAction<Ts...>([this](Ts... x) {
      this->iteration_++;
      if (this->iteration_ == this->count_.value(x...))
        this->play_next_tuple_(this->var_);
      else
        this->then_.play_tuple(this->var_);
    }));
  }

  void play_complex(Ts... x) override {
    this->num_running_++;
    this->var_ = std::make_tuple(x...);
    this->iteration_ = 0;
    this->then_.play_tuple(this->var_);
  }

  void play(Ts... x) override { /* ignore - see play_complex */
  }

  void stop() override { this->then_.stop(); }

 protected:
  uint32_t iteration_;
  ActionList<Ts...> then_;
  std::tuple<Ts...> var_;
};

template<typename... Ts> class WaitUntilAction : public Action<Ts...>, public Component {
 public:
  WaitUntilAction(Condition<Ts...> *condition) : condition_(condition) {}

  TEMPLATABLE_VALUE(uint32_t, timeout_value)

  void play_complex(Ts... x) override {
    this->num_running_++;
    // Check if we can continue immediately.
    if (this->condition_->check(x...)) {
      if (this->num_running_ > 0) {
        this->play_next_(x...);
      }
      return;
    }
    this->var_ = std::make_tuple(x...);

    if (this->timeout_value_.has_value()) {
      auto f = std::bind(&WaitUntilAction<Ts...>::play_next_, this, x...);
      this->set_timeout("timeout", this->timeout_value_.value(x...), f);
    }

    this->loop();
  }

  void loop() override {
    if (this->num_running_ == 0)
      return;

    if (!this->condition_->check_tuple(this->var_)) {
      return;
    }

    this->cancel_timeout("timeout");

    this->play_next_tuple_(this->var_);
  }

  float get_setup_priority() const override { return setup_priority::DATA; }

  void play(Ts... x) override { /* ignore - see play_complex */
  }

  void stop() override { this->cancel_timeout("timeout"); }

 protected:
  Condition<Ts...> *condition_;
  std::tuple<Ts...> var_{};
};

template<typename... Ts> class UpdateComponentAction : public Action<Ts...> {
 public:
  UpdateComponentAction(PollingComponent *component) : component_(component) {}

  void play(Ts... x) override {
    if (this->component_->is_failed())
      return;
    this->component_->update();
  }

 protected:
  PollingComponent *component_;
};

}  // namespace esphome
