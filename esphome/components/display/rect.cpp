#include "rect.h"

#include "esphome/core/log.h"

namespace esphome {
namespace display {

static const char *const TAG = "display";

void Rect::expand(int16_t horizontal, int16_t vertical) {
  if (this->is_set() && (this->w >= (-2 * horizontal)) && (this->h >= (-2 * vertical))) {
    this->x = this->x - horizontal;
    this->y = this->y - vertical;
    this->w = this->w + (2 * horizontal);
    this->h = this->h + (2 * vertical);
  }
}

void Rect::extend(Rect rect) {
  if (!this->is_set()) {
    this->x = rect.x;
    this->y = rect.y;
    this->w = rect.w;
    this->h = rect.h;
  } else {
    if (this->x > rect.x) {
      this->w = this->w + (this->x - rect.x);
      this->x = rect.x;
    }
    if (this->y > rect.y) {
      this->h = this->h + (this->y - rect.y);
      this->y = rect.y;
    }
    if (this->x2() < rect.x2()) {
      this->w = rect.x2() - this->x;
    }
    if (this->y2() < rect.y2()) {
      this->h = rect.y2() - this->y;
    }
  }
}
void Rect::shrink(Rect rect) {
  if (!this->inside(rect)) {
    (*this) = Rect();
  } else {
    if (this->x2() > rect.x2()) {
      this->w = rect.x2() - this->x;
    }
    if (this->x < rect.x) {
      this->w = this->w + (this->x - rect.x);
      this->x = rect.x;
    }
    if (this->y2() > rect.y2()) {
      this->h = rect.y2() - this->y;
    }
    if (this->y < rect.y) {
      this->h = this->h + (this->y - rect.y);
      this->y = rect.y;
    }
  }
}

bool Rect::equal(Rect rect) const {
  return (rect.x == this->x) && (rect.w == this->w) && (rect.y == this->y) && (rect.h == this->h);
}

bool Rect::inside(int16_t test_x, int16_t test_y, bool absolute) const {  // NOLINT
  if (!this->is_set()) {
    return true;
  }
  if (absolute) {
    return ((test_x >= this->x) && (test_x <= this->x2()) && (test_y >= this->y) && (test_y <= this->y2()));
  } else {
    return ((test_x >= 0) && (test_x <= this->w) && (test_y >= 0) && (test_y <= this->h));
  }
}

bool Rect::inside(Rect rect, bool absolute) const {
  if (!this->is_set() || !rect.is_set()) {
    return true;
  }
  if (absolute) {
    return ((rect.x <= this->x2()) && (rect.x2() >= this->x) && (rect.y <= this->y2()) && (rect.y2() >= this->y));
  } else {
    return ((rect.x <= this->w) && (rect.w >= 0) && (rect.y <= this->h) && (rect.h >= 0));
  }
}

void Rect::info(const std::string &prefix) {
  if (this->is_set()) {
    ESP_LOGI(TAG, "%s [%3d,%3d,%3d,%3d] (%3d,%3d)", prefix.c_str(), this->x, this->y, this->w, this->h, this->x2(),
             this->y2());
  } else {
    ESP_LOGI(TAG, "%s ** IS NOT SET **", prefix.c_str());
  }
}

}  // namespace display
}  // namespace esphome
