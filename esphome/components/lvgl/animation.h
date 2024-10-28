#pragma once
#include "esphome/core/defines.h"

#ifdef USE_LVGL_ANIMATION
#include "lvgl_esphome.h"
#include "esphome/core/hal.h"
#include <functional>

namespace esphome::lvgl {

static const char *const TAG = "lvgl.animation";

enum class AnimationState {
  STOPPED,
  STARTED,
  RUNNING,
};

class LvAnimationTiming {
 public:
  /**
   * Map progress in the range [0, 1]
   */
  virtual float map_progress(float value) = 0;
};

class LvAnimationTimingRoundTrip : public LvAnimationTiming {
 public:
  float map_progress(float value) override {
    value *= 2.0f;
    if (value > 1.0f)
      return 2.0f - value;
    return value;
  }
};

class LvAnimationTimingGravity : public LvAnimationTiming {
 public:
  LvAnimationTimingGravity(float acceleration, float bounce) : acceleration_(acceleration), bounce_(bounce) {}
  float map_progress(float value) override {
    if (value == 0.0f) {
      this->initial_position_ = 0.0f;
      this->initial_speed_ = 0.0f;
      this->initial_time_ = 0.0f;
    }
    auto position = this->calc_pos_(value);
    if (position > 1.0f) {
      auto initial_time = this->calc_end_time_();
      this->initial_speed_ = -this->calc_speed_(initial_time) * this->bounce_;
      this->initial_position_ = 1.0f;
      this->initial_time_ = initial_time;
      position = calc_pos_(value);
      if (position > 1.0f) {
        position = 1.0f;
      }
    }
    return position;
  }

 protected:
  float calc_pos_(float value) {
    value -= this->initial_time_;
    return (0.5 * value * this->acceleration_ + this->initial_speed_) * value + this->initial_position_;
  }

  float calc_speed_(float value) {
    value -= this->initial_time_;
    return this->acceleration_ * value + this->initial_speed_;
  }

  float calc_end_time_() {
    return (-this->initial_speed_ + std::sqrt(this->initial_speed_ * this->initial_speed_ -
                                              4.0f * this->acceleration_ / 2.0 * (this->initial_position_ - 1.0f))) /
               this->acceleration_ +
           this->initial_time_;
  }

  float acceleration_;
  float bounce_;
  float initial_position_{0.0f};
  float initial_time_{0.0f};
  float initial_speed_{0.0f};
};

class LvAnimationTimingEaseInOut : public LvAnimationTiming {
 public:
  LvAnimationTimingEaseInOut(float slope) : slope_(slope) {}
  float map_progress(float value) override {
    float sqr = value * value;
    sqr = sqr / (2.0f * (sqr - value) + 1.0f);
    return this->slope_ * sqr + (1.0 - this->slope_) * value;
  }

 protected:
  float slope_;
};

template<size_t DATA_SIZE> class LvAnimation : public Component {
 public:
  LvAnimation(std::function<void(const uint32_t *data)> update_callback, std::vector<TemplatableValue<uint32_t>> from,
              std::vector<TemplatableValue<uint32_t>> to)
      : update_callback_(std::move(update_callback)) {
    std::copy(from.begin(), from.end(), this->from_);
    std::copy(to.begin(), to.end(), this->to_);
  }

  void start() {
    if (this->state_ >= AnimationState::STOPPED)
      this->stop();
    if (this->duration_ == 0)
      return;
    // evaluate any lambdas
    for (size_t i = 0; i != DATA_SIZE; i++) {
      this->data_from_[i] = this->from_[i].value();
      this->data_to_[i] = this->to_[i].value();
    }
    this->start_time_ = millis();
    this->state_ = AnimationState::STARTED;
    this->loop();
  }

  void stop() { this->state_ = AnimationState::STOPPED; }

  void loop() override {
    if (this->state_ == AnimationState::STOPPED)
      return;
    uint32_t elapsed = millis() - this->start_time_;
    float progress = static_cast<float>(elapsed) / static_cast<float>(this->duration_);
    switch (this->state_) {
      case AnimationState::STARTED:
        if (elapsed < this->start_delay_)
          return;
        this->state_ = AnimationState::RUNNING;
        this->start_time_ = millis();
        progress = 0.0f;
        break;
      case AnimationState::RUNNING:
        if (progress >= 1.0f) {
          progress = 1.0f;
          this->stop();
        }
        break;
      default:
        return;
    }

    for (auto timing : this->timings_) {
      progress = timing->map_progress(progress);
    }
    uint32_t data[DATA_SIZE];
    for (size_t i = 0; i < DATA_SIZE; i++) {
      data[i] =
          (uint32_t) (roundf(this->data_from_[i] + (int32_t) (this->data_to_[i] - this->data_from_[i]) * progress));
    }
    this->update_callback_(data);
  }

  void set_duration(uint32_t duration) { this->duration_ = duration; }
  void set_start_delay(uint32_t start_delay) { this->start_delay_ = start_delay; }
  void add_timing(LvAnimationTiming *timing) { this->timings_.push_back(timing); }

 protected:
  std::function<void(const uint32_t *)> update_callback_;
  TemplatableValue<uint32_t> from_[DATA_SIZE]{};
  TemplatableValue<uint32_t> to_[DATA_SIZE]{};
  uint32_t duration_{0};
  uint32_t start_delay_{0};
  uint32_t start_time_{0};
  uint32_t data_from_[DATA_SIZE]{0};
  uint32_t data_to_[DATA_SIZE]{0};
  AnimationState state_{AnimationState::STOPPED};
  std::vector<LvAnimationTiming *> timings_{};
};

}  // namespace esphome::lvgl

#endif  // USE_LVGL_ANIMATION
