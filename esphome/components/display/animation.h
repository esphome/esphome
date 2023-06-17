#pragma once
#include "image.h"

namespace esphome {
namespace display {

class Animation : public Image {
 public:
  Animation(const uint8_t *data_start, int width, int height, uint32_t animation_frame_count, ImageType type);

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

}  // namespace display
}  // namespace esphome
