#pragma once

#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR_FILTER

#include <array>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::binary_sensor {

class BinarySensor;

class Filter {
 public:
  virtual optional<bool> new_value(bool value) = 0;

  virtual void input(bool value);

  void output(bool value);

 protected:
  friend BinarySensor;

  Filter *next_{nullptr};
  BinarySensor *parent_{nullptr};
  Deduplicator<bool> dedup_;
};

class TimeoutFilter : public Filter {
 public:
  optional<bool> new_value(bool value) override { return value; }
  void input(bool value) override;
  template<typename T> void set_timeout_value(T timeout) { this->timeout_delay_ = timeout; }

 protected:
  TemplatableFn<uint32_t> timeout_delay_{};
};

class DelayedOnOffFilter final : public Filter {
 public:
  optional<bool> new_value(bool value) override;

  template<typename T> void set_on_delay(T delay) { this->on_delay_ = delay; }
  template<typename T> void set_off_delay(T delay) { this->off_delay_ = delay; }

 protected:
  TemplatableFn<uint32_t> on_delay_{};
  TemplatableFn<uint32_t> off_delay_{};
};

class DelayedOnFilter : public Filter {
 public:
  // User provided, not "= default": `new(p) DelayedOnFilter()` would zero-fill .bss that is already zero.
  DelayedOnFilter() {}

  optional<bool> new_value(bool value) override;

  template<typename T> void set_delay(T delay) { this->delay_ = delay; }

 protected:
  TemplatableFn<uint32_t> delay_{};
};

class DelayedOffFilter : public Filter {
 public:
  // User provided, not "= default": `new(p) DelayedOffFilter()` would zero-fill .bss that is already zero.
  DelayedOffFilter() {}

  optional<bool> new_value(bool value) override;

  template<typename T> void set_delay(T delay) { this->delay_ = delay; }

 protected:
  TemplatableFn<uint32_t> delay_{};
};

class InvertFilter : public Filter {
 public:
  optional<bool> new_value(bool value) override { return !value; }
};

// Inverts only while the condition is true. T is `TemplatableFn<bool>` (a lambda, stored as a
// function pointer) or `Condition<> *` (an automation condition, e.g. `switch.is_on: my_switch`).
// The condition may depend on components that are not ready during setup, so it is checked again
// once in the first loop, and the output is republished if the result changed.
template<typename T> class ConditionalInvertFilter : public Filter {
 public:
  explicit ConditionalInvertFilter(T condition) : condition_(condition) {}

  optional<bool> new_value(bool value) override;

 protected:
  bool should_invert_();
  void recheck_();

  T condition_;
  bool last_input_{false};
  bool last_invert_{false};
  bool rechecked_{false};
};

struct AutorepeatFilterTiming {
  uint32_t delay;
  uint32_t time_off;
  uint32_t time_on;
};

/// Non-template base for AutorepeatFilter — all methods in filter.cpp.
/// Lambdas capture this base pointer, so set_timeout/cancel_timeout are instantiated once.
/// The two scheduled timers are keyed off `this` and `&active_timing_`; since the address
/// of `active_timing_` is taken as a scheduler key, the class must not be copied or moved.
class AutorepeatFilterBase : public Filter {
 public:
  optional<bool> new_value(bool value) override;
  AutorepeatFilterBase(const AutorepeatFilterBase &) = delete;
  AutorepeatFilterBase &operator=(const AutorepeatFilterBase &) = delete;

 protected:
  AutorepeatFilterBase() = default;
  void next_timing_();
  void next_value_(bool val);

  const AutorepeatFilterTiming *timings_{nullptr};
  uint8_t timings_count_{0};
  uint8_t active_timing_{0};
};

/// Template wrapper that provides inline std::array storage for timings.
/// N is set by code generation to match the exact number of timings configured in YAML.
template<size_t N> class AutorepeatFilter : public AutorepeatFilterBase {
 public:
  explicit AutorepeatFilter(std::initializer_list<AutorepeatFilterTiming> timings) {
    init_array_from(this->timings_storage_, timings);
    this->timings_ = this->timings_storage_.data();
    this->timings_count_ = N;
  }

 protected:
  std::array<AutorepeatFilterTiming, N> timings_storage_{};
};

class LambdaFilter : public Filter {
 public:
  explicit LambdaFilter(std::function<optional<bool>(bool)> f);

  optional<bool> new_value(bool value) override;

 protected:
  std::function<optional<bool>(bool)> f_;
};

/** Optimized lambda filter for stateless lambdas (no capture).
 *
 * Uses function pointer instead of std::function to reduce memory overhead.
 * Memory: 4 bytes (function pointer on 32-bit) vs 32 bytes (std::function).
 */
class StatelessLambdaFilter : public Filter {
 public:
  explicit StatelessLambdaFilter(optional<bool> (*f)(bool)) : f_(f) {}

  optional<bool> new_value(bool value) override { return this->f_(value); }

 protected:
  optional<bool> (*f_)(bool);
};

class SettleFilter : public Filter {
 public:
  // User provided, not "= default": `new(p) SettleFilter()` would zero-fill .bss that is already zero.
  SettleFilter() {}
  optional<bool> new_value(bool value) override;

  template<typename T> void set_delay(T delay) { this->delay_ = delay; }

 protected:
  TemplatableFn<uint32_t> delay_{};
  bool steady_{true};
};

}  // namespace esphome::binary_sensor

#endif  // USE_BINARY_SENSOR_FILTER
