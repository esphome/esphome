#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include <concepts>
#include <functional>
#include <utility>
#include <vector>

namespace esphome {

// C++20 std::index_sequence is now used for tuple unpacking
// Legacy seq<>/gens<> pattern deprecated but kept for backwards compatibility
// https://stackoverflow.com/questions/7858817/unpacking-a-tuple-to-call-a-matching-function-pointer/7858971#7858971
// Remove before 2026.6.0
// NOLINTBEGIN(readability-identifier-naming)
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

template<int...> struct ESPDEPRECATED("Use std::index_sequence instead. Removed in 2026.6.0", "2025.12.0") seq {};
template<int N, int... S>
struct ESPDEPRECATED("Use std::make_index_sequence instead. Removed in 2026.6.0", "2025.12.0") gens
    : gens<N - 1, N - 1, S...> {};
template<int... S> struct gens<0, S...> { using type = seq<S...>; };

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
// NOLINTEND(readability-identifier-naming)

#define TEMPLATABLE_VALUE_(type, name) \
 protected: \
  TemplatableValue<type, Ts...> name##_{}; \
\
 public: \
  template<typename V> void set_##name(V name) { this->name##_ = name; }

#define TEMPLATABLE_VALUE(type, name) TEMPLATABLE_VALUE_(type, name)

template<typename T, typename... X> class TemplatableValue {
 public:
  TemplatableValue() : type_(NONE) {}

  // For const char* when T is std::string: store pointer directly, no heap allocation
  // String remains in flash and is only converted to std::string when value() is called
  TemplatableValue(const char *str) requires std::same_as<T, std::string> : type_(STATIC_STRING) {
    this->static_str_ = str;
  }

  template<typename F> TemplatableValue(F value) requires(!std::invocable<F, X...>) : type_(VALUE) {
    new (&this->value_) T(std::move(value));
  }

  // For stateless lambdas (convertible to function pointer): use function pointer
  template<typename F>
  TemplatableValue(F f) requires std::invocable<F, X...> && std::convertible_to<F, T (*)(X...)>
      : type_(STATELESS_LAMBDA) {
    this->stateless_f_ = f;  // Implicit conversion to function pointer
  }

  // For stateful lambdas (not convertible to function pointer): use std::function
  template<typename F>
  TemplatableValue(F f) requires std::invocable<F, X...> &&(!std::convertible_to<F, T (*)(X...)>) : type_(LAMBDA) {
    this->f_ = new std::function<T(X...)>(std::move(f));
  }

  // Copy constructor
  TemplatableValue(const TemplatableValue &other) : type_(other.type_) {
    if (this->type_ == VALUE) {
      new (&this->value_) T(other.value_);
    } else if (this->type_ == LAMBDA) {
      this->f_ = new std::function<T(X...)>(*other.f_);
    } else if (this->type_ == STATELESS_LAMBDA) {
      this->stateless_f_ = other.stateless_f_;
    } else if (this->type_ == STATIC_STRING) {
      this->static_str_ = other.static_str_;
    }
  }

  // Move constructor
  TemplatableValue(TemplatableValue &&other) noexcept : type_(other.type_) {
    if (this->type_ == VALUE) {
      new (&this->value_) T(std::move(other.value_));
    } else if (this->type_ == LAMBDA) {
      this->f_ = other.f_;
      other.f_ = nullptr;
    } else if (this->type_ == STATELESS_LAMBDA) {
      this->stateless_f_ = other.stateless_f_;
    } else if (this->type_ == STATIC_STRING) {
      this->static_str_ = other.static_str_;
    }
    other.type_ = NONE;
  }

  // Assignment operators
  TemplatableValue &operator=(const TemplatableValue &other) {
    if (this != &other) {
      this->~TemplatableValue();
      new (this) TemplatableValue(other);
    }
    return *this;
  }

  TemplatableValue &operator=(TemplatableValue &&other) noexcept {
    if (this != &other) {
      this->~TemplatableValue();
      new (this) TemplatableValue(std::move(other));
    }
    return *this;
  }

  ~TemplatableValue() {
    if (this->type_ == VALUE) {
      this->value_.~T();
    } else if (this->type_ == LAMBDA) {
      delete this->f_;
    }
    // STATELESS_LAMBDA/STATIC_STRING/NONE: no cleanup needed (pointers, not heap-allocated)
  }

  bool has_value() { return this->type_ != NONE; }

  T value(X... x) {
    switch (this->type_) {
      case STATELESS_LAMBDA:
        return this->stateless_f_(x...);  // Direct function pointer call
      case LAMBDA:
        return (*this->f_)(x...);  // std::function call
      case VALUE:
        return this->value_;
      case STATIC_STRING:
        // if constexpr required: code must compile for all T, but STATIC_STRING
        // can only be set when T is std::string (enforced by constructor constraint)
        if constexpr (std::same_as<T, std::string>) {
          return std::string(this->static_str_);
        }
        __builtin_unreachable();
      case NONE:
      default:
        return T{};
    }
  }

  optional<T> optional_value(X... x) {
    if (!this->has_value()) {
      return {};
    }
    return this->value(x...);
  }

  T value_or(X... x, T default_value) {
    if (!this->has_value()) {
      return default_value;
    }
    return this->value(x...);
  }

 protected:
  enum : uint8_t {
    NONE,
    VALUE,
    LAMBDA,
    STATELESS_LAMBDA,
    STATIC_STRING,  // For const char* when T is std::string - avoids heap allocation
  } type_;

  union {
    T value_;
    std::function<T(X...)> *f_;
    T (*stateless_f_)(X...);
    const char *static_str_;  // For STATIC_STRING type
  };
};

/** Base class for all automation conditions.
 *
 * @tparam Ts The template parameters to pass when executing.
 */
template<typename... Ts> class Condition {
 public:
  /// Check whether this condition passes. This condition check must be instant, and not cause any delays.
  virtual bool check(const Ts &...x) = 0;

  /// Call check with a tuple of values as parameter.
  bool check_tuple(const std::tuple<Ts...> &tuple) {
    return this->check_tuple_(tuple, std::make_index_sequence<sizeof...(Ts)>{});
  }

 protected:
  template<size_t... S> bool check_tuple_(const std::tuple<Ts...> &tuple, std::index_sequence<S...> /*unused*/) {
    return this->check(std::get<S>(tuple)...);
  }
};

template<typename... Ts> class Automation;

template<typename... Ts> class Trigger {
 public:
  /// Inform the parent automation that the event has triggered.
  void trigger(const Ts &...x) {
    if (this->automation_parent_ == nullptr)
      return;
    this->automation_parent_->trigger(x...);
  }
  void set_automation_parent(Automation<Ts...> *automation_parent) { this->automation_parent_ = automation_parent; }

  /// Stop any action connected to this trigger.
  void stop_action() {
    if (this->automation_parent_ == nullptr)
      return;
    this->automation_parent_->stop();
  }
  /// Returns true if any action connected to this trigger is running.
  bool is_action_running() {
    if (this->automation_parent_ == nullptr)
      return false;
    return this->automation_parent_->is_running();
  }

 protected:
  Automation<Ts...> *automation_parent_{nullptr};
};

template<typename... Ts> class ActionList;

template<typename... Ts> class Action {
 public:
  virtual void play_complex(const Ts &...x) {
    this->num_running_++;
    this->play(x...);
    this->play_next_(x...);
  }
  virtual void stop_complex() {
    if (num_running_) {
      this->stop();
      this->num_running_ = 0;
    }
    this->stop_next_();
  }
  /// Check if this or any of the following actions are currently running.
  virtual bool is_running() { return this->num_running_ > 0 || this->is_running_next_(); }

  /// The total number of actions that are currently running in this plus any of
  /// the following actions in the chain.
  int num_running_total() {
    int total = this->num_running_;
    if (this->next_ != nullptr)
      total += this->next_->num_running_total();
    return total;
  }

 protected:
  friend ActionList<Ts...>;
  template<typename... Us> friend class ContinuationAction;

  virtual void play(const Ts &...x) = 0;
  void play_next_(const Ts &...x) {
    if (this->num_running_ > 0) {
      this->num_running_--;
      if (this->next_ != nullptr) {
        this->next_->play_complex(x...);
      }
    }
  }
  template<size_t... S> void play_next_tuple_(const std::tuple<Ts...> &tuple, std::index_sequence<S...> /*unused*/) {
    this->play_next_(std::get<S>(tuple)...);
  }
  void play_next_tuple_(const std::tuple<Ts...> &tuple) {
    this->play_next_tuple_(tuple, std::make_index_sequence<sizeof...(Ts)>{});
  }

  virtual void stop() {}
  void stop_next_() {
    if (this->next_ != nullptr) {
      this->next_->stop_complex();
    }
  }

  bool is_running_next_() {
    if (this->next_ == nullptr)
      return false;
    return this->next_->is_running();
  }

  Action<Ts...> *next_{nullptr};

  /// The number of instances of this sequence in the list of actions
  /// that is currently being executed.
  int num_running_{0};
};

template<typename... Ts> class ActionList {
 public:
  void add_action(Action<Ts...> *action) {
    if (this->actions_end_ == nullptr) {
      this->actions_begin_ = action;
    } else {
      this->actions_end_->next_ = action;
    }
    this->actions_end_ = action;
  }
  void add_actions(const std::initializer_list<Action<Ts...> *> &actions) {
    for (auto *action : actions) {
      this->add_action(action);
    }
  }
  void play(const Ts &...x) {
    if (this->actions_begin_ != nullptr)
      this->actions_begin_->play_complex(x...);
  }
  void play_tuple(const std::tuple<Ts...> &tuple) {
    this->play_tuple_(tuple, std::make_index_sequence<sizeof...(Ts)>{});
  }
  void stop() {
    if (this->actions_begin_ != nullptr)
      this->actions_begin_->stop_complex();
  }
  bool empty() const { return this->actions_begin_ == nullptr; }

  /// Check if any action in this action list is currently running.
  bool is_running() {
    if (this->actions_begin_ == nullptr)
      return false;
    return this->actions_begin_->is_running();
  }
  /// Return the number of actions in this action list that are currently running.
  int num_running() {
    if (this->actions_begin_ == nullptr)
      return 0;
    return this->actions_begin_->num_running_total();
  }

 protected:
  template<size_t... S> void play_tuple_(const std::tuple<Ts...> &tuple, std::index_sequence<S...> /*unused*/) {
    this->play(std::get<S>(tuple)...);
  }

  Action<Ts...> *actions_begin_{nullptr};
  Action<Ts...> *actions_end_{nullptr};
};

template<typename... Ts> class Automation {
 public:
  explicit Automation(Trigger<Ts...> *trigger) : trigger_(trigger) { this->trigger_->set_automation_parent(this); }

  void add_action(Action<Ts...> *action) { this->actions_.add_action(action); }
  void add_actions(const std::initializer_list<Action<Ts...> *> &actions) { this->actions_.add_actions(actions); }

  void stop() { this->actions_.stop(); }

  void trigger(const Ts &...x) { this->actions_.play(x...); }

  bool is_running() { return this->actions_.is_running(); }

  /// Return the number of actions in the action part of this automation that are currently running.
  int num_running() { return this->actions_.num_running(); }

 protected:
  Trigger<Ts...> *trigger_;
  ActionList<Ts...> actions_;
};

}  // namespace esphome
