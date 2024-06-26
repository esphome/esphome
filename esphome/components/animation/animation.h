#pragma once
#include "esphome/components/image/image.h"

#include "esphome/core/automation.h"

namespace esphome {
namespace animation {

class Animation : public image::Image {
 public:
  Animation(const uint8_t *data_start, int width, int height, uint32_t animation_frame_count, image::ImageType type);

  uint32_t get_animation_frame_count() const;
  int get_current_frame() const;
  void next_frame();
  void prev_frame();

  /** Selects a specific frame within the animation.
   *
   * @param frame If possitive, advance to the frame. If negative, recede to that frame from the end frame.
   */
  void set_frame(int frame);

  void set_loop(uint32_t start_frame, uint32_t end_frame, int count);

 protected:
  void update_data_start_();

  const uint8_t *animation_data_start_;
  int current_frame_;
  uint32_t animation_frame_count_;
  uint32_t loop_start_frame_;
  uint32_t loop_end_frame_;
  int loop_count_;
  int loop_current_iteration_;
};

template<typename... Ts> class AnimationNextFrameAction : public Action<Ts...> {
 public:
  AnimationNextFrameAction(Animation *parent) : parent_(parent) {}
  void play(Ts... x) override { this->parent_->next_frame(); }

 protected:
  Animation *parent_;
};

template<typename... Ts> class AnimationPrevFrameAction : public Action<Ts...> {
 public:
  AnimationPrevFrameAction(Animation *parent) : parent_(parent) {}
  void play(Ts... x) override { this->parent_->prev_frame(); }

 protected:
  Animation *parent_;
};

template<typename... Ts> class AnimationSetFrameAction : public Action<Ts...> {
 public:
  AnimationSetFrameAction(Animation *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(uint16_t, frame)
  void play(Ts... x) override { this->parent_->set_frame(this->frame_.value(x...)); }

 protected:
  Animation *parent_;
};

}  // namespace animation
}  // namespace esphome
