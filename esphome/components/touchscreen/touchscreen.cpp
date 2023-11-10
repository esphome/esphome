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

void Touchscreen::update() {
  if (!this->store_.init) {
    this->store_.touched = true;
  }
}
void Touchscreen::loop() {
  if (this->store_.touched) {
    this->store_.touched = false;
    this->first_touch_ = this->touches_.empty();
    this->need_update_ = false;
    this->is_touched_ = false;
    for (auto &i : this->touches_) {
      i.second.state = 0;
      i.second.x_prev = i.second.x;
      i.second.y_prev = i.second.y;
    }
    this->update_touches();
    this->send_touches_();
  }
}

void Touchscreen::set_raw_touch_posistion_(uint8_t id, int16_t x_raw, int16_t y_raw, int16_t z_raw) {
  TouchPoint tp;
  uint16_t x, y;
  if (this->touches_.count(id) == 0) {
    tp.state = 1;
  } else {
    tp = this->touches_[id];
    tp.state = 2;
  }
  tp.x_raw = x_raw;
  tp.y_raw = y_raw;
  tp.z_raw = z_raw;
  
  x = normalize(x_raw, this->x_raw_min_, this->x_raw_max_, this->invert_x_);
  y = normalize(y_raw, this->y_raw_min_, this->y_raw_max_, this->invert_y_);

  if (this->swap_x_y_) {
    std::swap(x, y);
  }

  switch (this->get_rotation_()) {
    case ROTATE_0_DEGREES:
      break;
    case ROTATE_90_DEGREES:
      std::swap(x, y);
      y = 0xfff - y;
      break;
    case ROTATE_180_DEGREES:
      x = 0xfff - x;
      y = 0xfff - y;
      break;
    case ROTATE_270_DEGREES:
      std::swap(x, y);
      x = 0xfff - x;
      break;
  }

  tp.x = (uint16_t) ((int) x * this->get_width_() / 0xfff);
  tp.y = (uint16_t) ((int) y * this->get_height_() / 0xfff);
  if (tp.state == 1) {
     tp.x_org = tp.x;
     tp.y_org = tp.y;
  }

  this->touches_[id] = tp;

  this->is_touched_ = true;
  if ((tp.x != tp.x_prev) || (tp.y != tp.y_prev)) {
    this->need_update_ = true;
  }
}

void Touchscreen::send_touches_() {
  if (!this->is_touched_) {
    this->release_trigger_.trigger();
    for (auto *listener : this->touch_listeners_)
      listener->release();
    this->touches_.clear();
  } else {
    TouchPoints_t touches;
    for (auto i : this->touches_) {
      if (i.second.state != 0) {
        touches.push_back(i.second);
      }
    }
    if (this->first_touch_) {
      TouchPoint tp = touches.front();
      this->touch_trigger_.trigger(tp, touches);
      for (auto *listener : this->touch_listeners_) {
        listener->touch(tp);
        listener->touch(touches);
      }
    } else {
      TouchPoint tp = touches.front();
      this->update_trigger_.trigger(touches);
      for (auto *listener : this->touch_listeners_) {
        listener->update(touches);
      }
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

}  // namespace touchscreen
}  // namespace esphome
