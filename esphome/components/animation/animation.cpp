#include "animation.h"

#include "esphome/core/hal.h"

namespace esphome {
namespace animation {

Animation::Animation(const uint8_t *data_start, int width, int height, uint32_t animation_frame_count,
                     image::ImageType type, image::Transparency transparent)
    : Image(data_start, width, height, type, transparent),
      animation_data_start_(data_start),
      current_frame_(0),
      animation_frame_count_(animation_frame_count),
      loop_start_frame_(0),
      loop_end_frame_(animation_frame_count_),
      loop_count_(0),
      loop_current_iteration_(1) {}
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
  if (loop_count_ && this->current_frame_ == loop_end_frame_ &&
      (this->loop_current_iteration_ < loop_count_ || loop_count_ < 0)) {
    this->current_frame_ = loop_start_frame_;
    this->loop_current_iteration_++;
  }
  if (this->current_frame_ >= animation_frame_count_) {
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

}  // namespace animation
}  // namespace esphome
