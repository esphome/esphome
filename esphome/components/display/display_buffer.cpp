#include "display_buffer.h"

#include <utility>

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
  if (this->is_point_clipped(x, y))
    return;  // NOLINT

  switch (this->rotation_) {
    case DISPLAY_ROTATION_0_DEGREES:
      break;
    case DISPLAY_ROTATION_90_DEGREES:
      std::swap(x, y);
      x = this->internal_width_() - x - 1;
      break;
    case DISPLAY_ROTATION_180_DEGREES:
      x = this->internal_width_() - x - 1;
      y = this->internal_height_() - y - 1;
      break;
    case DISPLAY_ROTATION_270_DEGREES:
      std::swap(x, y);
      y = this->internal_height_() - y - 1;
      break;
  }
  this->draw_absolute_pixel_internal(x, y, color);
  this->feed_wdt_per_pixel_();
}

}  // namespace esphome::display
