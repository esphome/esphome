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

template<size_t DATA_SIZE> class LvAnimation : public Parented<LvglComponent>, public LvglLooper {
 public:
  LvAnimation(
      std::function<void(uint32_t data[DATA_SIZE])> update_callback,
      TemplatableValue<float> from[DATA_SIZE],
      TemplatableValue<float> to[DATA_SIZE])
      : update_callback_(std::move(update_callback)), from_(from), to_(to) {}

  void start() {
    if (this->duration_ == 0) {
      this->state_ = AnimationState::STOPPED;
      return;
    }
    // evaluate any lambdas
    for (size_t i = 0; i != DATA_SIZE; i++) {
      this->data_from_[i] = this->from_[i].value();
      this->data_to_[i] = this->to_[i].value();
    }
    this->start_time_ = esphome::millis();
    if (this->state_ == AnimationState::STOPPED)
      this->parent_->add_looper(&this);
    this->state_ = AnimationState::STARTED;
    this->update();
  }

  void stop() { this->state_ = AnimationState::STOPPED; }

  void update() override {
    uint32_t elapsed = esphome::millis() - this->start_time_;
    switch (this->state_) {
      case AnimationState::STARTED:
        if (elapsed < this->start_delay_)
          return;
        this->state_ = AnimationState::RUNNING;
        break;
      case AnimationState::RUNNING:
        if (elapsed >= this->duration_)
          this->state_ = AnimationState::STOPPED;
        break;
      case AnimationState::STOPPED:
        this->parent_->remove_looper(&this);
        return;
    }

    float progress = static_cast<float>(elapsed) / static_cast<float>(this->duration_);
    if (progress > 1.0f)
      progress = 1.0f;
    uint32_t data[DATA_SIZE];
    for (size_t i = 0; i < DATA_SIZE; i++)
      data[i] = static_cast<uint32_t>(roundf(this->data_from_[i] + (this->data_to_[i] - this->data_from_[i]) * progress));
    this->update_callback_(data);
  }

  void set_duration(uint32_t duration) { this->duration_ = duration; }
  void set_start_delay(uint32_t start_delay) { this->start_delay_ = start_delay; }

 protected:
  std::function<void(uint32_t data[DATA_SIZE])> update_callback_;
  TemplatableValue<float> from_[DATA_SIZE]{};
  TemplatableValue<float> to_[DATA_SIZE]{};
  uint32_t duration_{0};
  uint32_t start_delay_{0};
  uint32_t start_time_{0};
  float data_from_[DATA_SIZE]{0};
  float data_to_[DATA_SIZE]{0};
  AnimationState state_{AnimationState::STOPPED};
};

} // namespace esphome::lvgl

#endif
