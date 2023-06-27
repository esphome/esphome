#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <vector>

namespace esphome {

namespace binary_sensor {

class BinarySensor;

class Filter {
 public:
  virtual optional<bool> new_value(bool value, bool is_initial) = 0;

  void input(bool value, bool is_initial);

  void output(bool value, bool is_initial);

 protected:
  friend BinarySensor;

  Filter *next_{nullptr};
  BinarySensor *parent_{nullptr};
  Deduplicator<bool> dedup_;
};

class DelayedOnOffFilter : public Filter, public Component {
 public:
  explicit DelayedOnOffFilter(uint32_t delay);

  optional<bool> new_value(bool value, bool is_initial) override;

  float get_setup_priority() const override;

 protected:
  uint32_t delay_;
};

class DelayedOnFilter : public Filter, public Component {
 public:
  explicit DelayedOnFilter(uint32_t delay);

  optional<bool> new_value(bool value, bool is_initial) override;

  float get_setup_priority() const override;

 protected:
  uint32_t delay_;
};

class DelayedOffFilter : public Filter, public Component {
 public:
  explicit DelayedOffFilter(uint32_t delay);

  optional<bool> new_value(bool value, bool is_initial) override;

  float get_setup_priority() const override;

 protected:
  uint32_t delay_;
};

class InvertFilter : public Filter {
 public:
  optional<bool> new_value(bool value, bool is_initial) override;
};

struct AutorepeatFilterTiming {
  AutorepeatFilterTiming(uint32_t delay, uint32_t off, uint32_t on) {
    this->delay = delay;
    this->time_off = off;
    this->time_on = on;
  }
  uint32_t delay;
  uint32_t time_off;
  uint32_t time_on;
};

class AutorepeatFilter : public Filter, public Component {
 public:
  explicit AutorepeatFilter(std::vector<AutorepeatFilterTiming> timings);

  optional<bool> new_value(bool value, bool is_initial) override;

  float get_setup_priority() const override;

 protected:
  void next_timing_();
  void next_value_(bool val);

  std::vector<AutorepeatFilterTiming> timings_;
  uint8_t active_timing_{0};
};

class LambdaFilter : public Filter {
 public:
  explicit LambdaFilter(std::function<optional<bool>(bool)> f);

  optional<bool> new_value(bool value, bool is_initial) override;

 protected:
  std::function<optional<bool>(bool)> f_;
};

class UniqueFilter : public Filter {
 public:
  optional<bool> new_value(bool value, bool is_initial) override;

 protected:
  optional<bool> last_value_{};
};

}  // namespace binary_sensor

}  // namespace esphome
