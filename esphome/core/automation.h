#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/core/progmem.h"
#include "esphome/core/string_ref.h"
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

/// Function-pointer-only templatable storage (4 bytes on 32-bit).
/// Used by the TEMPLATABLE_VALUE macro for codegen-managed fields.
/// Codegen wraps constants in stateless lambdas so only a function pointer is needed.
template<typename T, typename... X> class TemplatableFn {
 public:
  TemplatableFn() = default;
  TemplatableFn(std::nullptr_t) = delete;

  // Exact return type match — direct function pointer storage
  template<typename F> TemplatableFn(F f) requires std::convertible_to<F, T (*)(X...)> : f_(f) {}

  // Convertible return type (e.g., int -> uint8_t) — casting trampoline.
  // Stateless lambdas are default-constructible in C++20, so F{} recreates the lambda inside
  // the trampoline without capturing. This compiles to the same code as a direct call + cast.
  // Deprecated: codegen should use the correct output type to avoid the trampoline.
  template<typename F>
      [[deprecated("Lambda return type does not match TemplatableFn<T> — use the correct type in "
                   "codegen")]] TemplatableFn(F) requires(!std::convertible_to<F, T (*)(X...)>) &&
      std::invocable<F, X...> &&std::convertible_to<std::invoke_result_t<F, X...>, T> &&std::is_empty_v<F>
          &&std::default_initializable<F> : f_([](X... x) -> T { return static_cast<T>(F{}(x...)); }) {}

  // Reject any callable that didn't match the above (stateful lambdas or inconvertible return types)
  template<typename F>
  TemplatableFn(F) requires std::invocable<F, X...> &&
      (!std::convertible_to<F, T (*)(X...)>) &&(!std::is_empty_v<F> ||
                                                !std::convertible_to<std::invoke_result_t<F, X...>, T> ||
                                                !std::default_initializable<F>) = delete;

  // Reject raw (non-callable) values with a helpful diagnostic pointing at the Python-side fix.
  // TemplatableFn stores only a function pointer (4 bytes), so constants must be wrapped in a
  // stateless lambda by codegen. External components hitting this error should use
  // `cg.templatable(value, args, type)` in their Python __init__.py before passing to the setter.
  template<typename V> TemplatableFn(V) requires(!std::invocable<V, X...>) && (!std::convertible_to<V, T (*)(X...)>) {
    static_assert(sizeof(V) == 0, "Missing cg.templatable(...) in Python codegen for this TEMPLATABLE_VALUE "
                                  "field. The wrapper was always required; it worked by accident because the old "
                                  "TemplatableValue implicitly converted raw constants. TemplatableFn cannot. See "
                                  "https://developers.esphome.io/blog/2026/04/09/"
                                  "templatablefn-4-byte-templatable-storage-for-trivially-copyable-types/");
  }

  bool has_value() const { return this->f_ != nullptr; }

  T value(X... x) const { return this->f_ ? this->f_(x...) : T{}; }

  optional<T> optional_value(X... x) const {
    if (!this->f_)
      return {};
    return this->f_(x...);
  }

  T value_or(X... x, T default_value) const { return this->f_ ? this->f_(x...) : default_value; }

 protected:
  T (*f_)(X...){nullptr};
};

// Forward declaration for TemplatableValue (string specialization needs it)
template<typename T, typename... X> class TemplatableValue;

/// Selects TemplatableFn (4 bytes) for trivially copyable types, TemplatableValue (8 bytes) otherwise.
/// Non-trivial types (std::string, std::vector<uint8_t>, etc.) need TemplatableValue for raw value
/// storage, PROGMEM/FlashStringHelper support (strings), and proper copy/move/destruction.
template<typename T, typename... X>
using TemplatableStorage =
    std::conditional_t<std::is_trivially_copyable_v<T>, TemplatableFn<T, X...>, TemplatableValue<T, X...>>;

#define TEMPLATABLE_VALUE_(type, name) \
 protected: \
  TemplatableStorage<type, Ts...> name##_{}; \
\
 public: \
  template<typename V> void set_##name(V name) { this->name##_ = name; }

#define TEMPLATABLE_VALUE(type, name) TEMPLATABLE_VALUE_(type, name)

