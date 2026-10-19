#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "light_color_values.h"
#include "light_state.h"
#include "light_transformer.h"

namespace esphome::light {

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
    this->changing_color_mode_ = this->start_values_.get_color_mode() != this->end_values_.get_color_mode();
    if (this->changing_color_mode_) {
      this->intermediate_values_ = this->start_values_;
      this->intermediate_values_.set_state(false);
    }
  }

  optional<LightColorValues> apply() override {
    float p = this->get_progress_();

    // Halfway through, when intermediate state (off) is reached, flip it to the target, but remain off.
    if (this->changing_color_mode_ && p > 0.5f &&
        this->intermediate_values_.get_color_mode() != this->end_values_.get_color_mode()) {
      this->intermediate_values_ = this->end_values_;
      this->intermediate_values_.set_state(false);
    }

    LightColorValues &start = this->changing_color_mode_ && p > 0.5f ? this->intermediate_values_ : this->start_values_;
    LightColorValues &end = this->changing_color_mode_ && p < 0.5f ? this->intermediate_values_ : this->end_values_;
    if (this->changing_color_mode_)
      p = p < 0.5f ? p * 2 : (p - 0.5f) * 2;

    float v = LightTransformer::smoothed_progress(p);
    return LightColorValues::lerp(start, end, v);
  }

 protected:
  LightColorValues end_values_{};
  LightColorValues intermediate_values_{};
  bool changing_color_mode_{false};
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
    this->transition_running_ = true;
  }

  optional<LightColorValues> apply() override {
    if (!this->transition_running_ && !this->begun_lightstate_restore_ &&
        millis() - this->start_time_ > this->length_ - this->transition_length_) {
      // second transition back to start value, run by the same transformer as the first
      this->transformer_->setup(this->state_.current_values, this->get_start_values(), this->transition_length_);
      this->begun_lightstate_restore_ = true;
      this->transition_running_ = true;
    }

    if (!this->transition_running_)
      return {};

    optional<LightColorValues> result = this->transformer_->apply();
    if (this->transformer_->is_finished()) {
      this->transformer_->stop();
      this->transition_running_ = false;
    }
    return result;
  }

  // Restore the original values after the flash.
  void stop() override {
    if (this->transition_running_)
      this->transformer_->stop();
    this->state_.current_values = this->get_start_values();
    this->state_.remote_values = this->get_start_values();
#ifdef USE_LIGHT_TRANSITION_PUBLISH_INTERVAL
    // The light reports the end state itself once the transformer finishes
    if (this->state_.transition_publish_enabled_)
      return;
#endif
    this->state_.publish_state();
  }

  bool is_finished() override { return this->begun_lightstate_restore_ && LightTransformer::is_finished(); }

 protected:
  LightState &state_;
  std::unique_ptr<LightTransformer> transformer_{nullptr};
  uint32_t transition_length_;
  bool begun_lightstate_restore_;
  bool transition_running_;
};

}  // namespace esphome::light
