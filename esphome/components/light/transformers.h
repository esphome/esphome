#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "light_color_values.h"
#include "light_state.h"
#include "light_transformer.h"

namespace esphome {
namespace light {

class LightTransitionTransformer : public LightTransformer {
 public:
  void start() override {
    // When turning light on from off state, use target state and only increase brightness from zero.
    if (!this->start_values_.is_on() && this->target_values_.is_on()) {
      this->start_values_ = LightColorValues(this->target_values_);
      this->start_values_.set_brightness(0.0f);
    }

    // When turning light off from on state, use source state and only decrease brightness to zero. Use a second
    // variable for transition end state, as overwriting target_values breaks LightState logic.
    if (this->start_values_.is_on() && !this->target_values_.is_on()) {
      this->end_values_ = LightColorValues(this->start_values_);
      this->end_values_.set_brightness(0.0f);
    } else {
      this->end_values_ = LightColorValues(this->target_values_);
    }

    // When changing color mode, go through off state, as color modes are orthogonal and there can't be two active.
    if (this->start_values_.get_color_mode() != this->target_values_.get_color_mode()) {
      this->changing_color_mode_ = true;
      this->intermediate_values_ = this->start_values_;
      this->intermediate_values_.set_state(false);
    }
  }

  optional<LightColorValues> apply() override {
    float p = this->get_progress_();

    // Halfway through, when intermediate state (off) is reached, flip it to the target, but remain off.
    if (this->changing_color_mode_ && p > 0.5f &&
        this->intermediate_values_.get_color_mode() != this->target_values_.get_color_mode()) {
      this->intermediate_values_ = this->target_values_;
      this->intermediate_values_.set_state(false);
    }

    LightColorValues &start = this->changing_color_mode_ && p > 0.5f ? this->intermediate_values_ : this->start_values_;
    LightColorValues &end = this->changing_color_mode_ && p < 0.5f ? this->intermediate_values_ : this->end_values_;
    if (this->changing_color_mode_)
      p = p < 0.5f ? p * 2 : (p - 0.5) * 2;

    float v = LightTransitionTransformer::smoothed_progress(p);
    return LightColorValues::lerp(start, end, v);
  }

 protected:
  // This looks crazy, but it reduces to 6x^5 - 15x^4 + 10x^3 which is just a smooth sigmoid-like
  // transition from 0 to 1 on x = [0, 1]
  static float smoothed_progress(float x) { return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f); }

  bool changing_color_mode_{false};
  LightColorValues end_values_{};
  LightColorValues intermediate_values_{};
};

class LightFlashTransformer : public LightTransformer {
 public:
  LightFlashTransformer(LightState &state) : state_(state) {}

  void start() override {
    this->transition_length_ = this->state_.get_flash_transition_length();
    if (this->transition_length_ * 2 > this->length_)
      this->transition_length_ = this->length_ / 2;

    this->begun_lightstate_restore_ = false;

    // first transition to original target
    this->transformer_ = this->state_.get_output()->create_default_transition();
    this->transformer_->setup(this->state_.current_values, this->target_values_, this->transition_length_);
  }

  optional<LightColorValues> apply() override {
    optional<LightColorValues> result = {};

    if (this->transformer_ == nullptr && millis() > this->start_time_ + this->length_ - this->transition_length_) {
      // second transition back to start value
      this->transformer_ = this->state_.get_output()->create_default_transition();
      this->transformer_->setup(this->state_.current_values, this->get_start_values(), this->transition_length_);
      this->begun_lightstate_restore_ = true;
    }

    if (this->transformer_ != nullptr) {
      result = this->transformer_->apply();

      if (this->transformer_->is_finished()) {
        this->transformer_->stop();
        this->transformer_ = nullptr;
      }
    }

    return result;
  }

  // Restore the original values after the flash.
  void stop() override {
    if (this->transformer_ != nullptr) {
      this->transformer_->stop();
      this->transformer_ = nullptr;
    }
    this->state_.current_values = this->get_start_values();
    this->state_.remote_values = this->get_start_values();
    this->state_.publish_state();
  }

  bool is_finished() override { return this->begun_lightstate_restore_ && LightTransformer::is_finished(); }

 protected:
  LightState &state_;
  uint32_t transition_length_;
  std::unique_ptr<LightTransformer> transformer_{nullptr};
  bool begun_lightstate_restore_;
};

}  // namespace light
}  // namespace esphome
