#pragma once
#include "esphome/components/image/image.h"

#include "esphome/core/automation.h"

namespace esphome {
namespace animation {

class Animation : public image::Image {
 public:
  Animation(const uint8_t *data_start, int width, int height, uint32_t animation_frame_count, image::ImageType type,
            image::Transparency transparent, const uint32_t *frame_durations = nullptr,
            uint32_t default_duration_ms = 100);

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

  /** Get duration for specific frame (milliseconds). */
  uint32_t get_frame_duration(int frame) const;

  /** Get average duration across all frames (milliseconds). */
  uint32_t get_average_duration() const;

  /** Get total animation duration (milliseconds). */
  uint32_t get_total_duration() const;

  /** Check if frame timing data is available. */
  bool has_frame_timing() const;

  /** Get base animation data pointer (for LVGL descriptor generation). */
  const uint8_t *get_animation_data_start() const;

 protected:
  void update_data_start_();

  const uint8_t *animation_data_start_;
  int current_frame_;
  uint32_t animation_frame_count_;
  uint32_t loop_start_frame_;
  uint32_t loop_end_frame_;
  int loop_count_;
  int loop_current_iteration_;
  const uint32_t *frame_durations_{nullptr};
  uint32_t default_frame_duration_{100};
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
