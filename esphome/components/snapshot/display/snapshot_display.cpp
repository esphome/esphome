#ifdef USE_HOST
#include "snapshot_display.h"
#include "esphome/components/display/display_color_utils.h"
#include "esphome/core/log.h"

#include <cstring>

namespace esphome::snapshot {

static const char *const TAG = "snapshot.display";

namespace {

/// Spread a channel that only goes up to `max` over the whole 0 to 255 range, so that the
/// brightest value stays the brightest. This is the same arithmetic SDL uses, which is what makes
/// a picture taken here come out identical to the same picture taken from an SDL window.
constexpr uint8_t expand_channel(uint16_t value, uint16_t max) { return static_cast<uint8_t>(value * 255 / max); }

constexpr uint16_t RED_MAX = 0x1F;
constexpr uint16_t GREEN_MAX = 0x3F;
constexpr uint16_t BLUE_MAX = 0x1F;

}  // namespace

void SnapshotDisplay::setup() {
  this->init_internal_(static_cast<uint32_t>(this->width_) * this->height_ * 2);
  if (this->buffer_ == nullptr) {
    this->mark_failed(LOG_STR("Could not allocate display buffer"));
  }
}

void SnapshotDisplay::dump_config() { LOG_DISPLAY("", "Snapshot", this); }

void SnapshotDisplay::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (this->buffer_ == nullptr || x < 0 || x >= this->width_ || y < 0 || y >= this->height_)
    return;
  this->pixels_()[y * this->width_ + x] = display::ColorUtil::color_to_565(color, display::COLOR_ORDER_RGB);
}

void SnapshotDisplay::draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr,
                                     display::ColorOrder order, display::ColorBitness bitness, bool big_endian,
                                     int x_offset, int y_offset, int x_pad) {
  if (this->buffer_ == nullptr)
    return;
  // Anything that is not already laid out the way the buffer is, or that would reach outside it,
  // goes through the base class, which turns it into one call per pixel with the bounds checked.
  const bool copyable = this->rotation_ == display::DISPLAY_ROTATION_0_DEGREES &&
                        bitness == display::COLOR_BITNESS_565 && !big_endian && x_start >= 0 && y_start >= 0 &&
                        x_start + w <= this->width_ && y_start + h <= this->height_;
  if (!copyable) {
    DisplayBuffer::draw_pixels_at(x_start, y_start, w, h, ptr, order, bitness, big_endian, x_offset, y_offset, x_pad);
    return;
  }
  const size_t stride = static_cast<size_t>(x_offset) + w + x_pad;
  const uint8_t *src = ptr + (stride * y_offset + x_offset) * 2;
  for (int y = 0; y != h; y++) {
    memcpy(&this->pixels_()[(y_start + y) * this->width_ + x_start], src + y * stride * 2, w * 2);
  }
}

bool SnapshotDisplay::capture_bgr(uint8_t *dest, size_t row_stride) {
  if (this->buffer_ == nullptr) {
    ESP_LOGE(TAG, "Snapshot requested but there is no buffer to read");
    return false;
  }
  const uint16_t *src = this->pixels_();
  for (int y = 0; y != this->height_; y++) {
    uint8_t *out = dest + y * row_stride;
    for (int x = 0; x != this->width_; x++) {
      const uint16_t pixel = *src++;
      *out++ = expand_channel(pixel & BLUE_MAX, BLUE_MAX);
      *out++ = expand_channel((pixel >> 5) & GREEN_MAX, GREEN_MAX);
      *out++ = expand_channel(pixel >> 11, RED_MAX);
    }
  }
  return true;
}

}  // namespace esphome::snapshot
#endif
