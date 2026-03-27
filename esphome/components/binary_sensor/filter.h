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

class TimeoutFilter : public Filter, public Component {
 public:
  optional<bool> new_value(bool value) override { return value; }
  void input(bool value) override;
  template<typename T> void set_timeout_value(T timeout) { this->timeout_delay_ = timeout; }

 protected:
  TemplatableValue<uint32_t> timeout_delay_{};
};

class DelayedOnOffFilter final : public Filter, public Component {
 public:
  optional<bool> new_value(bool value) override;

  float get_setup_priority() const override;

  template<typename T> void set_on_delay(T delay) { this->on_delay_ = delay; }
  template<typename T> void set_off_delay(T delay) { this->off_delay_ = delay; }

 protected:
  TemplatableValue<uint32_t> on_delay_{};
  TemplatableValue<uint32_t> off_delay_{};
};

class DelayedOnFilter : public Filter, public Component {
 public:
  optional<bool> new_value(bool value) override;

  float get_setup_priority() const override;

  template<typename T> void set_delay(T delay) { this->delay_ = delay; }

 protected:
  TemplatableValue<uint32_t> delay_{};
};

class DelayedOffFilter : public Filter, public Component {
 public:
  optional<bool> new_value(bool value) override;

  float get_setup_priority() const override;

  template<typename T> void set_delay(T delay) { this->delay_ = delay; }

 protected:
  TemplatableValue<uint32_t> delay_{};
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

template<size_t N> class AutorepeatFilter : public Filter, public Component {
  static constexpr uint32_t TIMING_ID = 0;
  static constexpr uint32_t ON_OFF_ID = 1;

 public:
  explicit AutorepeatFilter(std::initializer_list<AutorepeatFilterTiming> timings) {
    size_t i = 0;
    for (const auto &t : timings) {
      if (i >= N)
        break;
      this->timings_[i++] = t;
    }
  }

  optional<bool> new_value(bool value) override {
    if (value) {
      if (this->active_timing_ != 0)
        return {};
      this->next_timing_();
      return true;
    } else {
      this->cancel_timeout(TIMING_ID);
      this->cancel_timeout(ON_OFF_ID);
      this->active_timing_ = 0;
      return false;
    }
  }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void next_timing_() {
    if (this->active_timing_ < N) {
      this->set_timeout(TIMING_ID, this->timings_[this->active_timing_].delay, [this]() { this->next_timing_(); });
    }
    if (this->active_timing_ <= N) {
      this->active_timing_++;
    }
    if (this->active_timing_ == 2)
      this->next_value_(false);
  }

  void next_value_(bool val) {
    const AutorepeatFilterTiming &timing = this->timings_[this->active_timing_ - 2];
    this->output(val);
    this->set_timeout(ON_OFF_ID, val ? timing.time_on : timing.time_off, [this, val]() { this->next_value_(!val); });
  }

  std::array<AutorepeatFilterTiming, N> timings_{};
  uint8_t active_timing_{0};
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

class SettleFilter : public Filter, public Component {
 public:
  optional<bool> new_value(bool value) override;

  float get_setup_priority() const override;

  template<typename T> void set_delay(T delay) { this->delay_ = delay; }

 protected:
  TemplatableValue<uint32_t> delay_{};
  bool steady_{true};
};

}  // namespace esphome::binary_sensor

#endif  // USE_BINARY_SENSOR_FILTER
