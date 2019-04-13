#pragma once
#include "esphome/core/automation.h"

namespace esphome {

template<typename... Ts>
bool Condition<Ts...>::check_tuple(const std::tuple<Ts...> &tuple) {
  return this->check_tuple_(tuple, typename gens<sizeof...(Ts)>::type());
}
template<typename... Ts>
template<int... S>
bool Condition<Ts...>::check_tuple_(const std::tuple<Ts...> &tuple, seq<S...>) {
  return this->check(std::get<S>(tuple)...);
}

template<typename... Ts>
bool AndCondition<Ts...>::check(Ts... x) {
  for (auto *condition : this->conditions_) {
    if (!condition->check(x...))
      return false;
  }

  return true;
}

template<typename... Ts>
AndCondition<Ts...>::AndCondition(const std::vector<Condition < Ts...> *> &conditions)
  : conditions_(conditions) {}

template<typename... Ts>
bool OrCondition<Ts...>::check(Ts... x) {
  for (auto *condition : this->conditions_) {
    if (condition->check(x...))
      return true;
  }

  return false;
}

template<typename... Ts>
OrCondition<Ts...>::OrCondition(const std::vector<Condition < Ts...> *> &conditions)
  : conditions_(conditions) {}

template<typename... Ts>
void Trigger<Ts...>::set_parent(Automation<Ts...> *parent) { this->parent_ = parent; }

template<typename... Ts>
void Trigger<Ts...>::trigger(Ts... x) {
  if (this->parent_ == nullptr)
    return;
  this->parent_->trigger(x...);
}
template<typename... Ts>
void Trigger<Ts...>::stop() {
  if (this->parent_ == nullptr)
    return;
  this->parent_->stop();
}

template<typename... Ts>
void Action<Ts...>::play_next(Ts... x) {
  if (this->next_ != nullptr) {
    this->next_->play(x...);
  }
}
template<typename... Ts>
void Action<Ts...>::stop() { this->stop_next(); }
template<typename... Ts>
void Action<Ts...>::stop_next() {
  if (this->next_ != nullptr) {
    this->next_->stop();
  }
}
template<typename... Ts>
void Action<Ts...>::play_next_tuple(const std::tuple<Ts...> &tuple) {
  this->play_next_tuple_(tuple, typename gens<sizeof...(Ts)>::type());
}
template<typename... Ts>
template<int... S>
void Action<Ts...>::play_next_tuple_(const std::tuple<Ts...> &tuple, seq<S...>) {
  this->play_next(std::get<S>(tuple)...);
}

template<typename... Ts>
DelayAction<Ts...>::DelayAction() = default;

template<typename... Ts>
void DelayAction<Ts...>::play(Ts... x) {
  auto f = std::bind(&DelayAction<Ts...>::play_next, this, x...);
  this->set_timeout(this->delay_.value(x...), f);
}
template<typename... Ts>
float DelayAction<Ts...>::get_setup_priority() const { return setup_priority::HARDWARE; }
template<typename... Ts>
void DelayAction<Ts...>::stop() {
  this->cancel_timeout("");
  this->stop_next();
}

template<typename... Ts>
Condition<Ts...> *Automation<Ts...>::add_condition(Condition<Ts...> *condition) {
  this->conditions_.push_back(condition);
  return condition;
}
template<typename... Ts>
void Automation<Ts...>::add_conditions(const std::vector<Condition < Ts...> *> &conditions) {
  for (auto *condition : conditions) {
    this->add_condition(condition);
  }
}
template<typename... Ts>
Automation<Ts...>::Automation(Trigger<Ts...> *trigger) : trigger_(trigger) {
  this->trigger_->set_parent(this);
}
template<typename... Ts>
Action<Ts...> *Automation<Ts...>::add_action(Action<Ts...> *action) {
  this->actions_.add_action(action);
}
template<typename... Ts>
void Automation<Ts...>::add_actions(const std::vector<Action < Ts...> *> &actions) {
  this->actions_.add_actions(actions);
}
template<typename... Ts>
void Automation<Ts...>::trigger(Ts... x) {
  for (auto *condition : this->conditions_) {
    if (!condition->check(x...))
      return;
  }

  this->actions_.play(x...);
}
template<typename... Ts>
void Automation<Ts...>::stop() { this->actions_.stop(); }
template<typename... Ts>
LambdaCondition<Ts...>::LambdaCondition(std::function<bool(Ts...)> &&f) : f_(std::move(f)) {}
template<typename... Ts>
bool LambdaCondition<Ts...>::check(Ts... x) { return this->f_(x...); }

template<typename... Ts>
LambdaAction<Ts...>::LambdaAction(std::function<void(Ts...)> &&f) : f_(std::move(f)) {}
template<typename... Ts>
void LambdaAction<Ts...>::play(Ts... x) {
  this->f_(x...);
  this->play_next(x...);
}

template<typename... Ts>
Action<Ts...> *ActionList<Ts...>::add_action(Action<Ts...> *action) {
  if (this->actions_end_ == nullptr) {
    this->actions_begin_ = action;
  } else {
    this->actions_end_->next_ = action;
  }
  return this->actions_end_ = action;
}
template<typename... Ts>
void ActionList<Ts...>::add_actions(const std::vector<Action < Ts...> *> &actions) {
  for (auto *action: actions) {
    this->add_action(action);
  }
}
template<typename... Ts>
void ActionList<Ts...>::play(Ts... x) {
  if (this->actions_begin_ != nullptr)
    this->actions_begin_->play(x...);
}
template<typename... Ts>
void ActionList<Ts...>::stop() {
  if (this->actions_begin_ != nullptr)
    this->actions_begin_->stop();
}
template<typename... Ts>
bool ActionList<Ts...>::empty() const { return this->actions_begin_ == nullptr; }
template<typename... Ts>
IfAction<Ts...>::IfAction(const std::vector<Condition < Ts...> *
> conditions) :
conditions_(conditions) {}
template<typename... Ts>
void IfAction<Ts...>::play(Ts... x) {
  bool res = true;
  for (auto *condition : this->conditions_) {
    if (!condition->check(x...)) {
      res = false;
      break;
    }
  }
  if (res) {
    if (this->then_.empty()) {
      this->play_next(x...);
    } else {
      this->then_.play(x...);
    }
  } else {
    if (this->else_.empty()) {
      this->play_next(x...);
    } else {
      this->else_.play(x...);
    }
  }
}
template<typename... Ts>
void IfAction<Ts...>::add_then(const std::vector<Action < Ts...> *> &actions) {
  this->then_.add_actions(actions);
  this->then_.add_action(new LambdaAction<Ts...>([this](Ts... x) {
    this->play_next(x...);
  }));
}
template<typename... Ts>
void IfAction<Ts...>::add_else(const std::vector<Action < Ts...> *> &actions) {
  this->else_.add_actions(actions);
  this->else_.add_action(new LambdaAction<Ts...>([this](Ts... x) {
    this->play_next(x...);
  }));
}
template<typename... Ts>
void IfAction<Ts...>::stop() {
  this->then_.stop();
  this->else_.stop();
  this->stop_next();
}

template<typename... Ts>
void UpdateComponentAction<Ts...>::play(Ts... x) {
  this->component_->update();
  this->play_next(x...);
}

template<typename... Ts>
UpdateComponentAction<Ts...>::UpdateComponentAction(PollingComponent *component) : component_(component) {}

template<typename... Ts>
WhileAction<Ts...>::WhileAction(const std::vector<Condition < Ts...> *> &conditions) : conditions_(conditions) {}

template<typename... Ts>
void WhileAction<Ts...>::add_then(const std::vector<Action < Ts...> *> &actions) {
  this->then_.add_actions(actions);
  this->then_.add_action(new LambdaAction<Ts...>([this](Ts... x) {
    this->is_running_ = false;
    this->play(x...);
  }));
}
template<typename... Ts>
void WhileAction<Ts...>::play(Ts... x) {
  if (this->is_running_)
    return;

  for (auto *condition : this->conditions_) {
    if (!condition->check(x...)) {
      this->play_next(x...);
      return;
    }
  }
  this->is_running_ = true;
  this->then_.play(x...);
}
template<typename... Ts>
void WhileAction<Ts...>::stop() {
  this->then_.stop();
  this->is_running_ = false;
  this->stop_next();
}

template<typename... Ts>
WaitUntilAction<Ts...>::WaitUntilAction(const std::vector<Condition < Ts...> *
> &conditions) : conditions_(conditions) {}
template<typename... Ts>
void WaitUntilAction<Ts...>::play(Ts... x) {
  this->var_ = std::make_tuple(x...);
  this->triggered_ = true;
  this->loop();
}
template<typename... Ts>
void WaitUntilAction<Ts...>::stop() {
  this->triggered_ = false;
  this->stop_next();
}
template<typename... Ts>
void WaitUntilAction<Ts...>::loop() {
  if (!this->triggered_)
    return;

  for (auto *condition : this->conditions_) {
    if (!condition->check_tuple(this->var_)) {
      return;
    }
  }

  this->triggered_ = false;
  this->play_next_tuple(this->var_);
}
template<typename... Ts>
float WaitUntilAction<Ts...>::get_setup_priority() const {
  return setup_priority::DATA;
}

}  // namespace esphome
