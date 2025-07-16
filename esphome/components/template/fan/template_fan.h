#pragma once

#include <set>

#include "esphome/core/component.h"
#include "esphome/components/fan/fan.h"

namespace esphome {
namespace template_ {

class TemplateFan : public Component, public fan::Fan {
 public:
  TemplateFan() {}
  void setup() override;
  void dump_config() override;
  void set_has_direction(bool has_direction) { this->has_direction_ = has_direction; }
  void set_has_oscillating(bool has_oscillating) { this->has_oscillating_ = has_oscillating; }
  void set_speed_count(int count) { this->speed_count_ = count; }
  void set_preset_modes(const std::set<std::string> &presets) { this->preset_modes_ = presets; }
  fan::FanTraits get_traits() override { return this->traits_; }

 protected:
  void control(const fan::FanCall &call) override;

  bool has_oscillating_{false};
  bool has_direction_{false};
  int speed_count_{0};
  fan::FanTraits traits_;
  std::set<std::string> preset_modes_{};
};

}  // namespace template_
}  // namespace esphome