/// Primary TemplatableValue: stores either a constant value or a function pointer.
/// No std::function, no string-specific paths. 8 bytes on 32-bit.
/// Accepts raw constants for backward compatibility with direct C++ usage.
template<typename T, typename... X> class TemplatableValue {
 public:
  TemplatableValue() = default;
  TemplatableValue(std::nullptr_t) = delete;

  // Accept raw constants
  template<typename V> TemplatableValue(V value) requires(!std::invocable<V, X...>) : tag_(VALUE) {
    new (&this->storage_.value_) T(static_cast<T>(std::move(value)));
  }

  // Accept stateless lambdas (convertible to function pointer)
  template<typename F> TemplatableValue(F f) requires std::convertible_to<F, T (*)(X...)> : tag_(FN) {
    this->storage_.f_ = f;
  }

  // Convertible return type (e.g., int -> uint8_t) — casting trampoline
  template<typename F>
      [[deprecated("Lambda return type does not match TemplatableValue<T> — use the correct type in "
                   "codegen")]] TemplatableValue(F) requires(!std::convertible_to<F, T (*)(X...)>) &&
      std::invocable<F, X...> &&std::convertible_to<std::invoke_result_t<F, X...>, T> &&std::is_empty_v<F>
          &&std::default_initializable<F> : tag_(FN) {
    this->storage_.f_ = [](X... x) -> T { return static_cast<T>(F{}(x...)); };
  }

  // Reject any callable that didn't match the above
  template<typename F>
  TemplatableValue(F) requires std::invocable<F, X...> &&
      (!std::convertible_to<F, T (*)(X...)>) &&(!std::is_empty_v<F> ||
                                                !std::convertible_to<std::invoke_result_t<F, X...>, T> ||
                                                !std::default_initializable<F>) = delete;

  TemplatableValue(const TemplatableValue &other) : tag_(other.tag_) {
    if (this->tag_ == VALUE) {
      new (&this->storage_.value_) T(other.storage_.value_);
    } else if (this->tag_ == FN) {
      this->storage_.f_ = other.storage_.f_;
    }
  }

  TemplatableValue(TemplatableValue &&other) noexcept : tag_(other.tag_) {
    if (this->tag_ == VALUE) {
      new (&this->storage_.value_) T(std::move(other.storage_.value_));
      other.destroy_();
    } else if (this->tag_ == FN) {
      this->storage_.f_ = other.storage_.f_;
    }
    other.tag_ = NONE;
  }

  TemplatableValue &operator=(const TemplatableValue &other) {
    if (this != &other) {
      this->destroy_();
      this->tag_ = other.tag_;
      if (this->tag_ == VALUE) {
        new (&this->storage_.value_) T(other.storage_.value_);
      } else if (this->tag_ == FN) {
        this->storage_.f_ = other.storage_.f_;
      }
    }
    return *this;
  }

  TemplatableValue &operator=(TemplatableValue &&other) noexcept {
    if (this != &other) {
      this->destroy_();
      this->tag_ = other.tag_;
      if (this->tag_ == VALUE) {
        new (&this->storage_.value_) T(std::move(other.storage_.value_));
        other.destroy_();
      } else if (this->tag_ == FN) {
        this->storage_.f_ = other.storage_.f_;
      }
      other.tag_ = NONE;
    }
    return *this;
  }

  ~TemplatableValue() { this->destroy_(); }

  bool has_value() const { return this->tag_ != NONE; }

  T value(X... x) const {
    if (this->tag_ == FN)
      return this->storage_.f_(x...);
    if (this->tag_ == VALUE)
      return this->storage_.value_;
    return T{};
  }

  optional<T> optional_value(X... x) const {
    if (this->tag_ == NONE)
      return {};
    return this->value(x...);
  }

  T value_or(X... x, T default_value) const {
    if (this->tag_ == NONE)
      return default_value;
    return this->value(x...);
  }

 protected:
  void destroy_() {
    if constexpr (!std::is_trivially_destructible_v<T>) {
      if (this->tag_ == VALUE)
        this->storage_.value_.~T();
    }
  }

  enum Tag : uint8_t { NONE, VALUE, FN } tag_{NONE};
  // Union with explicit ctor/dtor to support non-trivially-constructible/destructible T
  // (e.g., std::vector<uint8_t>). Lifetime of value_ is managed externally via
  // placement new and destroy_().
  union Storage {
    constexpr Storage() : f_(nullptr) {}
    constexpr ~Storage() {}
    T value_;
    T (*f_)(X...);
  } storage_;
};

