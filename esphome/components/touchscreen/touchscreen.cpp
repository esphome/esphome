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
  ESP_LOGD(TAG, "Attach Touch Interupt");
}

void Touchscreen::call_setup() {
  if (this->display_ != nullptr) {
    this->display_width_ = this->display_->get_width();
    this->display_height_ = this->display_->get_height();
  }
  PollingComponent::call_setup();
}

void Touchscreen::update() {
  if (!this->store_.init) {
    this->store_.touched = true;
  } else {
    // no need to poll if we have interrupts.
    ESP_LOGW(TAG, "Touch Polling Stopped. You can safely remove the 'update_interval:' variable from the YAML file.");
    this->stop_poller();
  }
}

void Touchscreen::loop() {
  if (this->store_.touched) {
    ESP_LOGVV(TAG, "<< Do Touch loop >>");
    this->first_touch_ = this->touches_.empty();
    this->need_update_ = false;
    this->is_touched_ = false;
    this->skip_update_ = false;
    for (auto &tp : this->touches_) {
      if (tp.second.state == STATE_PRESSED || tp.second.state == STATE_UPDATED) {
        tp.second.state |= STATE_RELEASING;
      } else {
        tp.second.state = STATE_RELEASED;
      }
      tp.second.x_prev = tp.second.x;
      tp.second.y_prev = tp.second.y;
    }
    this->update_touches();
    if (this->skip_update_) {
      for (auto &tp : this->touches_) {
        tp.second.state &= ~STATE_RELEASING;
      }
    } else {
      this->store_.touched = false;
      this->defer([this]() { this->send_touches_(); });
      if (this->touch_timeout_ > 0) {
        // Simulate a touch after <this->touch_timeout_> ms. This will reset any existing timeout operation.
        // This is to detect touch release.
        if (this->is_touched_) {
          this->set_timeout(TAG, this->touch_timeout_, [this]() { this->store_.touched = true; });
        } else {
          this->cancel_timeout(TAG);
        }
      }
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
    tp.y_prev = tp.y;
    tp.x_prev = tp.x;
  }
  tp.x_raw = x_raw;
  tp.y_raw = y_raw;
  tp.z_raw = z_raw;
  if (this->x_raw_max_ != this->x_raw_min_ and this->y_raw_max_ != this->y_raw_min_) {
    x = this->normalize_(x_raw, this->x_raw_min_, this->x_raw_max_, this->invert_x_);
    y = this->normalize_(y_raw, this->y_raw_min_, this->y_raw_max_, this->invert_y_);

    if (this->swap_x_y_) {
      std::swap(x, y);
    }

    tp.x = (uint16_t) ((int) x * this->display_width_ / 0x1000);
    tp.y = (uint16_t) ((int) y * this->display_height_ / 0x1000);
  } else {
    tp.state |= STATE_CALIBRATE;
  }
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
  TouchPoints_t touches;
  ESP_LOGV(TAG, "Touch status: is_touched=%d, was_touched=%d", this->is_touched_, this->was_touched_);
  for (auto tp : this->touches_) {
    ESP_LOGV(TAG, "Touch status: %d/%d: raw:(%4d,%4d,%4d) calc:(%3d,%4d)", tp.second.id, tp.second.state,
             tp.second.x_raw, tp.second.y_raw, tp.second.z_raw, tp.second.x, tp.second.y);
    touches.push_back(tp.second);
  }
  if (this->need_update_ || (!this->is_touched_ && this->was_touched_)) {
    this->update_trigger_.trigger(touches);
    for (auto *listener : this->touch_listeners_) {
      listener->update(touches);
    }
  }
  if (!this->is_touched_) {
    if (this->was_touched_) {
      this->release_trigger_.trigger();
      for (auto *listener : this->touch_listeners_)
        listener->release();
      this->touches_.clear();
    }
  } else {
    if (this->first_touch_) {
      TouchPoint tp = this->touches_.begin()->second;
      this->touch_trigger_.trigger(tp, touches);
      for (auto *listener : this->touch_listeners_) {
        listener->touch(tp);
      }
    }
  }
  this->was_touched_ = this->is_touched_;
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
