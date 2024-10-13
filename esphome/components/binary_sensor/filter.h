#pragma once

#include "esphome/core/automation.h"
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
  optional<bool> new_value(bool value, bool is_initial) override;

  float get_setup_priority() const override;

  template<typename T> void set_on_delay(T delay) { this->on_delay_ = delay; }
  template<typename T> void set_off_delay(T delay) { this->off_delay_ = delay; }

 protected:
  TemplatableValue<uint32_t> on_delay_{};
  TemplatableValue<uint32_t> off_delay_{};
};

class DelayedOnFilter : public Filter, public Component {
 public:
  optional<bool> new_value(bool value, bool is_initial) override;

  float get_setup_priority() const override;

  template<typename T> void set_delay(T delay) { this->delay_ = delay; }

 protected:
  TemplatableValue<uint32_t> delay_{};
};

class DelayedOffFilter : public Filter, public Component {
 public:
  optional<bool> new_value(bool value, bool is_initial) override;

  float get_setup_priority() const override;

  template<typename T> void set_delay(T delay) { this->delay_ = delay; }

 protected:
  TemplatableValue<uint32_t> delay_{};
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

class SettleFilter : public Filter, public Component {
 public:
  optional<bool> new_value(bool value, bool is_initial) override;

  float get_setup_priority() const override;

  template<typename T> void set_delay(T delay) { this->delay_ = delay; }

 protected:
  TemplatableValue<uint32_t> delay_{};
  bool steady_{true};
};

class GlitchFilter : public Filter, public Component {
 public:
  optional<bool> new_value(bool value, bool is_initial) override;

  float get_setup_priority() const override;

  template<typename T> void set_duration(T duration) { this->timeout_ = duration; }
  template<typename T> void set_count(T count) { this->count_ = count; }

 protected:
  TemplatableValue<uint32_t> timeout_{};
  TemplatableValue<uint32_t> count_{};
  uint32_t repeats_{0};
};

}  // namespace binary_sensor

}  // namespace esphome
