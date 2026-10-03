#pragma once

#include "esphome/core/defines.h"

#include <type_traits>
#ifdef USE_BINARY_SENSOR_FILTER

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
  optional<bool> new_value(bool value) override;
};

struct AutorepeatFilterTiming {
  uint32_t delay;
  uint32_t time_off;
  uint32_t time_on;
};
// Read straight from flash on ESP8266, so every field must stay a word.
static_assert(std::is_same_v<decltype(AutorepeatFilterTiming::delay), uint32_t>,
              "AutorepeatFilterTiming fields must stay uint32_t");
static_assert(std::is_same_v<decltype(AutorepeatFilterTiming::time_off), uint32_t>,
              "AutorepeatFilterTiming fields must stay uint32_t");
static_assert(std::is_same_v<decltype(AutorepeatFilterTiming::time_on), uint32_t>,
              "AutorepeatFilterTiming fields must stay uint32_t");
static_assert(sizeof(AutorepeatFilterTiming) == 3 * sizeof(uint32_t),
              "AutorepeatFilterTiming is read from flash with word loads");

/// Timings live in a PROGMEM table emitted by codegen, ended by an entry whose delay is
/// SCHEDULER_DONT_RUN (a step that could never advance anyway). Aligned loads are ESP8266 safe.
/// The two scheduled timers are keyed off `this` and `&active_timing_`; since the address
/// of `active_timing_` is taken as a scheduler key, the class must not be copied or moved.
class AutorepeatFilter : public Filter {
 public:
  explicit AutorepeatFilter(const AutorepeatFilterTiming *timings) : timings_(timings) {}
  AutorepeatFilter(const AutorepeatFilter &) = delete;
  AutorepeatFilter &operator=(const AutorepeatFilter &) = delete;

  optional<bool> new_value(bool value) override;

 protected:
  void next_timing_();
  void next_value_(bool val);

  uint8_t active_timing_{0};  // before timings_ so it fills the padding after the Filter base
  const AutorepeatFilterTiming *timings_;
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