/// Specialization for std::string: supports VALUE, STATIC_STRING, FLASH_STRING,
/// stateless lambdas, and stateful lambdas (std::function).
template<typename... X> class TemplatableValue<std::string, X...> {
 public:
  TemplatableValue() : type_(NONE) {}

  // For const char*: store pointer directly, no heap allocation.
  // String remains in flash and is only converted to std::string when value() is called.
  TemplatableValue(const char *str) : type_(STATIC_STRING) { this->static_str_ = str; }

#ifdef USE_ESP8266
  // On ESP8266, __FlashStringHelper* is a distinct type from const char*.
  // ESPHOME_F(s) expands to F(s) which returns __FlashStringHelper* pointing to PROGMEM.
  // Store as FLASH_STRING — value()/is_empty()/ref_or_copy_to() use _P functions.
  TemplatableValue(const __FlashStringHelper *str) : type_(FLASH_STRING) {
    this->static_str_ = reinterpret_cast<const char *>(str);
  }
#endif

  template<typename F> TemplatableValue(F value) requires(!std::invocable<F, X...>) : type_(VALUE) {
    this->value_ = new std::string(std::move(value));
  }

  // For stateless lambdas (convertible to function pointer): use function pointer
  template<typename F>
  TemplatableValue(F f) requires std::invocable<F, X...> && std::convertible_to<F, std::string (*)(X...)>
      : type_(STATELESS_LAMBDA) {
    this->stateless_f_ = f;  // Implicit conversion to function pointer
  }

  // For stateful lambdas (not convertible to function pointer): use std::function
  template<typename F>
  TemplatableValue(F f) requires std::invocable<F, X...> &&(!std::convertible_to<F, std::string (*)(X...)>)
      : type_(LAMBDA) {
    this->f_ = new std::function<std::string(X...)>(std::move(f));
  }

  // Copy constructor
  TemplatableValue(const TemplatableValue &other) : type_(other.type_) {
    if (this->type_ == VALUE) {
      this->value_ = new std::string(*other.value_);
    } else if (this->type_ == LAMBDA) {
      this->f_ = new std::function<std::string(X...)>(*other.f_);
    } else if (this->type_ == STATELESS_LAMBDA) {
      this->stateless_f_ = other.stateless_f_;
    } else if (this->type_ == STATIC_STRING || this->type_ == FLASH_STRING) {
      this->static_str_ = other.static_str_;
    }
  }

  // Move constructor
  TemplatableValue(TemplatableValue &&other) noexcept : type_(other.type_) {
    if (this->type_ == VALUE) {
      this->value_ = other.value_;
      other.value_ = nullptr;
    } else if (this->type_ == LAMBDA) {
      this->f_ = other.f_;
      other.f_ = nullptr;
    } else if (this->type_ == STATELESS_LAMBDA) {
      this->stateless_f_ = other.stateless_f_;
    } else if (this->type_ == STATIC_STRING || this->type_ == FLASH_STRING) {
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
      delete this->value_;
    } else if (this->type_ == LAMBDA) {
      delete this->f_;
    }
    // STATELESS_LAMBDA/STATIC_STRING/FLASH_STRING/NONE: no cleanup needed (pointers, not heap-allocated)
  }

  bool has_value() const { return this->type_ != NONE; }

  std::string value(X... x) const {
    switch (this->type_) {
      case STATELESS_LAMBDA:
        return this->stateless_f_(x...);  // Direct function pointer call
      case LAMBDA:
        return (*this->f_)(x...);  // std::function call
      case VALUE:
        return *this->value_;
      case STATIC_STRING:
        return std::string(this->static_str_);
#ifdef USE_ESP8266
      case FLASH_STRING: {
        // PROGMEM pointer — must use _P functions to access on ESP8266
        size_t len = strlen_P(this->static_str_);
        std::string result(len, '\0');
        memcpy_P(result.data(), this->static_str_, len);
        return result;
      }
#endif
      case NONE:
      default:
        return {};
    }
  }

  optional<std::string> optional_value(X... x) const {
    if (!this->has_value())
      return {};
    return this->value(x...);
  }

  std::string value_or(X... x, std::string default_value) const {
    if (!this->has_value())
      return default_value;
    return this->value(x...);
  }

  /// Check if this holds a static string (const char* stored without allocation)
  /// The pointer is always directly readable (RAM or flash-mapped).
  /// Returns false for FLASH_STRING (PROGMEM on ESP8266, requires _P functions).
  bool is_static_string() const { return this->type_ == STATIC_STRING; }

  /// Get the static string pointer (only valid if is_static_string() returns true)
  /// The pointer is always directly readable — FLASH_STRING uses a separate type.
  const char *get_static_string() const { return this->static_str_; }

  /// Check if the string value is empty without allocating.
  /// For NONE, returns true. For STATIC_STRING/VALUE, checks without allocation.
  /// For LAMBDA/STATELESS_LAMBDA, must call value() which may allocate.
  bool is_empty() const {
    switch (this->type_) {
      case NONE:
        return true;
      case STATIC_STRING:
        return this->static_str_ == nullptr || this->static_str_[0] == '\0';
#ifdef USE_ESP8266
      case FLASH_STRING:
        // PROGMEM pointer — must use progmem_read_byte on ESP8266
        return this->static_str_ == nullptr ||
               progmem_read_byte(reinterpret_cast<const uint8_t *>(this->static_str_)) == '\0';
#endif
      case VALUE:
        return this->value_->empty();
      default:  // LAMBDA/STATELESS_LAMBDA - must call value()
        return this->value().empty();
    }
  }

  /// Get a StringRef to the string value without heap allocation when possible.
  /// For STATIC_STRING/VALUE, returns reference to existing data (no allocation).
  /// For FLASH_STRING (ESP8266 PROGMEM), copies to provided buffer via _P functions.
  /// For LAMBDA/STATELESS_LAMBDA, calls value(), copies to provided buffer, returns ref to buffer.
  /// @param lambda_buf Buffer used only for copy cases (must remain valid while StringRef is used).
  /// @param lambda_buf_size Size of the buffer.
  /// @return StringRef pointing to the string data.
  StringRef ref_or_copy_to(char *lambda_buf, size_t lambda_buf_size) const {
    switch (this->type_) {
      case NONE:
        return StringRef();
      case STATIC_STRING:
        if (this->static_str_ == nullptr)
          return StringRef();
        return StringRef(this->static_str_, strlen(this->static_str_));
#ifdef USE_ESP8266
      case FLASH_STRING:
        if (this->static_str_ == nullptr)
          return StringRef();
        {
          // PROGMEM pointer — copy to buffer via _P functions
          size_t len = strlen_P(this->static_str_);
          size_t copy_len = std::min(len, lambda_buf_size - 1);
          memcpy_P(lambda_buf, this->static_str_, copy_len);
          lambda_buf[copy_len] = '\0';
          return StringRef(lambda_buf, copy_len);
        }
#endif
      case VALUE:
        return StringRef(this->value_->data(), this->value_->size());
      default: {  // LAMBDA/STATELESS_LAMBDA - must call value() and copy
        std::string result = this->value();
        size_t copy_len = std::min(result.size(), lambda_buf_size - 1);
        memcpy(lambda_buf, result.data(), copy_len);
        lambda_buf[copy_len] = '\0';
        return StringRef(lambda_buf, copy_len);
      }
    }
  }

 protected:
  enum : uint8_t {
    NONE,
    VALUE,
    LAMBDA,
    STATELESS_LAMBDA,
    STATIC_STRING,  // For const char* — avoids heap allocation
    FLASH_STRING,   // PROGMEM pointer on ESP8266; never set on other platforms
  } type_;
  union {
    std::string *value_;                   // Heap-allocated string (VALUE)
    std::function<std::string(X...)> *f_;  // Heap-allocated std::function (LAMBDA)
    std::string (*stateless_f_)(X...);     // Function pointer (STATELESS_LAMBDA)
    const char *static_str_;               // For STATIC_STRING and FLASH_STRING types
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
  // Force-inline: collapses the Trigger→Automation→ActionList forwarding
  // chain into a single frame, reducing automation call stack depth.
  inline void trigger(const Ts &...x) ESPHOME_ALWAYS_INLINE {
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
    // Walk to end of chain - action lists are short and only built during setup()
    Action<Ts...> **tail = &this->actions_;
    while (*tail != nullptr)
      tail = &(*tail)->next_;
    *tail = action;
  }
  void add_actions(const std::initializer_list<Action<Ts...> *> &actions) {
    // Find tail once, then append all actions in a single pass
    Action<Ts...> **tail = &this->actions_;
    while (*tail != nullptr)
      tail = &(*tail)->next_;
    for (auto *action : actions) {
      *tail = action;
      tail = &action->next_;
    }
  }
  // Force-inline: part of the Trigger→Automation→ActionList forwarding
  // chain collapsed to reduce automation call stack depth.
  inline void play(const Ts &...x) ESPHOME_ALWAYS_INLINE {
    if (this->actions_ != nullptr)
      this->actions_->play_complex(x...);
  }
  void play_tuple(const std::tuple<Ts...> &tuple) {
    this->play_tuple_(tuple, std::make_index_sequence<sizeof...(Ts)>{});
  }
  void stop() {
    if (this->actions_ != nullptr)
      this->actions_->stop_complex();
  }
  bool empty() const { return this->actions_ == nullptr; }

  /// Check if any action in this action list is currently running.
  bool is_running() {
    if (this->actions_ == nullptr)
      return false;
    return this->actions_->is_running();
  }
  /// Return the number of actions in this action list that are currently running.
  int num_running() {
    if (this->actions_ == nullptr)
      return 0;
    return this->actions_->num_running_total();
  }

 protected:
  template<size_t... S> void play_tuple_(const std::tuple<Ts...> &tuple, std::index_sequence<S...> /*unused*/) {
    this->play(std::get<S>(tuple)...);
  }

  Action<Ts...> *actions_{nullptr};
};

template<typename... Ts> class Automation {
 public:
  /// Default constructor for use with TriggerForwarder (no Trigger object needed).
  Automation() = default;
  explicit Automation(Trigger<Ts...> *trigger) { trigger->set_automation_parent(this); }

  void add_action(Action<Ts...> *action) { this->actions_.add_action(action); }
  void add_actions(const std::initializer_list<Action<Ts...> *> &actions) { this->actions_.add_actions(actions); }

  void stop() { this->actions_.stop(); }

  // Force-inline: part of the Trigger→Automation→ActionList forwarding
  // chain collapsed to reduce automation call stack depth.
  inline void trigger(const Ts &...x) ESPHOME_ALWAYS_INLINE { this->actions_.play(x...); }

  bool is_running() { return this->actions_.is_running(); }

  /// Return the number of actions in the action part of this automation that are currently running.
  int num_running() { return this->actions_.num_running(); }

 protected:
  ActionList<Ts...> actions_;
};

/// Callback forwarder that triggers an Automation directly.
/// One operator() instantiation per Automation<Ts...> signature, shared across all call sites.
/// Must stay pointer-sized to fit inline in Callback::ctx_ without heap allocation.
template<typename... Ts> struct TriggerForwarder {
  Automation<Ts...> *automation;
  void operator()(const Ts &...args) const { this->automation->trigger(args...); }
};

/// Callback forwarder that triggers an Automation<> only when the bool arg is true.
/// Must stay pointer-sized to fit inline in Callback::ctx_ without heap allocation.
struct TriggerOnTrueForwarder {
  Automation<> *automation;
  void operator()(bool state) const {
    if (state)
      this->automation->trigger();
  }
};

/// Callback forwarder that triggers an Automation<> only when the bool arg is false.
/// Must stay pointer-sized to fit inline in Callback::ctx_ without heap allocation.
struct TriggerOnFalseForwarder {
  Automation<> *automation;
  void operator()(bool state) const {
    if (!state)
      this->automation->trigger();
  }
};

// Ensure forwarders fit in Callback::ctx_ (pointer-sized inline storage).
// If these fail, the forwarder would heap-allocate in Callback::create().
static_assert(sizeof(TriggerForwarder<>) <= sizeof(void *));
static_assert(sizeof(TriggerOnTrueForwarder) <= sizeof(void *));
static_assert(sizeof(TriggerOnFalseForwarder) <= sizeof(void *));
static_assert(std::is_trivially_copyable_v<TriggerForwarder<>>);
static_assert(std::is_trivially_copyable_v<TriggerOnTrueForwarder>);
static_assert(std::is_trivially_copyable_v<TriggerOnFalseForwarder>);

}  // namespace esphome
