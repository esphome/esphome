#include "touchscreen.h"

#include "esphome/core/log.h"

namespace esphome {
namespace touchscreen {

static const char *const TAG = "touchscreen";

void TouchscreenInterupt::gpio_intr(TouchscreenInterupt *store) { store->touched = true; }

void Touchscreen::attach_interrupt_(InternalGPIOPin *irq_pin, esphome::gpio::InterruptType type) {
  irq_pin->attach_interrupt(TouchscreenInterupt::gpio_intr, &this->store_, type);
  this->store_.init = true;
  this->store_.touched = false;
}

void Touchscreen::send_touch_(TouchPoints_t touches) {
  bool first_touch = this->first_touch_.empty();

  for (auto &i : this->first_touch_) {
    i.second.state = 0;
  }

  for (auto &tp : touches) {
    tp.x = normalize(tp.x_raw, this->x_raw_min_, this->x_raw_max_, this->invert_x_);
    tp.y = normalize(tp.y_raw, this->y_raw_min_, this->y_raw_max_, this->invert_y_);

    if (this->swap_x_y_) {
      std::swap(tp.x, tp.y);
    }

    switch (static_cast<TouchRotation>(this->get_rotation_())) {
      case ROTATE_0_DEGREES:
        break;
      case ROTATE_90_DEGREES:
        std::swap(tp.x, tp.y);
        tp.y = 0xfff - tp.y;
        break;
      case ROTATE_180_DEGREES:
        tp.x = 0xfff - tp.x;
        tp.y = 0xfff - tp.y;
        break;
      case ROTATE_270_DEGREES:
        std::swap(tp.x, tp.y);
        tp.x = 0xfff - tp.x;
        break;
    }

    tp.x = (int16_t) ((int) tp.x * this->get_width_() / 0xfff);
    tp.y = (int16_t) ((int) tp.y * this->get_height_() / 0xfff);

    if (this->first_touch_.count(tp.id) == 0) {
      this->first_touch_[tp.id] = tp;
      this->first_touch_[tp.id].state = 1;
      tp.state = 1;
    } else {
      this->first_touch_[tp.id].state = 2;
      tp.state = 2;
    }
  }

  if (touches.empty()) {
    this->release_trigger_.trigger();
    for (auto *listener : this->touch_listeners_)
      listener->release();
    this->first_touch_.clear();
  } else if (first_touch) {
    TouchPoint tp = touches.front();
    ESP_LOGV(TAG, "Touch (x=%d, y=%d)", tp.x, tp.y);
    this->touch_trigger_.trigger(tp, touches);
    for (auto *listener : this->touch_listeners_) {
      listener->touch(tp);
      listener->touch(touches);
    }
  } else {
    TouchPoint tp = touches.front();
    ESP_LOGV(TAG, "Update Touch (x=%d, y=%d)", tp.x, tp.y);
    this->update_trigger_.trigger(touches);
    for (auto *listener : this->touch_listeners_) {
      listener->update(touches);
    }
  }
}

int16_t Touchscreen::normalize(int16_t val, int16_t min_val, int16_t max_val, bool inverted) {
  int16_t ret;

  if (val <= min_val) {
    ret = 0;
  } else if (val >= max_val) {
    ret = 0xfff;
  } else {
    ret = (int16_t) ((int) 0xfff * (val - min_val) / (max_val - min_val));
  }

  ret = (inverted) ? 0xfff - ret : ret;

  return ret;
}

void Touchscreen::loop() {
  if (this->store_.touched) {
    this->store_.touched = false;
    TouchPoints_t touches;
    handle_touch(touches);
    send_touch_(touches);
  }
}

void Touchscreen::update() {
  if (!this->store_.init) {
    this->store_.touched = true;
  }
}

}  // namespace touchscreen
}  // namespace esphome
