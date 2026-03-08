#pragma once

#include "esphome/core/component.h"
#include "esphome/components/fan/fan.h"

namespace esphome::template_ {

class TemplateFan final : public Component, public fan::Fan {
 public:
  TemplateFan() {}
  void setup() override;
  void dump_config() override;
  void set_has_direction(bool has_direction) { this->has_direction_ = has_direction; }
  void set_has_oscillating(bool has_oscillating) { this->has_oscillating_ = has_oscillating; }
  void set_speed_count(int count) { this->speed_count_ = count; }
  void set_preset_modes(std::initializer_list<const char *> presets) { this->preset_modes_ = presets; }
  fan::FanTraits get_traits() override { return this->traits_; }

 protected:
  void control(const fan::FanCall &call) override;

  bool has_oscillating_{false};
  bool has_direction_{false};
  int speed_count_{0};
  fan::FanTraits traits_;
  std::vector<const char *> preset_modes_{};
};

}  // namespace esphome::template_
