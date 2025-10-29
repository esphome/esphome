#pragma once

#ifdef USE_HOST
#include "../sdl_esphome.h"
#include "esphome/components/touchscreen/touchscreen.h"

namespace esphome {
namespace sdl {

class SdlTouchscreen : public touchscreen::Touchscreen, public Parented<Sdl> {
 public:
  void setup() override {
    this->x_raw_max_ = this->display_->get_width();
    this->y_raw_max_ = this->display_->get_height();
  }

  void update_touches() override {
    if (this->parent_->mouse_down) {
      add_raw_touch_position_(0, this->parent_->mouse_x, this->parent_->mouse_y);
    }
  }
};

}  // namespace sdl
}  // namespace esphome
#endif
