#include "animation.h"

#include "esphome/core/hal.h"

namespace esphome {
namespace animation {

Animation::Animation(const uint8_t *data_start, int width, int height, uint32_t animation_frame_count,
                     image::ImageType type, image::Transparency transparent, const uint32_t *frame_durations,
                     uint32_t default_duration_ms)
    : Image(data_start, width, height, type, transparent),
      animation_data_start_(data_start),
      current_frame_(0),
      animation_frame_count_(animation_frame_count),
      loop_start_frame_(0),
      loop_end_frame_(animation_frame_count_),
      loop_count_(0),
      loop_current_iteration_(1),
      frame_durations_(frame_durations),
      default_frame_duration_(default_duration_ms) {}
void Animation::set_loop(uint32_t start_frame, uint32_t end_frame, int count) {
  loop_start_frame_ = std::min(start_frame, animation_frame_count_);
  loop_end_frame_ = std::min(end_frame, animation_frame_count_);
  loop_count_ = count;
  loop_current_iteration_ = 1;
}

uint32_t Animation::get_animation_frame_count() const { return this->animation_frame_count_; }
int Animation::get_current_frame() const { return this->current_frame_; }
void Animation::next_frame() {
  this->current_frame_++;
  if (loop_count_ && static_cast<uint32_t>(this->current_frame_) == loop_end_frame_ &&
      (this->loop_current_iteration_ < loop_count_ || loop_count_ < 0)) {
    this->current_frame_ = loop_start_frame_;
    this->loop_current_iteration_++;
  }
  if (static_cast<uint32_t>(this->current_frame_) >= animation_frame_count_) {
    this->loop_current_iteration_ = 1;
    this->current_frame_ = 0;
  }

  this->update_data_start_();
}
void Animation::prev_frame() {
  this->current_frame_--;
  if (this->current_frame_ < 0) {
    this->current_frame_ = this->animation_frame_count_ - 1;
  }

  this->update_data_start_();
}

void Animation::set_frame(int frame) {
  unsigned abs_frame = abs(frame);

  if (abs_frame < this->animation_frame_count_) {
    if (frame >= 0) {
      this->current_frame_ = frame;
    } else {
      this->current_frame_ = this->animation_frame_count_ - abs_frame;
    }
  }

  this->update_data_start_();
}

void Animation::update_data_start_() {
  const uint32_t image_size = this->get_width_stride() * this->height_;
  this->data_start_ = this->animation_data_start_ + image_size * this->current_frame_;
}

uint32_t Animation::get_frame_duration(int frame) const {
  if (this->frame_durations_ != nullptr && frame >= 0 && static_cast<uint32_t>(frame) < this->animation_frame_count_) {
    return this->frame_durations_[frame];
  }
  return this->default_frame_duration_;
}

uint32_t Animation::get_average_duration() const {
  if (this->frame_durations_ == nullptr) {
    return this->default_frame_duration_;
  }
  if (this->animation_frame_count_ == 0) {
    return this->default_frame_duration_;
  }

  uint64_t sum = 0;
  for (uint32_t i = 0; i < this->animation_frame_count_; i++) {
    sum += this->frame_durations_[i];
  }
  return static_cast<uint32_t>(sum / this->animation_frame_count_);
}

uint32_t Animation::get_total_duration() const {
  if (this->frame_durations_ == nullptr) {
    return this->animation_frame_count_ * this->default_frame_duration_;
  }

  uint32_t total = 0;
  for (uint32_t i = 0; i < this->animation_frame_count_; i++) {
    total += this->frame_durations_[i];
  }
  return total;
}

bool Animation::has_frame_timing() const { return this->frame_durations_ != nullptr; }

const uint8_t *Animation::get_animation_data_start() const { return this->animation_data_start_; }

}  // namespace animation
}  // namespace esphome
