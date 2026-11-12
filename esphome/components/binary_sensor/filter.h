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

  /// Forget the last value and drop any pending scheduled output; called when the sensor's state is invalidated.
  virtual void reset();

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
  void reset() override;
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

/// Codegen use only: output `value` from `filter` after `delay` ms, replacing its pending output, and return empty
/// to stop the chain for now.
optional<bool> delay_output(Filter *filter, uint32_t delay, bool value);

/// Codegen use only: the filter's work and constants are one generated function, so the object is only the base.
/// The function gets the filter, so it can schedule output on it.
template<auto Fn> class ApplyFilter final : public Filter {
 public:
  optional<bool> new_value(bool value) override { return Fn(this, value); }
};

class SettleFilter : public Filter {
 public:
  // User provided, not "= default": `new(p) SettleFilter()` would zero-fill .bss that is already zero.
  SettleFilter() {}
  optional<bool> new_value(bool value) override;

  template<typename T> void set_delay(T delay) { this->delay_ = delay; }

 protected:
  void reset() override;

  bool steady_{true};  // Before delay_ so it fills the Filter base's tail padding
  TemplatableFn<uint32_t> delay_{};
};

}  // namespace esphome::binary_sensor

#endif  // USE_BINARY_SENSOR_FILTER
