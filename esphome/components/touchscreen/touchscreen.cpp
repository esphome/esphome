#include "touchscreen.h"

#include "esphome/core/log.h"

namespace esphome {
namespace touchscreen {

static const char *const TAG = "touchscreen";

void Touchscreen::set_display(display::Display *display) {
  this->display_ = display;
  this->display_width_ = display->get_width();
  this->display_height_ = display->get_height();
  this->rotation_ = static_cast<TouchRotation>(display->get_rotation());

  if (this->rotation_ == ROTATE_90_DEGREES || this->rotation_ == ROTATE_270_DEGREES) {
    std::swap(this->display_width_, this->display_height_);
  }
}

void Touchscreen::send_release_() {
  for (auto *listener : this->touch_listeners_)
    listener->release();
}

void Touchscreen::send_touch_(TouchPoint tp) {
  ESP_LOGV(TAG, "Touch (x=%d, y=%d)", tp.x, tp.y);
  this->touch_trigger_.trigger(tp);
  for (auto *listener : this->touch_listeners_)
    listener->touch(tp);
}

}  // namespace touchscreen
}  // namespace esphome
