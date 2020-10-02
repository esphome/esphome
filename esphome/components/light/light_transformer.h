#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "light_color_values.h"

namespace esphome {
namespace light {

/// Base-class for all light color transformers, such as transitions or flashes.
class LightTransformer {
 public:
  LightTransformer(uint32_t start_time, uint32_t length, const LightColorValues &start_values,
                   const LightColorValues &target_values)
      : start_time_(start_time), length_(length), start_values_(start_values), target_values_(target_values) {}

  LightTransformer() = delete;

  /// Whether this transformation is finished
  virtual bool is_finished() { return this->get_progress() >= 1.0f; }

  /// This will be called to get the current values for output.
  virtual LightColorValues get_values() = 0;

  /// The values that should be reported to the front-end.
  virtual LightColorValues get_remote_values() { return this->get_target_values_(); }

  /// The values that should be set after this transformation is complete.
  virtual LightColorValues get_end_values() { return this->get_target_values_(); }

  virtual bool publish_at_end() = 0;
  virtual bool is_transition() = 0;

  float get_progress() { return clamp((millis() - this->start_time_) / float(this->length_), 0.0f, 1.0f); }

 protected:
  const LightColorValues &get_start_values_() const { return this->start_values_; }

  const LightColorValues &get_target_values_() const { return this->target_values_; }

  uint32_t start_time_;
  uint32_t length_;
  LightColorValues start_values_;
  LightColorValues target_values_;
};

class LightTransitionTransformer : public LightTransformer {
 public:
  LightTransitionTransformer(uint32_t start_time, uint32_t length, const LightColorValues &start_values,
                             const LightColorValues &target_values)
      : LightTransformer(start_time, length, start_values, target_values) {
    // When turning light on from off state, use colors from new.
    if (!this->start_values_.is_on() && this->target_values_.is_on()) {
      this->start_values_.set_brightness(0.0f);
      this->start_values_.set_red(target_values.get_red());
      this->start_values_.set_green(target_values.get_green());
      this->start_values_.set_blue(target_values.get_blue());
      this->start_values_.set_white(target_values.get_white());
      this->start_values_.set_color_temperature(target_values.get_color_temperature());
    }
  }

  LightColorValues get_values() override {
    float v = LightTransitionTransformer::smoothed_progress(this->get_progress());
    return LightColorValues::lerp(this->get_start_values_(), this->get_target_values_(), v);
  }

  bool publish_at_end() override { return false; }
  bool is_transition() override { return true; }

  static float smoothed_progress(float x) { return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f); }
};

class LightFlashTransformer : public LightTransformer {
 public:
  LightFlashTransformer(uint32_t start_time, uint32_t length, const LightColorValues &start_values,
                        const LightColorValues &target_values)
      : LightTransformer(start_time, length, start_values, target_values) {}

  LightColorValues get_values() override { return this->get_target_values_(); }

  LightColorValues get_end_values() override { return this->get_start_values_(); }

  bool publish_at_end() override { return true; }
  bool is_transition() override { return false; }
};

}  // namespace light
}  // namespace esphome
