#include "display_buffer.h"

#include <utility>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::display {

static const char *const TAG = "display";

void DisplayBuffer::init_internal_(uint32_t buffer_length) {
  RAMAllocator<uint8_t> allocator;
  this->buffer_ = allocator.allocate(buffer_length);
  if (this->buffer_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate buffer for display!");
    return;
  }
  this->clear();
}

int DisplayBuffer::get_width() {
  switch (this->rotation_) {
    case DISPLAY_ROTATION_90_DEGREES:
    case DISPLAY_ROTATION_270_DEGREES:
      return this->get_height_internal();
    case DISPLAY_ROTATION_0_DEGREES:
    case DISPLAY_ROTATION_180_DEGREES:
    default:
      return this->get_width_internal();
  }
}

int DisplayBuffer::get_height() {
  switch (this->rotation_) {
    case DISPLAY_ROTATION_0_DEGREES:
    case DISPLAY_ROTATION_180_DEGREES:
      return this->get_height_internal();
    case DISPLAY_ROTATION_90_DEGREES:
    case DISPLAY_ROTATION_270_DEGREES:
    default:
      return this->get_width_internal();
  }
}

void HOT DisplayBuffer::draw_pixel_at(int x, int y, Color color) {
  // Test the active clipping rectangle in place; copying it out through
  // get_clipping() cost a call and a copy per pixel when nothing is clipped.
  if (!this->clipping_rectangle_.empty() && !this->clipping_rectangle_.back().inside(x, y))
    return;  // NOLINT

  switch (this->rotation_) {
    case DISPLAY_ROTATION_0_DEGREES:
      break;
    case DISPLAY_ROTATION_90_DEGREES:
      std::swap(x, y);
      x = this->get_width_internal() - x - 1;
      break;
    case DISPLAY_ROTATION_180_DEGREES:
      x = this->get_width_internal() - x - 1;
      y = this->get_height_internal() - y - 1;
      break;
    case DISPLAY_ROTATION_270_DEGREES:
      std::swap(x, y);
      y = this->get_height_internal() - y - 1;
      break;
  }
  this->draw_absolute_pixel_internal(x, y, color);
  // Feeding the watchdog reads the clock, so do it every 256 pixels rather
  // than every pixel; that is microseconds, far inside any watchdog window.
  if (++this->wdt_pixel_counter_ == 0)
    App.feed_wdt();
}

}  // namespace esphome::display
