#include "touchscreen.h"

#include "esphome/core/log.h"

namespace esphome {
namespace touchscreen {

static const char *const TAG = "touchscreen";

void TouchscreenInterrupt::gpio_intr(TouchscreenInterrupt *store) { store->touched = true; }

void Touchscreen::attach_interrupt_(InternalGPIOPin *irq_pin, esphome::gpio::InterruptType type) {
  irq_pin->attach_interrupt(TouchscreenInterrupt::gpio_intr, &this->store_, type);
  this->store_.init = true;
  this->store_.touched = false;
}

void Touchscreen::update() {
  if (!this->store_.init) {
    this->store_.touched = true;
  } else {
    // no need to poll if we have interrupts.
    this->stop_poller();
  }
}

void Touchscreen::loop() {
  if (this->store_.touched) {
    this->first_touch_ = this->touches_.empty();
    this->need_update_ = false;
    this->is_touched_ = false;
    this->skip_update_ = false;
    for (auto &tp : this->touches_) {
      if (tp.second.state == STATE_PRESSED || tp.second.state == STATE_UPDATED) {
        tp.second.state = tp.second.state | STATE_RELEASING;
      } else {
        tp.second.state = STATE_RELEASED;
      }
      tp.second.x_prev = tp.second.x;
      tp.second.y_prev = tp.second.y;
    }
    this->update_touches();
    if (this->skip_update_) {
      for (auto &tp : this->touches_) {
        tp.second.state = tp.second.state & -STATE_RELEASING;
      }
    } else {
      this->store_.touched = false;
      this->defer([this]() { this->send_touches_(); });
    }
  }
}

void Touchscreen::add_raw_touch_position_(uint8_t id, int16_t x_raw, int16_t y_raw, int16_t z_raw) {
  TouchPoint tp;
  uint16_t x, y;
  if (this->touches_.count(id) == 0) {
    tp.state = STATE_PRESSED;
    tp.id = id;
  } else {
    tp = this->touches_[id];
    tp.state = STATE_UPDATED;
  }
  tp.x_raw = x_raw;
  tp.y_raw = y_raw;
  tp.z_raw = z_raw;

  x = this->normalize_(x_raw, this->x_raw_min_, this->x_raw_max_, this->invert_x_);
  y = this->normalize_(y_raw, this->y_raw_min_, this->y_raw_max_, this->invert_y_);

  if (this->swap_x_y_) {
    std::swap(x, y);
  }

  tp.x = (uint16_t) ((int) x * this->get_width_() / 0x1000);
  tp.y = (uint16_t) ((int) y * this->get_height_() / 0x1000);

  if (tp.state == STATE_PRESSED) {
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
    for (auto tp : this->touches_) {
      touches.push_back(tp.second);
    }
    if (this->first_touch_) {
      TouchPoint tp = this->touches_.begin()->second;
      this->touch_trigger_.trigger(tp, touches);
      for (auto *listener : this->touch_listeners_) {
        listener->touch(tp);
      }
    }
    if (this->need_update_) {
      this->update_trigger_.trigger(touches);
      for (auto *listener : this->touch_listeners_) {
        listener->update(touches);
      }
    }
  }
}

int16_t Touchscreen::normalize_(int16_t val, int16_t min_val, int16_t max_val, bool inverted) {
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
